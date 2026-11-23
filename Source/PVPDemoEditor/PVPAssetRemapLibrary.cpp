// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPAssetRemapLibrary.h"

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Blueprint.h"
#include "UObject/FindObjectFlags.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPAssetRemap, Log, All);

namespace
{
	/** Writes the report object as pretty JSON; creates the target directory when missing. */
	bool WriteReport(const FString& ReportPath, const TSharedRef<FJsonObject>& Report)
	{
		if (ReportPath.IsEmpty())
		{
			return false;
		}

		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		if (!FJsonSerializer::Serialize(Report, Writer))
		{
			UE_LOG(LogPVPAssetRemap, Error, TEXT("failed to serialize remap report"));
			return false;
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
		return FFileHelper::SaveStringToFile(Serialized, *ReportPath);
	}

	/**
	 * Blueprints are usually referenced through their generated class, so mapping the UBlueprint object
	 * alone would leave class references (anim notifies, components, spawn classes) untouched.
	 */
	UObject* ResolveRemapTarget(UObject* Loaded)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Loaded))
		{
			return Blueprint->GeneratedClass;
		}
		return Loaded;
	}

	/** Recursively walks a property (structs / arrays included) and remaps every soft object path found. */
	void RemapSoftPathsRecursive(FProperty* Property, void* ValuePtr,
		TFunctionRef<bool(FSoftObjectPath&)> RemapSoft,
		TFunctionRef<bool(FTopLevelAssetPath&)> RemapTopLevel,
		TFunctionRef<bool(FString&)> RemapString,
		int32& OutCount)
	{
		if (!Property || !ValuePtr)
		{
			return;
		}

		if (FSoftObjectProperty* SoftProperty = CastField<FSoftObjectProperty>(Property))
		{
			FSoftObjectPtr* SoftPtr = static_cast<FSoftObjectPtr*>(ValuePtr);
			FSoftObjectPath Path = SoftPtr->ToSoftObjectPath();
			if (RemapSoft(Path))
			{
				*SoftPtr = FSoftObjectPtr(Path);
				++OutCount;
			}
			return;
		}

		if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			if (RemapString(*static_cast<FString*>(ValuePtr)))
			{
				++OutCount;
			}
			return;
		}

		if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			FName* Name = static_cast<FName*>(ValuePtr);
			FString Value = Name->ToString();
			if (RemapString(Value))
			{
				*Name = FName(*Value);
				++OutCount;
			}
			return;
		}

		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == TBaseStructure<FSoftObjectPath>::Get())
			{
				if (RemapSoft(*static_cast<FSoftObjectPath*>(ValuePtr)))
				{
					++OutCount;
				}
				return;
			}
			if (StructProperty->Struct == TBaseStructure<FTopLevelAssetPath>::Get())
			{
				if (RemapTopLevel(*static_cast<FTopLevelAssetPath*>(ValuePtr)))
				{
					++OutCount;
				}
				return;
			}

			for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
			{
				RemapSoftPathsRecursive(*It, It->ContainerPtrToValuePtr<void>(ValuePtr), RemapSoft, RemapTopLevel, RemapString, OutCount);
			}
			return;
		}

		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, ValuePtr);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				RemapSoftPathsRecursive(ArrayProperty->Inner, Helper.GetRawPtr(Index), RemapSoft, RemapTopLevel, RemapString, OutCount);
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> ToJsonArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Reserve(Values.Num());
		for (const FString& Value : Values)
		{
			Result.Add(MakeShared<FJsonValueString>(Value));
		}
		return Result;
	}
}

bool UPVPAssetRemapLibrary::RemapAssetReferences(const TArray<FString>& InFromPackages,
	const TArray<FString>& InToPackages,
	const TArray<FString>& InPackagesToScan,
	const FString& InReportPath)
{
	const double StartSeconds = FPlatformTime::Seconds();

	const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("tool"), TEXT("UPVPAssetRemapLibrary::RemapAssetReferences"));
	Report->SetNumberField(TEXT("mappings_requested"), InFromPackages.Num());
	Report->SetNumberField(TEXT("scan_prefixes"), InPackagesToScan.Num());

	auto Fail = [&Report, &InReportPath, StartSeconds](const FString& Reason) -> bool
	{
		Report->SetStringField(TEXT("result"), TEXT("failed"));
		Report->SetStringField(TEXT("error"), Reason);
		Report->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - StartSeconds);
		WriteReport(InReportPath, Report);
		UE_LOG(LogPVPAssetRemap, Error, TEXT("RemapAssetReferences failed: %s"), *Reason);
		return false;
	};

	if (InFromPackages.Num() == 0 || InFromPackages.Num() != InToPackages.Num())
	{
		return Fail(FString::Printf(TEXT("mapping size mismatch: from=%d to=%d"),
			InFromPackages.Num(), InToPackages.Num()));
	}
	if (InPackagesToScan.Num() == 0)
	{
		return Fail(TEXT("no scan prefixes provided"));
	}

	// 1) Resolve each mapping to concrete objects. A missing side is reported and skipped so the caller
	//    can copy the asset or fix the mapping without losing the mappings that do resolve.
	TArray<UObject*> NewObjects;
	TArray<TArray<UObject*>> OldObjectArrays;
	TArray<FString> MissingFrom;
	TArray<FString> MissingTo;
	NewObjects.Reserve(InFromPackages.Num());
	OldObjectArrays.Reserve(InFromPackages.Num());

	for (int32 Index = 0; Index < InFromPackages.Num(); ++Index)
	{
		const FString& FromPath = InFromPackages[Index];
		const FString& ToPath = InToPackages[Index];

		UObject* OldObject = ResolveRemapTarget(LoadObject<UObject>(nullptr, *FromPath, nullptr, LOAD_NoWarn | LOAD_Quiet));
		UObject* NewObject = ResolveRemapTarget(LoadObject<UObject>(nullptr, *ToPath, nullptr, LOAD_NoWarn | LOAD_Quiet));

		if (!OldObject)
		{
			MissingFrom.Add(FromPath);
			continue;
		}
		if (!NewObject)
		{
			MissingTo.Add(ToPath);
			continue;
		}

		NewObjects.Add(NewObject);
		OldObjectArrays.AddDefaulted();
		OldObjectArrays.Last().Add(OldObject);
	}

	Report->SetNumberField(TEXT("mappings_resolved"), NewObjects.Num());
	Report->SetArrayField(TEXT("missing_from"), ToJsonArray(MissingFrom));
	Report->SetArrayField(TEXT("missing_to"), ToJsonArray(MissingTo));

	if (NewObjects.Num() == 0)
	{
		return Fail(TEXT("no mapping resolved to loaded objects"));
	}

	// 2) Collect every object inside the scanned packages: ForceReplaceReferences only rewrites
	//    references held by objects in this set.
	IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
	TSet<UObject*> ObjectsToReplaceWithin;
	TSet<UPackage*> PackagesTouched;
	int32 AssetsScanned = 0;
	int32 ObjectsScanned = 0;

	for (const FString& Prefix : InPackagesToScan)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(*Prefix));
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = false;

		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssets(Filter, AssetDataList);

		for (const FAssetData& AssetData : AssetDataList)
		{
			UPackage* Package = AssetData.GetPackage();
			if (!Package)
			{
				continue;
			}

			++AssetsScanned;
			PackagesTouched.Add(Package);

			ForEachObjectWithPackage(Package, [&ObjectsToReplaceWithin, &ObjectsScanned](UObject* Object)
			{
				if (Object)
				{
					ObjectsToReplaceWithin.Add(Object);
					++ObjectsScanned;
				}
				return true;
			}, EGetObjectsFlags::IncludeNestedObjects);
		}
	}

	Report->SetNumberField(TEXT("assets_scanned"), AssetsScanned);
	Report->SetNumberField(TEXT("objects_scanned"), ObjectsScanned);

	if (ObjectsToReplaceWithin.Num() == 0)
	{
		return Fail(TEXT("scan prefixes matched no loaded objects"));
	}

	// 3) Batch replacement. Views are built only after the backing arrays are final so no reallocation
	//    can invalidate them.
	TArray<ObjectTools::FReplaceRequest> Requests;
	Requests.Reserve(OldObjectArrays.Num());
	for (int32 Index = 0; Index < OldObjectArrays.Num(); ++Index)
	{
		ObjectTools::FReplaceRequest Request;
		Request.New = NewObjects[Index];
		Request.Old = MakeArrayView(OldObjectArrays[Index]);
		Requests.Add(Request);
	}

	ObjectTools::ForceReplaceReferences(Requests, ObjectsToReplaceWithin);

	// 3b) Two reference kinds are not covered by the reflected reference walk:
	//     - instanced anim notify objects: their class lives in the object's class field, not in a property;
	//     - soft object paths: they are serialized strings, not hard UObject references.
	//     Rebuild notify instances from the project copy and rewrite soft paths by package prefix.
	TMap<UClass*, UClass*> ClassMap;
	for (int32 Index = 0; Index < NewObjects.Num(); ++Index)
	{
		UClass* OldClass = Cast<UClass>(OldObjectArrays[Index].Num() > 0 ? OldObjectArrays[Index][0] : nullptr);
		UClass* NewClass = Cast<UClass>(NewObjects[Index]);
		if (OldClass && NewClass)
		{
			ClassMap.Add(OldClass, NewClass);
		}
	}

	auto RemapSoftPath = [&InFromPackages, &InToPackages](FSoftObjectPath& Path) -> bool
	{
		const FString Current = Path.ToString();
		for (int32 Index = 0; Index < InFromPackages.Num(); ++Index)
		{
			if (Current == InFromPackages[Index] || Current.StartsWith(InFromPackages[Index] + TEXT(".")))
			{
				const FString ObjectSuffix = Current.RightChop(InFromPackages[Index].Len());
				Path.SetPath(InToPackages[Index] + ObjectSuffix);
				return true;
			}
		}
		return false;
	};

	auto RemapTopLevelPath = [&InFromPackages, &InToPackages](FTopLevelAssetPath& Path) -> bool
	{
		const FString PackageName = Path.GetPackageName().ToString();
		for (int32 Index = 0; Index < InFromPackages.Num(); ++Index)
		{
			if (PackageName == InFromPackages[Index])
			{
				Path = FTopLevelAssetPath(FName(*InToPackages[Index]), Path.GetAssetName());
				return true;
			}
		}
		return false;
	};

	auto RemapStringPath = [&InFromPackages, &InToPackages](FString& Value) -> bool
	{
		for (int32 Index = 0; Index < InFromPackages.Num(); ++Index)
		{
			if (Value == InFromPackages[Index] || Value.StartsWith(InFromPackages[Index] + TEXT(".")))
			{
				Value = InToPackages[Index] + Value.RightChop(InFromPackages[Index].Len());
				return true;
			}
		}
		return false;
	};

	int32 NotifyInstancesRemapped = 0;
	int32 SoftPathsRemapped = 0;
	TArray<UPackage*> ExtraDirtyPackages;

	for (UObject* Object : ObjectsToReplaceWithin)
	{
		bool bObjectChanged = false;

		// Instanced anim notifies (montage / sequence notify objects).
		if (UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Object))
		{
			auto RemapNotifyInstance = [&](UObject* OldInstance) -> UObject*
			{
				if (!OldInstance)
				{
					return nullptr;
				}
				UClass* const* NewClassPtr = ClassMap.Find(OldInstance->GetClass());
				if (!NewClassPtr || !*NewClassPtr)
				{
					return nullptr;
				}
				UObject* NewInstance = NewObject<UObject>(Sequence, *NewClassPtr, NAME_None, RF_Transactional);
				if (!NewInstance)
				{
					return nullptr;
				}
				for (TFieldIterator<FProperty> It(OldInstance->GetClass()); It; ++It)
				{
					if (It->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
					{
						continue;
					}
					It->CopyCompleteValue_InContainer(NewInstance, OldInstance);
				}
				++NotifyInstancesRemapped;
				return NewInstance;
			};

			for (FAnimNotifyEvent& Event : Sequence->Notifies)
			{
				if (UObject* NewNotify = RemapNotifyInstance(Event.Notify))
				{
					Event.Notify = Cast<UAnimNotify>(NewNotify);
					bObjectChanged = true;
				}
				if (UObject* NewNotifyState = RemapNotifyInstance(Event.NotifyStateClass))
				{
					Event.NotifyStateClass = Cast<UAnimNotifyState>(NewNotifyState);
					bObjectChanged = true;
				}
			}
		}

		// Soft references (preview meshes, soft class paths, ...), including ones nested in structs / arrays.
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			void* ValuePtr = It->ContainerPtrToValuePtr<void>(Object);
			int32 Remapped = 0;
			RemapSoftPathsRecursive(*It, ValuePtr, RemapSoftPath, RemapTopLevelPath, RemapStringPath, Remapped);
			if (Remapped > 0)
			{
				SoftPathsRemapped += Remapped;
				bObjectChanged = true;
			}
		}

		if (bObjectChanged)
		{
			Object->MarkPackageDirty();
			ExtraDirtyPackages.AddUnique(Object->GetOutermost());
		}
	}

	UE_LOG(LogPVPAssetRemap, Log, TEXT("extra remaps: notify instances=%d soft paths=%d objects=%d"),
		NotifyInstancesRemapped, SoftPathsRemapped, ExtraDirtyPackages.Num());
	for (UPackage* ExtraPackage : ExtraDirtyPackages)
	{
		PackagesTouched.Add(ExtraPackage);
	}

	// 4) Persist only what the replacement actually dirtied.
	TArray<UPackage*> DirtyPackages;
	for (UPackage* Package : PackagesTouched)
	{
		if (Package && Package->IsDirty())
		{
			DirtyPackages.Add(Package);
		}
	}

	bool bSaved = true;
	if (DirtyPackages.Num() > 0)
	{
		bSaved = UEditorLoadingAndSavingUtils::SavePackages(DirtyPackages, /*bOnlyDirty*/ true);
	}

	Report->SetNumberField(TEXT("packages_dirty"), DirtyPackages.Num());
	Report->SetBoolField(TEXT("saved"), bSaved);
	Report->SetBoolField(TEXT("ok"), bSaved && MissingFrom.Num() == 0 && MissingTo.Num() == 0);
	Report->SetStringField(TEXT("result"), (bSaved && MissingFrom.Num() == 0 && MissingTo.Num() == 0) ? TEXT("ok") : TEXT("partial"));
	Report->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - StartSeconds);

	WriteReport(InReportPath, Report);

	UE_LOG(LogPVPAssetRemap, Log,
		TEXT("remap done: mappings=%d resolved=%d assets=%d objects=%d dirty=%d saved=%d missing_from=%d missing_to=%d"),
		InFromPackages.Num(), NewObjects.Num(), AssetsScanned, ObjectsScanned,
		DirtyPackages.Num(), bSaved ? 1 : 0, MissingFrom.Num(), MissingTo.Num());

	return bSaved && MissingFrom.Num() == 0 && MissingTo.Num() == 0;
}

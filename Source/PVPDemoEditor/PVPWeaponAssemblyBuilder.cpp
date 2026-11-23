// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPWeaponAssemblyBuilder.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPWeaponAssembly, Log, All);

namespace
{
	const TCHAR* AttachmentTagPrefix = TEXT("PVP.Attachment.");

	/** Socket owner used by every attachment that is not parented to the handguard. */
	const FName DefaultParentComponentName(TEXT("Mesh1P"));

	bool WriteJsonReport(const FString& ReportPath, const TSharedRef<FJsonObject>& Report)
	{
		if (ReportPath.IsEmpty())
		{
			return false;
		}

		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		if (!FJsonSerializer::Serialize(Report, Writer))
		{
			UE_LOG(LogPVPWeaponAssembly, Error, TEXT("failed to serialize weapon assembly report"));
			return false;
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
		return FFileHelper::SaveStringToFile(Serialized, *ReportPath);
	}

	FString ObjectPathName(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString();
	}

	bool IsAttachmentComponent(const UActorComponent* Component)
	{
		if (!Component)
		{
			return false;
		}

		for (const FName& Tag : Component->ComponentTags)
		{
			if (Tag.ToString().StartsWith(AttachmentTagPrefix))
			{
				return true;
			}
		}
		return false;
	}

	bool IsAttachmentNode(const USCS_Node* Node)
	{
		return Node && IsAttachmentComponent(Node->ComponentTemplate);
	}

	UBlueprint* LoadWeaponBlueprint(const FString& BlueprintPath, TArray<TSharedPtr<FJsonValue>>& Errors)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
		if (!Blueprint)
		{
			Errors.Add(MakeShared<FJsonValueString>(
				FString::Printf(TEXT("cannot load weapon blueprint: %s"), *BlueprintPath)));
			return nullptr;
		}

		if (!Blueprint->GeneratedClass)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}

		if (!Blueprint->SimpleConstructionScript)
		{
			Errors.Add(MakeShared<FJsonValueString>(
				FString::Printf(TEXT("%s has no SimpleConstructionScript"), *BlueprintPath)));
			return nullptr;
		}

		return Blueprint;
	}

	/** Native (C++) component template of the blueprint CDO, e.g. Mesh1P created in APVPWeaponActor. */
	USceneComponent* FindNativeComponentTemplate(UBlueprint* Blueprint, FName ComponentName)
	{
		if (!Blueprint || !Blueprint->GeneratedClass)
		{
			return nullptr;
		}

		AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
		if (!CDO)
		{
			return nullptr;
		}

		TArray<USceneComponent*> SceneComponents;
		CDO->GetComponents<USceneComponent>(SceneComponents);
		for (USceneComponent* Component : SceneComponents)
		{
			if (Component && Component->GetFName() == ComponentName)
			{
				return Component;
			}
		}
		return nullptr;
	}

	FName ResolveComponentName(const FPVPWeaponAttachmentSpec& Spec)
	{
		return Spec.ComponentName.IsNone()
			? PVPWeaponAttachment::MakeComponentName(Spec.Slot)
			: Spec.ComponentName;
	}

	TSharedPtr<FJsonObject> MakeComponentJson(const USCS_Node* Node, USimpleConstructionScript* SCS)
	{
		const TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		const UStaticMeshComponent* Template = Cast<UStaticMeshComponent>(Node->ComponentTemplate);

		// Native parents are stored on the node; SCS parents live in the node tree only.
		FName ParentName = Node->ParentComponentOrVariableName;
		if (ParentName.IsNone() && SCS)
		{
			if (const USCS_Node* ParentNode = SCS->FindParentNode(const_cast<USCS_Node*>(Node)))
			{
				ParentName = ParentNode->GetVariableName();
			}
		}

		Json->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
		Json->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : FString());
		Json->SetStringField(TEXT("mesh"), Template ? ObjectPathName(Template->GetStaticMesh()) : FString());
		Json->SetStringField(TEXT("socket"), Node->AttachToName.ToString());
		Json->SetStringField(TEXT("template_socket"),
			Template ? Template->GetAttachSocketName().ToString() : FString());
		Json->SetStringField(TEXT("parent"), ParentName.ToString());
		Json->SetBoolField(TEXT("parent_native"), Node->bIsParentComponentNative);
		Json->SetBoolField(TEXT("visible"), Template ? Template->IsVisible() : false);
		Json->SetBoolField(TEXT("hidden_in_game"), Template ? Template->bHiddenInGame : false);

		TArray<TSharedPtr<FJsonValue>> Tags;
		if (Template)
		{
			for (const FName& Tag : Template->ComponentTags)
			{
				Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
		}
		Json->SetArrayField(TEXT("tags"), Tags);

		if (Template)
		{
			const FTransform RelativeTransform = Template->GetRelativeTransform();
			Json->SetStringField(TEXT("relative_location"), RelativeTransform.GetLocation().ToString());
			Json->SetStringField(TEXT("relative_rotation"),
				RelativeTransform.GetRotation().Rotator().ToString());
			Json->SetStringField(TEXT("relative_scale"), RelativeTransform.GetScale3D().ToString());
		}

		return Json;
	}

	void CollectAttachmentComponents(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutComponents)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			return;
		}

		USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
		for (const USCS_Node* Node : SCS->GetAllNodes())
		{
			if (IsAttachmentNode(Node))
			{
				OutComponents.Add(MakeShared<FJsonValueObject>(MakeComponentJson(Node, SCS)));
			}
		}
	}

	bool SaveBlueprintPackage(UBlueprint* Blueprint)
	{
		UPackage* Package = Blueprint ? Blueprint->GetOutermost() : nullptr;
		if (!Package)
		{
			return false;
		}

		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;

		return UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs);
	}
}

bool UPVPWeaponAssemblyBuilder::AssembleAttachments(
	const FString& BlueprintPath,
	const TArray<FPVPWeaponAttachmentSpec>& Specs,
	bool bRemoveUnlisted,
	const FString& ReportPath)
{
	UE_LOG(LogPVPWeaponAssembly, Log, TEXT("assemble %d attachment slot(s) into %s (remove_unlisted=%s)."),
		Specs.Num(), *BlueprintPath, bRemoveUnlisted ? TEXT("true") : TEXT("false"));

	TArray<TSharedPtr<FJsonValue>> Errors;
	TArray<TSharedPtr<FJsonValue>> Created;
	TArray<TSharedPtr<FJsonValue>> Updated;
	TArray<TSharedPtr<FJsonValue>> Removed;
	TArray<TSharedPtr<FJsonValue>> SlotReports;

	UBlueprint* Blueprint = LoadWeaponBlueprint(BlueprintPath, Errors);
	USimpleConstructionScript* SCS = Blueprint ? Blueprint->SimpleConstructionScript : nullptr;

	USceneComponent* DefaultParent = SCS ? FindNativeComponentTemplate(Blueprint, DefaultParentComponentName) : nullptr;
	if (SCS && !DefaultParent)
	{
		Errors.Add(MakeShared<FJsonValueString>(
			FString::Printf(TEXT("%s has no native %s component to parent attachments to"),
				*BlueprintPath, *DefaultParentComponentName.ToString())));
	}

	// Existing attachment nodes, so a spec may reference a parent slot that is listed later.
	TMap<E_PVPWeaponAttachmentSlot, USCS_Node*> NodesBySlot;
	if (SCS)
	{
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (!IsAttachmentNode(Node))
			{
				continue;
			}

			for (const FName& Tag : Node->ComponentTemplate->ComponentTags)
			{
				const E_PVPWeaponAttachmentSlot Slot = PVPWeaponAttachment::SlotFromComponentTag(Tag);
				if (Slot != E_PVPWeaponAttachmentSlot::None)
				{
					NodesBySlot.Add(Slot, Node);
					break;
				}
			}
		}
	}

	if (SCS && DefaultParent)
	{
		for (const FPVPWeaponAttachmentSpec& Spec : Specs)
		{
			const TSharedPtr<FJsonObject> SlotReport = MakeShared<FJsonObject>();
			SlotReport->SetStringField(TEXT("slot"), PVPWeaponAttachment::SlotToName(Spec.Slot).ToString());

			if (Spec.Slot == E_PVPWeaponAttachmentSlot::None || !Spec.Mesh)
			{
				const FString Message = FString::Printf(TEXT("slot %s has no mesh and was skipped"),
					*PVPWeaponAttachment::SlotToName(Spec.Slot).ToString());
				SlotReport->SetStringField(TEXT("error"), Message);
				Errors.Add(MakeShared<FJsonValueString>(Message));
				SlotReports.Add(MakeShared<FJsonValueObject>(SlotReport));
				continue;
			}

			const FName ComponentName = ResolveComponentName(Spec);
			SlotReport->SetStringField(TEXT("component"), ComponentName.ToString());
			SlotReport->SetStringField(TEXT("socket"), Spec.SocketName.ToString());
			SlotReport->SetStringField(TEXT("parent_slot"),
				PVPWeaponAttachment::SlotToName(Spec.ParentSlot).ToString());

			USCS_Node* Node = SCS->FindSCSNode(ComponentName);
			const bool bWasCreated = (Node == nullptr);

			if (Node && (!Node->ComponentClass
				|| !Node->ComponentClass->IsChildOf(UStaticMeshComponent::StaticClass())))
			{
				const FString Message = FString::Printf(
					TEXT("%s exists but is not a StaticMeshComponent; delete it manually and re-run"),
					*ComponentName.ToString());
				SlotReport->SetStringField(TEXT("error"), Message);
				Errors.Add(MakeShared<FJsonValueString>(Message));
				SlotReports.Add(MakeShared<FJsonValueObject>(SlotReport));
				continue;
			}

			if (!Node)
			{
				Node = SCS->CreateNode(UStaticMeshComponent::StaticClass(), ComponentName);
			}

			UStaticMeshComponent* Template = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr;
			if (!Template)
			{
				const FString Message = FString::Printf(TEXT("cannot create component %s"),
					*ComponentName.ToString());
				SlotReport->SetStringField(TEXT("error"), Message);
				Errors.Add(MakeShared<FJsonValueString>(Message));
				SlotReports.Add(MakeShared<FJsonValueObject>(SlotReport));
				continue;
			}

			// Resolve the parent: Handguard children live under the handguard component.
			USCS_Node* ParentNode = nullptr;
			if (Spec.ParentSlot != E_PVPWeaponAttachmentSlot::None)
			{
				if (USCS_Node** FoundParent = NodesBySlot.Find(Spec.ParentSlot))
				{
					ParentNode = *FoundParent;
				}
				else
				{
					ParentNode = SCS->FindSCSNode(PVPWeaponAttachment::MakeComponentName(Spec.ParentSlot));
				}

				if (!ParentNode)
				{
					const FString Message = FString::Printf(
						TEXT("%s needs parent slot %s but that component does not exist"),
						*ComponentName.ToString(),
						*PVPWeaponAttachment::SlotToName(Spec.ParentSlot).ToString());
					SlotReport->SetStringField(TEXT("error"), Message);
					Errors.Add(MakeShared<FJsonValueString>(Message));
					if (bWasCreated)
					{
						SCS->RemoveNode(Node);
					}
					SlotReports.Add(MakeShared<FJsonValueObject>(SlotReport));
					continue;
				}
			}

			if (bWasCreated)
			{
				if (ParentNode)
				{
					ParentNode->AddChildNode(Node);
				}
				else
				{
					SCS->AddNode(Node);
					Node->SetParent(DefaultParent);
				}
			}
			else if (ParentNode)
			{
				if (!ParentNode->GetChildNodes().Contains(Node))
				{
					ParentNode->AddChildNode(Node);
				}
			}
			else
			{
				Node->SetParent(DefaultParent);
			}

			Node->Modify();
			Template->Modify();
			Template->SetStaticMesh(Spec.Mesh);
			Template->SetMobility(EComponentMobility::Movable);
			Template->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Template->SetGenerateOverlapEvents(false);
			Template->SetRelativeTransform(FTransform::Identity);
			// USceneComponent::AttachSocketName is private; the SCS node field is what
			// USCS_Node::ExecuteNode feeds into SetupAttachment(Parent, AttachToName).
			Node->AttachToName = Spec.SocketName;

			Template->ComponentTags.Reset();
			Template->ComponentTags.Add(PVPWeaponAttachment::MakeComponentTag(Spec.Slot));

			Template->SetVisibility(Spec.bEnabled, true);
			Template->SetHiddenInGame(!Spec.bEnabled, true);

			NodesBySlot.Add(Spec.Slot, Node);
			(bWasCreated ? Created : Updated).Add(
				MakeShared<FJsonValueString>(ComponentName.ToString()));

			SlotReport->SetBoolField(TEXT("created"), bWasCreated);
			SlotReport->SetStringField(TEXT("mesh"), ObjectPathName(Spec.Mesh));
			SlotReports.Add(MakeShared<FJsonValueObject>(SlotReport));

			UE_LOG(LogPVPWeaponAssembly, Log, TEXT("%s %s (mesh=%s socket=%s parent=%s)."),
				bWasCreated ? TEXT("created") : TEXT("updated"),
				*ComponentName.ToString(),
				*ObjectPathName(Spec.Mesh),
				*Spec.SocketName.ToString(),
				ParentNode ? *ParentNode->GetVariableName().ToString() : *DefaultParentComponentName.ToString());
		}
	}

	// Optionally drop assembled attachments that are not part of this loadout.
	if (SCS && bRemoveUnlisted)
	{
		TSet<FName> KeepNames;
		for (const FPVPWeaponAttachmentSpec& Spec : Specs)
		{
			KeepNames.Add(ResolveComponentName(Spec));
		}

		TArray<USCS_Node*> AttachmentNodes;
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (IsAttachmentNode(Node))
			{
				AttachmentNodes.Add(Node);
			}
		}

		for (USCS_Node* Node : AttachmentNodes)
		{
			if (KeepNames.Contains(Node->GetVariableName()))
			{
				continue;
			}

			bool bHasAttachmentChild = false;
			for (const USCS_Node* Child : Node->GetChildNodes())
			{
				if (IsAttachmentNode(Child))
				{
					bHasAttachmentChild = true;
					break;
				}
			}

			if (bHasAttachmentChild)
			{
				Errors.Add(MakeShared<FJsonValueString>(FString::Printf(
					TEXT("%s is not in the loadout but still parents attachment children; kept"),
					*Node->GetVariableName().ToString())));
				continue;
			}

			const FString RemovedName = Node->GetVariableName().ToString();
			SCS->RemoveNode(Node);
			Removed.Add(MakeShared<FJsonValueString>(RemovedName));
			UE_LOG(LogPVPWeaponAssembly, Log, TEXT("removed %s (not in loadout)."), *RemovedName);
		}
	}

	bool bCompiled = false;
	int32 CompileErrors = 0;
	int32 CompileWarnings = 0;
	bool bSaved = false;

	if (Blueprint && SCS)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		Blueprint->GetOutermost()->MarkPackageDirty();

		FCompilerResultsLog CompileLog;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);
		CompileErrors = CompileLog.NumErrors;
		CompileWarnings = CompileLog.NumWarnings;
		bCompiled = (Blueprint->Status == BS_UpToDate
			|| Blueprint->Status == BS_UpToDateWithWarnings)
			&& Blueprint->GeneratedClass != nullptr;

		if (!bCompiled)
		{
			Errors.Add(MakeShared<FJsonValueString>(FString::Printf(
				TEXT("blueprint failed to compile: status=%s errors=%d warnings=%d"),
				*UEnum::GetValueAsString(Blueprint->Status), CompileErrors, CompileWarnings)));
		}
		else
		{
			bSaved = SaveBlueprintPackage(Blueprint);
			if (!bSaved)
			{
				Errors.Add(MakeShared<FJsonValueString>(TEXT("failed to save the weapon blueprint package")));
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> Components;
	CollectAttachmentComponents(Blueprint, Components);

	const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("blueprint"), BlueprintPath);
	Report->SetBoolField(TEXT("remove_unlisted"), bRemoveUnlisted);
	Report->SetNumberField(TEXT("spec_count"), Specs.Num());
	Report->SetArrayField(TEXT("slots"), SlotReports);
	Report->SetArrayField(TEXT("created"), Created);
	Report->SetArrayField(TEXT("updated"), Updated);
	Report->SetArrayField(TEXT("removed"), Removed);
	Report->SetArrayField(TEXT("components"), Components);
	Report->SetNumberField(TEXT("component_count"), Components.Num());
	Report->SetBoolField(TEXT("compiled"), bCompiled);
	Report->SetNumberField(TEXT("compile_errors"), CompileErrors);
	Report->SetNumberField(TEXT("compile_warnings"), CompileWarnings);
	Report->SetBoolField(TEXT("saved"), bSaved);
	Report->SetArrayField(TEXT("errors"), Errors);
	Report->SetBoolField(TEXT("success"), Errors.Num() == 0 && bCompiled);
	WriteJsonReport(ReportPath, Report);

	return Errors.Num() == 0 && bCompiled;
}

bool UPVPWeaponAssemblyBuilder::ReadbackAttachments(const FString& BlueprintPath, const FString& ReportPath)
{
	TArray<TSharedPtr<FJsonValue>> Errors;
	UBlueprint* Blueprint = LoadWeaponBlueprint(BlueprintPath, Errors);

	TArray<TSharedPtr<FJsonValue>> Components;
	CollectAttachmentComponents(Blueprint, Components);

	const bool bCompiled = Blueprint
		&& (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings)
		&& Blueprint->GeneratedClass != nullptr;

	const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("blueprint"), BlueprintPath);
	Report->SetNumberField(TEXT("component_count"), Components.Num());
	Report->SetArrayField(TEXT("components"), Components);
	Report->SetBoolField(TEXT("compiled"), bCompiled);
	Report->SetArrayField(TEXT("errors"), Errors);
	Report->SetBoolField(TEXT("success"), Errors.Num() == 0 && Components.Num() > 0);
	WriteJsonReport(ReportPath, Report);

	UE_LOG(LogPVPWeaponAssembly, Log, TEXT("read back %d attachment component(s) from %s."),
		Components.Num(), *BlueprintPath);

	return Components.Num() > 0 && bCompiled;
}

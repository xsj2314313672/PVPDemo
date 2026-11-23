// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PVPDemo/Equipment/PVPWeaponAttachmentTypes.h"

#include "PVPWeaponAssemblyBuilder.generated.h"

/**
 * Editor-only builder that assembles weapon attachments inside a weapon Blueprint's component tree.
 *
 * Why C++ and not Python: the attachment socket is stored on USceneComponent::AttachSocketName
 * (UPROPERTY(ReplicatedUsing=...), no CPF_Edit) and USCS_Node::AttachToName (bare UPROPERTY).
 * Python's set_editor_property refuses both, while SCS construction relies on
 * SetupAttachment(ParentComponent, AttachToName). The builder therefore owns:
 *   - creating / updating / removing the ATT_<Slot> UStaticMeshComponent SCS nodes;
 *   - writing AttachToName + ComponentTags (PVP.Attachment.<Slot>);
 *   - compiling the blueprint and writing a JSON report of the write and the read-back.
 *
 * Driven by Content/Python/setup_pvp_weapon_assembly.py through a commandlet.
 */
UCLASS()
class UPVPWeaponAssemblyBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Assembles the given attachment specs into the weapon blueprint.
	 *
	 * Idempotent: an existing component with the same name is updated in place (mesh, socket, tags,
	 * relative transform, visibility). A component of the wrong class aborts that slot with an error.
	 * When bRemoveUnlisted is true, assembled attachment components that are not part of Specs are
	 * removed, so the blueprint exactly matches the requested loadout.
	 *
	 * @param BlueprintPath   Full package path of the weapon Blueprint (e.g. /Game/PVP/Actors/BP_PVP_Weapon_AR).
	 * @param Specs           Slots to assemble; ParentSlot must already exist in the list or in the blueprint.
	 * @param bRemoveUnlisted Remove attachment-tagged components that are missing from Specs.
	 * @param ReportPath      Absolute JSON report path.
	 * @return true when every requested slot was written and the blueprint compiled.
	 */
	UFUNCTION(BlueprintCallable, Category = "PVP|Weapon")
	static bool AssembleAttachments(
		const FString& BlueprintPath,
		const TArray<FPVPWeaponAttachmentSpec>& Specs,
		bool bRemoveUnlisted,
		const FString& ReportPath);

	/**
	 * Reads every assembled attachment component back from the blueprint and writes a JSON report.
	 * Returns true when at least one attachment component was found and the blueprint compiled state
	 * is up to date.
	 */
	UFUNCTION(BlueprintCallable, Category = "PVP|Weapon")
	static bool ReadbackAttachments(const FString& BlueprintPath, const FString& ReportPath);
};

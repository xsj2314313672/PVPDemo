// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "PVPAnimBlueprintBuilder.generated.h"

class UAnimBlueprint;
class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class UBlendSpace;
class USkeletalMesh;
class USkeleton;

/**
 * Editor-only builder for the project locomotion AnimBlueprint.
 *
 * The engine does not expose AnimGraph node creation to Python (UEdGraph::AddNode / CreateNode are
 * not reflected, and the ue5-automation bridge only reads AnimBlueprints), so the graph is built in
 * C++ and driven from Content/Python/setup_pvp_anim_blueprint.py through a commandlet.
 *
 * The generated graph is fixed to the current Move / Run / Jump / Aim scope:
 *
 *   Ground    : BS_PVP_FP_AR_Locomotion_Standing (Speed / Direction) blended with
 *               A_PVP_FP_AR_Run_F_Loop by "bIsRunning AND bIsMoving", then blended with
 *               BS_PVP_FP_AR_Locomotion_Aimed by "bIsAiming" (aim pose on BlendPose_0 because
 *               UAnimNode_BlendListByBool selects index 0 for bActiveValue=true)
 *   JumpStart : A_PVP_FP_AR_Jump_Start
 *   Falling   : A_PVP_FP_AR_Jump_Falling_Loop
 *   Land      : A_PVP_FP_AR_Jump_End, auto transition back to Ground when it ends
 *
 * Transitions use the UPVPAnimInstanceBase variables only; the builder never writes character state.
 */
UCLASS()
class UPVPAnimBlueprintBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates the locomotion AnimBlueprint and builds its AnimGraph state machine.
	 *
	 * The target asset must not exist yet (the Python driver deletes the previous generated asset).
	 * Returns the created blueprint even when compilation fails, so the caller can inspect the report;
	 * returns nullptr when asset creation or graph construction fails.
	 *
	 * @param AssetPath         Full package path, e.g. /Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion.
	 * @param ParentClass       AnimInstance parent class (UPVPAnimInstanceBase).
	 * @param Skeleton          Target skeleton, shared by every animation asset.
	 * @param PreviewMesh       Preview skeletal mesh for Persona.
	 * @param GroundBlendSpace  Idle / Walk ground blend space.
	 * @param AimBlendSpace     Aimed idle / walk ground blend space (must share the ground axes).
	 * @param RunLoop           Run forward loop sequence.
	 * @param JumpStart         Jump start sequence (single shot).
	 * @param JumpFallingLoop   Falling loop sequence.
	 * @param JumpEnd           Jump end sequence (single shot, drives the Land auto transition).
	 * @param ReportPath        Absolute path of the JSON report to write.
	 */
	UFUNCTION(BlueprintCallable, Category = "PVP|Animation")
	static UAnimBlueprint* BuildLocomotionAnimBlueprint(
		const FString& AssetPath,
		TSubclassOf<UAnimInstance> ParentClass,
		USkeleton* Skeleton,
		USkeletalMesh* PreviewMesh,
		UBlendSpace* GroundBlendSpace,
		UBlendSpace* AimBlendSpace,
		UAnimSequence* RunLoop,
		UAnimSequence* JumpStart,
		UAnimSequence* JumpFallingLoop,
		UAnimSequence* JumpEnd,
		const FString& ReportPath);

	/**
	 * Creates a weapon AnimBlueprint with a fixed graph:
	 *
	 *   SequencePlayer(ReferencePose) -> Slot(MontageSlotName) -> Root
	 *
	 * The reference pose keeps the weapon in its authored pose when no montage is playing; the slot
	 * plays the weapon montages baked against the same skeleton (SKEL_PVP_AR).
	 *
	 * The target asset must not exist yet (the Python driver deletes the previous generated asset).
	 */
	UFUNCTION(BlueprintCallable, Category = "PVP|Animation")
	static UAnimBlueprint* BuildWeaponAnimBlueprint(
		const FString& AssetPath,
		USkeleton* Skeleton,
		USkeletalMesh* PreviewMesh,
		UAnimSequence* ReferencePose,
		FName MontageSlotName,
		const FString& ReportPath);

	/**
	 * Inserts a montage slot between the current root pose source and the root of an existing
	 * AnimBlueprint, without deleting the asset (protects blueprint references such as
	 * BP_PVPBaseCharacter's Mesh AnimClass override).
	 *
	 * Idempotent: when the slot already exists and is wired to the root, nothing is changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "PVP|Animation")
	static bool EnsureMontageSlot(
		const FString& AnimBlueprintPath,
		FName MontageSlotName,
		const FString& ReportPath);

	/**
	 * Creates a single-track montage asset from an AnimSequence (UAnimMontageFactory), used for the
	 * aimed fire montage that was authored only as a sequence. The montage is written into the
	 * requested slot (DefaultSlot when the animation blueprints expose it), gets a "Default" section
	 * starting at 0 and the preview mesh, then the builder writes a JSON read-back report.
	 *
	 * The target asset must not exist yet (the Python driver removes the previous generated asset).
	 */
	UFUNCTION(BlueprintCallable, Category = "PVP|Animation")
	static UAnimMontage* BuildMontageFromSequence(
		const FString& AssetPath,
		UAnimSequence* SourceSequence,
		USkeleton* Skeleton,
		USkeletalMesh* PreviewMesh,
		FName MontageSlotName,
		const FString& ReportPath);
};

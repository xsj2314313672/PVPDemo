// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "PVPAnimBlueprintBuilder.generated.h"

class UAnimBlueprint;
class UAnimInstance;
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
 * The generated graph is fixed to the current Move / Run / Jump scope:
 *
 *   Ground    : BS_PVP_FP_AR_Locomotion_Standing (Speed / Direction) blended with
 *               A_PVP_FP_AR_Run_F_Loop by "bIsRunning AND bIsMoving"
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
		UAnimSequence* RunLoop,
		UAnimSequence* JumpStart,
		UAnimSequence* JumpFallingLoop,
		UAnimSequence* JumpEnd,
		const FString& ReportPath);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"

#include "PVPBaseCharacter.generated.h"

class UCameraComponent;
class UPVPInputConfig;

/**
 * First person base character for the PVP project.
 *
 * The component layout and the first person camera mirror the project-local character blueprint
 * /Game/PVP/Characters/BP_PVP_BaseCharacter (migrated from the third-party showcase map):
 *
 *   CapsuleComponent (root)
 *   +- CharacterMesh0 (inherited Mesh)  relative location (0,0,-88), rotation (0,-90,0)
 *      +- CameraFP                       attached to the "head" bone, rotation (0,90,-90), FOV 100
 *
 * All defaults below are the exact values exported from that blueprint
 * (Saved/PVPCharacterDump/tfa_base_character.json). They must stay in sync with it,
 * otherwise the 1P view stops matching the reference character.
 *
 * "head" is a bone of SKEL_PVP_Mannequin rather than a socket, so the camera position follows the
 * head pose produced by ABP_PVP_FP_BaseCharacter (Content/PVP/Animation/AnimBP). The view rotation
 * is owned by the controller
 * (CameraFP->bUsePawnControlRotation), keeping aim/recoil independent from the animation pose.
 * Re-run Content/Python/dump_tfa_character.py and Content/Python/compare_tfa_character.py after
 * changing the reference blueprint or these defaults; the parity report is written to
 * Saved/PVPCharacterDump/parity_report.md.
 */
UCLASS(Blueprintable)
class PVPDEMO_API APVPBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APVPBaseCharacter();

	//~ Begin AActor / APawn Interface
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End AActor / APawn Interface

	/**
	 * First person camera. Attached to the head bone of the first person mesh exactly like the
	 * reference blueprint, so the head / aim animation drives the view rotation. Per-view
	 * settings (FOV etc.) can still be overridden on a derived blueprint.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVP|Camera")
	TObjectPtr<UCameraComponent> CameraFP;

	/**
	 * Tag -> InputAction mapping (DA_PVP_InputConfig), assigned on BP_PVPBaseCharacter.
	 * Native bindings are created in SetupPlayerInputComponent; a missing config only logs a
	 * warning so a bare C++ pawn still spawns.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Input")
	TObjectPtr<UPVPInputConfig> InputConfig;

	/** MaxWalkSpeed used while the Run action is not held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 300.0f;

	/** MaxWalkSpeed used while the Run action is held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 600.0f;

	/** Running state for animation / camera layers (read-only for presentation). */
	UPROPERTY(BlueprintReadOnly, Category = "PVP|Movement")
	bool bIsRunning = false;

protected:
	/** Move relative to the control rotation yaw; ignored while the controller is missing. */
	void Input_Move(const FInputActionValue& Value);

	/** Yaw / pitch go to the controller: the first person animation drives the head bone camera. */
	void Input_Look(const FInputActionValue& Value);

	void Input_JumpStarted(const FInputActionValue& Value);
	void Input_JumpCompleted(const FInputActionValue& Value);

	void Input_RunStarted(const FInputActionValue& Value);
	void Input_RunCompleted(const FInputActionValue& Value);

	/** Apply the walk / run speed for the new running state. */
	void SetRunning(bool bNewRunning);

	// TODO(3P): the reference blueprint additionally has CameraPivotPoint (StaticMeshComponent
	// gizmo) -> SpringArm -> CameraTP, followed by PP_Bodycam and SpotLight. Third person support,
	// the E_TFA_CameraPerspectives switching logic and the Enhanced Input bindings
	// (IA_TFA_ToggleCameraPerspective / IA_TFA_ToggleCameraAnimation) are intentionally not
	// implemented yet; add them here without reparenting CameraFP.
};

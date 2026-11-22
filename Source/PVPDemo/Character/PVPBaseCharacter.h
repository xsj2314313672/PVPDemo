// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "PVPBaseCharacter.generated.h"

class UCameraComponent;

/**
 * First person base character for the PVP project.
 *
 * The component layout and the first person camera mirror the reference character
 * /Game/InfimaGames/TacticalFPSAnimations/Common/Core/Characters/BP_TFA_BaseCharacter:
 *
 *   CapsuleComponent (root)
 *   +- CharacterMesh0 (inherited Mesh)  relative location (0,0,-88), rotation (0,-90,0)
 *      +- CameraFP                       attached to the "head" bone, rotation (0,90,-90), FOV 100
 *
 * All defaults below are the exact values exported from that blueprint
 * (Saved/PVPCharacterDump/tfa_base_character.json). They must stay in sync with it,
 * otherwise the 1P view stops matching the reference character.
 *
 * "head" is a bone of SKEL_TFA_Mannequin rather than a socket, so the camera follows the head
 * pose produced by ABP_TFA_FP_BaseCharacter. Re-run Content/Python/dump_tfa_character.py and
 * Content/Python/compare_tfa_character.py after changing the reference blueprint or these
 * defaults; the parity report is written to Saved/PVPCharacterDump/parity_report.md.
 */
UCLASS(Blueprintable)
class PVPDEMO_API APVPBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APVPBaseCharacter();

	/**
	 * First person camera. Attached to the head bone of the first person mesh exactly like the
	 * reference blueprint, so the head / aim animation drives the view rotation. Per-view
	 * settings (FOV etc.) can still be overridden on a derived blueprint.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVP|Camera")
	TObjectPtr<UCameraComponent> CameraFP;

	// TODO(3P): the reference blueprint additionally has CameraPivotPoint (StaticMeshComponent
	// gizmo) -> SpringArm -> CameraTP, followed by PP_Bodycam and SpotLight. Third person support,
	// the E_TFA_CameraPerspectives switching logic and the Enhanced Input bindings
	// (IA_TFA_ToggleCameraPerspective / IA_TFA_ToggleCameraAnimation) are intentionally not
	// implemented yet; add them here without reparenting CameraFP.
};

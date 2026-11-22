// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Character/PVPBaseCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace PVPBaseCharacterDefaults
{
	/** Capsule size of BP_TFA_BaseCharacter. */
	static constexpr float CapsuleRadius = 5.0f;
	static constexpr float CapsuleHalfHeight = 88.0f;

	/** Relative transform of CharacterMesh0 in BP_TFA_BaseCharacter. */
	static const FVector MeshRelativeLocation(0.0f, 0.0f, -88.0f);
	static const FRotator MeshRelativeRotation(0.0f, -90.0f, 0.0f);

	/** CameraFP is attached to this bone of the character mesh in the reference blueprint. */
	static const FName CameraAttachBone(TEXT("head"));
	static const FRotator CameraRelativeRotation(0.0f, 90.0f, -90.0f);
	static constexpr float CameraFieldOfView = 100.0f;

	/** First person assets used by the reference blueprint while in first person perspective. */
	static const TCHAR* FirstPersonMeshPath =
		TEXT("/Game/InfimaGames/TacticalFPSAnimations/Common/Characters/Mannequins/Meshes/SKM_FP_Manny_Simple.SKM_FP_Manny_Simple");
	static const TCHAR* FirstPersonAnimBlueprintPath =
		TEXT("/Game/InfimaGames/TacticalFPSAnimations/Common/Core/Characters/ABP_TFA_FP_BaseCharacter.ABP_TFA_FP_BaseCharacter_C");
}

APVPBaseCharacter::APVPBaseCharacter()
{
	using namespace PVPBaseCharacterDefaults;

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	// The reference keeps the actor yaw aligned with the controller; pitch and roll come from the
	// head bone driven by the first person animation blueprint instead of the component rotation.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	BaseEyeHeight = 64.0f;
	CrouchedEyeHeight = 32.0f;

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	check(CharacterMesh);
	CharacterMesh->SetRelativeLocationAndRotation(MeshRelativeLocation, MeshRelativeRotation);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> FirstPersonMeshFinder(FirstPersonMeshPath);
	if (FirstPersonMeshFinder.Succeeded())
	{
		CharacterMesh->SetSkeletalMeshAsset(FirstPersonMeshFinder.Object);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> FirstPersonAnimBlueprintFinder(FirstPersonAnimBlueprintPath);
	if (FirstPersonAnimBlueprintFinder.Succeeded())
	{
		CharacterMesh->SetAnimInstanceClass(FirstPersonAnimBlueprintFinder.Class);
	}

	CameraFP = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraFP"));
	CameraFP->SetupAttachment(CharacterMesh, CameraAttachBone);
	CameraFP->SetRelativeLocation(FVector::ZeroVector);
	CameraFP->SetRelativeRotation(CameraRelativeRotation);
	CameraFP->FieldOfView = CameraFieldOfView;
	CameraFP->bUsePawnControlRotation = false;
	CameraFP->PostProcessBlendWeight = 1.0f;
	// The reference toggles camera components on perspective changes; this class is first person
	// only, so the first person camera is active from the start.
	CameraFP->bAutoActivate = true;
}

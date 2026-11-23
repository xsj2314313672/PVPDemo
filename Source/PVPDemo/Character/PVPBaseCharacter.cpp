// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Character/PVPBaseCharacter.h"

#include "PVPDemo/Equipment/PVPEquipmentComponent.h"
#include "PVPDemo/GAS/PVPGameplayTags.h"
#include "PVPDemo/Input/PVPInputComponent.h"
#include "PVPDemo/Input/PVPInputConfig.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPBaseCharacter, Log, All);

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

	/** Project-local first person assets (the third-party originals were migrated and renamed). */
	static const TCHAR* FirstPersonMeshPath =
		TEXT("/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_FP_Manny_Simple.SKM_PVP_FP_Manny_Simple");
	static const TCHAR* FirstPersonAnimBlueprintPath =
		TEXT("/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter.ABP_PVP_FP_BaseCharacter_C");
}

APVPBaseCharacter::APVPBaseCharacter()
{
	using namespace PVPBaseCharacterDefaults;

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	// The actor yaw follows the controller so movement stays aligned with the view. Pitch and roll
	// are intentionally NOT applied to the actor: the first person camera consumes the controller
	// rotation for its view (see CameraFP below), while the head bone only drives its position.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	BaseEyeHeight = 64.0f;
	CrouchedEyeHeight = 32.0f;

	// The pawn owns the native input bindings (Tag -> InputAction), so the tag based input component
	// is created through the pawn hook instead of changing DefaultInput.ini. Requires UE 5.9
	// (APawn::OverrideInputComponentClass).
	OverrideInputComponentClass = UPVPInputComponent::StaticClass();

	// Movement starts in the walking state; Run temporarily raises MaxWalkSpeed (see SetRunning).
	bIsRunning = false;
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = WalkSpeed;
	}

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
	// First person view rotation is owned by the controller: yaw + pitch (clamped by the player
	// controller) come from ControlRotation, while the attachment keeps the camera position on the
	// head bone so head bob / crouch pose still drive the 1P feel. Anim-driven head rotation
	// (recoil, lean, hit reactions) must go through controller input / camera modifiers instead.
	CameraFP->bUsePawnControlRotation = true;
	CameraFP->PostProcessBlendWeight = 1.0f;
	// The reference toggles camera components on perspective changes; this class is first person
	// only, so the first person camera is active from the start.
	CameraFP->bAutoActivate = true;

	// Weapons are picked up by overlapping actors that resolve this component on the pawn. The
	// component is intentionally independent from Character/ (it only knows ACharacter), keeping
	// the dependency one way: Character -> Equipment.
	EquipmentComponent = CreateDefaultSubobject<UPVPEquipmentComponent>(TEXT("EquipmentComponent"));
}

void APVPBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint subclasses may override WalkSpeed/RunSpeed, so resolve the movement speed after the
	// class defaults have been applied instead of relying on the constructor value alone.
	bIsRunning = false;
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = WalkSpeed;
	}
}

void APVPBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UPVPInputComponent* PVPInputComponent = Cast<UPVPInputComponent>(PlayerInputComponent);
	if (!PVPInputComponent)
	{
		UE_LOG(LogPVPBaseCharacter, Warning,
			TEXT("%s: input component is %s, expected UPVPInputComponent; native bindings skipped."),
			*GetName(), *GetNameSafe(PlayerInputComponent));
		return;
	}

	if (!InputConfig)
	{
		UE_LOG(LogPVPBaseCharacter, Warning,
			TEXT("%s: no UPVPInputConfig assigned; native bindings skipped."), *GetName());
		return;
	}

	int32 BoundBindings = 0;
	const int32 ExpectedBindings = 6;

	if (PVPInputComponent->BindNativeAction(InputConfig, PVPGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move))
	{
		++BoundBindings;
	}
	if (PVPInputComponent->BindNativeAction(InputConfig, PVPGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look))
	{
		++BoundBindings;
	}
	if (PVPInputComponent->BindNativeAction(InputConfig, PVPGameplayTags::InputTag_Jump, ETriggerEvent::Started, this, &ThisClass::Input_JumpStarted))
	{
		++BoundBindings;
	}
	if (PVPInputComponent->BindNativeAction(InputConfig, PVPGameplayTags::InputTag_Jump, ETriggerEvent::Completed, this, &ThisClass::Input_JumpCompleted))
	{
		++BoundBindings;
	}
	if (PVPInputComponent->BindNativeAction(InputConfig, PVPGameplayTags::InputTag_Run, ETriggerEvent::Started, this, &ThisClass::Input_RunStarted))
	{
		++BoundBindings;
	}
	if (PVPInputComponent->BindNativeAction(InputConfig, PVPGameplayTags::InputTag_Run, ETriggerEvent::Completed, this, &ThisClass::Input_RunCompleted))
	{
		++BoundBindings;
	}

	UE_LOG(LogPVPBaseCharacter, Log, TEXT("%s: bound %d/%d native input bindings (config=%s)"),
		*GetName(), BoundBindings, ExpectedBindings, *GetNameSafe(InputConfig));
}

void APVPBaseCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	AController* OwningController = GetController();
	if (!OwningController || MovementVector.IsNearlyZero())
	{
		return;
	}

	// Movement follows the control rotation yaw only: pitch must not tilt the movement plane.
	const FRotationMatrix ControlYawRotation(FRotator(0.0f, OwningController->GetControlRotation().Yaw, 0.0f));
	AddMovementInput(ControlYawRotation.GetUnitAxis(EAxis::X), MovementVector.Y);
	AddMovementInput(ControlYawRotation.GetUnitAxis(EAxis::Y), MovementVector.X);
}

void APVPBaseCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	// CameraFP uses bUsePawnControlRotation, so writing the controller rotation is what actually
	// turns the view. The animation blueprint only drives the camera position through the head bone.
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void APVPBaseCharacter::Input_JumpStarted(const FInputActionValue& /*Value*/)
{
	Jump();
}

void APVPBaseCharacter::Input_JumpCompleted(const FInputActionValue& /*Value*/)
{
	StopJumping();
}

void APVPBaseCharacter::Input_RunStarted(const FInputActionValue& /*Value*/)
{
	SetRunning(true);
}

void APVPBaseCharacter::Input_RunCompleted(const FInputActionValue& /*Value*/)
{
	SetRunning(false);
}

void APVPBaseCharacter::SetRunning(bool bNewRunning)
{
	if (bIsRunning == bNewRunning)
	{
		return;
	}

	bIsRunning = bNewRunning;

	// TODO(Network/GAS): the running speed is applied locally (autonomous proxy / standalone) only.
	// Move it to a replicated movement state, the planned PVPCharacterMovementComponent, or a GAS
	// speed attribute when the ability system lands, so the server validates the same speed.
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = bIsRunning ? RunSpeed : WalkSpeed;
	}

	UE_LOG(LogPVPBaseCharacter, Verbose, TEXT("%s: running=%s max_walk_speed=%.1f"),
		*GetName(), bIsRunning ? TEXT("true") : TEXT("false"),
		GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 0.0f);
}

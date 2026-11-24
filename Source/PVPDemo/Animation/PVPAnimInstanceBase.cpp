// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Animation/PVPAnimInstanceBase.h"

#include "PVPDemo/Character/PVPBaseCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPAnimInstanceBase, Log, All);

void UPVPAnimInstanceBase::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CachedCharacter = Cast<APVPBaseCharacter>(GetOwningActor());
	if (!CachedCharacter)
	{
		UE_LOG(LogPVPAnimInstanceBase, Warning,
			TEXT("%s: owning actor is %s, expected APVPBaseCharacter; animation variables stay at defaults."),
			*GetName(), *GetNameSafe(GetOwningActor()));
		CachedMovementComponent = nullptr;
		return;
	}

	CachedMovementComponent = CachedCharacter->GetCharacterMovement();
	if (!CachedMovementComponent)
	{
		UE_LOG(LogPVPAnimInstanceBase, Warning,
			TEXT("%s: %s has no character movement component; animation variables stay at defaults."),
			*GetName(), *GetNameSafe(CachedCharacter));
	}
}

void UPVPAnimInstanceBase::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Editor / debug flows can create the anim instance before the pawn is fully set up, so retry the
	// lookup here (game thread) as a fallback for NativeInitializeAnimation.
	if (!CachedCharacter || !CachedMovementComponent)
	{
		CachedCharacter = Cast<APVPBaseCharacter>(GetOwningActor());
		CachedMovementComponent = CachedCharacter ? CachedCharacter->GetCharacterMovement() : nullptr;
	}

	bHasValidSnapshot = CachedCharacter != nullptr && CachedMovementComponent != nullptr;
	if (!bHasValidSnapshot)
	{
		SampledVelocity = FVector::ZeroVector;
		SampledActorYaw = 0.0f;
		SampledMaxWalkSpeed = 0.0f;
		SampledRunSpeed = 0.0f;
		bSampledIsRunning = false;
		bSampledIsAiming = false;
		bSampledIsMovingOnGround = false;
		bSampledIsFalling = false;
		return;
	}

	// Only plain values are sampled here: the thread safe update must not touch the owning actor.
	SampledVelocity = CachedMovementComponent->Velocity;
	SampledActorYaw = CachedCharacter->GetActorRotation().Yaw;
	SampledMaxWalkSpeed = CachedMovementComponent->MaxWalkSpeed;
	SampledRunSpeed = CachedCharacter->RunSpeed;
	bSampledIsRunning = CachedCharacter->bIsRunning;
	bSampledIsAiming = CachedCharacter->bIsAiming;
	bSampledIsMovingOnGround = CachedMovementComponent->IsMovingOnGround();
	bSampledIsFalling = CachedMovementComponent->IsFalling();
}

void UPVPAnimInstanceBase::NativeThreadSafeUpdateAnimation(float /*DeltaSeconds*/)
{
	if (!bHasValidSnapshot)
	{
		ResetAnimationState();
		return;
	}

	Velocity = SampledVelocity;
	Speed = SampledVelocity.Size2D();
	SpeedZ = SampledVelocity.Z;
	MaxSpeed = SampledMaxWalkSpeed;
	SpeedRatio = SampledRunSpeed > UE_KINDA_SMALL_NUMBER
		? FMath::Clamp(Speed / SampledRunSpeed, 0.0f, 1.0f)
		: 0.0f;
	bIsMoving = Speed > MoveSpeedThreshold;

	// CalculateDirection expects the base rotation without pitch / roll, matching the actor yaw used by
	// the movement input plane.
	Direction = bIsMoving
		? UKismetAnimationLibrary::CalculateDirection(SampledVelocity, FRotator(0.0f, SampledActorYaw, 0.0f))
		: 0.0f;

	bIsRunning = bSampledIsRunning;
	bIsAiming = bSampledIsAiming;
	bIsOnGround = bSampledIsMovingOnGround;
	bIsInAir = bSampledIsFalling;
	bIsJumping = bIsInAir && SpeedZ > 0.0f;
	bIsFalling = bIsInAir && SpeedZ <= 0.0f;
}

void UPVPAnimInstanceBase::ResetAnimationState()
{
	Velocity = FVector::ZeroVector;
	Speed = 0.0f;
	SpeedZ = 0.0f;
	MaxSpeed = 0.0f;
	SpeedRatio = 0.0f;
	Direction = 0.0f;
	bIsMoving = false;
	bIsRunning = false;
	bIsAiming = false;
	bIsOnGround = false;
	bIsInAir = false;
	bIsJumping = false;
	bIsFalling = false;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "PVPAnimInstanceBase.generated.h"

class APVPBaseCharacter;
class UCharacterMovementComponent;

/**
 * Base animation instance for the PVP characters.
 *
 * The owning APVPBaseCharacter is resolved once in NativeInitializeAnimation, and the locomotion state
 * of the currently implemented actions is exposed as Blueprint read only variables so a new Animation
 * Blueprint only has to pick this class as its parent:
 *
 *   Move -> Velocity / Speed / SpeedZ / MaxSpeed / SpeedRatio / Direction / bIsMoving
 *   Run  -> bIsRunning
 *   Jump -> bIsOnGround / bIsInAir / bIsJumping / bIsFalling
 *
 * Look is intentionally not exposed: the first person view rotation is driven by the controller and
 * does not take part in the animation pose (see Docs/Source-Structure.md and Docs/Input/README.md).
 *
 * The raw state is sampled on the game thread in NativeUpdateAnimation and the Blueprint variables are
 * computed on the animation worker thread in NativeThreadSafeUpdateAnimation, following the role
 * separation recommended by UAnimInstance. Do not call GetOwningActor() / GetWorld() / UE_LOG from the
 * thread safe path.
 *
 * This class is not wired to APVPBaseCharacter yet: the character still uses the migrated third party
 * animation blueprint. Create an Animation Blueprint in the editor with this class as its parent, then
 * assign it on BP_PVPBaseCharacter (Anim Class) to start using it.
 */
UCLASS(Blueprintable)
class PVPDEMO_API UPVPAnimInstanceBase : public UAnimInstance
{
	GENERATED_BODY()

public:
	//~ Begin UAnimInstance Interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	//~ End UAnimInstance Interface

	/** Full velocity sampled this frame, in cm/s. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	FVector Velocity = FVector::ZeroVector;

	/** Planar speed (Velocity.Size2D()), in cm/s; ignores vertical movement. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	float Speed = 0.0f;

	/** Vertical speed (Velocity.Z), in cm/s; positive while moving up, negative while falling. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	float SpeedZ = 0.0f;

	/** Current MaxWalkSpeed: WalkSpeed while walking, RunSpeed while the Run action is held. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	float MaxSpeed = 0.0f;

	/** Planar speed relative to APVPBaseCharacter::RunSpeed, clamped to 0..1: about 0.5 walking, 1.0 running. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	float SpeedRatio = 0.0f;

	/** Velocity direction relative to the actor forward axis, in degrees (-180..180); 0 while not moving. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	float Direction = 0.0f;

	/** True when the planar speed exceeds MoveSpeedThreshold. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	bool bIsMoving = false;

	/** Mirrors APVPBaseCharacter::bIsRunning (true while the Run action is held). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	bool bIsRunning = false;

	/** True while the movement mode is walkable ground (Walking / NavWalking). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	bool bIsOnGround = false;

	/** True while the movement mode is Falling. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	bool bIsInAir = false;

	/** True while in air and still moving up (SpeedZ > 0). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	bool bIsJumping = false;

	/** True while in air and no longer moving up (SpeedZ <= 0), including the apex of a jump. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")
	bool bIsFalling = false;

protected:
	/** Planar speed threshold in cm/s used to derive bIsMoving. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Animation|Locomotion", meta = (ClampMin = "0.0"))
	float MoveSpeedThreshold = 1.0f;

private:
	/** Reset every Blueprint output to its default value (invalid owner / movement component). */
	void ResetAnimationState();

	/** Owning character, resolved on the game thread in NativeInitializeAnimation. */
	UPROPERTY(Transient)
	TObjectPtr<APVPBaseCharacter> CachedCharacter;

	/** Movement component of CachedCharacter, resolved on the game thread. */
	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

	/** Game thread snapshot consumed by NativeThreadSafeUpdateAnimation. */
	FVector SampledVelocity = FVector::ZeroVector;
	float SampledActorYaw = 0.0f;
	float SampledMaxWalkSpeed = 0.0f;
	float SampledRunSpeed = 0.0f;
	bool bSampledIsRunning = false;
	bool bSampledIsMovingOnGround = false;
	bool bSampledIsFalling = false;
	bool bHasValidSnapshot = false;
};

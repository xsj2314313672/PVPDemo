// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PVPWeaponActor.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;
class USkeletalMesh;
class USkeletalMeshComponent;
class USphereComponent;
class UPVPEquipmentComponent;

/**
 * A character / weapon animation pair played on two different skeletons.
 *
 * The first person arms montage targets SKEL_PVP_Mannequin (character mesh) and the weapon montage
 * targets SKEL_PVP_AR (weapon mesh). Both montages must use the "DefaultSlot" montage slot, which is
 * exposed by ABP_PVP_FP_Locomotion and ABP_PVP_FP_Weapon_AR.
 */
USTRUCT(BlueprintType)
struct FPVPWeaponMontagePair
{
	GENERATED_BODY()

	/** Montage played on the owning character mesh (arms / full body). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Animation")
	TObjectPtr<UAnimMontage> CharacterMontage = nullptr;

	/** Montage played on the weapon skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Animation")
	TObjectPtr<UAnimMontage> WeaponMontage = nullptr;

	/** Shared play rate for both montages. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Animation", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	bool IsEmpty() const
	{
		return CharacterMontage == nullptr && WeaponMontage == nullptr;
	}
};

/**
 * First person weapon actor.
 *
 * The actor keeps its presentation data on the actor (not on the inherited component templates) so a
 * weapon blueprint can be generated entirely from Python by setting class defaults. OnConstruction
 * applies the configuration to Mesh1P / Mesh3P / PickupSphere; no asset path is hardcoded in C++.
 *
 * Layout:
 *   SceneRoot
 *   +- Mesh1P        first person weapon mesh (visible, no collision)
 *   +- Mesh3P        third person weapon mesh (hidden until the 3P character lands)
 *   +- PickupSphere  overlap-only pickup trigger (Pawns only)
 *
 * Pickup flow: a pawn overlapping PickupSphere is resolved to UPVPEquipmentComponent; the equipment
 * component owns the "one weapon at a time" rule and calls EquipTo. The weapon then attaches to the
 * character mesh socket (ik_hand_gun by default) and disables its pickup sphere until it is dropped.
 */
UCLASS(Blueprintable)
class PVPDEMO_API APVPWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	APVPWeaponActor();

	//~ Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** Root scene component; the weapon mesh components and the pickup trigger hang from it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** First person weapon mesh, visible while held and while lying in the world. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Components")
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	/** Third person weapon mesh, hidden until the third person character is implemented. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Components")
	TObjectPtr<USkeletalMeshComponent> Mesh3P;

	/** Overlap trigger used for auto pickup (Pawn channel only). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Components")
	TObjectPtr<USphereComponent> PickupSphere;

	//~ Begin Weapon configuration (set on BP_PVP_Weapon_AR, applied by ApplyConfiguration)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Mesh")
	TObjectPtr<USkeletalMesh> Mesh1PAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Mesh")
	TSubclassOf<UAnimInstance> Mesh1PAnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Mesh")
	TObjectPtr<USkeletalMesh> Mesh3PAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Mesh")
	TSubclassOf<UAnimInstance> Mesh3PAnimClass;

	/**
	 * Character mesh socket the weapon attaches to.
	 * The migrated first person mesh uses the TFA gun IK target "ik_hand_gun"; the skeleton string
	 * table contains "weapon_r" but that socket is not present on SKM_PVP_FP_Manny_Simple.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FName AttachSocketName = TEXT("ik_hand_gun");

	/** Additional transform applied after snapping the actor to AttachSocketName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FTransform AttachOffset = FTransform::Identity;

	/** Socket queried by GetMuzzleLocation (SKEL_PVP_AR exposes SOCKET_Muzzle). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FName MuzzleSocketName = TEXT("SOCKET_Muzzle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Pickup", meta = (ClampMin = "0.0"))
	float PickupRadius = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Pickup")
	bool bAutoPickup = true;

	/** Delay before a dropped weapon may be picked up again (prevents instant re-pickup). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Pickup", meta = (ClampMin = "0.0"))
	float PickupCooldown = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Pickup")
	float DropForwardOffset = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Pickup")
	float DropHeightOffset = -40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Animation")
	bool bPlayEquipMontageOnPickup = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Animation")
	FPVPWeaponMontagePair EquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Animation")
	FPVPWeaponMontagePair FireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Weapon|Animation")
	FPVPWeaponMontagePair ReloadMontage;
	//~ End Weapon configuration

	/** Attaches the weapon to the character mesh socket and disables the pickup trigger. */
	UFUNCTION(BlueprintCallable, Category = "PVP|Weapon")
	bool EquipTo(ACharacter* NewOwnerCharacter);

	/** Detaches from the current character. Optionally drops the weapon in front of them. */
	UFUNCTION(BlueprintCallable, Category = "PVP|Weapon")
	void Unequip(bool bDropWeapon);

	/** Detaches and drops the weapon in front of the current character. */
	UFUNCTION(BlueprintCallable, Category = "PVP|Weapon")
	void DropWeapon();

	/** Plays the configured character / weapon equip pair. */
	UFUNCTION(BlueprintCallable, Category = "PVP|Weapon|Animation")
	void PlayEquipMontage();

	/** Plays the configured character / weapon fire pair. */
	UFUNCTION(BlueprintCallable, Category = "PVP|Weapon|Animation")
	void PlayFireMontage();

	/** Plays the configured character / weapon reload pair. */
	UFUNCTION(BlueprintCallable, Category = "PVP|Weapon|Animation")
	void PlayReloadMontage();

	UFUNCTION(BlueprintPure, Category = "PVP|Weapon")
	bool IsEquipped() const { return bIsEquipped; }

	/** Currently visible weapon mesh (Mesh1P until the 3P character is implemented). */
	UFUNCTION(BlueprintPure, Category = "PVP|Weapon")
	USkeletalMeshComponent* GetActiveMesh() const;

	/** World location of MuzzleSocketName on the active mesh, falling back to the actor location. */
	UFUNCTION(BlueprintPure, Category = "PVP|Weapon")
	FVector GetMuzzleLocation() const;

	/** Switches mesh visibility for the future 1P / 3P perspective system. */
	UFUNCTION(BlueprintCallable, Category = "PVP|Weapon")
	void SetPerspective(bool bFirstPerson);

	UFUNCTION(BlueprintPure, Category = "PVP|Weapon")
	ACharacter* GetOwningCharacter() const { return OwningCharacter.Get(); }

protected:
	UFUNCTION()
	void HandlePickupOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	/** Applies Mesh1PAsset / anim classes / pickup radius / visibility to the components. */
	void ApplyConfiguration();

	/** Plays both halves of a montage pair on the owning character and on the active weapon mesh. */
	void PlayPairedMontage(const FPVPWeaponMontagePair& MontagePair);

	void SetPickupEnabled(bool bEnabled);

	void SetPickupSuppressed(bool bSuppressed);

	void ResumePickup();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Weapon")
	bool bIsEquipped = false;

	/** True while the dropped weapon waits out PickupCooldown. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Weapon")
	bool bPickupSuppressed = false;

	/** True while Mesh1P is the visible weapon mesh. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Weapon")
	bool bUseFirstPersonMesh = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> OwningCharacter;

	FTimerHandle PickupCooldownTimer;
};

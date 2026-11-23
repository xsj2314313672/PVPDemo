// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "PVPEquipmentComponent.generated.h"

class ACharacter;
class APVPWeaponActor;

/**
 * Character equipment component.
 *
 * Owns the "one held weapon at a time" rule and is the entry point used by weapon pickup overlaps.
 * It deliberately only depends on ACharacter (Engine) and APVPWeaponActor, never on the concrete
 * APVPBaseCharacter class, so Equipment/ never depends back on Character/ source files.
 *
 * The current implementation does not replicate: weapons are local presentation actors and weapon
 * swapping / inventory slots are future work.
 */
UCLASS(ClassGroup = (PVP), meta = (BlueprintSpawnableComponent))
class PVPDEMO_API UPVPEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPVPEquipmentComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	/** Currently held weapon, or null. Read-only for animation / UI consumers. */
	UPROPERTY(BlueprintReadOnly, Category = "PVP|Equipment")
	TObjectPtr<APVPWeaponActor> CurrentWeapon;

	/**
	 * Equips the weapon when this component does not hold one yet.
	 * The weapon performs the attachment itself and reports success.
	 */
	UFUNCTION(BlueprintCallable, Category = "PVP|Equipment")
	bool EquipWeapon(APVPWeaponActor* Weapon);

	/** Detaches the current weapon. bDropWeapon also places it in front of the owner. */
	UFUNCTION(BlueprintCallable, Category = "PVP|Equipment")
	void UnequipCurrentWeapon(bool bDropWeapon);

	/** Detaches and drops the current weapon. */
	UFUNCTION(BlueprintCallable, Category = "PVP|Equipment")
	void DropCurrentWeapon();

	UFUNCTION(BlueprintPure, Category = "PVP|Equipment")
	APVPWeaponActor* GetCurrentWeapon() const { return CurrentWeapon; }

	UFUNCTION(BlueprintPure, Category = "PVP|Equipment")
	bool HasWeapon() const { return CurrentWeapon != nullptr; }

protected:
	UFUNCTION(BlueprintPure, Category = "PVP|Equipment")
	ACharacter* GetOwnerCharacter() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> CachedOwnerCharacter;
};

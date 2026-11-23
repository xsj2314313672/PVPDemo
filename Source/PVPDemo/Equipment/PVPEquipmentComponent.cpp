// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Equipment/PVPEquipmentComponent.h"

#include "PVPDemo/Equipment/PVPWeaponActor.h"

#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPEquipmentComponent, Log, All);

UPVPEquipmentComponent::UPVPEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPVPEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedOwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!CachedOwnerCharacter.IsValid())
	{
		UE_LOG(LogPVPEquipmentComponent, Warning,
			TEXT("%s: owner is %s, expected ACharacter; weapon attachment will be unavailable."),
			*GetName(), *GetNameSafe(GetOwner()));
	}
}

void UPVPEquipmentComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// The weapon actor is level-owned (not a child of the character), so it survives its holder.
	// Only drop the pointer here; the weapon restores its own pickup state if it is still equipped.
	CurrentWeapon = nullptr;
	CachedOwnerCharacter = nullptr;

	Super::EndPlay(EndPlayReason);
}

bool UPVPEquipmentComponent::EquipWeapon(APVPWeaponActor* Weapon)
{
	if (!Weapon)
	{
		return false;
	}

	if (CurrentWeapon == Weapon)
	{
		return true;
	}

	if (CurrentWeapon)
	{
		UE_LOG(LogPVPEquipmentComponent, Warning,
			TEXT("%s: %s is already holding %s, %s ignored."),
			*GetName(), *GetNameSafe(GetOwner()), *CurrentWeapon->GetName(), *Weapon->GetName());
		return false;
	}

	ACharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		UE_LOG(LogPVPEquipmentComponent, Warning,
			TEXT("%s: owner is not a character, cannot equip %s."), *GetName(), *Weapon->GetName());
		return false;
	}

	if (!Weapon->EquipTo(Character))
	{
		UE_LOG(LogPVPEquipmentComponent, Warning, TEXT("%s: %s rejected equipment by %s."),
			*GetName(), *Weapon->GetName(), *Character->GetName());
		return false;
	}

	CurrentWeapon = Weapon;

	UE_LOG(LogPVPEquipmentComponent, Log, TEXT("%s: equipped %s on %s."),
		*GetName(), *Weapon->GetName(), *Character->GetName());

	return true;
}

void UPVPEquipmentComponent::UnequipCurrentWeapon(bool bDropWeapon)
{
	if (!CurrentWeapon)
	{
		return;
	}

	APVPWeaponActor* Weapon = CurrentWeapon;
	CurrentWeapon = nullptr;

	Weapon->Unequip(bDropWeapon);

	UE_LOG(LogPVPEquipmentComponent, Log, TEXT("%s: unequipped %s (drop=%s)."),
		*GetName(), *Weapon->GetName(), bDropWeapon ? TEXT("true") : TEXT("false"));
}

void UPVPEquipmentComponent::DropCurrentWeapon()
{
	UnequipCurrentWeapon(true);
}

ACharacter* UPVPEquipmentComponent::GetOwnerCharacter() const
{
	if (CachedOwnerCharacter.IsValid())
	{
		return CachedOwnerCharacter.Get();
	}

	return Cast<ACharacter>(GetOwner());
}

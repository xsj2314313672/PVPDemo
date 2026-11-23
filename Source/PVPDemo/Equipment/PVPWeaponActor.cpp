// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Equipment/PVPWeaponActor.h"

#include "PVPDemo/Equipment/PVPEquipmentComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPWeaponActor, Log, All);

APVPWeaponActor::APVPWeaponActor()
{
	PrimaryActorTick.bCanEverTick = false;
	// Weapons are spawned locally on every client (the planned equipment layer is not replicated yet).
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(SceneRoot);
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh1P->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh1P->SetGenerateOverlapEvents(false);
	Mesh1P->SetVisibility(true, true);
	Mesh1P->SetHiddenInGame(false, true);

	Mesh3P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh3P"));
	Mesh3P->SetupAttachment(SceneRoot);
	Mesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh3P->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh3P->SetGenerateOverlapEvents(false);
	Mesh3P->SetVisibility(false, true);
	Mesh3P->SetHiddenInGame(true, true);

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(SceneRoot);
	PickupSphere->InitSphereRadius(PickupRadius);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &APVPWeaponActor::HandlePickupOverlap);
}

void APVPWeaponActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyConfiguration();
}

void APVPWeaponActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyConfiguration();

	if (!bIsEquipped && !bPickupSuppressed)
	{
		SetPickupEnabled(true);
	}
}

void APVPWeaponActor::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(PickupCooldownTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void APVPWeaponActor::ApplyConfiguration()
{
	if (Mesh1P)
	{
		if (Mesh1PAsset)
		{
			Mesh1P->SetSkeletalMeshAsset(Mesh1PAsset);
		}
		if (Mesh1PAnimClass)
		{
			Mesh1P->SetAnimInstanceClass(Mesh1PAnimClass);
		}
		Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh1P->SetGenerateOverlapEvents(false);
	}

	if (Mesh3P)
	{
		if (Mesh3PAsset)
		{
			Mesh3P->SetSkeletalMeshAsset(Mesh3PAsset);
		}
		if (Mesh3PAnimClass)
		{
			Mesh3P->SetAnimInstanceClass(Mesh3PAnimClass);
		}
		Mesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh3P->SetGenerateOverlapEvents(false);
	}

	if (PickupSphere)
	{
		PickupSphere->SetSphereRadius(PickupRadius, true);
	}

	// The third person character is not implemented yet: keep the 3P mesh hidden but fully set up.
	SetPerspective(true);
}

bool APVPWeaponActor::EquipTo(ACharacter* NewOwnerCharacter)
{
	if (!NewOwnerCharacter)
	{
		UE_LOG(LogPVPWeaponActor, Warning, TEXT("%s: EquipTo called without a character."), *GetName());
		return false;
	}

	USkeletalMeshComponent* CharacterMesh = NewOwnerCharacter->GetMesh();
	if (!CharacterMesh)
	{
		UE_LOG(LogPVPWeaponActor, Warning, TEXT("%s: %s has no skeletal mesh component."),
			*GetName(), *NewOwnerCharacter->GetName());
		return false;
	}

	if (bIsEquipped)
	{
		const bool bSameCharacter = OwningCharacter.Get() == NewOwnerCharacter;
		UE_LOG(LogPVPWeaponActor, Warning, TEXT("%s: already equipped by %s (requested by %s)."),
			*GetName(), *GetNameSafe(OwningCharacter.Get()), *NewOwnerCharacter->GetName());
		return bSameCharacter;
	}

	OwningCharacter = NewOwnerCharacter;
	bIsEquipped = true;
	bPickupSuppressed = false;
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(PickupCooldownTimer);
	}
	SetPickupEnabled(false);

	if (!AttachSocketName.IsNone() && !CharacterMesh->DoesSocketExist(AttachSocketName))
	{
		UE_LOG(LogPVPWeaponActor, Warning,
			TEXT("%s: socket '%s' was not found on %s, falling back to the mesh root."),
			*GetName(), *AttachSocketName.ToString(), *CharacterMesh->GetName());
	}

	const FName SocketName = AttachSocketName.IsNone() ? NAME_None : AttachSocketName;
	AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	SetActorRelativeTransform(AttachOffset);

	UE_LOG(LogPVPWeaponActor, Log, TEXT("%s: equipped by %s (socket=%s)."),
		*GetName(), *NewOwnerCharacter->GetName(), *SocketName.ToString());

	if (bPlayEquipMontageOnPickup)
	{
		PlayEquipMontage();
	}

	return true;
}

void APVPWeaponActor::Unequip(bool bDropWeapon)
{
	if (!bIsEquipped)
	{
		return;
	}

	ACharacter* Character = OwningCharacter.Get();

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	OwningCharacter = nullptr;
	bIsEquipped = false;

	if (bDropWeapon && Character)
	{
		const FVector DropLocation = Character->GetActorLocation()
			+ Character->GetActorForwardVector() * DropForwardOffset
			+ FVector(0.0f, 0.0f, DropHeightOffset);
		SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	SetPickupSuppressed(true);

	UE_LOG(LogPVPWeaponActor, Log, TEXT("%s: unequipped (drop=%s)."),
		*GetName(), bDropWeapon ? TEXT("true") : TEXT("false"));
}

void APVPWeaponActor::DropWeapon()
{
	Unequip(true);
}

void APVPWeaponActor::PlayEquipMontage()
{
	PlayPairedMontage(EquipMontage);
}

void APVPWeaponActor::PlayFireMontage()
{
	PlayPairedMontage(FireMontage);
}

void APVPWeaponActor::PlayReloadMontage()
{
	PlayPairedMontage(ReloadMontage);
}

USkeletalMeshComponent* APVPWeaponActor::GetActiveMesh() const
{
	USkeletalMeshComponent* ActiveMesh = bUseFirstPersonMesh ? Mesh1P : Mesh3P;
	if (!ActiveMesh)
	{
		ActiveMesh = Mesh1P ? Mesh1P : Mesh3P;
	}
	return ActiveMesh;
}

FVector APVPWeaponActor::GetMuzzleLocation() const
{
	const USkeletalMeshComponent* ActiveMesh = GetActiveMesh();
	if (ActiveMesh && !MuzzleSocketName.IsNone() && ActiveMesh->DoesSocketExist(MuzzleSocketName))
	{
		return ActiveMesh->GetSocketLocation(MuzzleSocketName);
	}

	return GetActorLocation();
}

void APVPWeaponActor::SetPerspective(bool bFirstPerson)
{
	bUseFirstPersonMesh = bFirstPerson;

	if (Mesh1P)
	{
		Mesh1P->SetVisibility(bFirstPerson, true);
		Mesh1P->SetHiddenInGame(!bFirstPerson, true);
	}

	if (Mesh3P)
	{
		Mesh3P->SetVisibility(!bFirstPerson, true);
		Mesh3P->SetHiddenInGame(bFirstPerson, true);
	}
}

void APVPWeaponActor::HandlePickupOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!bAutoPickup || bIsEquipped || bPickupSuppressed)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	UPVPEquipmentComponent* EquipmentComponent = Pawn->FindComponentByClass<UPVPEquipmentComponent>();
	if (!EquipmentComponent)
	{
		return;
	}

	EquipmentComponent->EquipWeapon(this);
}

void APVPWeaponActor::PlayPairedMontage(const FPVPWeaponMontagePair& MontagePair)
{
	if (MontagePair.IsEmpty())
	{
		UE_LOG(LogPVPWeaponActor, Verbose, TEXT("%s: montage pair is empty."), *GetName());
		return;
	}

	const float PlayRate = FMath::Max(MontagePair.PlayRate, 0.01f);

	if (MontagePair.CharacterMontage)
	{
		if (ACharacter* Character = OwningCharacter.Get())
		{
			if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
			{
				if (UAnimInstance* CharacterAnimInstance = CharacterMesh->GetAnimInstance())
				{
					CharacterAnimInstance->Montage_Play(MontagePair.CharacterMontage, PlayRate);
				}
				else
				{
					UE_LOG(LogPVPWeaponActor, Warning,
						TEXT("%s: %s has no anim instance, character montage '%s' skipped."),
						*GetName(), *Character->GetName(), *MontagePair.CharacterMontage->GetName());
				}
			}
		}
	}

	if (MontagePair.WeaponMontage)
	{
		if (USkeletalMeshComponent* WeaponMesh = GetActiveMesh())
		{
			if (UAnimInstance* WeaponAnimInstance = WeaponMesh->GetAnimInstance())
			{
				WeaponAnimInstance->Montage_Play(MontagePair.WeaponMontage, PlayRate);
			}
			else
			{
				UE_LOG(LogPVPWeaponActor, Warning,
					TEXT("%s: weapon mesh has no anim instance, weapon montage '%s' skipped."),
					*GetName(), *MontagePair.WeaponMontage->GetName());
			}
		}
	}
}

void APVPWeaponActor::SetPickupEnabled(bool bEnabled)
{
	if (!PickupSphere)
	{
		return;
	}

	const bool bShouldEnable = bEnabled && !bIsEquipped && !bPickupSuppressed;
	PickupSphere->SetCollisionEnabled(
		bShouldEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	PickupSphere->SetGenerateOverlapEvents(bShouldEnable);

	if (bShouldEnable)
	{
		// Overlap events are only produced when an overlap starts; re-evaluating here lets a weapon
		// dropped under the character's feet become pickable again once the cooldown elapses.
		PickupSphere->UpdateOverlaps();
	}
}

void APVPWeaponActor::SetPickupSuppressed(bool bSuppressed)
{
	bPickupSuppressed = bSuppressed;

	if (bSuppressed)
	{
		SetPickupEnabled(false);

		if (GetWorld())
		{
			if (PickupCooldown > 0.0f)
			{
				GetWorldTimerManager().SetTimer(
					PickupCooldownTimer, this, &APVPWeaponActor::ResumePickup, PickupCooldown, false);
			}
			else
			{
				ResumePickup();
			}
		}
	}
	else if (!bIsEquipped)
	{
		SetPickupEnabled(true);
	}
}

void APVPWeaponActor::ResumePickup()
{
	bPickupSuppressed = false;
	if (!bIsEquipped)
	{
		SetPickupEnabled(true);
	}
}

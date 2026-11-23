// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PVPWeaponAttachmentTypes.generated.h"

class UStaticMesh;

/**
 * Attachment slot of the PVP assault rifle.
 *
 * This enum is the single source of truth for the attachment taxonomy: the editor assembly builder,
 * the runtime weapon actor and the attachment catalog data asset all use it. Blueprint assets must
 * not redeclare the same enum (two definitions would drift silently).
 *
 * Parent chain:
 *   None (Mesh1P)  -> Handguard, RearSight, Optic, Magazine
 *   Handguard      -> FrontSight, ForeGrip, Muzzle, Laser
 */
UENUM(BlueprintType)
enum class E_PVPWeaponAttachmentSlot : uint8
{
	None UMETA(DisplayName = "None"),
	Handguard UMETA(DisplayName = "Handguard"),
	FrontSight UMETA(DisplayName = "Front Sight"),
	RearSight UMETA(DisplayName = "Rear Sight"),
	Optic UMETA(DisplayName = "Optic"),
	ForeGrip UMETA(DisplayName = "Fore Grip"),
	Muzzle UMETA(DisplayName = "Muzzle"),
	Laser UMETA(DisplayName = "Laser"),
	Magazine UMETA(DisplayName = "Magazine")
};

/**
 * One assembled attachment: which slot it fills, which socket it snaps to and which mesh it uses.
 *
 * SocketName is resolved on ParentSlot's mesh component, not on Mesh1P: SOCKET_Sight_Front,
 * SOCKET_Grip, SOCKET_Laser and SOCKET_Muzzle only exist on SM_PVP_AR_Handguard_*.
 */
USTRUCT(BlueprintType)
struct FPVPWeaponAttachmentSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	E_PVPWeaponAttachmentSlot Slot = E_PVPWeaponAttachmentSlot::None;

	/** None = attach to Mesh1P; Handguard = attach to the handguard component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	E_PVPWeaponAttachmentSlot ParentSlot = E_PVPWeaponAttachmentSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Blueprint component variable name (ATT_<Slot>); the assembly builder keeps it stable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FName ComponentName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	bool bEnabled = true;
};

/** One interchangeable mesh for a slot (all options share the slot's socket and parent). */
USTRUCT(BlueprintType)
struct FPVPWeaponAttachmentOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	bool bIsDefaultOption = false;
};

/** Static description of a slot: attachment parent, socket, component name and the mesh options. */
USTRUCT(BlueprintType)
struct FPVPWeaponAttachmentSlotDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	E_PVPWeaponAttachmentSlot Slot = E_PVPWeaponAttachmentSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	E_PVPWeaponAttachmentSlot ParentSlot = E_PVPWeaponAttachmentSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FName ComponentName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	TArray<FPVPWeaponAttachmentOption> Options;
};

/** One slot choice inside a named loadout. */
USTRUCT(BlueprintType)
struct FPVPWeaponAttachmentLoadoutEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	E_PVPWeaponAttachmentSlot Slot = E_PVPWeaponAttachmentSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	TObjectPtr<UStaticMesh> Mesh = nullptr;
};

/** A named assembly preset (Basic / Tactical). */
USTRUCT(BlueprintType)
struct FPVPWeaponAttachmentLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FName LoadoutName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVP|Weapon|Attachment")
	TArray<FPVPWeaponAttachmentLoadoutEntry> Entries;
};

/** Slot <-> name / tag / socket conventions shared by runtime code, the builder and the scripts. */
namespace PVPWeaponAttachment
{
	inline FName SlotToName(E_PVPWeaponAttachmentSlot Slot)
	{
		switch (Slot)
		{
		case E_PVPWeaponAttachmentSlot::Handguard: return FName(TEXT("Handguard"));
		case E_PVPWeaponAttachmentSlot::FrontSight: return FName(TEXT("FrontSight"));
		case E_PVPWeaponAttachmentSlot::RearSight: return FName(TEXT("RearSight"));
		case E_PVPWeaponAttachmentSlot::Optic: return FName(TEXT("Optic"));
		case E_PVPWeaponAttachmentSlot::ForeGrip: return FName(TEXT("ForeGrip"));
		case E_PVPWeaponAttachmentSlot::Muzzle: return FName(TEXT("Muzzle"));
		case E_PVPWeaponAttachmentSlot::Laser: return FName(TEXT("Laser"));
		case E_PVPWeaponAttachmentSlot::Magazine: return FName(TEXT("Magazine"));
		default: return FName(TEXT("None"));
		}
	}

	/** Blueprint component variable name, e.g. ATT_Handguard. */
	inline FName MakeComponentName(E_PVPWeaponAttachmentSlot Slot)
	{
		return FName(*FString::Printf(TEXT("ATT_%s"), *SlotToName(Slot).ToString()));
	}

	/** Component tag used by APVPWeaponActor to build its slot table, e.g. PVP.Attachment.Handguard. */
	inline FName MakeComponentTag(E_PVPWeaponAttachmentSlot Slot)
	{
		return FName(*FString::Printf(TEXT("PVP.Attachment.%s"), *SlotToName(Slot).ToString()));
	}

	/** Socket on the parent mesh that the attachment snaps to. */
	inline FName DefaultSocketName(E_PVPWeaponAttachmentSlot Slot)
	{
		switch (Slot)
		{
		case E_PVPWeaponAttachmentSlot::Handguard: return FName(TEXT("SOCKET_Handguard"));
		case E_PVPWeaponAttachmentSlot::FrontSight: return FName(TEXT("SOCKET_Sight_Front"));
		case E_PVPWeaponAttachmentSlot::RearSight: return FName(TEXT("SOCKET_Sight_Rear"));
		case E_PVPWeaponAttachmentSlot::Optic: return FName(TEXT("SOCKET_Scope"));
		case E_PVPWeaponAttachmentSlot::ForeGrip: return FName(TEXT("SOCKET_Grip"));
		case E_PVPWeaponAttachmentSlot::Muzzle: return FName(TEXT("SOCKET_Muzzle"));
		case E_PVPWeaponAttachmentSlot::Laser: return FName(TEXT("SOCKET_Laser"));
		case E_PVPWeaponAttachmentSlot::Magazine: return FName(TEXT("SOCKET_Magazine"));
		default: return NAME_None;
		}
	}

	/** Socket owners: the four sub-slot sockets only exist on the handguard meshes. */
	inline E_PVPWeaponAttachmentSlot DefaultParentSlot(E_PVPWeaponAttachmentSlot Slot)
	{
		switch (Slot)
		{
		case E_PVPWeaponAttachmentSlot::FrontSight:
		case E_PVPWeaponAttachmentSlot::ForeGrip:
		case E_PVPWeaponAttachmentSlot::Muzzle:
		case E_PVPWeaponAttachmentSlot::Laser:
			return E_PVPWeaponAttachmentSlot::Handguard;
		default:
			return E_PVPWeaponAttachmentSlot::None;
		}
	}

	inline E_PVPWeaponAttachmentSlot SlotFromComponentTag(FName Tag)
	{
		const E_PVPWeaponAttachmentSlot AllSlots[] = {
			E_PVPWeaponAttachmentSlot::Handguard,
			E_PVPWeaponAttachmentSlot::FrontSight,
			E_PVPWeaponAttachmentSlot::RearSight,
			E_PVPWeaponAttachmentSlot::Optic,
			E_PVPWeaponAttachmentSlot::ForeGrip,
			E_PVPWeaponAttachmentSlot::Muzzle,
			E_PVPWeaponAttachmentSlot::Laser,
			E_PVPWeaponAttachmentSlot::Magazine,
		};
		for (const E_PVPWeaponAttachmentSlot Slot : AllSlots)
		{
			if (MakeComponentTag(Slot) == Tag)
			{
				return Slot;
			}
		}
		return E_PVPWeaponAttachmentSlot::None;
	}

	inline bool IsValidSlot(E_PVPWeaponAttachmentSlot Slot)
	{
		return Slot != E_PVPWeaponAttachmentSlot::None;
	}
}

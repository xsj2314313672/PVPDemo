// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Equipment/PVPWeaponAttachmentCatalog.h"

namespace
{
	FPVPWeaponAttachmentSpec MakeSpec(
		const FPVPWeaponAttachmentSlotDefinition& Definition,
		UStaticMesh* Mesh,
		bool bEnabled)
	{
		FPVPWeaponAttachmentSpec Spec;
		Spec.Slot = Definition.Slot;
		Spec.ParentSlot = Definition.ParentSlot;
		Spec.SocketName = Definition.SocketName;
		Spec.ComponentName = Definition.ComponentName.IsNone()
			? PVPWeaponAttachment::MakeComponentName(Definition.Slot)
			: Definition.ComponentName;
		Spec.Mesh = Mesh;
		Spec.bEnabled = bEnabled;
		return Spec;
	}

	UStaticMesh* FindDefaultMesh(const FPVPWeaponAttachmentSlotDefinition& Definition)
	{
		for (const FPVPWeaponAttachmentOption& Option : Definition.Options)
		{
			if (Option.bIsDefaultOption && Option.Mesh)
			{
				return Option.Mesh;
			}
		}
		for (const FPVPWeaponAttachmentOption& Option : Definition.Options)
		{
			if (Option.Mesh)
			{
				return Option.Mesh;
			}
		}
		return nullptr;
	}
}

bool UPVPWeaponAttachmentCatalog::FindSlotDefinition(
	E_PVPWeaponAttachmentSlot Slot,
	FPVPWeaponAttachmentSlotDefinition& OutDefinition) const
{
	for (const FPVPWeaponAttachmentSlotDefinition& Definition : Slots)
	{
		if (Definition.Slot == Slot)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	return false;
}

bool UPVPWeaponAttachmentCatalog::FindLoadout(FName LoadoutName, FPVPWeaponAttachmentLoadout& OutLoadout) const
{
	for (const FPVPWeaponAttachmentLoadout& Loadout : Loadouts)
	{
		if (Loadout.LoadoutName == LoadoutName)
		{
			OutLoadout = Loadout;
			return true;
		}
	}
	return false;
}

TArray<FPVPWeaponAttachmentSpec> UPVPWeaponAttachmentCatalog::BuildLoadoutAssembly(FName LoadoutName) const
{
	TArray<FPVPWeaponAttachmentSpec> Result;

	FPVPWeaponAttachmentLoadout Loadout;
	if (!FindLoadout(LoadoutName, Loadout))
	{
		return Result;
	}

	for (const FPVPWeaponAttachmentLoadoutEntry& Entry : Loadout.Entries)
	{
		FPVPWeaponAttachmentSlotDefinition Definition;
		if (!FindSlotDefinition(Entry.Slot, Definition))
		{
			continue;
		}

		UStaticMesh* Mesh = Entry.Mesh;
		if (!Mesh)
		{
			Mesh = FindDefaultMesh(Definition);
		}
		if (Mesh)
		{
			Result.Add(MakeSpec(Definition, Mesh, true));
		}
	}

	return Result;
}

TArray<FPVPWeaponAttachmentSpec> UPVPWeaponAttachmentCatalog::BuildDefaultAssembly() const
{
	TArray<FPVPWeaponAttachmentSpec> Result;
	Result.Reserve(Slots.Num());

	for (const FPVPWeaponAttachmentSlotDefinition& Definition : Slots)
	{
		if (UStaticMesh* Mesh = FindDefaultMesh(Definition))
		{
			Result.Add(MakeSpec(Definition, Mesh, true));
		}
	}

	return Result;
}

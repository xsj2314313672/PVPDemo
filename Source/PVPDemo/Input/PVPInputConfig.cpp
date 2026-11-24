// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Input/PVPInputConfig.h"

#include "PVPDemo/GAS/PVPGameplayTags.h"

namespace
{
	/**
	 * Single source of truth for the native tag slot order. The constructor seeds a fresh asset and
	 * EnsureNativeInputActionSlots migrates an asset saved before Aim / Fire existed.
	 */
	void AppendNativeInputTagOrder(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Add(PVPGameplayTags::InputTag_Move);
		OutTags.Add(PVPGameplayTags::InputTag_Look);
		OutTags.Add(PVPGameplayTags::InputTag_Jump);
		OutTags.Add(PVPGameplayTags::InputTag_Run);
		OutTags.Add(PVPGameplayTags::InputTag_Aim);
		OutTags.Add(PVPGameplayTags::InputTag_Fire);
	}
}

UPVPInputConfig::UPVPInputConfig()
{
	// Seed one slot per native input tag. Gameplay tags cannot be built from Python (the tag name is
	// private to the gameplay tag manager), so the setup script only fills InputAction on the
	// existing slots. The order matches setup_pvp_input.py: Move / Look / Jump / Run / Aim / Fire.
	TArray<FGameplayTag> TagOrder;
	AppendNativeInputTagOrder(TagOrder);
	for (const FGameplayTag& Tag : TagOrder)
	{
		NativeInputActions.AddDefaulted_GetRef().InputTag = Tag;
	}
}

int32 UPVPInputConfig::EnsureNativeInputActionSlots()
{
	TArray<FGameplayTag> TagOrder;
	AppendNativeInputTagOrder(TagOrder);

	int32 AddedCount = 0;
	for (const FGameplayTag& Tag : TagOrder)
	{
		const bool bSlotExists = NativeInputActions.ContainsByPredicate(
			[&Tag](const FPVPInputAction& Entry) { return Entry.InputTag == Tag; });
		if (bSlotExists)
		{
			continue;
		}

		if (AddedCount == 0)
		{
			Modify();
		}

		NativeInputActions.AddDefaulted_GetRef().InputTag = Tag;
		++AddedCount;
	}

	if (AddedCount > 0)
	{
		MarkPackageDirty();
	}

	return AddedCount;
}

bool UPVPInputConfig::SetNativeInputActionByIndex(int32 Index, UInputAction* InInputAction)
{
	if (!NativeInputActions.IsValidIndex(Index))
	{
		return false;
	}

	Modify();
	NativeInputActions[Index].InputAction = InInputAction;
	MarkPackageDirty();
	return true;
}

const UInputAction* UPVPInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag) const
{
	for (const FPVPInputAction& NativeInputAction : NativeInputActions)
	{
		if (NativeInputAction.InputAction && NativeInputAction.InputTag == InputTag)
		{
			return NativeInputAction.InputAction;
		}
	}

	return nullptr;
}

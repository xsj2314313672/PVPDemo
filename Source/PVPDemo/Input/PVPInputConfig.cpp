// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Input/PVPInputConfig.h"

#include "PVPDemo/GAS/PVPGameplayTags.h"

UPVPInputConfig::UPVPInputConfig()
{
	// Seed one slot per native input tag. Gameplay tags cannot be built from Python (the tag name is
	// private to the gameplay tag manager), so the setup script only fills InputAction on the
	// existing slots. The order matches setup_pvp_input.py: Move / Look / Jump / Run.
	NativeInputActions.AddDefaulted_GetRef().InputTag = PVPGameplayTags::InputTag_Move;
	NativeInputActions.AddDefaulted_GetRef().InputTag = PVPGameplayTags::InputTag_Look;
	NativeInputActions.AddDefaulted_GetRef().InputTag = PVPGameplayTags::InputTag_Jump;
	NativeInputActions.AddDefaulted_GetRef().InputTag = PVPGameplayTags::InputTag_Run;
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

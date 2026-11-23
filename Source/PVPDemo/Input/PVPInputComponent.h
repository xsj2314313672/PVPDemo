// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"

#include "PVPDemo/Input/PVPInputConfig.h"

#include "PVPInputComponent.generated.h"

/**
 * Enhanced Input component with tag based binding helpers (Lyra style).
 *
 * The pawn uses this class through APawn::OverrideInputComponentClass, so no change to
 * Config/DefaultInput.ini is required. BindNativeAction returns the new binding on success and
 * null when the config has no action for the tag, which lets the caller log and skip.
 */
UCLASS()
class PVPDEMO_API UPVPInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	template <class UserClass, typename FuncType>
	FEnhancedInputActionEventBinding* BindNativeAction(
		const UPVPInputConfig* InputConfig,
		const FGameplayTag& InputTag,
		ETriggerEvent TriggerEvent,
		UserClass* Object,
		FuncType Func)
	{
		if (!InputConfig)
		{
			return nullptr;
		}

		if (const UInputAction* InputAction = InputConfig->FindNativeInputActionForTag(InputTag))
		{
			return &BindAction(InputAction, TriggerEvent, Object, Func);
		}

		return nullptr;
	}
};

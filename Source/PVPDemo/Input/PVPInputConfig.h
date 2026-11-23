// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "PVPInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * A native input action together with the gameplay tag that identifies it.
 *
 * Mirrors the Lyra input config: the character binds by tag ("InputTag.Move"), the data asset maps
 * the tag to the concrete IA asset, so the binding code never references content paths.
 */
USTRUCT(BlueprintType)
struct FPVPInputAction
{
	GENERATED_BODY()

	// EditAnywhere (not EditDefaultsOnly) on the struct members: the containing array is
	// EditDefaultsOnly, and script / details customization must be able to edit the nested struct.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InputAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * An input mapping context and its priority, applied by APVPPlayerController on BeginPlay.
 *
 * Lyra keeps a similar struct on its hero component. The player controller can only add mapping
 * contexts once a local player exists, hence the entry lives here instead of on the pawn.
 */
USTRUCT(BlueprintType)
struct FPVPInputMappingContextAndPriority
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMapping = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 Priority = 0;
};

/**
 * Data asset that maps native input tags to Enhanced Input actions (Lyra style).
 *
 * One asset (DA_PVP_InputConfig) is assigned to BP_PVPBaseCharacter. Sprint, crouch and ability
 * inputs are added here later without touching the character binding flow.
 */
UCLASS(BlueprintType)
class PVPDEMO_API UPVPInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPVPInputConfig();

	/** Returns the action registered for InputTag, or null when the tag is not configured. */
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag) const;

	/**
	 * Sets the action of an existing tag slot. Used by Content/Python/setup_pvp_input.py: gameplay
	 * tags cannot be built from script, so the tag slots are seeded in the constructor and script
	 * only fills the actions in by index.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool SetNativeInputActionByIndex(int32 Index, UInputAction* InInputAction);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FPVPInputAction> NativeInputActions;
};

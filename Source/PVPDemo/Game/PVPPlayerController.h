// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "PVPDemo/Input/PVPInputConfig.h"

#include "PVPPlayerController.generated.h"

class APVPBaseCharacter;

/**
 * Player entry controller for the PVP project.
 *
 * It owns the input mode, the possession bookkeeping, the default input mapping contexts and a
 * read-only accessor for the possessed character. Native action bindings live on the pawn
 * (APVPBaseCharacter::SetupPlayerInputComponent) so the Input layer keeps one tag -> action config;
 * this controller only applies the mapping contexts once a local player exists.
 */
UCLASS(Blueprintable)
class PVPDEMO_API APVPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APVPPlayerController();

	//~ Begin APlayerController Interface
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	//~ End APlayerController Interface

	/**
	 * Enhanced Input mapping contexts applied on BeginPlay, lowest priority first.
	 *
	 * The constructor seeds it with IMC_PVP_Default (no blueprint setup required); derived
	 * blueprints can replace or extend the list.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PVP|Input")
	TArray<FPVPInputMappingContextAndPriority> DefaultMappingContexts;

	/** Possessed PVP character, or null when the pawn is not a APVPBaseCharacter. Read-only for animation / camera layers. */
	UFUNCTION(BlueprintPure, Category = "PVP|Pawn")
	APVPBaseCharacter* GetPVPCharacter() const;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Game/PVPPlayerController.h"

#include "PVPDemo/Character/PVPBaseCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPPlayerController, Log, All);

namespace PVPPlayerControllerDefaults
{
	/** Code default for the mapping context applied on BeginPlay (no blueprint setup required). */
	static const TCHAR* DefaultInputMappingContextPath =
		TEXT("/Game/PVP/Input/IMC_PVP_Default.IMC_PVP_Default");
}

APVPPlayerController::APVPPlayerController()
{
	// First person shooter: the viewport stays mouse-locked and the cursor is hidden.
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	// Lyra style: provide the default mapping context in code so even a bare controller is
	// playable. The array stays editable, so derived blueprints can override or extend it.
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultMappingContextFinder(
		PVPPlayerControllerDefaults::DefaultInputMappingContextPath);
	if (DefaultMappingContextFinder.Succeeded())
	{
		FPVPInputMappingContextAndPriority& DefaultMapping = DefaultMappingContexts.AddDefaulted_GetRef();
		DefaultMapping.InputMapping = DefaultMappingContextFinder.Object;
		DefaultMapping.Priority = 0;
	}
}

void APVPPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Keep gameplay input focused on the world.
	SetInputMode(FInputModeGameOnly());

	// Apply the default Enhanced Input mapping contexts. Remote controllers on a listen server (and
	// dedicated servers) have no local player, hence the guard.
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->ClearAllMappings();

			int32 AppliedContexts = 0;
			for (const FPVPInputMappingContextAndPriority& DefaultMapping : DefaultMappingContexts)
			{
				if (DefaultMapping.InputMapping)
				{
					InputSubsystem->AddMappingContext(DefaultMapping.InputMapping, DefaultMapping.Priority);
					++AppliedContexts;
				}
			}

			if (AppliedContexts > 0)
			{
				UE_LOG(LogPVPPlayerController, Log,
					TEXT("%s: applied %d default input mapping context(s)"), *GetName(), AppliedContexts);
			}
			else
			{
				UE_LOG(LogPVPPlayerController, Warning,
					TEXT("%s: no default input mapping context applied (IMC asset missing?), input will not work."),
					*GetName());
			}
		}
	}

	UE_LOG(LogPVPPlayerController, Log, TEXT("%s: BeginPlay (GameOnly input mode)"), *GetName());
}

void APVPPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Native action bindings live on the pawn (APVPBaseCharacter::SetupPlayerInputComponent) through
	// UPVPInputComponent, so both native and future ability input share PVPInputConfig. This
	// controller only applies the default mapping contexts in BeginPlay.
}

void APVPPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogPVPPlayerController, Log, TEXT("%s: OnPossess pawn=%s path=%s"),
		*GetName(),
		*GetNameSafe(InPawn),
		InPawn ? *InPawn->GetPathName() : TEXT("None"));
}

APVPBaseCharacter* APVPPlayerController::GetPVPCharacter() const
{
	return Cast<APVPBaseCharacter>(GetPawn());
}

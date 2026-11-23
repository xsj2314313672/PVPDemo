// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemo/Game/PVPGameMode.h"

#include "PVPDemo/Character/PVPBaseCharacter.h"
#include "PVPDemo/Game/PVPPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPGameMode, Log, All);

APVPGameMode::APVPGameMode()
{
	PlayerControllerClass = APVPPlayerController::StaticClass();
	DefaultPawnClass = APVPBaseCharacter::StaticClass();

	// Players spawn as pawns right away; spectating / waiting states belong to a later match flow.
	bStartPlayersAsSpectators = false;
}

void APVPGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UE_LOG(LogPVPGameMode, Log, TEXT("%s: InitGame map=%s options=%s"), *GetName(), *MapName, *Options);
}

void APVPGameMode::StartPlay()
{
	Super::StartPlay();

	UE_LOG(LogPVPGameMode, Log, TEXT("%s: StartPlay default_pawn=%s player_controller=%s"),
		*GetName(),
		*GetNameSafe(DefaultPawnClass),
		*GetNameSafe(PlayerControllerClass));
}

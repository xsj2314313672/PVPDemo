// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "PVPGameMode.generated.h"

/**
 * Session and player entry game mode for the PVP project.
 *
 * Responsibilities:
 *  - declare the native defaults for the player controller (APVPPlayerController) and the pawn
 *    (APVPBaseCharacter), so a bare C++ game mode is already playable;
 *  - log the session entry points (InitGame / StartPlay) so the actual game mode in use can be
 *    verified from the log.
 *
 * No blueprint path is hard coded here on purpose: pointing DefaultPawnClass at BP_PVPBaseCharacter
 * is the job of the BP_PVPGameMode class default object, so this class never fails to construct when
 * content is missing. GameState / PlayerState / match flow are intentionally left for later layers.
 */
UCLASS(Blueprintable)
class PVPDEMO_API APVPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APVPGameMode();

	//~ Begin AGameModeBase Interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	//~ End AGameModeBase Interface
};

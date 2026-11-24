// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Native gameplay tags owned by the PVP runtime module.
 *
 * Only the tags that are actually consumed by the current milestone live here. Input tags are the
 * semantic names used by UPVPInputConfig: the character binds native input by tag instead of by a
 * hard reference, so adding Sprint / Crouch / ability inputs later only extends the config asset.
 */
namespace PVPGameplayTags
{
	/** Move the character relative to the control rotation yaw (IA_PVP_Move). */
	extern PVPDEMO_API FNativeGameplayTag InputTag_Move;

	/** Yaw / pitch look input (IA_PVP_Look). */
	extern PVPDEMO_API FNativeGameplayTag InputTag_Look;

	/** Jump while pressed, stop jumping when released (IA_PVP_Jump). */
	extern PVPDEMO_API FNativeGameplayTag InputTag_Jump;

	/** Hold to run: switches the walk speed to the run speed (IA_PVP_Run). */
	extern PVPDEMO_API FNativeGameplayTag InputTag_Run;

	/** Hold to aim down sights: aimed locomotion / camera zoom (IA_PVP_ADS). */
	extern PVPDEMO_API FNativeGameplayTag InputTag_Aim;

	/** Press to fire one shot: plays one paired fire montage (IA_PVP_Fire). */
	extern PVPDEMO_API FNativeGameplayTag InputTag_Fire;
}

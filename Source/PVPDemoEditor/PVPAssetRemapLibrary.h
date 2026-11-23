// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "PVPAssetRemapLibrary.generated.h"

/**
 * Editor-only asset maintenance helpers.
 *
 * The project keeps every piece of content it uses under /Game/PVPDemo (art) and /Game/PVP (logic).
 * Third-party packages are read-only references at development time only. RemapAssetReferences() is
 * the one-shot tool that rewrites references from the third-party packages to the project copies:
 * it uses ObjectTools::ForceReplaceReferences, which walks the reflected (serialized) object graph of
 * the scanned packages, so blueprint graphs, material parameters and component templates are covered.
 *
 * Out of scope on purpose: an AActor's class reference is not a reflected property, so placed actor
 * instances are NOT re-classed by this tool. The Python driver re-instantiates those actors from the
 * project-local copies instead.
 */
UCLASS()
class UPVPAssetRemapLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Replaces references to InFromPackages with InToPackages inside every asset under InPackagesToScan.
	 *
	 * @param InFromPackages   Source package paths (index aligned with InToPackages).
	 * @param InToPackages     Project-local package paths replacing the source packages.
	 * @param InPackagesToScan Only assets below these package path prefixes are rewritten.
	 * @param InReportPath     Absolute path of the JSON report to write.
	 * @return true when every mapping resolved and the modified packages were saved.
	 */
	UFUNCTION(BlueprintCallable, Category = "PVP|Tools")
	static bool RemapAssetReferences(const TArray<FString>& InFromPackages,
		const TArray<FString>& InToPackages,
		const TArray<FString>& InPackagesToScan,
		const FString& InReportPath);
};

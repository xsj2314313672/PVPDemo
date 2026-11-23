// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * Editor-only module for the PVP project.
 *
 * It hosts editor-side maintenance tooling that must never be linked into the runtime module,
 * e.g. the asset reference remapper used to make the project content self contained.
 * Registered in PVPDemo.uproject with "Type": "Editor" and LoadingPhase PostEngineInit.
 */
public class PVPDemoEditor : ModuleRules
{
	public PVPDemoEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetRegistry",
			"Json",
			// Weapon attachment builder shares the runtime slot types (E_PVPWeaponAttachmentSlot,
			// FPVPWeaponAttachmentSpec) with the runtime module. Editor -> Runtime is one way only.
			"PVPDemo"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetTools",
			"Projects",
			// AnimGraph builder: graph nodes, anim graph schemas and blueprint K2 rule nodes.
			"AnimGraph",
			"AnimGraphRuntime",
			"BlueprintGraph"
		});
	}
}

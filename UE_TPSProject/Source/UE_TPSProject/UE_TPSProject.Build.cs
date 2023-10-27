// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE_TPSProject : ModuleRules
{
	public UE_TPSProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}

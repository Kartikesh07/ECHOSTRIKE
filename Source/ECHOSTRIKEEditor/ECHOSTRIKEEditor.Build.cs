using UnrealBuildTool;
public class ECHOSTRIKEEditor : ModuleRules {
 public ECHOSTRIKEEditor(ReadOnlyTargetRules Target) : base(Target) {
 PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
 PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "ECHOSTRIKE", "UnrealEd", "UMG", "UMGEditor", "Slate", "SlateCore", "Kismet", "MovieScene", "MovieSceneTracks" });
 }
}

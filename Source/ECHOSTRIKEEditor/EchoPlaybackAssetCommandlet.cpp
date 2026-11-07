#include "EchoPlaybackAssetCommandlet.h"
#include "TimeEcho/EchoGhostCharacter.h"
#include "Player/EchoCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

UEchoPlaybackAssetCommandlet::UEchoPlaybackAssetCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UEchoPlaybackAssetCommandlet::Main(const FString& Params)
{
	auto Save = [](UObject* Asset)
	{
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
	};
	const TCHAR* MaterialPath = TEXT("/Game/EchoStrike/Materials/M_EchoHologram");
	UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/EchoStrike/Materials/M_EchoHologram.M_EchoHologram"));
	if (!Material)
	{
		Material = NewObject<UMaterial>(CreatePackage(MaterialPath), TEXT("M_EchoHologram"), RF_Public | RF_Standalone);
		Material->SetShadingModel(MSM_Unlit);
		Material->BlendMode = BLEND_Translucent;
		Material->TwoSided = true;
		Material->bUsedWithSkeletalMesh = true;
		auto* Color = NewObject<UMaterialExpressionVectorParameter>(Material);
		Color->ParameterName = TEXT("HologramColor");
		Color->DefaultValue = FLinearColor(0.02f, 2.f, 3.f, 1.f);
		auto* Opacity = NewObject<UMaterialExpressionScalarParameter>(Material);
		Opacity->ParameterName = TEXT("HologramOpacity");
		Opacity->DefaultValue = .42f;
		Material->GetExpressionCollection().AddExpression(Color);
		Material->GetExpressionCollection().AddExpression(Opacity);
		Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Color);
		Material->GetEditorOnlyData()->Opacity.Connect(0, Opacity);
		Material->PostEditChange();
		if (!Save(Material)) return 1;
	}
	const TCHAR* BPPath = TEXT("/Game/EchoStrike/Blueprints/BP_EchoGhostCharacter");
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/EchoStrike/Blueprints/BP_EchoGhostCharacter.BP_EchoGhostCharacter"));
	if (!BP)
	{
		BP = FKismetEditorUtilities::CreateBlueprint(AEchoGhostCharacter::StaticClass(), CreatePackage(BPPath),
			TEXT("BP_EchoGhostCharacter"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		FKismetEditorUtilities::CompileBlueprint(BP);
		UClass* CharacterClass = LoadClass<AEchoCharacter>(nullptr, TEXT("/Game/EchoStrike/Blueprints/BP_EchoCharacter.BP_EchoCharacter_C"));
		if (!CharacterClass || !BP->GeneratedClass) return 1;
		const USkeletalMeshComponent* SourceMesh = CharacterClass->GetDefaultObject<AEchoCharacter>()->GetMesh();
		auto* Ghost = BP->GeneratedClass->GetDefaultObject<AEchoGhostCharacter>();
		Ghost->Mesh->SetSkeletalMesh(SourceMesh->GetSkeletalMeshAsset());
		Ghost->Mesh->SetRelativeTransform(SourceMesh->GetRelativeTransform());
		Ghost->Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		for (int32 Slot = 0; Slot < Ghost->Mesh->GetNumMaterials(); ++Slot) Ghost->Mesh->SetMaterial(Slot, Material);
		if (!Save(BP)) return 1;
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error) return 1;
	const auto* Ghost = BP->GeneratedClass->GetDefaultObject<AEchoGhostCharacter>();
	if (!Ghost->Mesh->GetSkeletalMeshAsset() || Ghost->Mesh->GetMaterial(0) != Material) return 1;
	UE_LOG(LogTemp, Display, TEXT("EchoPlayback: hologram material and ghost Blueprint compiled and verified."));
	return 0;
}

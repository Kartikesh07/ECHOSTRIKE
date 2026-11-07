#include "EchoHUDAssetCommandlet.h"
#include "Modules/ModuleManager.h"
#include "UI/EchoHUDWidgets.h"
#include "UI/EchoCycleWidget.h"
#include "Misc/Parse.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/SafeZone.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Engine/Font.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/ConstructorHelpers.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ECHOSTRIKEEditor)
namespace EchoHUDAssets
{
const FLinearColor Cyan(.04f, .82f, 1.f, 1.f);
UWidgetBlueprint* Make(const TCHAR* Name, UClass* Parent, bool& bNew)
{
	const FString Path = FString(TEXT("/Game/EchoStrike/UI/")) + Name;
	if (auto* Existing = LoadObject<UWidgetBlueprint>(nullptr, *(Path + TEXT(".") + Name)))
	{
		bNew = false;
		return Existing;
	}
	bNew = true;
	auto* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = Parent;
	auto* BP = CastChecked<UWidgetBlueprint>(Factory->FactoryCreateNew(
		UWidgetBlueprint::StaticClass(), CreatePackage(*Path), FName(Name), RF_Public | RF_Standalone, nullptr, GWarn));
	BP->WidgetTree = NewObject<UWidgetTree>(BP, TEXT("WidgetTree"), RF_Transactional);
	return BP;
}
template <class T> T* Widget(UWidgetBlueprint* BP, const TCHAR* Name)
{
	auto* W = BP->WidgetTree->ConstructWidget<T>(T::StaticClass(), FName(Name));
	W->SetVisibility(ESlateVisibility::HitTestInvisible);
	return W;
}
UTextBlock* Text(UWidgetBlueprint* BP, const TCHAR* Name, const TCHAR* Value, int32 Size, FLinearColor Color = Cyan)
{
	auto* W = Widget<UTextBlock>(BP, Name);
	W->SetText(FText::FromString(Value));
	// A UObject font reference survives asset serialization and is included when cooking.
	W->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")), Size,
		FName("Regular")));
	W->SetColorAndOpacity(Color);
	W->SetShadowColorAndOpacity(FLinearColor(0.f, .3f, .4f, .7f));
	W->SetShadowOffset(FVector2D(0, 1));
	return W;
}
UVerticalBox* Panel(UWidgetBlueprint* BP)
{
	auto* B = Widget<UBorder>(BP, TEXT("Panel"));
	B->SetBrushColor(FLinearColor(.008f, .018f, .032f, .84f));
	B->SetPadding(FMargin(18, 14));
	BP->WidgetTree->RootWidget = B;
	auto* V = Widget<UVerticalBox>(BP, TEXT("Content"));
	B->SetContent(V);
	return V;
}
void Line(UVerticalBox* Box, UWidget* W, float Padding = 6)
{
	Box->AddChildToVerticalBox(W)->SetPadding(FMargin(0, Padding, 0, 0));
}
void Bar(UWidgetBlueprint* BP, UVerticalBox* V, const TCHAR* Name, float Height)
{
	auto* S = Widget<USizeBox>(BP, *(FString(Name) + TEXT("Size")));
	S->SetHeightOverride(Height);
	auto* P = Widget<UProgressBar>(BP, Name);
	FProgressBarStyle Style;
	FSlateBrush Background, Fill;
	Background.TintColor = FSlateColor(FLinearColor(.04f, .08f, .12f, 1));
	Fill.TintColor = FSlateColor(FLinearColor::White);
	Style.SetBackgroundImage(Background).SetFillImage(Fill);
	P->SetWidgetStyle(Style);
	P->SetFillColorAndOpacity(Cyan);
	P->SetPercent(1);
	S->SetContent(P);
	Line(V, S, 10);
}
void Pulse(UWidgetBlueprint* BP, const TCHAR* Name, const TCHAR* Target, float From, float To, float Duration)
{
	auto* A = NewObject<UWidgetAnimation>(BP, FName(Name), RF_Transactional);
	A->MovieScene = NewObject<UMovieScene>(A, FName(Name), RF_Transactional);
	A->MovieScene->SetDisplayRate(FFrameRate(60, 1));
	A->MovieScene->SetTickResolutionDirectly(FFrameRate(60, 1));
	const int32 End = FMath::RoundToInt(Duration * 60);
	A->MovieScene->SetPlaybackRange(0, End);
	const FGuid Guid = A->MovieScene->AddPossessable(Target, UWidget::StaticClass());
	FWidgetAnimationBinding Binding;
	Binding.AnimationGuid = Guid;
	Binding.WidgetName = FName(Target);
	A->AnimationBindings.Add(Binding);
	auto* Track = A->MovieScene->AddTrack<UMovieSceneFloatTrack>(Guid);
	Track->SetPropertyNameAndPath(TEXT("RenderOpacity"), TEXT("RenderOpacity"));
	auto* Section = CastChecked<UMovieSceneFloatSection>(Track->CreateNewSection());
	Section->SetRange(TRange<FFrameNumber>(0, End));
	Section->GetChannel().AddCubicKey(FFrameNumber(0), From);
	Section->GetChannel().AddCubicKey(FFrameNumber(End - 1), To);
	Track->AddSection(*Section);
	BP->Animations.Add(A);
}
bool Save(UWidgetBlueprint* BP)
{
	// Repair previously generated text without overwriting designer-selected fonts or layouts.
	UFont* FontAsset = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	if (!FontAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("EchoHUD: built-in Roboto font asset is unavailable."));
		return false;
	}
	BP->WidgetTree->ForEachWidget([FontAsset](UWidget* Widget)
	{
		if (auto* TextBlock = Cast<UTextBlock>(Widget))
		{
			FSlateFontInfo Font = TextBlock->GetFont();
			if (!Font.FontObject)
			{
				Font.FontObject = FontAsset;
				Font.CompositeFont.Reset();
				TextBlock->SetFont(Font);
				UE_LOG(LogTemp, Display, TEXT("EchoHUD: repaired font on %s"), *TextBlock->GetPathName());
			}
		}
	});
	// UE 5.7 requires GUIDs when extending an already compiled Widget Blueprint.
	BP->ForEachSourceWidget([BP](UWidget* Widget)
	{
		if (!BP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())) BP->OnVariableAdded(Widget->GetFName());
	});
	for (UWidgetAnimation* Animation : BP->Animations)
		if (Animation && !BP->WidgetVariableNameToGuidMap.Contains(Animation->GetFName())) BP->OnVariableAdded(Animation->GetFName());
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error)
		return false;
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	const FString Filename = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),
																	 FPackageName::GetAssetPackageExtension());
	return UPackage::SavePackage(BP->GetOutermost(), BP, *Filename, Args);
}
void Place(UCanvasPanel* Canvas, UWidget* W, FVector2D Anchor, FVector2D Alignment, FVector2D Position, FVector2D Size)
{
	auto* S = Canvas->AddChildToCanvas(W);
	S->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
	S->SetAlignment(Alignment);
	S->SetPosition(Position);
	S->SetSize(Size);
}
/** Additive and idempotent: never rebuild an existing HUD tree or designer animations. */
bool AddCycleIndicator(UWidgetBlueprint* HUD)
{
	bool bNew = false;
	auto* Cycle = Make(TEXT("WBP_EchoCycleIndicator"), UEchoCycleWidget::StaticClass(), bNew);
	if (bNew)
	{
		auto* V = Panel(Cycle);
		Line(V, Text(Cycle, TEXT("CycleText"), TEXT("CYCLE --"), 18, FLinearColor(.88f, .95f, 1, 1)), 0);
		Line(V, Text(Cycle, TEXT("CountdownText"), TEXT("NEXT ECHO / --"), 14));
		Line(V, Text(Cycle, TEXT("StatusText"), TEXT("UNAVAILABLE"), 12));
		Bar(Cycle, V, TEXT("RecordingFill"), 4);
		Line(V, Text(Cycle, TEXT("ProgressLabel"), TEXT("RECORDING PROGRESS"), 9), 4);
		Pulse(Cycle, TEXT("CycleTransition"), TEXT("CycleText"), .55f, 1.f, .3f);
		Pulse(Cycle, TEXT("EchoActivation"), TEXT("StatusText"), .4f, 1.f, .4f);
	}
	if (!Cycle->ParentClass->IsChildOf(UEchoCycleWidget::StaticClass()) || !Save(Cycle)) return false;
	if (UWidget* Existing = HUD->WidgetTree->FindWidget(TEXT("CycleIndicator")))
	{
		if (!Existing->IsA(UEchoCycleWidget::StaticClass())) return false;
	}
	else
	{
		auto* Canvas = Cast<UCanvasPanel>(HUD->WidgetTree->FindWidget(TEXT("HUDLayout")));
		if (!Canvas)
		{
			UE_LOG(LogTemp, Error, TEXT("EchoHUD: HUDLayout canvas missing; preserving existing layout."));
			return false;
		}
		auto* Indicator = HUD->WidgetTree->ConstructWidget<UUserWidget>(
			TSubclassOf<UUserWidget>(Cycle->GeneratedClass.Get()), TEXT("CycleIndicator"));
		Indicator->SetVisibility(ESlateVisibility::HitTestInvisible);
		// Existing safe-zone and project DPI scaling apply; all placement remains editable in UMG.
		Place(Canvas, Indicator, {0, 0}, {0, 0}, {32, 32}, {300, 150});
	}
	if (!Save(HUD)) return false;
	const auto* Defaults = Cast<UEchoCycleWidget>(Cycle->GeneratedClass->GetDefaultObject());
	if (!Defaults || !Cycle->WidgetTree->FindWidget(TEXT("CycleText"))
		|| !Cycle->WidgetTree->FindWidget(TEXT("CountdownText"))
		|| !Cycle->WidgetTree->FindWidget(TEXT("StatusText"))
		|| !Cycle->WidgetTree->FindWidget(TEXT("RecordingFill"))) return false;
	UE_LOG(LogTemp, Display, TEXT("EchoCycleHUD: indicator and integrated HUD compiled, saved, and bindings verified."));
	return true;
}
} // namespace EchoHUDAssets
UEchoHUDAssetCommandlet::UEchoHUDAssetCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}
int32 UEchoHUDAssetCommandlet::Main(const FString& Params)
{
	using namespace EchoHUDAssets;
	if (FParse::Param(*Params, TEXT("CycleOnly")))
	{
		auto* HUD = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/EchoStrike/UI/WBP_EchoHUD.WBP_EchoHUD"));
		return HUD && AddCycleIndicator(HUD) ? 0 : 1;
	}
	bool bNew;
	auto* Health = Make(TEXT("WBP_HealthBar"), UEchoHealthBar::StaticClass(), bNew);
	if (bNew)
	{
		auto* V = Panel(Health);
		Line(V, Text(Health, TEXT("IdentityText"), TEXT("VITALS  /  YOU"), 12), 0);
		Line(V, Text(Health, TEXT("HealthText"), TEXT(""), 22, FLinearColor(.88f, .95f, 1, 1)));
		Bar(Health, V, TEXT("HealthFill"), 6);
		Pulse(Health, TEXT("DamagePulse"), TEXT("Panel"), .35f, 1, .24f);
	}
	if (!Save(Health))
		return 1;
	auto* Dash = Make(TEXT("WBP_DashIndicator"), UEchoDashIndicator::StaticClass(), bNew);
	if (bNew)
	{
		auto* V = Panel(Dash);
		Line(V, Text(Dash, TEXT("StatusText"), TEXT("DASH  /  READY"), 12), 0);
		Bar(Dash, V, TEXT("CooldownFill"), 4);
		Pulse(Dash, TEXT("ReadyPulse"), TEXT("Panel"), .4f, 1, .3f);
	}
	if (!Save(Dash))
		return 1;
	auto* RespawnBP = Make(TEXT("WBP_Respawn"), UEchoRespawnWidget::StaticClass(), bNew);
	if (bNew)
	{
		auto* V = Panel(RespawnBP);
		auto* Title = Text(RespawnBP, TEXT("Title"), TEXT("ELIMINATED"), 28, FLinearColor(1, .35f, .28f, 1));
		Title->SetJustification(ETextJustify::Center);
		Line(V, Title, 0);
		auto* Count = Text(RespawnBP, TEXT("CountdownText"), TEXT(""), 14);
		Count->SetJustification(ETextJustify::Center);
		Line(V, Count, 12);
		Pulse(RespawnBP, TEXT("Reveal"), TEXT("Panel"), 0, 1, .2f);
	}
	if (!Save(RespawnBP))
		return 1;
	auto* Cursor = Make(TEXT("WBP_EchoCrosshair"), UUserWidget::StaticClass(), bNew);
	if (bNew)
	{
		auto* Size = Widget<USizeBox>(Cursor, TEXT("CursorSize"));
		Size->SetWidthOverride(24);
		Size->SetHeightOverride(24);
		Cursor->WidgetTree->RootWidget = Size;
		auto* Canvas = Widget<UCanvasPanel>(Cursor, TEXT("Reticle"));
		Size->SetContent(Canvas);
		struct FStroke
		{
			const TCHAR* Name;
			FVector2D Pos, Size;
		};
		const FStroke Strokes[] = {{TEXT("Left"), {3, 11}, {5, 2}},
								   {TEXT("Right"), {16, 11}, {5, 2}},
								   {TEXT("Top"), {11, 3}, {2, 5}},
								   {TEXT("Bottom"), {11, 16}, {2, 5}},
								   {TEXT("Dot"), {11, 11}, {2, 2}}};
		for (const auto& Stroke : Strokes)
		{
			auto* B = Widget<UBorder>(Cursor, Stroke.Name);
			B->SetBrushColor(Cyan);
			B->SetPadding(FMargin(0));
			Place(Canvas, B, {0, 0}, {0, 0}, Stroke.Pos, Stroke.Size);
		}
	}
	if (!Save(Cursor))
		return 1;
	auto* HUD = Make(TEXT("WBP_EchoHUD"), UEchoHUDWidget::StaticClass(), bNew);
	if (bNew)
	{
		auto* Safe = Widget<USafeZone>(HUD, TEXT("SafeArea"));
		HUD->WidgetTree->RootWidget = Safe;
		auto* Canvas = Widget<UCanvasPanel>(HUD, TEXT("HUDLayout"));
		Safe->SetContent(Canvas);
		auto Add = [&](UWidgetBlueprint* BP, const TCHAR* Name, FVector2D Anchor, FVector2D Align, FVector2D Pos,
					   FVector2D Size)
		{
			auto* W = HUD->WidgetTree->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(BP->GeneratedClass.Get()),
																	FName(Name));
			W->SetVisibility(ESlateVisibility::HitTestInvisible);
			Place(Canvas, W, Anchor, Align, Pos, Size);
		};
		Add(Health, TEXT("PlayerHealth"), {0, 1}, {0, 1}, {32, -32}, {300, 108});
		Add(Health, TEXT("OpponentHealth"), {1, 0}, {1, 0}, {-32, 32}, {300, 108});
		Add(Dash, TEXT("DashIndicator"), {1, 1}, {1, 1}, {-32, -32}, {230, 70});
		Add(RespawnBP, TEXT("Respawn"), {.5f, .5f}, {.5f, .5f}, {0, 60}, {380, 120});
	}
	if (!AddCycleIndicator(HUD))
		return 1;
	UE_LOG(LogTemp, Display, TEXT("EchoHUD: six Widget Blueprints compiled and saved."));
	return 0;
}

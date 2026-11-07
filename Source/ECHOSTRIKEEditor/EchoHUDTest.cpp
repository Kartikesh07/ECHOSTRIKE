#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/EchoHUDWidgets.h"
#include "Player/EchoPlayerController.h"
#include "Player/EchoCharacter.h"
#include "Combat/EchoHealthComponent.h"
#include "Core/EchoGameMode.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Components/TextBlock.h"
#include "UObject/UnrealType.h"
#include "Components/ProgressBar.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEchoHUDTest, "EchoStrike.UI.HUDLifecycle",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEchoHUDTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* GI = NewObject<UGameInstance>(GEngine);
	World->SetGameInstance(GI);
	FURL URL;
	URL.AddOption(TEXT("game=/Script/ECHOSTRIKE.EchoGameMode"));
	World->SetGameMode(URL);
	World->SpawnActor<APlayerStart>(FVector(0, 0, 200), FRotator::ZeroRotator);
	World->SpawnActor<APlayerStart>(FVector(1000, 0, 200), FRotator::ZeroRotator);
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	auto* PC = World->SpawnActor<AEchoPlayerController>();
	auto* Other = World->SpawnActor<AEchoPlayerController>();
	auto* Local = NewObject<ULocalPlayer>(GEngine);
	PC->SetPlayer(Local);
	auto* Mode = World->GetAuthGameMode<AEchoGameMode>();
	Mode->RestartPlayer(PC);
	Mode->RestartPlayer(Other);
	auto* Class = LoadClass<UEchoHUDWidget>(nullptr, TEXT("/Game/EchoStrike/UI/WBP_EchoHUD.WBP_EchoHUD_C"));
	auto* HUD = CreateWidget<UEchoHUDWidget>(PC, Class);
	if (TestNotNull(TEXT("HUD asset instantiates"), HUD))
	{
		auto Slate = HUD->TakeWidget();
		auto* Own = Cast<UEchoHealthBar>(HUD->GetWidgetFromName(TEXT("PlayerHealth")));
		auto* Opponent = Cast<UEchoHealthBar>(HUD->GetWidgetFromName(TEXT("OpponentHealth")));
		auto* Respawn = Cast<UEchoRespawnWidget>(HUD->GetWidgetFromName(TEXT("Respawn")));
		auto* Dash = Cast<UEchoDashIndicator>(HUD->GetWidgetFromName(TEXT("DashIndicator")));
		if (TestNotNull(TEXT("Own health binding"), Own) && TestNotNull(TEXT("Opponent health binding"), Opponent) &&
			TestNotNull(TEXT("Respawn binding"), Respawn) && TestNotNull(TEXT("Dash binding"), Dash))
		{
			// Check fonts on reloaded widget assets; transient Slate fonts disappear on serialization.
			const UUserWidget* TextOwners[] = {Own, Own, Dash, Respawn, Respawn};
			const FName TextNames[] = {TEXT("IdentityText"), TEXT("HealthText"), TEXT("StatusText"),
				TEXT("Title"), TEXT("CountdownText")};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(TextNames); ++Index)
			{
				const auto* Text = Cast<UTextBlock>(TextOwners[Index]->GetWidgetFromName(TextNames[Index]));
				if (TestNotNull(TEXT("HUD text exists"), Text))
				{
					TestNotNull(*FString::Printf(TEXT("%s has a persistent font asset"), *TextNames[Index].ToString()),
						Cast<UFont>(Text->GetFont().FontObject.Get()));
				}
			}
			TestNotNull(TEXT("Damage animation bound"),
						FindFProperty<FObjectPropertyBase>(Own->GetClass(), TEXT("DamagePulse"))
							->GetObjectPropertyValue_InContainer(Own));
			TestNotNull(TEXT("Dash animation bound"),
						FindFProperty<FObjectPropertyBase>(Dash->GetClass(), TEXT("ReadyPulse"))
							->GetObjectPropertyValue_InContainer(Dash));
			TestNotNull(TEXT("Respawn animation bound"),
						FindFProperty<FObjectPropertyBase>(Respawn->GetClass(), TEXT("Reveal"))
							->GetObjectPropertyValue_InContainer(Respawn));
			TestEqual(TEXT("Live player health visible"), Own->GetVisibility(), ESlateVisibility::HitTestInvisible);
			TestEqual(TEXT("Opponent visible"), Opponent->GetVisibility(), ESlateVisibility::HitTestInvisible);
			TestEqual(TEXT("Respawn initially hidden"), Respawn->GetVisibility(), ESlateVisibility::Collapsed);
			auto* OppPawn = CastChecked<AEchoCharacter>(Other->GetPawn());
			UGameplayStatics::ApplyDamage(OppPawn, 25, PC, PC->GetPawn(), nullptr);
			auto* OppText = CastChecked<UTextBlock>(Opponent->GetWidgetFromName(TEXT("HealthText")));
			TestTrue(TEXT("Opponent delegate updates displayed health"),
					 OppText->GetText().ToString().Contains(TEXT("75")));
			for (int Cycle = 0; Cycle < 3; ++Cycle)
			{
				auto* Pawn = CastChecked<AEchoCharacter>(PC->GetPawn());
				UGameplayStatics::ApplyDamage(Pawn, Pawn->HealthComponent->GetMaxHealth(), Other, Other->GetPawn(),
											  nullptr);
				HUD->RefreshRespawn();
				TestTrue(TEXT("Authority supplies respawn deadline"),
						 PC->GetRespawnDeadline() > World->GetTimeSeconds());
				TestEqual(TEXT("Elimination shows countdown"), Respawn->GetVisibility(),
						  ESlateVisibility::HitTestInvisible);
				for (int Step = 0; Step < 33; ++Step)
				{
					++GFrameCounter;
					World->Tick(LEVELTICK_All, .1f);
				}
				HUD->RefreshRespawn();
				TestTrue(TEXT("Fresh pawn after each elimination"), PC->GetPawn() != Pawn);
				TestEqual(TEXT("Respawn clears deadline"), PC->GetRespawnDeadline(), -1.0);
				TestEqual(TEXT("Countdown hidden after respawn"), Respawn->GetVisibility(),
						  ESlateVisibility::Collapsed);
				auto* OwnText = CastChecked<UTextBlock>(Own->GetWidgetFromName(TEXT("HealthText")));
				TestTrue(TEXT("Rebound health restored"), OwnText->GetText().ToString().Contains(TEXT("100")));
			}
			auto* HealthFill = CastChecked<UProgressBar>(Own->GetWidgetFromName(TEXT("HealthFill")));
			TestEqual(TEXT("Health transition reaches full"), HealthFill->GetPercent(), 1.f);
		}
		HUD->ReleaseSlateResources(true);
	}
	World->BeginTearingDown();
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif

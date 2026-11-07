#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Combat/EchoHealthComponent.h"
#include "Core/EchoGameMode.h"
#include "Core/EchoGameState.h"
#include "Core/EchoPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/EchoCharacter.h"
#include "Player/EchoCharacterMovementComponent.h"
#include "Player/EchoPlayerController.h"
#include "TimeEcho/EchoRecorderComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEchoRecordingTest, "EchoStrike.TimeEcho.Recording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEchoRecordingTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	FURL URL;
	URL.AddOption(TEXT("game=/Script/ECHOSTRIKE.EchoGameMode"));
	World->SetGameMode(URL);
	AActor* Floor = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(10000, 10000, 50));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->RegisterComponent();
	Floor->SetActorLocation(FVector(0, 0, -50));
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	AEchoGameState* State = World->GetGameState<AEchoGameState>();
	AEchoPlayerController* First = World->SpawnActor<AEchoPlayerController>();
	AEchoPlayerController* Second = World->SpawnActor<AEchoPlayerController>();
	// Standalone spawned controllers bypass Login, which normally assigns IDs.
	First->PlayerState->SetPlayerId(1);
	Second->PlayerState->SetPlayerId(2);
	AEchoCharacter* FirstPawn = World->SpawnActor<AEchoCharacter>(FVector(0, 0, 100), FRotator(0, 30, 0));
	AEchoCharacter* SecondPawn = World->SpawnActor<AEchoCharacter>(FVector(0, 2000, 100), FRotator(0, 90, 0));
	First->Possess(FirstPawn);
	Second->Possess(SecondPawn);
	UEchoRecorderComponent* FirstRecorder = First->GetPlayerState<AEchoPlayerState>()->EchoRecorder;
	UEchoRecorderComponent* SecondRecorder = Second->GetPlayerState<AEchoPlayerState>()->EchoRecorder;
	auto Tick = [&](int32 Frames)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 0.05f);
		}
	};
	auto CountEvents = [](const FEchoRecording& Recording, EEchoRecordedEvent Type)
	{
		return Recording.Events.FilterByPredicate([Type](const FEchoGameplayEvent& Event) { return Event.Type == Type; }).Num();
	};
	Tick(20);
	TestEqual(TEXT("Independent player identities"), FirstRecorder->GetCurrentRecording().PlayerId != SecondRecorder->GetCurrentRecording().PlayerId, true);
	TestEqual(TEXT("Synchronized cycle"), FirstRecorder->GetCurrentRecording().Cycle, SecondRecorder->GetCurrentRecording().Cycle);
	TestEqual(TEXT("Synchronized epoch"), FirstRecorder->GetCurrentRecording().ServerStartTime, SecondRecorder->GetCurrentRecording().ServerStartTime);
	const auto& Samples = FirstRecorder->GetCurrentRecording().Samples;
	AddInfo(FString::Printf(TEXT("Samples=%d Now=%f Last=%f Previous=%f Mode=%d"), Samples.Num(), World->GetTimeSeconds(),
		Samples.IsEmpty() ? -1.f : Samples.Last().Time, Samples.Num() < 2 ? -1.f : Samples[Samples.Num() - 2].Time,
		int32(FirstPawn->GetCharacterMovement()->MovementMode)));
	TestTrue(TEXT("20 Hz sample count over one second"), Samples.Num() >= 19 && Samples.Num() <= 22);
	if (Samples.Num() > 1)
	{
		TestTrue(TEXT("Sample timestamp cadence"), FMath::IsNearlyEqual(Samples.Last().Time - Samples[Samples.Num() - 2].Time, 0.05f, 0.002f));
		TestTrue(TEXT("Server position captured"), Samples.Last().Location.Equals(FirstPawn->GetActorLocation(), 0.01));
		TestTrue(TEXT("Server facing captured"), Samples.Last().Rotation.Equals(FirstPawn->GetActorRotation(), 0.01));
	}
	// Exercise the actual authority shooting path through its reflected RPC entry point.
	struct FFireParameters { bool bFiring; } FireParameters{true};
	UFunction* Fire = FirstPawn->FindFunctionChecked(TEXT("ServerSetFiring"));
	FirstPawn->ProcessEvent(Fire, &FireParameters);
	FireParameters.bFiring = false;
	FirstPawn->ProcessEvent(Fire, &FireParameters);
	TestEqual(TEXT("One successful shot recorded"), CountEvents(FirstRecorder->GetCurrentRecording(), EEchoRecordedEvent::Shot), 1);
	auto* Movement = CastChecked<UEchoCharacterMovementComponent>(FirstPawn->GetCharacterMovement());
	// Emulate validated server movement packets (the fixture has no local input player).
	Movement->MoveAutonomous(1.01f, 0.01f, FSavedMove_Character::FLAG_Custom_0, FVector::ZeroVector);
	Movement->MoveAutonomous(1.02f, 0.01f, FSavedMove_Character::FLAG_Custom_0, FVector::ZeroVector);
	Tick(2);
	TestEqual(TEXT("One confirmed dash recorded"), CountEvents(FirstRecorder->GetCurrentRecording(), EEchoRecordedEvent::Dash), 1);
	TestTrue(TEXT("Other player has no events"), SecondRecorder->GetCurrentRecording().Events.IsEmpty());
	UGameplayStatics::ApplyDamage(FirstPawn, 100.f, Second, SecondPawn, nullptr);
	FirstPawn->HealthComponent->OnEliminated.Broadcast(FirstPawn->HealthComponent);
	TestEqual(TEXT("Elimination deduplicated"), CountEvents(FirstRecorder->GetCurrentRecording(), EEchoRecordedEvent::Elimination), 1);
	// Replace the pawn exactly as respawn does, without relying on map PlayerStarts.
	First->UnPossess();
	FirstPawn->Destroy();
	AEchoCharacter* Respawned = World->SpawnActor<AEchoCharacter>(FVector(1000, 0, 100), FRotator::ZeroRotator);
	First->Possess(Respawned);
	Tick(2);
	TestEqual(TEXT("Recorder survives pawn destruction"), CountEvents(FirstRecorder->GetCurrentRecording(), EEchoRecordedEvent::Elimination), 1);
	TestEqual(TEXT("New life marked"), FirstRecorder->GetCurrentRecording().Samples.Last().Life, 2);
	Tick(220); // Cross first 12-second boundary.
	TestEqual(TEXT("Both first cycles archived"), State->GetCompletedRecordings().Num(), 2);
	for (const FEchoRecording& Recording : State->GetCompletedRecordings())
	{
		TestTrue(TEXT("Full cycle"), Recording.bCompleteCycle);
		TestEqual(TEXT("12-second duration"), Recording.Duration, 12.f);
		for (const FEchoGameplayEvent& Event : Recording.Events)
			TestTrue(TEXT("Cycle-relative server event time"), Event.Time >= 1.f && Event.Time < 1.3f);
	}
	TestEqual(TEXT("Recording continues in cycle one"), FirstRecorder->GetCurrentRecording().Cycle, int64(1));
	Tick(240);
	TestEqual(TEXT("Multiple cycles archived"), State->GetCompletedRecordings().Num(), 4);
	State->RemovePlayerState(First->PlayerState);
	const int32 AfterDisconnect = State->GetCompletedRecordings().Num();
	FirstRecorder->FinishRecording();
	TestEqual(TEXT("Disconnect flushes once"), State->GetCompletedRecordings().Num(), AfterDisconnect);
	TestFalse(TEXT("Disconnect archive marked partial"), State->GetCompletedRecordings().Last().bCompleteCycle);
	TestEqual(TEXT("Disconnected player preserved"), State->GetCompletedRecordings().Last().PlayerId, First->PlayerState->GetPlayerId());
	// Verify the global bound even for data belonging to disconnected players.
	for (int32 Index = 0; Index < 40; ++Index) State->StoreRecording(FEchoRecording());
	TestEqual(TEXT("Archive globally bounded"), State->GetCompletedRecordings().Num(), 32);
	World->BeginTearingDown();
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

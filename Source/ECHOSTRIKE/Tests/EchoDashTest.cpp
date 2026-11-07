#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Player/EchoCharacter.h"
#include "Player/EchoCharacterMovementComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEchoDashTest, "EchoStrike.Movement.DirectionalDash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEchoDashTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	FURL URL;
	World->SetGameMode(URL);
	auto AddBox = [World](const FVector& Location, const FVector& Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	};
	AddBox(FVector(0, 0, -50), FVector(5000, 5000, 50));
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	AEchoCharacter* Character = World->SpawnActor<AEchoCharacter>(FVector(0, 0, 98), FRotator::ZeroRotator);
	if (TestNotNull(TEXT("Character"), Character))
	{
		Controller->Possess(Character);
		auto* Movement = CastChecked<UEchoCharacterMovementComponent>(Character->GetCharacterMovement());
		float Time = 0.0f;
		auto Move = [&](uint8 Flags = 0, FVector Input = FVector::ZeroVector)
		{
			Time += 0.01f;
			Movement->MoveAutonomous(Time, 0.01f, Flags, Input);
		};
		Move(0, FVector(0, Movement->MaxAcceleration, 0));
		Move();
		Controller->SetControlRotation(FRotator(0, 180, 0));
		const FVector Start = Character->GetActorLocation();
		Movement->RequestDash();
		auto* Prediction = Movement->GetPredictionData_Client_Character();
		FSavedMovePtr DashMove = Prediction->AllocateNewMove();
		DashMove->SetMoveFor(Character, 0.01f, FVector::ZeroVector, *Prediction);
		TestTrue(TEXT("Dash intent encoded in saved move"), (DashMove->GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0) != 0);
		TestTrue(TEXT("Lost dash marked important for retransmission"), DashMove->IsImportantMove(DashMove));
		Move(DashMove->GetCompressedFlags());
		TestTrue(TEXT("Dash starts"), Movement->IsDashing());
		TestTrue(TEXT("Last movement direction independent of aim"), Movement->GetDashDirection().Equals(FVector::RightVector, 0.001));
		for (int32 Step = 1; Step < 17; ++Step) { Move(); }
		TestTrue(TEXT("Dashing at 0.17 seconds"), Movement->IsDashing());
		Move();
		TestFalse(TEXT("Finished at 0.18 seconds"), Movement->IsDashing());
		TestTrue(TEXT("500 UU ground displacement"), FMath::IsNearlyEqual(static_cast<float>((Character->GetActorLocation() - Start).Size2D()), 500.0f, 1.0f));
		TestTrue(TEXT("Walking restored"), Movement->IsMovingOnGround());
		const float Cooldown = Movement->GetDashCooldownRemaining();
		Move(FSavedMove_Character::FLAG_Custom_0); // Malicious/repeated intent must be validated by simulation.
		TestFalse(TEXT("Cooldown rejects repeated compressed intent"), Movement->IsDashing());
		TestTrue(TEXT("Rejected intent does not reset cooldown"), Movement->GetDashCooldownRemaining() < Cooldown);
		for (int32 Step = 19; Step < 199; ++Step) { Move(); }
		TestTrue(TEXT("Cooldown remains before two seconds"), Movement->GetDashCooldownRemaining() > 0.005f);
		Move();
		TestTrue(TEXT("Cooldown expires at two seconds"), Movement->GetDashCooldownRemaining() <= UE_KINDA_SMALL_NUMBER);

		Character->SetActorLocation(FVector(0, 0, 98));
		Movement->StopMovementImmediately();
		AddBox(FVector(0, 250, 100), FVector(100, 20, 100));
		Move(FSavedMove_Character::FLAG_Custom_0, FVector(0, Movement->MaxAcceleration, 0));
		TestTrue(TEXT("Dash allowed after cooldown"), Movement->IsDashing());
		Move(FSavedMove_Character::FLAG_Custom_0);
		TestTrue(TEXT("Repeated intent cannot restart active dash"), Movement->GetDashCooldownRemaining() < 2.0f);
		for (int32 Step = 0; Step < 18; ++Step) { Move(); }
		TestFalse(TEXT("Wall ends dash"), Movement->IsDashing());
		TestTrue(TEXT("Capsule stops before wall"), Character->GetActorLocation().Y < 189.0 && Character->GetActorLocation().Y > 170.0);
		for (int32 Step = 0; Step < 201; ++Step) { Move(); }
		Move(FSavedMove_Character::FLAG_Custom_0, FVector(-Movement->MaxAcceleration, 0, 0));
		TestTrue(TEXT("Current input overrides last direction"), Movement->GetDashDirection().Equals(-FVector::ForwardVector, 0.001));
		TestTrue(TEXT("Second unobstructed dash starts"), Movement->IsDashing());
		FClientAdjustment Adjustment;
		Adjustment.bAckGoodMove = false;
		Adjustment.MovementMode = Movement->PackNetworkMovementMode();
		FEchoMoveResponse Response;
		Response.ServerFillResponseData(*Movement, Adjustment);
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		TestTrue(TEXT("Correction serializes"), Response.Serialize(*Movement, Writer, nullptr));
		FEchoMoveResponse Received;
		FMemoryReader Reader(Bytes);
		TestTrue(TEXT("Correction deserializes"), Received.Serialize(*Movement, Reader, nullptr));
		TestEqual(TEXT("Cooldown survives correction transport"), Received.CooldownRemaining, Response.CooldownRemaining);
		TestEqual(TEXT("Dash time survives correction transport"), Received.DashRemaining, Response.DashRemaining);
		TestTrue(TEXT("Direction survives correction transport"), FVector(Received.DashDirection).Equals(-FVector::ForwardVector, 0.001));
		UGameplayStatics::ApplyDamage(Character, 100.0f, Controller, nullptr, nullptr);
		TestFalse(TEXT("Elimination cancels dash immediately"), Movement->IsDashing());
		Move(FSavedMove_Character::FLAG_Custom_0);
		TestEqual(TEXT("Elimination keeps movement disabled"), Movement->MovementMode, MOVE_None);
	}
	World->BeginTearingDown();
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

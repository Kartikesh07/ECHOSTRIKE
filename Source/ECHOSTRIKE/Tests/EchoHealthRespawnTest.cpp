#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/EchoHealthComponent.h"
#include "Combat/EchoProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Core/EchoGameMode.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Player/EchoCharacter.h"
#include "Player/EchoPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEchoHealthRespawnTest, "EchoStrike.Combat.HealthAndRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEchoHealthRespawnTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	FURL URL;
	URL.AddOption(TEXT("game=/Script/ECHOSTRIKE.EchoGameMode"));
	World->SetGameMode(URL);
	World->SpawnActor<APlayerStart>(FVector(0, 0, 200), FRotator::ZeroRotator);
	World->SpawnActor<APlayerStart>(FVector(1000, 0, 200), FRotator::ZeroRotator);
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	AEchoGameMode* Mode = World->GetAuthGameMode<AEchoGameMode>();
	UClass* CharacterClass = LoadClass<AEchoCharacter>(nullptr,
		TEXT("/Game/EchoStrike/Blueprints/BP_EchoCharacter.BP_EchoCharacter_C"));
	if (TestNotNull(TEXT("Arena character Blueprint"), CharacterClass))
	{
		Mode->DefaultPawnClass = CharacterClass;
		const UFunction* EliminationEvent = CharacterClass->FindFunctionByName(TEXT("OnEliminated"));
		if (TestNotNull(TEXT("Elimination extension hook"), EliminationEvent))
		{
			AddInfo(FString::Printf(TEXT("BP_EchoCharacter OnEliminated implementation owner: %s"),
				*EliminationEvent->GetOuterUClass()->GetName()));
		}
	}
	AEchoPlayerController* Shooter = World->SpawnActor<AEchoPlayerController>();
	AEchoPlayerController* Victim = World->SpawnActor<AEchoPlayerController>();
	Mode->RestartPlayer(Shooter);
	Mode->RestartPlayer(Victim);
	AEchoCharacter* ShooterPawn = Cast<AEchoCharacter>(Shooter->GetPawn());
	AEchoCharacter* VictimPawn = Cast<AEchoCharacter>(Victim->GetPawn());
	if (TestNotNull(TEXT("Shooter pawn"), ShooterPawn) && TestNotNull(TEXT("Victim pawn"), VictimPawn))
	{
		TestEqual(TEXT("Shooter starts full"), ShooterPawn->HealthComponent->GetHealth(), 100.0f);
		TestEqual(TEXT("Victim starts full"), VictimPawn->HealthComponent->GetHealth(), 100.0f);
		UGameplayStatics::ApplyDamage(VictimPawn, -20.0f, Shooter, ShooterPawn, nullptr);
		TestEqual(TEXT("Negative damage rejected"), VictimPawn->HealthComponent->GetHealth(), 100.0f);
		for (int32 Shot = 1; Shot <= 5; ++Shot)
		{
			FActorSpawnParameters Spawn;
			Spawn.Owner = ShooterPawn;
			Spawn.Instigator = ShooterPawn;
			AEchoProjectile* Projectile = World->SpawnActor<AEchoProjectile>(
				FVector(500, 500, 200), FRotator::ZeroRotator, Spawn);
			USphereComponent* Collision = Projectile->FindComponentByClass<USphereComponent>();
			FHitResult Hit;
			Collision->OnComponentHit.Broadcast(Collision, ShooterPawn, nullptr, FVector::ZeroVector, Hit);
			TestEqual(TEXT("Self hit ignored"), ShooterPawn->HealthComponent->GetHealth(), 100.0f);
			Collision->OnComponentHit.Broadcast(Collision, VictimPawn, nullptr, FVector::ZeroVector, Hit);
			Collision->OnComponentHit.Broadcast(Collision, VictimPawn, nullptr, FVector::ZeroVector, Hit);
			TestEqual(TEXT("Each projectile damages exactly once"), VictimPawn->HealthComponent->GetHealth(), 100.0f - Shot * 20.0f);
		}
		TestTrue(TEXT("Eliminated at zero"), VictimPawn->IsEliminated());
		TestTrue(TEXT("Eliminated pawn immediately hidden"), VictimPawn->IsHidden());
		TestFalse(TEXT("Eliminated pawn collision disabled"), VictimPawn->GetActorEnableCollision());
		TestTrue(TEXT("Elimination remains relevant to owner"),
			VictimPawn->IsNetRelevantFor(Victim, VictimPawn, VictimPawn->GetActorLocation()));
		TestTrue(TEXT("Elimination remains relevant to other clients"),
			VictimPawn->IsNetRelevantFor(Shooter, ShooterPawn, ShooterPawn->GetActorLocation()));
		TestTrue(TEXT("Camera stays active while eliminated"), VictimPawn->GetFollowCamera()->IsActive());
		TestEqual(TEXT("Movement disabled"), VictimPawn->GetCharacterMovement()->MovementMode, MOVE_None);
		TestFalse(TEXT("Jump disabled"), VictimPawn->CanJump());
		Mode->RequestRespawn(VictimPawn);
		Mode->RestartPlayer(Victim);
		TestTrue(TEXT("Restart cannot bypass delay"), Victim->GetPawn() == VictimPawn);
		// Advance the actual world/timer lifecycle; no wall-clock waiting needed.
		for (int32 Step = 0; Step < 29; ++Step)
		{
			++GFrameCounter; // TimerManager only ticks once per engine frame.
			World->Tick(LEVELTICK_All, 0.1f);
		}
		TestTrue(TEXT("Still eliminated before three seconds"), Victim->GetPawn() == VictimPawn);
		TestTrue(TEXT("Stays hidden throughout respawn delay"), VictimPawn->IsHidden());
		TestFalse(TEXT("Collision stays disabled throughout delay"), VictimPawn->GetActorEnableCollision());
		for (int32 Step = 0; Step < 3; ++Step)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 0.1f);
		}
		AEchoCharacter* Respawned = Cast<AEchoCharacter>(Victim->GetPawn());
		if (TestNotNull(TEXT("Respawned pawn"), Respawned))
		{
			TestTrue(TEXT("Fresh pawn"), Respawned != VictimPawn);
			TestFalse(TEXT("Respawn visible"), Respawned->IsHidden());
			TestTrue(TEXT("Respawn mesh visible"), Respawned->GetMesh()->IsVisible());
			TestTrue(TEXT("Respawn collision enabled"), Respawned->GetActorEnableCollision());
			TestTrue(TEXT("Respawn capsule collision enabled"), Respawned->GetCapsuleComponent()->IsCollisionEnabled());
			TestTrue(TEXT("Respawn movement enabled"), Respawned->GetCharacterMovement()->MovementMode != MOVE_None);
			TestTrue(TEXT("Respawn camera active"), Respawned->GetFollowCamera()->IsActive());
			TestTrue(TEXT("Respawn aiming rotation enabled"), Respawned->GetCharacterMovement()->bUseControllerDesiredRotation);
			TestEqual(TEXT("Respawn full health"), Respawned->HealthComponent->GetHealth(), 100.0f);
			TestEqual(TEXT("Possession restored"), Respawned->GetController(), static_cast<AController*>(Victim));
			UGameplayStatics::ApplyDamage(Respawned, 100.0f, Shooter, ShooterPawn, nullptr);
			Mode->Logout(Victim);
			Victim->Destroy();
			for (int32 Step = 0; Step < 35; ++Step)
			{
				++GFrameCounter;
				World->Tick(LEVELTICK_All, 0.1f);
			}
			TestFalse(TEXT("Disconnected pawn cleaned up"), IsValid(Respawned));
		}
	}
	World->BeginTearingDown();
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif

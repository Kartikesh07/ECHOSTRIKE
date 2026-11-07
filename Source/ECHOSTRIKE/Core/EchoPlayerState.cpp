#include "Core/EchoPlayerState.h"
#include "TimeEcho/EchoRecorderComponent.h"
#include "TimeEcho/EchoGhostCharacter.h"
#include "TimeEcho/EchoPlaybackComponent.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AEchoPlayerState::AEchoPlayerState()
{
	EchoRecorder = CreateDefaultSubobject<UEchoRecorderComponent>(TEXT("EchoRecorder"));
	static ConstructorHelpers::FClassFinder<AEchoGhostCharacter> GhostClass(TEXT("/Game/EchoStrike/Blueprints/BP_EchoGhostCharacter"));
	EchoGhostClass = GhostClass.Class;
}

void AEchoPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) EchoRecorder->OnCycleCompleted.AddUObject(this, &AEchoPlayerState::HandleCycleCompleted);
}

void AEchoPlayerState::ClearEcho()
{
	if (!HasAuthority()) return;
	if (IsValid(ActiveEcho)) ActiveEcho->Destroy();
	ActiveEcho = nullptr;
}

void AEchoPlayerState::HandleCycleCompleted(const FEchoRecording& Recording, int64 NewCycle, double ServerStartTime)
{
	if (!HasAuthority()) return;
	if (IsValid(ActiveEcho)) ActiveEcho->Playback->FlushPendingEvents();
	ClearEcho();
	// Never replay stale data after skipped cycles, or recordings from disconnected players.
	if (IsInactive() || !EchoGhostClass || Recording.Cycle != NewCycle - 1 || Recording.Samples.IsEmpty()) return;
	const FTransform Transform(Recording.Samples[0].Rotation, Recording.Samples[0].Location);
	if (Transform.ContainsNaN()) return;
	AEchoGhostCharacter* Ghost = GetWorld()->SpawnActorDeferred<AEchoGhostCharacter>(EchoGhostClass,
		Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Ghost) return;
	if (!Ghost->Playback->Initialize(Recording, ServerStartTime)) { Ghost->Destroy(); return; }
	ActiveEcho = Ghost;
	Ghost->FinishSpawning(Transform);
	// Playback drains the final frame before destroying the ghost at the cycle boundary.
}

void AEchoPlayerState::EndPlay(const EEndPlayReason::Type Reason)
{
	EchoRecorder->OnCycleCompleted.RemoveAll(this);
	ClearEcho();
	Super::EndPlay(Reason);
}

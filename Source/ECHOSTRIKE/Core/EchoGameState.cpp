#include "Core/EchoGameState.h"
#include "Core/EchoPlayerState.h"
#include "TimeEcho/EchoRecorderComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
void AEchoGameState::AddPlayerState(APlayerState* Player) { Super::AddPlayerState(Player); OnRosterChanged.Broadcast(); }
void AEchoGameState::RemovePlayerState(APlayerState* Player)
{
	if (HasAuthority())
		if (AEchoPlayerState* EchoPlayer = Cast<AEchoPlayerState>(Player))
			EchoPlayer->EchoRecorder->FinishRecording();
	Super::RemovePlayerState(Player);
	OnRosterChanged.Broadcast();
}

void AEchoGameState::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;
	CycleDuration = FMath::Clamp(FMath::IsFinite(RecordingDuration) ? RecordingDuration : 12.f, 1.f, 120.f);
	const float Frequency = FMath::Clamp(FMath::IsFinite(SamplingFrequency) ? SamplingFrequency : 20.f, 1.f, 120.f);
	EventLimit = FMath::Clamp(MaxEventsPerCycle, 1, 4096);
	// Leave bounded headroom for immediate possession and terminal samples.
	SampleLimit = FMath::CeilToInt(CycleDuration * Frequency) + EventLimit + 2;
	ArchiveLimit = FMath::Clamp(MaxCompletedRecordings, 1, 128);
	RecordingEpoch = GetWorld()->GetTimeSeconds();
	SamplePeriod = 1.0 / Frequency;
	SampleRecorders();
}

void AEchoGameState::SampleRecorders()
{
	const double Now = GetWorld()->GetTimeSeconds();
	for (APlayerState* Player : PlayerArray)
		if (AEchoPlayerState* EchoPlayer = Cast<AEchoPlayerState>(Player); IsValid(EchoPlayer))
			EchoPlayer->EchoRecorder->Sample(Now);
	// Anchor every wakeup to the epoch, avoiding timer drift and catch-up bursts.
	// Missed slots remain honest timestamp gaps after a hitch. The tiny lead also
	// avoids TimerManager's strict expiry comparison skipping exact frame multiples.
	const double NextSlot = FMath::FloorToDouble((Now - RecordingEpoch) / SamplePeriod + 0.0001) + 1.0;
	const float Delay = FMath::Max(0.0001f, static_cast<float>(RecordingEpoch + NextSlot * SamplePeriod - Now - 0.000001));
	GetWorldTimerManager().SetTimer(RecordingTimer, this, &AEchoGameState::SampleRecorders, Delay, false);
}

void AEchoGameState::StoreRecording(FEchoRecording&& Recording)
{
	if (!HasAuthority() || IsActorBeingDestroyed()) return;
	if (CompletedRecordings.Num() >= ArchiveLimit)
		CompletedRecordings.RemoveAt(0, CompletedRecordings.Num() - ArchiveLimit + 1, EAllowShrinking::No);
	CompletedRecordings.Add(MoveTemp(Recording));
}

void AEchoGameState::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(RecordingTimer);
	Super::EndPlay(Reason);
}

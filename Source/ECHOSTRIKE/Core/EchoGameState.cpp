#include "Core/EchoGameState.h"
#include "Core/EchoPlayerState.h"
#include "TimeEcho/EchoRecorderComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
void AEchoGameState::AddPlayerState(APlayerState* Player) { Super::AddPlayerState(Player); OnRosterChanged.Broadcast(); }
void AEchoGameState::RemovePlayerState(APlayerState* Player)
{
	if (HasAuthority())
		if (AEchoPlayerState* EchoPlayer = Cast<AEchoPlayerState>(Player))
		{
			EchoPlayer->EchoRecorder->FinishRecording();
			EchoPlayer->ClearEcho();
		}
	Super::RemovePlayerState(Player);
	OnRosterChanged.Broadcast();
}

void AEchoGameState::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;
	CycleState.Duration = FMath::Clamp(FMath::IsFinite(RecordingDuration) ? RecordingDuration : 12.f, 1.f, 120.f);
	const float Frequency = FMath::Clamp(FMath::IsFinite(SamplingFrequency) ? SamplingFrequency : 20.f, 1.f, 120.f);
	EventLimit = FMath::Clamp(MaxEventsPerCycle, 1, 4096);
	// Leave bounded headroom for immediate possession and terminal samples.
	SampleLimit = FMath::CeilToInt(CycleState.Duration * Frequency) + EventLimit + 2;
	ArchiveLimit = FMath::Clamp(MaxCompletedRecordings, 1, 128);
	CycleState.Epoch = GetServerWorldTimeSeconds();
	CycleState.Cycle = 0;
	CycleState.bReady = true;
	SamplePeriod = 1.0 / Frequency;
	SampleRecorders();
	ForceNetUpdate();
	OnRep_CycleState();
}

void AEchoGameState::SampleRecorders()
{
	const double Now = GetServerWorldTimeSeconds();
	const int64 NewCycle = GetCycleAtServerTime(Now);
	for (APlayerState* Player : PlayerArray)
		if (AEchoPlayerState* EchoPlayer = Cast<AEchoPlayerState>(Player); IsValid(EchoPlayer))
			EchoPlayer->EchoRecorder->Sample(Now);
	if (NewCycle != CycleState.Cycle)
	{
		CycleState.Cycle = NewCycle;
		ForceNetUpdate();
		OnRep_CycleState();
	}
	// Anchor every wakeup to the epoch, avoiding timer drift and catch-up bursts.
	// Missed slots remain honest timestamp gaps after a hitch. The tiny lead also
	// avoids TimerManager's strict expiry comparison skipping exact frame multiples.
	const double NextSlot = FMath::FloorToDouble((Now - CycleState.Epoch) / SamplePeriod + 0.0001) + 1.0;
	const float Delay = FMath::Max(0.0001f, static_cast<float>(CycleState.Epoch + NextSlot * SamplePeriod - Now - 0.000001));
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

void AEchoGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEchoGameState, CycleState);
}

void AEchoGameState::OnRep_CycleState()
{
	OnCycleStateChanged.Broadcast();
}

int64 AEchoGameState::GetCycleAtServerTime(double ServerTime) const
{
	if (!CycleState.bReady || !FMath::IsFinite(ServerTime) || CycleState.Duration <= 0.f) return INDEX_NONE;
	return FMath::FloorToInt64(FMath::Max(0., ServerTime - CycleState.Epoch) / CycleState.Duration);
}

int64 AEchoGameState::GetCurrentCycle() const
{
	return GetCycleAtServerTime(GetServerWorldTimeSeconds());
}

float AEchoGameState::GetCycleRemaining() const
{
	const double Now = GetServerWorldTimeSeconds();
	const int64 Cycle = GetCycleAtServerTime(Now);
	if (Cycle == INDEX_NONE) return 0.f;
	return FMath::Clamp(static_cast<float>(CycleState.Epoch + (Cycle + 1) * CycleState.Duration - Now), 0.f, CycleState.Duration);
}

float AEchoGameState::GetCycleProgress() const
{
	return CycleState.bReady ? 1.f - GetCycleRemaining() / CycleState.Duration : 0.f;
}

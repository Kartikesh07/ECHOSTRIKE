#include "UI/EchoCycleWidget.h"
#include "Core/EchoGameState.h"
#include "Core/EchoPlayerState.h"
#include "Combat/EchoHealthComponent.h"
#include "Player/EchoCharacter.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "TimerManager.h"

UEchoCycleWidget::UEchoCycleWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	RecordingLabel = NSLOCTEXT("EchoCycle", "Recording", "RECORDING");
	EchoActiveLabel = NSLOCTEXT("EchoCycle", "Active", "ECHO ACTIVE");
	UnavailableLabel = NSLOCTEXT("EchoCycle", "Unavailable", "UNAVAILABLE");
}

void UEchoCycleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
	// Local display refresh only: no gameplay Tick, property bindings, or countdown replication.
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
		GetWorld()->GetTimerManager().SetTimer(CountdownTimer, this, &ThisClass::Refresh,
			FMath::Clamp(FMath::IsFinite(CountdownUpdateInterval) ? CountdownUpdateInterval : 0.1f, 0.02f, 1.f), true);
}

void UEchoCycleWidget::Observe(AEchoGameState* InState, AEchoPlayerState* InPlayer)
{
	if (State.Get() != InState || Player.Get() != InPlayer)
	{
		UnbindSources();
		State = InState;
		Player = InPlayer;
		LastActivatedCycle = INDEX_NONE;
		DisplayedCycle = INDEX_NONE;
		DisplayedSeconds = INDEX_NONE;
		bPresented = false;
		if (State.IsValid()) CycleHandle = State->OnCycleStateChanged.AddUObject(this, &ThisClass::Refresh);
		if (Player.IsValid())
		{
			EchoHandle = Player->OnEchoAvailabilityChanged.AddUObject(this, &ThisClass::Refresh);
			Player->OnPawnSet.AddUniqueDynamic(this, &ThisClass::PawnChanged);
		}
	}
	ObservePawn();
	Refresh();
}

void UEchoCycleWidget::ObservePawn()
{
	if (Health.IsValid()) Health->OnEliminated.RemoveDynamic(this, &ThisClass::Eliminated);
	const AEchoCharacter* Pawn = Player.IsValid() ? Cast<AEchoCharacter>(Player->GetPawn()) : nullptr;
	Health = Pawn ? Pawn->HealthComponent.Get() : nullptr;
	if (Health.IsValid()) Health->OnEliminated.AddUniqueDynamic(this, &ThisClass::Eliminated);
}

void UEchoCycleWidget::PawnChanged(APlayerState*, APawn*, APawn*) { ObservePawn(); Refresh(); }
void UEchoCycleWidget::Eliminated(UEchoHealthComponent*) { Refresh(); }

void UEchoCycleWidget::Refresh()
{
	if (!CycleText || !CountdownText || !StatusText || !RecordingFill) return;
	const int64 Cycle = State.IsValid() ? State->GetCurrentCycle() : INDEX_NONE;
	const bool bAvailable = Cycle >= 0 && Player.IsValid() && !Player->IsInactive() && !Player->IsOnlyASpectator();
	CycleNumber = Cycle >= 0 ? Cycle + 1 : 0;
	SecondsRemaining = Cycle >= 0 ? State->GetCycleRemaining() : 0.f;
	RecordingProgress = Cycle >= 0 ? State->GetCycleProgress() : 0.f;
	const int32 Seconds = Cycle >= 0 ? FMath::CeilToInt(SecondsRemaining) : INDEX_NONE;
	if (Cycle != DisplayedCycle || !bPresented)
	{
		CycleText->SetText(Cycle >= 0 ? FText::Format(NSLOCTEXT("EchoCycle", "Cycle", "CYCLE {0}"), FText::AsNumber(CycleNumber))
			: NSLOCTEXT("EchoCycle", "PendingCycle", "CYCLE --"));
		if (DisplayedCycle >= 0 && Cycle > DisplayedCycle)
		{
			if (CycleTransition) PlayAnimation(CycleTransition);
			OnCycleTransition(CycleNumber);
		}
		DisplayedCycle = Cycle;
	}
	if (Seconds != DisplayedSeconds || !bPresented)
	{
		CountdownText->SetText(Seconds >= 0 ? FText::Format(NSLOCTEXT("EchoCycle", "Countdown", "NEXT ECHO / {0}s"), FText::AsNumber(Seconds))
			: NSLOCTEXT("EchoCycle", "PendingCountdown", "NEXT ECHO / --"));
		DisplayedSeconds = Seconds;
	}
	RecordingFill->SetPercent(RecordingProgress);
	EEchoCycleStatus Status = EEchoCycleStatus::Unavailable;
	if (bAvailable)
	{
		const double Now = State->GetServerWorldTimeSeconds();
		const FEchoAvailability& Echo = Player->EchoAvailability;
		if (Echo.Cycle == Cycle && Now >= Echo.StartTime && Now < Echo.EndTime)
			Status = EEchoCycleStatus::EchoActive;
		else if (Health.IsValid() && !Health->IsEliminated())
			Status = EEchoCycleStatus::Recording;
	}
	if (Status != EchoStatus || !bPresented)
	{
		EchoStatus = Status;
		StatusText->SetText(Status == EEchoCycleStatus::EchoActive ? EchoActiveLabel
			: Status == EEchoCycleStatus::Recording ? RecordingLabel : UnavailableLabel);
		OnEchoStatusChanged(Status);
	}
	// A new echo can replace the previous one without an intervening status change.
	if (Status == EEchoCycleStatus::EchoActive && LastActivatedCycle != Cycle)
	{
		LastActivatedCycle = Cycle;
		if (EchoActivation) PlayAnimation(EchoActivation);
	}
	bPresented = true;
}

void UEchoCycleWidget::UnbindSources()
{
	if (State.IsValid()) State->OnCycleStateChanged.Remove(CycleHandle);
	if (Player.IsValid())
	{
		Player->OnEchoAvailabilityChanged.Remove(EchoHandle);
		Player->OnPawnSet.RemoveDynamic(this, &ThisClass::PawnChanged);
	}
	if (Health.IsValid()) Health->OnEliminated.RemoveDynamic(this, &ThisClass::Eliminated);
	Health.Reset();
	State.Reset();
	Player.Reset();
}

void UEchoCycleWidget::NativeDestruct()
{
	GetWorld()->GetTimerManager().ClearTimer(CountdownTimer);
	UnbindSources();
	LastActivatedCycle = INDEX_NONE;
	bPresented = false;
	Super::NativeDestruct();
}

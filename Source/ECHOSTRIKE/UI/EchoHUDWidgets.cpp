#include "UI/EchoHUDWidgets.h"
#include "Combat/EchoHealthComponent.h"
#include "Player/EchoCharacter.h"
#include "Player/EchoCharacterMovementComponent.h"
#include "Player/EchoPlayerController.h"
#include "Core/EchoGameState.h"
#include "GameFramework/PlayerState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UEchoHealthBar::Observe(UEchoHealthComponent* Component, const FText& Label)
{
	if (Source.IsValid())
		Source->OnHealthChanged.RemoveDynamic(this, &ThisClass::HealthChanged);
	GetWorld()->GetTimerManager().ClearTimer(TransitionTimer);
	Source = Component;
	IdentityText->SetText(Label);
	SetVisibility(Component ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Component)
	{
		Target = FMath::Clamp(Component->GetHealth() / Component->GetMaxHealth(), 0.f, 1.f);
		HealthFill->SetPercent(Target);
		Component->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HealthChanged);
		HealthChanged(Component, Component->GetHealth(), Component->GetHealth());
	}
}
void UEchoHealthBar::HealthChanged(UEchoHealthComponent* Component, float Health, float Previous)
{
	HealthText->SetText(FText::Format(NSLOCTEXT("EchoHUD", "HP", "{0} / {1} HP"),
									  FText::AsNumber(FMath::CeilToInt(Health)),
									  FText::AsNumber(FMath::CeilToInt(Component->GetMaxHealth()))));
	Start = HealthFill->GetPercent();
	Target = FMath::Clamp(Health / Component->GetMaxHealth(), 0.f, 1.f);
	if (Health < Previous && DamagePulse)
		PlayAnimation(DamagePulse);
	if (!FMath::IsNearlyEqual(Start, Target))
	{
		TransitionStart = GetWorld()->GetTimeSeconds();
		GetWorld()->GetTimerManager().SetTimer(TransitionTimer, this, &ThisClass::AnimateHealth, 1.f / 60.f, true);
	}
}
void UEchoHealthBar::AnimateHealth()
{
	const float Alpha = FMath::Clamp(
		float((GetWorld()->GetTimeSeconds() - TransitionStart) / FMath::Max(.01f, TransitionSeconds)), 0.f, 1.f);
	HealthFill->SetPercent(FMath::InterpEaseOut(Start, Target, Alpha, 2.f));
	if (Alpha >= 1)
		GetWorld()->GetTimerManager().ClearTimer(TransitionTimer);
}
void UEchoHealthBar::NativeDestruct()
{
	if (Source.IsValid())
		Source->OnHealthChanged.RemoveDynamic(this, &ThisClass::HealthChanged);
	GetWorld()->GetTimerManager().ClearTimer(TransitionTimer);
	Super::NativeDestruct();
}
void UEchoDashIndicator::Observe(UEchoCharacterMovementComponent* Component)
{
	if (Source.IsValid())
		Source->OnDashHUDChanged.Remove(DashHandle);
	Source = Component;
	SetVisibility(Component ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Component)
	{
		DashHandle = Component->OnDashHUDChanged.AddUObject(this, &ThisClass::DashChanged);
		bWasReady = true;
		DashChanged(Component->GetDashCooldownRemaining(), Component->DashCooldown, Component->IsDashing());
	}
}
void UEchoDashIndicator::DashChanged(float Remaining, float Duration, bool bDashing)
{
	const bool bReady = Remaining <= KINDA_SMALL_NUMBER && !bDashing;
	CooldownFill->SetPercent(Duration > 0 ? 1.f - FMath::Clamp(Remaining / Duration, 0.f, 1.f) : 1.f);
	StatusText->SetText(bReady	   ? NSLOCTEXT("EchoHUD", "Ready", "DASH  /  READY")
						: bDashing ? NSLOCTEXT("EchoHUD", "Dashing", "DASH  /  ACTIVE")
								   : FText::Format(NSLOCTEXT("EchoHUD", "Cooldown", "DASH  /  {0}s"),
												   FText::AsNumber(FMath::CeilToInt(Remaining))));
	if (bReady && !bWasReady && ReadyPulse)
		PlayAnimation(ReadyPulse);
	bWasReady = bReady;
}
void UEchoDashIndicator::NativeDestruct()
{
	if (Source.IsValid())
		Source->OnDashHUDChanged.Remove(DashHandle);
	Super::NativeDestruct();
}
void UEchoRespawnWidget::SetDeadline(double ServerDeadline)
{
	const bool bReveal = Deadline < 0 && ServerDeadline >= 0;
	Deadline = ServerDeadline;
	GetWorld()->GetTimerManager().ClearTimer(CountdownTimer);
	SetVisibility(Deadline >= 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Deadline >= 0)
	{
		if (bReveal && Reveal)
			PlayAnimation(Reveal);
		UpdateCountdown();
		GetWorld()->GetTimerManager().SetTimer(CountdownTimer, this, &ThisClass::UpdateCountdown, .1f, true);
	}
}
void UEchoRespawnWidget::UpdateCountdown()
{
	const auto* GS = GetWorld()->GetGameState();
	if (!GS)
		return;
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(Deadline - GS->GetServerWorldTimeSeconds()));
	CountdownText->SetText(
		Seconds > 0 ? FText::Format(NSLOCTEXT("EchoHUD", "Respawn", "REDEPLOYING IN {0}"), FText::AsNumber(Seconds))
					: NSLOCTEXT("EchoHUD", "Pending", "REDEPLOYING"));
}
void UEchoRespawnWidget::NativeDestruct()
{
	GetWorld()->GetTimerManager().ClearTimer(CountdownTimer);
	Super::NativeDestruct();
}
void UEchoHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GameStateHandle = GetWorld()->GameStateSetEvent.AddUObject(this, &ThisClass::GameStateChanged);
	GameStateChanged(GetWorld()->GetGameState());
}
void UEchoHUDWidget::GameStateChanged(AGameStateBase* NewState)
{
	if (State.IsValid())
		State->OnRosterChanged.Remove(RosterHandle);
	State = Cast<AEchoGameState>(NewState);
	if (State.IsValid())
		RosterHandle = State->OnRosterChanged.AddUObject(this, &ThisClass::RefreshSources);
	RefreshSources();
	RefreshRespawn();
}
void UEchoHUDWidget::RefreshSources()
{
	for (auto Player : ObservedPlayers)
		if (Player.IsValid())
			Player->OnPawnSet.RemoveDynamic(this, &ThisClass::PawnChanged);
	ObservedPlayers.Reset();
	auto* PC = GetOwningPlayer();
	auto* OwnPawn = PC ? Cast<AEchoCharacter>(PC->GetPawn()) : nullptr;
	PlayerHealth->Observe(OwnPawn ? OwnPawn->HealthComponent.Get() : nullptr,
						  NSLOCTEXT("EchoHUD", "You", "VITALS  /  YOU"));
	DashIndicator->Observe(OwnPawn && !OwnPawn->IsEliminated()
							   ? Cast<UEchoCharacterMovementComponent>(OwnPawn->GetCharacterMovement())
							   : nullptr);
	AEchoCharacter* Opponent = nullptr;
	if (State.IsValid())
		for (APlayerState* Player : State->PlayerArray)
		{
			Player->OnPawnSet.AddUniqueDynamic(this, &ThisClass::PawnChanged);
			ObservedPlayers.Add(Player);
			if (PC && Player != PC->PlayerState && !Player->IsOnlyASpectator() && !Opponent)
				Opponent = Cast<AEchoCharacter>(Player->GetPawn());
		}
	OpponentHealth->Observe(Opponent ? Opponent->HealthComponent.Get() : nullptr,
							NSLOCTEXT("EchoHUD", "Opponent", "TARGET  /  OPPONENT"));
}
void UEchoHUDWidget::PawnChanged(APlayerState*, APawn*, APawn*)
{
	RefreshSources();
	RefreshRespawn();
}
void UEchoHUDWidget::RefreshRespawn()
{
	if (const auto* PC = Cast<AEchoPlayerController>(GetOwningPlayer()))
		Respawn->SetDeadline(PC->GetRespawnDeadline());
}
void UEchoHUDWidget::NativeDestruct()
{
	GetWorld()->GameStateSetEvent.Remove(GameStateHandle);
	if (State.IsValid())
		State->OnRosterChanged.Remove(RosterHandle);
	for (auto Player : ObservedPlayers)
		if (Player.IsValid())
			Player->OnPawnSet.RemoveDynamic(this, &ThisClass::PawnChanged);
	ObservedPlayers.Reset();
	Super::NativeDestruct();
}

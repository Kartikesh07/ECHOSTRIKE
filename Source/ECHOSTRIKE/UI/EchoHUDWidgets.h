#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EchoHUDWidgets.generated.h"
class UProgressBar;
class UTextBlock;
class UWidgetAnimation;
class UEchoHealthComponent;
class UEchoCharacterMovementComponent;
class APlayerState;
class AEchoGameState;
class AGameStateBase;

UCLASS(Abstract)
class ECHOSTRIKE_API UEchoHealthBar : public UUserWidget
{
	GENERATED_BODY()
public:
	void Observe(UEchoHealthComponent* Component, const FText& Label);

protected:
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthFill;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> IdentityText;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> DamagePulse;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD", meta = (ClampMin = "0.01"))
	float TransitionSeconds = 0.18f;

private:
	UFUNCTION()
	void HealthChanged(UEchoHealthComponent* Component, float Health, float Previous);
	void AnimateHealth();
	TWeakObjectPtr<UEchoHealthComponent> Source;
	FTimerHandle TransitionTimer;
	float Target = 1, Start = 1;
	double TransitionStart = 0;
};

UCLASS(Abstract)
class ECHOSTRIKE_API UEchoDashIndicator : public UUserWidget
{
	GENERATED_BODY()
public:
	void Observe(UEchoCharacterMovementComponent* Component);

protected:
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CooldownFill;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ReadyPulse;

private:
	void DashChanged(float Remaining, float Duration, bool bDashing);
	TWeakObjectPtr<UEchoCharacterMovementComponent> Source;
	FDelegateHandle DashHandle;
	bool bWasReady = true;
};

UCLASS(Abstract)
class ECHOSTRIKE_API UEchoRespawnWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetDeadline(double ServerDeadline);

protected:
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CountdownText;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Reveal;

private:
	void UpdateCountdown();
	double Deadline = -1;
	FTimerHandle CountdownTimer;
};

UCLASS(Abstract)
class ECHOSTRIKE_API UEchoHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void RefreshSources();
	void RefreshRespawn();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEchoHealthBar> PlayerHealth;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEchoHealthBar> OpponentHealth;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEchoDashIndicator> DashIndicator;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEchoRespawnWidget> Respawn;

private:
	UFUNCTION()
	void PawnChanged(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);
	TWeakObjectPtr<AEchoGameState> State;
	TArray<TWeakObjectPtr<APlayerState>> ObservedPlayers;
	FDelegateHandle RosterHandle;
	FDelegateHandle GameStateHandle;
	void GameStateChanged(AGameStateBase* NewState);
};

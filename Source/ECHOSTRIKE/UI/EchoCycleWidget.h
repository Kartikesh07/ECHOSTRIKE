#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EchoCycleWidget.generated.h"

class AEchoGameState;
class AEchoPlayerState;
class APlayerState;
class UEchoHealthComponent;
class UProgressBar;
class UTextBlock;
class UWidgetAnimation;

UENUM(BlueprintType)
enum class EEchoCycleStatus : uint8
{
	Unavailable,
	Recording,
	EchoActive
};

/** Client presentation of the existing shared clock. Never starts recording or spawns echoes. */
UCLASS(Abstract, Blueprintable)
class ECHOSTRIKE_API UEchoCycleWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UEchoCycleWidget(const FObjectInitializer& ObjectInitializer);
	void Observe(AEchoGameState* InState, AEchoPlayerState* InPlayer);

	UPROPERTY(BlueprintReadOnly, Category = "Time Echo") int64 CycleNumber = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Time Echo") float SecondsRemaining = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Time Echo") float RecordingProgress = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Time Echo") EEchoCycleStatus EchoStatus = EEchoCycleStatus::Unavailable;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CycleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CountdownText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> RecordingFill;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> CycleTransition;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> EchoActivation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo", meta = (ClampMin = "0.02", ClampMax = "1", Units = "s"))
	float CountdownUpdateInterval = 0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo") FText RecordingLabel;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo") FText EchoActiveLabel;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo") FText UnavailableLabel;
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Time Echo")
	void OnCycleTransition(int64 NewCycleNumber);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Time Echo")
	void OnEchoStatusChanged(EEchoCycleStatus NewStatus);

private:
	void Refresh();
	void ObservePawn();
	void UnbindSources();
	UFUNCTION() void PawnChanged(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);
	UFUNCTION() void Eliminated(UEchoHealthComponent* Health);
	TWeakObjectPtr<AEchoGameState> State;
	TWeakObjectPtr<AEchoPlayerState> Player;
	TWeakObjectPtr<UEchoHealthComponent> Health;
	FDelegateHandle CycleHandle;
	FDelegateHandle EchoHandle;
	FTimerHandle CountdownTimer;
	int64 LastActivatedCycle = INDEX_NONE;
	int64 DisplayedCycle = INDEX_NONE;
	int32 DisplayedSeconds = INDEX_NONE;
	bool bPresented = false;
};

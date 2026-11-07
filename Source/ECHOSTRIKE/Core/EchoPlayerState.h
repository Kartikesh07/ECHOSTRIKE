#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "EchoPlayerState.generated.h"

class UEchoRecorderComponent;
class AEchoGhostCharacter;
struct FEchoRecording;
DECLARE_MULTICAST_DELEGATE(FEchoAvailabilityChanged);

/** Owner HUD metadata only; no additional ghost actor reference is replicated. */
USTRUCT(BlueprintType)
struct FEchoAvailability
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int64 Cycle = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) double StartTime = -1.;
	UPROPERTY(BlueprintReadOnly) double EndTime = -1.;
};

/** Uses the framework's replicated player identity and connection lifecycle. */
UCLASS()
class ECHOSTRIKE_API AEchoPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	AEchoPlayerState();
	FEchoAvailabilityChanged OnEchoAvailabilityChanged;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(ReplicatedUsing = OnRep_EchoAvailability, BlueprintReadOnly, Category = "Time Echo")
	FEchoAvailability EchoAvailability;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time Echo")
	TObjectPtr<UEchoRecorderComponent> EchoRecorder;
	/** Visual class, including mesh/material defaults, is selected in Blueprint assets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo")
	TSubclassOf<AEchoGhostCharacter> EchoGhostClass;
	void ClearEcho();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UFUNCTION() void OnRep_EchoAvailability();
	UFUNCTION() void HandleEchoDestroyed(AActor* DestroyedActor);
	void SetEchoAvailability(int64 Cycle, double PlaybackStart, double PlaybackEnd);
	int64 LastHandledCycle = INDEX_NONE;
	void HandleCycleCompleted(const FEchoRecording& Recording, int64 NewCycle, double ServerStartTime);
	UPROPERTY(Transient) TObjectPtr<AEchoGhostCharacter> ActiveEcho;
};

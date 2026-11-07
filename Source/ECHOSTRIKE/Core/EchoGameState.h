#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TimeEcho/EchoRecordingTypes.h"
#include "EchoGameState.generated.h"

DECLARE_MULTICAST_DELEGATE(FEchoRosterChanged);
DECLARE_MULTICAST_DELEGATE(FEchoCycleStateChanged);

/** One coherent replicated clock snapshot; cycle indices are zero-based. */
USTRUCT(BlueprintType)
struct FEchoCycleState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) double Epoch = 0.;
	UPROPERTY(BlueprintReadOnly) float Duration = 12.f;
	UPROPERTY(BlueprintReadOnly) int64 Cycle = 0;
	UPROPERTY(BlueprintReadOnly) bool bReady = false;
};

/** Replicated global framework state, including the connected PlayerArray. */
UCLASS()
class ECHOSTRIKE_API AEchoGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	FEchoRosterChanged OnRosterChanged;
	FEchoCycleStateChanged OnCycleStateChanged;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AddPlayerState(APlayerState* Player) override;
	virtual void RemovePlayerState(APlayerState* Player) override;

	/** Settings are latched at BeginPlay so every player uses the same clock. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo", meta = (ClampMin = "1", ClampMax = "120", Units = "s"))
	float RecordingDuration = 12.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo", meta = (ClampMin = "1", ClampMax = "120", Units = "Hz"))
	float SamplingFrequency = 20.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo", meta = (ClampMin = "1", ClampMax = "4096"))
	int32 MaxEventsPerCycle = 1024;
	/** Global FIFO bound includes disconnected players. Oldest recordings are evicted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaxCompletedRecordings = 32;

	UPROPERTY(ReplicatedUsing = OnRep_CycleState, BlueprintReadOnly, Category = "Time Echo")
	FEchoCycleState CycleState;
	UFUNCTION(BlueprintPure, Category = "Time Echo")
	double GetRecordingEpoch() const { return CycleState.Epoch; }
	UFUNCTION(BlueprintPure, Category = "Time Echo")
	float GetCycleDuration() const { return CycleState.Duration; }
	/** Uses synchronized server time, so a delayed replication update never stalls the countdown. */
	UFUNCTION(BlueprintPure, Category = "Time Echo")
	int64 GetCurrentCycle() const;
	int64 GetCycleAtServerTime(double ServerTime) const;
	UFUNCTION(BlueprintPure, Category = "Time Echo")
	float GetCycleRemaining() const;
	UFUNCTION(BlueprintPure, Category = "Time Echo")
	float GetCycleProgress() const;
	int32 GetSampleLimit() const { return SampleLimit; }
	int32 GetEventLimit() const { return EventLimit; }
	void StoreRecording(FEchoRecording&& Recording);
	/** Authority only, valid until the archive changes. Copy values for longer ownership. */
	const TArray<FEchoRecording>& GetCompletedRecordings() const { return CompletedRecordings; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void SampleRecorders();
	FTimerHandle RecordingTimer;
	UFUNCTION() void OnRep_CycleState();
	double SamplePeriod = 0.05;
	int32 SampleLimit = 242;
	int32 EventLimit = 1024;
	int32 ArchiveLimit = 32;
	// Intentionally neither replicated nor actor-referencing.
	TArray<FEchoRecording> CompletedRecordings;
};

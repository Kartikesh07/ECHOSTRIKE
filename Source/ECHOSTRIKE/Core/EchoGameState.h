#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TimeEcho/EchoRecordingTypes.h"
#include "EchoGameState.generated.h"

DECLARE_MULTICAST_DELEGATE(FEchoRosterChanged);

/** Replicated global framework state, including the connected PlayerArray. */
UCLASS()
class ECHOSTRIKE_API AEchoGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	FEchoRosterChanged OnRosterChanged;
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

	double GetRecordingEpoch() const { return RecordingEpoch; }
	float GetCycleDuration() const { return CycleDuration; }
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
	double RecordingEpoch = 0.;
	float CycleDuration = 12.f;
	double SamplePeriod = 0.05;
	int32 SampleLimit = 242;
	int32 EventLimit = 1024;
	int32 ArchiveLimit = 32;
	// Intentionally neither replicated nor actor-referencing.
	TArray<FEchoRecording> CompletedRecordings;
};

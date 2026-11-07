#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeEcho/EchoRecordingTypes.h"
#include "EchoRecorderComponent.generated.h"

class AEchoCharacter;
class AEchoGameState;
class APlayerState;
class UEchoHealthComponent;

/** Server-only PlayerState recorder. GameState supplies the shared sampling clock. */
UCLASS(ClassGroup = (TimeEcho))
class ECHOSTRIKE_API UEchoRecorderComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UEchoRecorderComponent();
	void Sample(double Now);
	void FinishRecording();
	const FEchoRecording& GetCurrentRecording() const { return Current; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool Advance(double Now);
	void CaptureTransform(double Now);
	void RecordEvent(FEchoGameplayEvent Event);
	void UnbindPawn();
	UFUNCTION() void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);
	UFUNCTION() void HandleEliminated(UEchoHealthComponent* Health);
	void HandleShot(const FTransform& Transform, float Speed, float Lifetime);
	void HandleDash(const FVector& Direction);

	TWeakObjectPtr<AEchoCharacter> Pawn;
	TWeakObjectPtr<AEchoGameState> RecordingState;
	FEchoRecording Current;
	int32 Life = 0;
	bool bStarted = false;
	bool bFinished = false;
	bool bEliminationRecorded = false;
};

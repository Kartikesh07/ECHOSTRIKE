#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeEcho/EchoRecordingTypes.h"
#include "EchoPlaybackComponent.generated.h"

USTRUCT()
struct FEchoPlaybackLifeEnd
{
	GENERATED_BODY()
	UPROPERTY() float Time = 0.f;
	UPROPERTY() int32 Life = 0;
};

/** Immutable initial payload: clients schedule cosmetics against the shared server clock. */
USTRUCT()
struct FEchoPlaybackData
{
	GENERATED_BODY()
	UPROPERTY() double ServerStartTime = 0.;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() TArray<FEchoTransformSample> Samples;
	UPROPERTY() TArray<FEchoPlaybackLifeEnd> LifeEnds;
	/** Only dash cosmetics are sent; replicated projectiles represent shots. */
	UPROPERTY() TArray<FEchoGameplayEvent> DashEvents;
};

UCLASS(ClassGroup = (TimeEcho))
class ECHOSTRIKE_API UEchoPlaybackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UEchoPlaybackComponent();
	bool Initialize(const FEchoRecording& Recording, double ServerStartTime);
	/** Drain due combat before a cycle replacement, including events in the final frame. */
	void FlushPendingEvents();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	/** Skip stale transient VFX after initial replication or a client hitch. */
	UPROPERTY(EditDefaultsOnly, Category = "Time Echo|Presentation", meta = (ClampMin = "0", Units = "s"))
	float MaxDashVisualLateness = 0.25f;
	virtual void BeginPlay() override;
private:
	UPROPERTY(ReplicatedUsing = OnRep_Playback) FEchoPlaybackData Playback;
	UFUNCTION() void OnRep_Playback();
	bool Validate() const;
	void Evaluate();
	bool IsLifeActiveAt(int32 Life, float Time) const;
	void ProcessCombat(double Time);
	void ProcessPresentation(double Time);
	// Full combat data remains authority-only; each cursor only advances, even if the clock corrects backward.
	UPROPERTY(Transient) TArray<FEchoGameplayEvent> CombatEvents;
	TSet<int32> EndedLives;
	int32 NextCombatEvent = 0;
	int32 NextDashEvent = 0;
	bool bReady = false;
};

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

/** One immutable, initial-only payload. Combat events never execute during playback. */
USTRUCT()
struct FEchoPlaybackData
{
	GENERATED_BODY()
	UPROPERTY() double ServerStartTime = 0.;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() TArray<FEchoTransformSample> Samples;
	UPROPERTY() TArray<FEchoPlaybackLifeEnd> LifeEnds;
};

UCLASS(ClassGroup = (TimeEcho))
class ECHOSTRIKE_API UEchoPlaybackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UEchoPlaybackComponent();
	bool Initialize(const FEchoRecording& Recording, double ServerStartTime);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void BeginPlay() override;
private:
	UPROPERTY(ReplicatedUsing = OnRep_Playback) FEchoPlaybackData Playback;
	UFUNCTION() void OnRep_Playback();
	bool Validate() const;
	void Evaluate();
	bool bReady = false;
};

#include "TimeEcho/EchoPlaybackComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UEchoPlaybackComponent::UEchoPlaybackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

bool UEchoPlaybackComponent::Initialize(const FEchoRecording& Recording, double ServerStartTime)
{
	if (bReady || !GetOwner()->HasAuthority() || !Recording.bCompleteCycle || Recording.bOverflowed) return false;
	Playback = FEchoPlaybackData();
	Playback.ServerStartTime = ServerStartTime;
	Playback.Duration = Recording.Duration;
	Playback.Samples = Recording.Samples;
	for (const FEchoGameplayEvent& Event : Recording.Events)
		if (Event.Type == EEchoRecordedEvent::Elimination)
		{
			FEchoPlaybackLifeEnd& End = Playback.LifeEnds.AddDefaulted_GetRef();
			End.Time = Event.Time;
			End.Life = Event.Life;
		}
	bReady = Validate();
	return bReady;
}

bool UEchoPlaybackComponent::Validate() const
{
	// Bound initial replication, including unusually configured or corrupted recordings.
	if (!FMath::IsFinite(Playback.ServerStartTime) || !FMath::IsFinite(Playback.Duration)
		|| Playback.Duration <= 0.f || Playback.Duration > 120.f || Playback.Samples.IsEmpty()
		|| Playback.Samples.Num() > 2048 || Playback.LifeEnds.Num() > 2048) return false;
	float PreviousTime = -1.f;
	int32 PreviousLife = 0;
	for (const FEchoTransformSample& Sample : Playback.Samples)
	{
		if (!FMath::IsFinite(Sample.Time) || Sample.Time < PreviousTime || Sample.Time < 0.f
			|| Sample.Time > Playback.Duration || Sample.Life < PreviousLife || Sample.Life <= 0
			|| Sample.Location.ContainsNaN() || Sample.Rotation.ContainsNaN()) return false;
		PreviousTime = Sample.Time;
		PreviousLife = Sample.Life;
	}
	for (const FEchoPlaybackLifeEnd& End : Playback.LifeEnds)
		if (!FMath::IsFinite(End.Time) || End.Time < 0.f || End.Time > Playback.Duration || End.Life <= 0) return false;
	return true;
}

void UEchoPlaybackComponent::BeginPlay()
{
	Super::BeginPlay();
	Evaluate();
}

void UEchoPlaybackComponent::OnRep_Playback()
{
	bReady = Validate();
	Evaluate();
}

void UEchoPlaybackComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UEchoPlaybackComponent, Playback, COND_InitialOnly);
}

void UEchoPlaybackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Evaluate();
}

void UEchoPlaybackComponent::Evaluate()
{
	AActor* Ghost = GetOwner();
	const AGameStateBase* State = GetWorld()->GetGameState();
	if (!bReady || !State) { Ghost->SetActorHiddenInGame(true); return; }
	const double Time = State->GetServerWorldTimeSeconds() - Playback.ServerStartTime;
	if (Time < 0. || Time >= Playback.Duration || Time < Playback.Samples[0].Time)
	{
		Ghost->SetActorHiddenInGame(true);
		return;
	}
	// Upper bound also selects the new life when two boundary samples share a timestamp.
	int32 Low = 0, High = Playback.Samples.Num();
	while (Low < High)
	{
		const int32 Mid = Low + (High - Low) / 2;
		if (Playback.Samples[Mid].Time <= Time) Low = Mid + 1; else High = Mid;
	}
	const FEchoTransformSample& A = Playback.Samples[Low - 1];
	for (const FEchoPlaybackLifeEnd& End : Playback.LifeEnds)
		if (End.Life == A.Life && Time >= End.Time) { Ghost->SetActorHiddenInGame(true); return; }
	FVector Location = A.Location;
	FQuat Rotation = A.Rotation.Quaternion();
	if (Low < Playback.Samples.Num())
	{
		const FEchoTransformSample& B = Playback.Samples[Low];
		if (A.Life == B.Life && B.Time > A.Time)
		{
			const double Alpha = FMath::Clamp((Time - A.Time) / (B.Time - A.Time), 0., 1.);
			Location = FMath::Lerp(A.Location, B.Location, Alpha);
			Rotation = FQuat::Slerp(Rotation, B.Rotation.Quaternion(), Alpha).GetNormalized();
		}
	}
	Ghost->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Ghost->SetActorHiddenInGame(false);
}

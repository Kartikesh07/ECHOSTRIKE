#include "TimeEcho/EchoPlaybackComponent.h"
#include "Engine/World.h"
#include "Combat/EchoProjectile.h"
#include "TimeEcho/EchoGhostCharacter.h"
#include "GameFramework/PlayerState.h"
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
	CombatEvents = Recording.Events;
	for (const FEchoGameplayEvent& Event : Recording.Events)
	{
		if (Event.Type == EEchoRecordedEvent::Elimination)
		{
			FEchoPlaybackLifeEnd& End = Playback.LifeEnds.AddDefaulted_GetRef();
			End.Time = Event.Time;
			End.Life = Event.Life;
		}
		else if (Event.Type == EEchoRecordedEvent::Dash)
			Playback.DashEvents.Add(Event);
	}
	// Reject malformed ordering rather than sorting away the meaning of equal-time events.
	float PreviousEventTime = -1.f;
	int32 PreviousEventLife = 0;
	for (const FEchoGameplayEvent& Event : CombatEvents)
	{
		if (!FMath::IsFinite(Event.Time) || Event.Time < 0.f || Event.Time < PreviousEventTime
			|| Event.Time > Playback.Duration || Event.Life <= 0 || Event.Life < PreviousEventLife
			|| Event.Transform.ContainsNaN() || !Event.Transform.GetRotation().IsNormalized()
			|| Event.Direction.ContainsNaN()) return false;
		if (Event.Type == EEchoRecordedEvent::Shot
			&& (!Event.ProjectileClass || !FMath::IsFinite(Event.ProjectileSpeed) || Event.ProjectileSpeed <= 0.f
				|| !FMath::IsFinite(Event.ProjectileLifetime) || Event.ProjectileLifetime <= 0.f)) return false;
		if (Event.Type != EEchoRecordedEvent::Shot && Event.Type != EEchoRecordedEvent::Dash
			&& Event.Type != EEchoRecordedEvent::Elimination) return false;
		PreviousEventTime = Event.Time;
		PreviousEventLife = Event.Life;
	}
	bReady = Validate();
	return bReady;
}

bool UEchoPlaybackComponent::Validate() const
{
	// Bound initial replication, including unusually configured or corrupted recordings.
	if (!FMath::IsFinite(Playback.ServerStartTime) || !FMath::IsFinite(Playback.Duration)
		|| Playback.Duration <= 0.f || Playback.Duration > 120.f || Playback.Samples.IsEmpty()
		|| Playback.Samples.Num() > 2048 || Playback.LifeEnds.Num() > 4096 || Playback.DashEvents.Num() > 4096 || CombatEvents.Num() > 4096) return false;
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
	PreviousTime = -1.f;
	for (const FEchoGameplayEvent& Event : Playback.DashEvents)
	{
		if (Event.Type != EEchoRecordedEvent::Dash || !FMath::IsFinite(Event.Time)
			|| Event.Time < 0.f || Event.Time < PreviousTime || Event.Time > Playback.Duration
			|| Event.Life <= 0 || Event.Transform.ContainsNaN() || Event.Direction.ContainsNaN()) return false;
		PreviousTime = Event.Time;
	}
	return true;
}

bool UEchoPlaybackComponent::IsLifeActiveAt(int32 Life, float Time) const
{
	bool bFoundLife = false;
	for (const FEchoTransformSample& Sample : Playback.Samples)
	{
		if (Sample.Time > Time) break;
		if (Sample.Life == Life) bFoundLife = true;
		if (Sample.Life > Life && Sample.Time < Time) return false;
	}
	for (const FEchoPlaybackLifeEnd& End : Playback.LifeEnds)
		if (End.Life == Life && End.Time < Time) return false;
	return bFoundLife;
}

void UEchoPlaybackComponent::FlushPendingEvents()
{
	if (!bReady || !GetOwner()->HasAuthority()) return;
	if (const AGameStateBase* State = GetWorld()->GetGameState())
		ProcessCombat(State->GetServerWorldTimeSeconds() - Playback.ServerStartTime);
}

void UEchoPlaybackComponent::ProcessCombat(double Time)
{
	if (!GetOwner()->HasAuthority() || Time < 0.) return;
	const double ThroughTime = FMath::Min(Time, static_cast<double>(Playback.Duration));
	while (CombatEvents.IsValidIndex(NextCombatEvent) && CombatEvents[NextCombatEvent].Time <= ThroughTime)
	{
		// Consume before spawning or callbacks: reentry cannot replay a shot.
		const FEchoGameplayEvent Event = CombatEvents[NextCombatEvent++];
		if (Event.Type == EEchoRecordedEvent::Elimination) { EndedLives.Add(Event.Life); continue; }
		if (Event.Type != EEchoRecordedEvent::Shot || EndedLives.Contains(Event.Life)
			|| !IsLifeActiveAt(Event.Life, Event.Time)) continue;
		APlayerState* Player = Cast<APlayerState>(GetOwner()->GetOwner());
		if (!IsValid(Player) || Player->IsInactive()) continue;
		AEchoProjectile::SpawnAuthoritative(GetOwner(), Player->GetPawn(), Player, Event.ProjectileClass,
			Event.Transform, Event.ProjectileSpeed, Event.ProjectileLifetime);
	}
}

void UEchoPlaybackComponent::ProcessPresentation(double Time)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	while (Playback.DashEvents.IsValidIndex(NextDashEvent) && Playback.DashEvents[NextDashEvent].Time <= Time)
	{
		const FEchoGameplayEvent Event = Playback.DashEvents[NextDashEvent++];
		if (Time >= Playback.Duration || Time - Event.Time > FMath::Max(0.f, MaxDashVisualLateness)
			|| !IsLifeActiveAt(Event.Life, Event.Time) || !IsLifeActiveAt(Event.Life, static_cast<float>(Time))) continue;
		bool bEliminated = false;
		for (const FEchoPlaybackLifeEnd& End : Playback.LifeEnds)
			bEliminated |= End.Life == Event.Life && End.Time <= Time;
		if (!bEliminated)
			if (AEchoGhostCharacter* Ghost = Cast<AEchoGhostCharacter>(GetOwner()))
				Ghost->OnDashReplay(Event.Transform, Event.Direction, static_cast<float>(Time - Event.Time));
	}
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
	ProcessCombat(Time);
	if (!IsValid(Ghost)) return;
	ProcessPresentation(Time);
	if (Time >= Playback.Duration && Ghost->HasAuthority()) { Ghost->Destroy(); return; }
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

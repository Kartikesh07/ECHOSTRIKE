#include "TimeEcho/EchoRecorderComponent.h"
#include "Combat/EchoHealthComponent.h"
#include "Core/EchoGameState.h"
#include "Core/EchoPlayerState.h"
#include "Player/EchoCharacter.h"
#include "Player/EchoCharacterMovementComponent.h"
#include "Engine/World.h"

UEchoRecorderComponent::UEchoRecorderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UEchoRecorderComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		APlayerState* Player = CastChecked<APlayerState>(GetOwner());
		Player->OnPawnSet.AddDynamic(this, &UEchoRecorderComponent::HandlePawnSet);
		HandlePawnSet(Player, Player->GetPawn(), nullptr);
	}
}

void UEchoRecorderComponent::UnbindPawn()
{
	if (AEchoCharacter* Character = Pawn.Get())
	{
		Character->OnShotFired.RemoveAll(this);
		CastChecked<UEchoCharacterMovementComponent>(Character->GetCharacterMovement())->OnDashConfirmed.RemoveAll(this);
		Character->HealthComponent->OnEliminated.RemoveDynamic(this, &UEchoRecorderComponent::HandleEliminated);
	}
	Pawn.Reset();
}

void UEchoRecorderComponent::HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	if (!GetOwner()->HasAuthority() || bFinished || Pawn.Get() == NewPawn) return;
	UnbindPawn();
	AEchoCharacter* Character = Cast<AEchoCharacter>(NewPawn);
	if (!IsValid(Character)) return;
	Pawn = Character;
	++Life;
	bEliminationRecorded = Character->IsEliminated();
	Character->OnShotFired.AddUObject(this, &UEchoRecorderComponent::HandleShot);
	CastChecked<UEchoCharacterMovementComponent>(Character->GetCharacterMovement())->OnDashConfirmed.AddUObject(this, &UEchoRecorderComponent::HandleDash);
	Character->HealthComponent->OnEliminated.AddDynamic(this, &UEchoRecorderComponent::HandleEliminated);
	Sample(GetWorld()->GetTimeSeconds());
}

bool UEchoRecorderComponent::Advance(double Now)
{
	if (!GetOwner()->HasAuthority() || bFinished) return false;
	AEchoGameState* State = RecordingState.Get();
	if (!State)
	{
		State = GetWorld()->GetGameState<AEchoGameState>();
		if (!IsValid(State) || !State->HasActorBegunPlay()) return false;
		RecordingState = State;
	}
	const double Duration = State->GetCycleDuration();
	const int64 Cycle = FMath::FloorToInt64(FMath::Max(0., Now - State->GetRecordingEpoch()) / Duration);
	if (bStarted && Current.Cycle == Cycle) return true;
	if (bStarted)
	{
		Current.bCompleteCycle = true;
		State->StoreRecording(MoveTemp(Current));
	}
	// After a server hitch skip wholly unobserved cycles; never fabricate samples.
	Current = FEchoRecording();
	Current.PlayerId = CastChecked<APlayerState>(GetOwner())->GetPlayerId();
	Current.Cycle = Cycle;
	Current.ServerStartTime = State->GetRecordingEpoch() + Cycle * Duration;
	Current.Duration = Duration;
	Current.Samples.Reserve(State->GetSampleLimit());
	Current.Events.Reserve(FMath::Min(State->GetEventLimit(), 128));
	bStarted = true;
	return true;
}

void UEchoRecorderComponent::CaptureTransform(double Now)
{
	AEchoCharacter* Character = Pawn.Get();
	if (!IsValid(Character)) return;
	const FVector Location = Character->GetActorLocation();
	const FRotator Rotation = Character->GetActorRotation();
	if (Location.ContainsNaN() || Rotation.ContainsNaN()) return;
	const float Time = FMath::Clamp(static_cast<float>(Now - Current.ServerStartTime), 0.f, Current.Duration);
	if (!Current.Samples.IsEmpty() && Current.Samples.Last().Time == Time && Current.Samples.Last().Life == Life) return;
	if (Current.Samples.Num() >= RecordingState->GetSampleLimit())
	{
		Current.bOverflowed = true;
		return;
	}
	FEchoTransformSample& Sample = Current.Samples.AddDefaulted_GetRef();
	Sample.Time = Time;
	Sample.Location = Location;
	Sample.Rotation = Rotation;
	Sample.Life = Life;
}

void UEchoRecorderComponent::Sample(double Now)
{
	if (Advance(Now) && Pawn.IsValid() && !Pawn->IsEliminated()) CaptureTransform(Now);
}

void UEchoRecorderComponent::RecordEvent(FEchoGameplayEvent Event)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Advance(Now) || !Pawn.IsValid() || Event.Transform.ContainsNaN()) return;
	Event.Time = FMath::Clamp(static_cast<float>(Now - Current.ServerStartTime), 0.f, Current.Duration);
	Event.Life = Life;
	if (Current.Events.Num() < RecordingState->GetEventLimit())
		Current.Events.Add(MoveTemp(Event));
	else
		Current.bOverflowed = true;
}

void UEchoRecorderComponent::HandleShot(const FTransform& Transform, float Speed, float Lifetime)
{
	FEchoGameplayEvent Event;
	Event.Type = EEchoRecordedEvent::Shot;
	Event.Transform = Transform;
	Event.Direction = Transform.GetRotation().GetForwardVector();
	Event.ProjectileSpeed = Speed;
	Event.ProjectileLifetime = Lifetime;
	RecordEvent(MoveTemp(Event));
}

void UEchoRecorderComponent::HandleDash(const FVector& Direction)
{
	if (!Pawn.IsValid()) return;
	FEchoGameplayEvent Event;
	Event.Type = EEchoRecordedEvent::Dash;
	Event.Transform = Pawn->GetActorTransform();
	Event.Direction = Direction;
	RecordEvent(MoveTemp(Event));
}

void UEchoRecorderComponent::HandleEliminated(UEchoHealthComponent* Health)
{
	if (bEliminationRecorded || !Pawn.IsValid()) return;
	bEliminationRecorded = true;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Advance(Now)) CaptureTransform(Now);
	FEchoGameplayEvent Event;
	Event.Type = EEchoRecordedEvent::Elimination;
	Event.Transform = Pawn->GetActorTransform();
	RecordEvent(MoveTemp(Event));
}

void UEchoRecorderComponent::FinishRecording()
{
	if (!GetOwner()->HasAuthority() || bFinished) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Advance(Now))
	{
		Current.Duration = FMath::Clamp(static_cast<float>(Now - Current.ServerStartTime), 0.f, Current.Duration);
		RecordingState->StoreRecording(MoveTemp(Current));
		Current = FEchoRecording();
	}
	bFinished = true;
	UnbindPawn();
}

void UEchoRecorderComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	FinishRecording();
	if (APlayerState* Player = Cast<APlayerState>(GetOwner()))
		Player->OnPawnSet.RemoveDynamic(this, &UEchoRecorderComponent::HandlePawnSet);
	Super::EndPlay(Reason);
}

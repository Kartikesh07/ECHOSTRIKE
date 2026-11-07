#include "Player/EchoCharacterMovementComponent.h"
#include "Player/EchoCharacter.h"

class FEchoSavedMove : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;
	bool bDash = false;
	bool bStartedDashing = false;
	float StartCooldown = 0.0f;
	FVector StartLastDirection = FVector::ForwardVector;

	virtual void Clear() override
	{
		Super::Clear();
		bDash = bStartedDashing = false;
		StartCooldown = 0.0f;
		StartLastDirection = FVector::ForwardVector;
	}
	virtual uint8 GetCompressedFlags() const override
	{
		return Super::GetCompressedFlags() | (bDash ? FLAG_Custom_0 : 0);
	}
	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override
	{
		// The standard old-move channel resends a lost dash edge.
		return bDash || Super::IsImportantMove(LastAckedMove);
	}
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, const FVector& NewAccel,
		FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
		const auto* Movement = CastChecked<UEchoCharacterMovementComponent>(Character->GetCharacterMovement());
		bDash = Movement->bDashRequested;
		bStartedDashing = Movement->IsDashing();
		StartCooldown = Movement->CooldownRemaining;
		StartLastDirection = Movement->LastDirection;
	}
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
	{
		const auto* Other = static_cast<const FEchoSavedMove*>(NewMove.Get());
		return !bDash && !Other->bDash && !bStartedDashing && !Other->bStartedDashing
			&& Super::CanCombineWith(NewMove, Character, MaxDelta);
	}
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* Character,
		APlayerController* PC, const FVector& OldStartLocation) override
	{
		Super::CombineWith(OldMove, Character, PC, OldStartLocation);
		const auto* Old = static_cast<const FEchoSavedMove*>(OldMove);
		auto* Movement = CastChecked<UEchoCharacterMovementComponent>(Character->GetCharacterMovement());
		// Combining rewinds movement. Rewind the cooldown as well, avoiding double advancement.
		Movement->CooldownRemaining = StartCooldown = Old->StartCooldown;
		Movement->LastDirection = StartLastDirection = Old->StartLastDirection;
	}
	// Replay deliberately restores only intent through compressed flags, not predicted cooldown/state.
	// The authoritative correction supplies the replay's starting state.
};

class FEchoPredictionData : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;
	explicit FEchoPredictionData(const UCharacterMovementComponent& Movement) : Super(Movement) {}
	virtual FSavedMovePtr AllocateNewMove() override { return FSavedMovePtr(new FEchoSavedMove()); }
};

UEchoCharacterMovementComponent::UEchoCharacterMovementComponent()
{
	SetMoveResponseDataContainer(MoveResponse);
}

void UEchoCharacterMovementComponent::RequestDash()
{
	if (CanStartDash())
	{
		bDashRequested = true;
	}
}

bool UEchoCharacterMovementComponent::CanStartDash() const
{
	const AEchoCharacter* Character = Cast<AEchoCharacter>(CharacterOwner);
	return Character && Character->GetController() && !Character->IsEliminated()
		&& !IsDashing() && (IsMovingOnGround() || IsFalling()) && CooldownRemaining <= UE_KINDA_SMALL_NUMBER
		&& FMath::IsFinite(DashDistance) && DashDistance > 0.0f
		&& FMath::IsFinite(DashDuration) && DashDuration >= 0.01f
		&& FMath::IsFinite(DashCooldown) && DashCooldown >= 0.0f;
}

void UEchoCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bDashRequested = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void UEchoCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	const FVector InputDirection = Acceleration.GetSafeNormal2D();
	if (!InputDirection.IsNearlyZero())
	{
		LastDirection = InputDirection;
	}
	if (bDashRequested)
	{
		if (CanStartDash())
		{
			DashDirection = LastDirection;
			DashRemaining = DashDuration;
			CooldownRemaining = DashCooldown;
			SetMovementMode(MOVE_Custom, DashMovementMode);
		}
		else if (CharacterOwner->HasAuthority() && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy)
		{
			// Correct rejected intent even when a wall made the position error negligible.
			GetPredictionData_Server_Character()->bForceClientUpdate = true;
			ForceClientAdjustment();
		}
		bDashRequested = false;
	}
}

void UEchoCharacterMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
	CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - DeltaSeconds);
}

void UEchoCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (!IsDashing())
	{
		Super::PhysCustom(DeltaTime, Iterations);
		return;
	}
	if (DeltaTime < MIN_TICK_TIME || !HasValidData())
	{
		return;
	}
	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// Proxies use replicated mode/velocity and the engine's usual network smoothing.
		FHitResult Hit;
		SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true, Hit);
		return;
	}
	const auto* Character = Cast<AEchoCharacter>(CharacterOwner);
	if (!Character || Character->IsEliminated() || !Character->GetController())
	{
		DisableMovement();
		return;
	}
	float RemainingTime = DeltaTime;
	while (RemainingTime >= MIN_TICK_TIME && IsDashing() && Iterations < MaxSimulationIterations)
	{
		++Iterations;
		const float Step = FMath::Min(GetSimulationTimeStep(RemainingTime, Iterations), DashRemaining);
		if (Step < MIN_TICK_TIME)
		{
			FinishDash();
			break;
		}
		Velocity = DashDirection * (DashDistance / DashDuration);
		FVector Delta = Velocity * Step;
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		if (CurrentFloor.IsWalkableFloor())
		{
			Delta = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
		}
		FHitResult Hit;
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
		RemainingTime -= Step;
		DashRemaining = FMath::Max(0.0f, DashRemaining - Step);
		if (Hit.IsValidBlockingHit())
		{
			HandleImpact(Hit, Step, Delta);
			if (IsDashing()) { FinishDash(); }
		}
		else if (DashRemaining <= UE_KINDA_SMALL_NUMBER)
		{
			FinishDash();
		}
	}
	if (!IsDashing() && RemainingTime >= MIN_TICK_TIME)
	{
		StartNewPhysics(RemainingTime, Iterations);
	}
}

void UEchoCharacterMovementComponent::FinishDash()
{
	DashRemaining = 0.0f;
	Velocity = Acceleration.GetSafeNormal2D() * MaxWalkSpeed;
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	SetMovementMode(CurrentFloor.IsWalkableFloor() ? MOVE_Walking : MOVE_Falling);
}

void UEchoCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMode, PreviousCustomMode);
	if (!IsDashing())
	{
		DashRemaining = 0.0f;
		bDashRequested = false;
	}
}

FNetworkPredictionData_Client* UEchoCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		auto* MutableThis = const_cast<UEchoCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FEchoPredictionData(*this);
	}
	return ClientPredictionData;
}

void FEchoMoveResponse::ServerFillResponseData(const UCharacterMovementComponent& Movement, const FClientAdjustment& Adjustment)
{
	Super::ServerFillResponseData(Movement, Adjustment);
	const auto& Echo = static_cast<const UEchoCharacterMovementComponent&>(Movement);
	// A throttled correction may describe an earlier move than the current simulation.
	const bool bUseSnapshot = Adjustment.TimeStamp == Echo.PendingDashTimeStamp;
	CooldownRemaining = bUseSnapshot ? Echo.PendingDashState.CooldownRemaining : Echo.CooldownRemaining;
	DashRemaining = bUseSnapshot ? Echo.PendingDashState.DashRemaining : Echo.DashRemaining;
	DashDirection = bUseSnapshot ? FVector(Echo.PendingDashState.DashDirection) : Echo.DashDirection;
	LastDirection = bUseSnapshot ? FVector(Echo.PendingDashState.LastDirection) : Echo.LastDirection;
}

void UEchoCharacterMovementComponent::ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime,
	const FVector& Accel, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	Super::ServerMoveHandleClientError(ClientTimeStamp, DeltaTime, Accel, RelativeClientLocation,
		ClientMovementBase, ClientBaseBoneName, ClientMovementMode);
	if (GetPredictionData_Server_Character()->PendingAdjustment.TimeStamp == ClientTimeStamp)
	{
		PendingDashTimeStamp = ClientTimeStamp;
		PendingDashState.CooldownRemaining = CooldownRemaining;
		PendingDashState.DashRemaining = DashRemaining;
		PendingDashState.DashDirection = DashDirection;
		PendingDashState.LastDirection = LastDirection;
	}
}

bool FEchoMoveResponse::Serialize(UCharacterMovementComponent& Movement, FArchive& Ar, UPackageMap* Map)
{
	const bool bBaseSuccess = Super::Serialize(Movement, Ar, Map);
	bool bDirectionSuccess = true;
	bool bLastDirectionSuccess = true;
	if (IsCorrection())
	{
		Ar << CooldownRemaining << DashRemaining;
		DashDirection.NetSerialize(Ar, Map, bDirectionSuccess);
		LastDirection.NetSerialize(Ar, Map, bLastDirectionSuccess);
	}
	return bBaseSuccess && bDirectionSuccess && bLastDirectionSuccess && !Ar.IsError();
}

void UEchoCharacterMovementComponent::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& Response)
{
	const bool bApplyState = Response.IsCorrection()
		&& GetPredictionData_Client_Character()->GetSavedMoveIndex(Response.ClientAdjustment.TimeStamp) != INDEX_NONE;
	Super::ClientHandleMoveResponse(Response);
	if (bApplyState)
	{
		const auto& Echo = static_cast<const FEchoMoveResponse&>(Response);
		CooldownRemaining = Echo.CooldownRemaining;
		DashRemaining = Echo.DashRemaining;
		DashDirection = FVector(Echo.DashDirection).GetSafeNormal2D();
		LastDirection = FVector(Echo.LastDirection).GetSafeNormal2D();
	}
}

bool UEchoCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	// Input dispatch precedes movement replay. Historical compressed flags must not
	// consume a fresh Shift press queued for the next live move.
	const bool bLiveDashRequested = bDashRequested;
	const bool bReplayed = Super::ClientUpdatePositionAfterServerUpdate();
	bDashRequested = bLiveDashRequested;
	return bReplayed;
}

#include "Player/EchoPlayerController.h"
#include "Player/EchoCharacter.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

AEchoPlayerController::AEchoPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContext(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	DefaultMappingContexts.Add(DefaultContext.Object);
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> FireContext(TEXT("/Game/Input/IMC_Fire.IMC_Fire"));
	DefaultMappingContexts.Add(FireContext.Object);
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DashContext(TEXT("/Game/Input/IMC_Dash.IMC_Dash"));
	DefaultMappingContexts.Add(DashContext.Object);
}

void AEchoPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		bShowMouseCursor = true;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;

		// Keep absolute cursor coordinates available, including while a mouse button
		// is held. There are no UI widgets; Enhanced Input still receives gameplay keys.
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}

void AEchoPlayerController::UpdateRotation(float DeltaTime)
{
	// Runs in the existing controller input tick, before CharacterMovement. No
	// character Tick or per-frame RPC is needed. Remote controllers retain the yaw
	// received by the server through CharacterMovement's packed movement updates.
	Super::UpdateRotation(DeltaTime);
	const AEchoCharacter* EchoPawn = Cast<AEchoCharacter>(GetPawn());
	if (IsLocalController() && EchoPawn && !EchoPawn->IsEliminated())
	{
		float AimYaw;
		if (GetCursorAimYaw(AimYaw))
		{
			SetControlRotation(FRotator(0.0, AimYaw, 0.0));
		}
	}
}

bool AEchoPlayerController::GetCursorAimYaw(float& OutYaw) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
		|| !LocalPlayer->ViewportClient->Viewport->HasFocus())
	{
		return false;
	}

	float MouseX, MouseY;
	int32 Width, Height;
	GetViewportSize(Width, Height);
	if (!GetMousePosition(MouseX, MouseY) || Width <= 0 || Height <= 0
		|| MouseX < Width * LocalPlayer->Origin.X || MouseY < Height * LocalPlayer->Origin.Y
		|| MouseX >= Width * (LocalPlayer->Origin.X + LocalPlayer->Size.X)
		|| MouseY >= Height * (LocalPlayer->Origin.Y + LocalPlayer->Size.Y))
	{
		return false; // Preserve the last valid yaw outside this player's viewport.
	}

	const FVector PawnLocation = GetPawn()->GetActorLocation();
	FVector2D PawnScreenPosition;
	if (ProjectWorldLocationToScreen(PawnLocation, PawnScreenPosition)
		&& FVector2D::DistSquared(PawnScreenPosition, FVector2D(MouseX, MouseY))
			<= FMath::Square(CursorDeadZonePixels))
	{
		return false;
	}

	FVector RayOrigin, RayDirection;
	if (!DeprojectScreenPositionToWorld(MouseX, MouseY, RayOrigin, RayDirection)
		|| RayDirection.Z >= -KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const double RayDistance = (AimPlaneHeight - RayOrigin.Z) / RayDirection.Z;
	if (RayDistance < 0.0 || !FMath::IsFinite(RayDistance))
	{
		return false;
	}

	const FVector AimOffset = (RayOrigin + RayDirection * RayDistance - PawnLocation) * FVector(1.0, 1.0, 0.0);
	if (AimOffset.ContainsNaN() || AimOffset.SizeSquared2D() <= FMath::Square(AimDeadZone))
	{
		return false;
	}

	OutYaw = AimOffset.Rotation().Yaw;
	return true;
}

#pragma once

#include "CoreMinimal.h"
#include "EchoRecordingTypes.generated.h"

UENUM(BlueprintType)
enum class EEchoRecordedEvent : uint8
{
	Shot,
	Dash,
	Elimination
};

/** Value-only data: never retains a pawn, projectile, controller or PlayerState. */
USTRUCT(BlueprintType)
struct ECHOSTRIKE_API FEchoTransformSample
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float Time = 0.f;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FRotator Rotation = FRotator::ZeroRotator;
	/** Changes on possession; playback must not interpolate between different lives. */
	UPROPERTY(BlueprintReadOnly) int32 Life = 0;
};

USTRUCT(BlueprintType)
struct ECHOSTRIKE_API FEchoGameplayEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EEchoRecordedEvent Type = EEchoRecordedEvent::Shot;
	UPROPERTY(BlueprintReadOnly) float Time = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Life = 0;
	/** Shot spawn transform, dash start transform, or elimination transform. */
	UPROPERTY(BlueprintReadOnly) FTransform Transform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) FVector Direction = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float ProjectileSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly) float ProjectileLifetime = 0.f;
};

USTRUCT(BlueprintType)
struct ECHOSTRIKE_API FEchoRecording
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) int64 Cycle = 0;
	UPROPERTY(BlueprintReadOnly) double ServerStartTime = 0.;
	UPROPERTY(BlueprintReadOnly) float Duration = 0.f;
	/** A disconnect can close a partial cycle. */
	UPROPERTY(BlueprintReadOnly) bool bCompleteCycle = false;
	UPROPERTY(BlueprintReadOnly) bool bOverflowed = false;
	UPROPERTY(BlueprintReadOnly) TArray<FEchoTransformSample> Samples;
	UPROPERTY(BlueprintReadOnly) TArray<FEchoGameplayEvent> Events;
};

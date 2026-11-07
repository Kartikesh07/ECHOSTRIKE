#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EchoPlaybackAssetCommandlet.generated.h"

/** Headless visual asset creation; preserves existing designer-authored assets. */
UCLASS()
class UEchoPlaybackAssetCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UEchoPlaybackAssetCommandlet();
	virtual int32 Main(const FString& Params) override;
};

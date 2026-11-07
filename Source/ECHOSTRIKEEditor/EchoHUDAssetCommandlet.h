#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EchoHUDAssetCommandlet.generated.h"
/** Creates missing HUD assets and adds the cycle indicator without rebuilding existing designer layouts. */
UCLASS()
class UEchoHUDAssetCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UEchoHUDAssetCommandlet();
	virtual int32 Main(const FString& Params) override;
};

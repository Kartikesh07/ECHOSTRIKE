#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EchoHUDAssetCommandlet.generated.h"
/** Creates missing HUD assets only. Existing designer work is never overwritten. */
UCLASS()
class UEchoHUDAssetCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UEchoHUDAssetCommandlet();
	virtual int32 Main(const FString& Params) override;
};

// UnrealEditor-Cmd MushokuRPG -run=MTValidateWorld -map=/Game/Maps/L_Fittoa
// Loads the map (all World Partition actors), runs UMTWorldValidationLibrary, writes Saved/Validation/<map>.json.
// Exit code: 0 = no errors, 1 = validation errors, 2 = map could not be loaded / not an editor build.
//
// NOTE: UCommandlet lives in the runtime Engine module, so the UCLASS is declared unconditionally (UHT cannot generate
// code for a UCLASS wrapped in #if WITH_EDITOR). Everything editor-only - includes, World Partition loading and the whole
// body of Main() - is guarded with #if WITH_EDITOR in MTValidateWorldCommandlet.cpp.
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MTValidateWorldCommandlet.generated.h"

UCLASS()
class MUSHOKURPG_API UMTValidateWorldCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMTValidateWorldCommandlet();
	virtual int32 Main(const FString& Params) override;
};

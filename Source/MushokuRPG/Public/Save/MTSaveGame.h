// USaveGame wrapper around the version-safe FMTSaveData payload.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save/MTSaveTypes.h"
#include "MTSaveGame.generated.h"

UCLASS()
class MUSHOKURPG_API UMTSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UMTSaveGame();

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Mushoku|Save")
	FMTSaveData Data;
};

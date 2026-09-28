#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "MTGameMode.generated.h"

UCLASS()
class MUSHOKURPG_API AMTGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMTGameMode();
};

/** Switches between gameplay input and menu (cursor) input based on the HUD state. */
UCLASS()
class MUSHOKURPG_API AMTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMTPlayerController();
	virtual void PlayerTick(float DeltaTime) override;

	/** Console: MTGiveSpins 10 (development convenience; spins still come from gameplay in normal play). */
	UFUNCTION(Exec) void MTGiveSpins(int32 Count);
	/** Console: MTSetCharacter Orsted (only owned characters). */
	UFUNCTION(Exec) void MTSetCharacter(FName CharacterId);
	/** Console: MTSaveNow */
	UFUNCTION(Exec) void MTSaveNow();

private:
	bool bMenuMode = false;
};

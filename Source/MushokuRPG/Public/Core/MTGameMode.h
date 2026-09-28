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
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

protected:
	/** LA PLACE title screen when the game starts (?Menu=0 skips it; the arena and showcase modes turn it off). */
	UPROPERTY(EditDefaultsOnly, Category = "Mushoku|FrontEnd") bool bFrontEndOnStart = true;
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
	/** 1 and 0 held together open the admin popup (UMTAdminSubsystem); edge-triggered. */
	bool bAdminComboHeld = false;
};

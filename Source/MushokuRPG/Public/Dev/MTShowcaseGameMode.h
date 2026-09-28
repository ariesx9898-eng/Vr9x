// Development tool: casts abilities one by one at a training dummy in front of a fixed camera and saves screenshots
// (Saved/Screenshots/Showcase/<n>_<AbilityId>_<ms>.png), then quits. Used to check every spell visually.
//   UnrealEditor <project> "/Game/Maps/L_Fittoa?game=/Script/MushokuRPG.MTShowcaseGameMode?Abilities=Fire_Fireball,Water_Flood" -game
// Without ?Abilities= it runs every hotbar ability of both characters and all element spells.
#pragma once

#include "CoreMinimal.h"
#include "Core/MTGameMode.h"
#include "MTShowcaseGameMode.generated.h"

class AMTEnemyCharacter;
class ACameraActor;

UCLASS()
class MUSHOKURPG_API AMTShowcaseGameMode : public AMTGameMode
{
	GENERATED_BODY()

public:
	AMTShowcaseGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void SetupStage();
	void StartAbility(int32 Index);
	void Capture(const FString& Label);

	TArray<FName> Abilities;
	TArray<float> ShotTimes;
	int32 Current = INDEX_NONE;
	float Clock = 0.f;
	float AbilityClock = 0.f;
	int32 NextShot = 0;
	bool bReleased = false;
	bool bStaged = false;
	FVector StageOrigin = FVector::ZeroVector;
	TWeakObjectPtr<AMTEnemyCharacter> Dummy;
	TWeakObjectPtr<ACameraActor> Camera;
};

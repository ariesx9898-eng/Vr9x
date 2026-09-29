// Development tool: casts abilities one by one at a group of training dummies in front of a fixed camera and saves
// screenshots (Saved/Screenshots/Showcase/<n>_<Label>_<ms>.png), logs one RESULT line per entry, then quits. Used to
// check every spell visually and for the grading loop (Docs/QA_Abilities.md).
//   UnrealEditor <project> "/Game/Maps/L_Fittoa?game=/Script/MushokuRPG.MTShowcaseGameMode?Abilities=Fire_Fireball,Water_Flood" -game
// Entries are separated by ','. "A+B+C" chains abilities as one combo (the next starts as soon as the previous lets go);
// "Orsted:Fire_Fireball" forces the caster's lineage (shared spells default to Rudeus). Without ?Abilities= it runs every
// hotbar ability of both characters, Orsted's versions of the element spells, and both signature combos.
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
	/** One showcase entry: a single ability or a combo chain, cast by one lineage. */
	struct FShowcaseEntry
	{
		TArray<FName> Chain;
		FName Lineage;
		FString Label;
		float Seconds = 3.8f;
	};

	void AddEntry(const FString& Token);
	void SetupStage();
	void StartEntry(int32 Index);
	void AdvanceChain();
	void FinishEntry();
	void Capture(const FString& Label);

	TArray<FShowcaseEntry> Entries;
	TArray<float> ShotTimes;
	int32 Current = INDEX_NONE;
	float Clock = 0.f;
	float EntryClock = 0.f;
	float StepClock = 0.f;
	int32 ChainIndex = 0;
	int32 Executed = 0;
	int32 NextShot = 0;
	float DummyHealthAtStart = 0.f;
	bool bReleased = false;
	bool bStaged = false;
	bool bWarmed = false;
	FVector StageOrigin = FVector::ZeroVector;
	TWeakObjectPtr<AMTEnemyCharacter> Dummy;
	TArray<TWeakObjectPtr<AMTEnemyCharacter>> Crowd;
	TWeakObjectPtr<ACameraActor> Camera;
};

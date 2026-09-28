// Version-safe save payload. Only append fields; bump MT_SAVE_VERSION and add a
// migration step in UMTSaveSubsystem::MigrateSave when semantics change.
#pragma once

#include "CoreMinimal.h"
#include "Core/MTTypes.h"
#include "MTSaveTypes.generated.h"

#define MT_SAVE_VERSION 1

USTRUCT(BlueprintType)
struct FMTMasterySave
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite) EMTMasteryTrack Track = EMTMasteryTrack::Character;
	/** Character id, element name ("Fire") or race name ("Human"). */
	UPROPERTY(SaveGame, BlueprintReadWrite) FName Key;
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 Level = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) float XP = 0.f;
};

USTRUCT(BlueprintType)
struct FMTQuestSaveState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite) FName QuestId;
	/** Current stage for sequential quests. */
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 Stage = 0;
	/** Progress count per objective. */
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<int32> Progress;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bActive = false;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bCompleted = false;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bFailed = false;
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 TimesCompleted = 0;
	/** UTC unix seconds of last completion (repeat cooldowns across sessions). */
	UPROPERTY(SaveGame, BlueprintReadWrite) int64 LastCompletedUnix = 0;
};

USTRUCT(BlueprintType)
struct FMTRollHistoryEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite) EMTRollCategory Category = EMTRollCategory::Character;
	/** Character id, element name or race name. */
	UPROPERTY(SaveGame, BlueprintReadWrite) FName ResultId;
	UPROPERTY(SaveGame, BlueprintReadWrite) EMTRarity Rarity = EMTRarity::Common;
	UPROPERTY(SaveGame, BlueprintReadWrite) int64 TimeUnix = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bPityTriggered = false;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bDuplicateRerolled = false;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bWasNew = false;
};

USTRUCT(BlueprintType)
struct FMTRollPityState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite) int32 SpinsSinceLegendary = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 SpinsSinceMythic = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 TotalSpins = 0;
};

USTRUCT(BlueprintType)
struct FMTSettingsSave
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite) float MouseSensitivity = 1.f;
	UPROPERTY(SaveGame, BlueprintReadWrite) float GamepadSensitivity = 1.f;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bInvertY = false;
	UPROPERTY(SaveGame, BlueprintReadWrite) float FieldOfView = 90.f;
	UPROPERTY(SaveGame, BlueprintReadWrite) float CameraShakeScale = 1.f;
	UPROPERTY(SaveGame, BlueprintReadWrite) float MasterVolume = 1.f;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bSkipRollAnimation = false;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bToggleSprint = false;
	/** 0 low .. 3 epic, applied via Scalability. */
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 GraphicsQuality = 3;
};

USTRUCT(BlueprintType)
struct FMTSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite) int32 SaveVersion = MT_SAVE_VERSION;
	UPROPERTY(SaveGame, BlueprintReadWrite) FString SavedAtUtc;

	// ---- Build (character + elements + race) ----
	UPROPERTY(SaveGame, BlueprintReadWrite) FName EquippedCharacter = TEXT("Rudeus");
	UPROPERTY(SaveGame, BlueprintReadWrite) EMTRace EquippedRace = EMTRace::Human;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<EMTElement> EquippedElements;
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 UnlockedElementSlots = 1;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<FName> OwnedCharacters;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<EMTElement> OwnedElements;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<EMTRace> OwnedRaces;

	// ---- Progression ----
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 Level = 1;
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 XP = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 Gold = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 AdventurerPoints = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) EMTAdventurerRank AdventurerRank = EMTAdventurerRank::F;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<FMTMasterySave> Masteries;
	/** Magic rank XP per element (rank derived from thresholds). */
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<EMTElement, float> MagicRankXP;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<FName> UnlockedAbilities;

	// ---- Rolls ----
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<EMTRollCategory, int32> Spins;
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<EMTRollCategory, FMTRollPityState> Pity;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<FMTRollHistoryEntry> RollHistory;

	// ---- Inventory / world ----
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<FName, int32> Inventory;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<FName> DiscoveredLocations;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<FName> FastTravelUnlocked;
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<FName, int32> BossKills;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<FName> AchievementsUnlocked;
	UPROPERTY(SaveGame, BlueprintReadWrite) int64 LastDailyChallengeUnix = 0;

	// ---- Quests ----
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<FMTQuestSaveState> Quests;
	UPROPERTY(SaveGame, BlueprintReadWrite) FName TrackedQuest;

	// ---- Player / world state ----
	UPROPERTY(SaveGame, BlueprintReadWrite) FName MapName;
	UPROPERTY(SaveGame, BlueprintReadWrite) FVector PlayerLocation = FVector::ZeroVector;
	UPROPERTY(SaveGame, BlueprintReadWrite) FRotator PlayerRotation = FRotator::ZeroRotator;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bHasPlayerTransform = false;
	UPROPERTY(SaveGame, BlueprintReadWrite) float TimeOfDayHours = 8.f;

	UPROPERTY(SaveGame, BlueprintReadWrite) FMTSettingsSave Settings;
};

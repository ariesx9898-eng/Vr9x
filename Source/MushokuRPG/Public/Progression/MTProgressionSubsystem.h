// Player progression: build ownership + equip (character / race / element slots), level & XP,
// mastery tracks, magic ranks, adventurer rank, roll spins + pity state, inventory, gold,
// discovery, settings. Single source of truth that UMTSaveSubsystem serialises.
//
// Spins are earned through gameplay only (level-ups, quests, bosses, rank-ups, achievements).
// There is intentionally no purchase path anywhere in this class.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/MTDataTypes.h"
#include "Save/MTSaveTypes.h"
#include "MTProgressionSubsystem.generated.h"

class AMTCharacterBase;
class UMTDataRegistry;
class UMTGameEvents;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnProgressionChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnProgressionLevelUp, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMTOnProgressionMasteryLevelUp, EMTMasteryTrack, Track, FName, Key, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnProgressionRankUp, EMTAdventurerRank, NewRank);

UCLASS()
class MUSHOKURPG_API UMTProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxElementSlots = 2;
	/** Level at which the second element slot (B) unlocks. */
	static constexpr int32 SecondElementSlotLevel = 10;
	static constexpr int32 MaxLevel = 100;
	static constexpr int32 MaxMasteryLevel = 10;
	static constexpr int32 MaxRollHistory = 100;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UMTProgressionSubsystem* Get(const UObject* WorldContext);

	/** Resets to the new-game state (Rudeus / Human / Earth, starter spins). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression")
	void ResetToNewGame();

	// ---------------------------------------------------------------- Build
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") FName GetEquippedCharacter() const { return EquippedCharacter; }
	/** Must be owned. Broadcasts OnBuildChanged and re-applies the build to the player pawn. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") bool EquipCharacter(FName CharacterId);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") EMTRace GetEquippedRace() const { return EquippedRace; }
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") bool EquipRace(EMTRace Race);

	/** Slot 0 = A, 1 = B. None when empty or locked. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") EMTElement GetEquippedElement(int32 SlotIndex) const;
	/** Element must be owned (None clears slot B). Equipping an element already in the other slot swaps them. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") bool EquipElement(int32 SlotIndex, EMTElement Element);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetUnlockedElementSlots() const { return UnlockedElementSlots; }

	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") bool OwnsCharacter(FName Id) const { return OwnedCharacters.Contains(Id); }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") bool OwnsElement(EMTElement E) const { return OwnedElements.Contains(E); }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") bool OwnsRace(EMTRace R) const { return OwnedRaces.Contains(R); }

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void GrantCharacter(FName Id);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void GrantElement(EMTElement E);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void GrantRace(EMTRace R);

	/** Applies lineage, race and every unlocked element slot to Character. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void ApplyBuildTo(AMTCharacterBase* Character) const;

	// ---------------------------------------------------------------- Level / XP
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void AddXP(int32 Amount);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetLevel() const { return Level; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetXP() const { return XP; }
	/** XP needed to go from the current level to the next (round(100 * L^1.5)). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetXPToNextLevel() const { return CalcXPToNextLevel(Level); }

	// ---------------------------------------------------------------- Mastery (levels 0..10)
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void AddMasteryXP(EMTMasteryTrack Track, FName Key, float InXP);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetMasteryLevel(EMTMasteryTrack Track, FName Key) const;
	/** 0..1 progress toward the next mastery level (1 at max). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") float GetMasteryProgress(EMTMasteryTrack Track, FName Key) const;

	// ---------------------------------------------------------------- Magic rank (per element)
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void AddMagicRankXP(EMTElement Element, float InXP);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") EMTMagicRank GetMagicRank(EMTElement Element) const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") float GetMagicRankXP(EMTElement Element) const;
	/** 0..1 progress toward the next magic rank (1 at God). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") float GetMagicRankProgress(EMTElement Element) const;

	// ---------------------------------------------------------------- Adventurer rank
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void AddAdventurerPoints(int32 Points);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") EMTAdventurerRank GetAdventurerRank() const { return AdventurerRank; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetAdventurerPoints() const { return AdventurerPoints; }

	// ---------------------------------------------------------------- Spins
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetSpins(EMTRollCategory C) const;
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void AddSpins(EMTRollCategory C, int32 N);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") bool ConsumeSpin(EMTRollCategory C);

	// ---------------------------------------------------------------- Inventory / gold
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void AddItem(FName ItemId, int32 Count);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") bool RemoveItem(FName ItemId, int32 Count);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetItemCount(FName ItemId) const;
	const TMap<FName, int32>& GetInventory() const { return Inventory; }

	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetGold() const { return Gold; }
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void AddGold(int32 Amount);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") bool SpendGold(int32 Amount);

	// ---------------------------------------------------------------- World
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void DiscoverLocation(FName LocationId);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") bool IsLocationDiscovered(FName LocationId) const { return DiscoveredLocations.Contains(LocationId); }
	/** Counts the kill; the first defeat of each boss grants a Character spin. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void RecordBossDefeated(FName BossId);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") int32 GetBossKills(FName BossId) const;

	// ---------------------------------------------------------------- Quests (filled by the quest system)
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void MarkQuestCompleted(FName QuestId);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") bool IsQuestCompleted(FName QuestId) const { return CompletedQuestIds.Contains(QuestId); }

	// ---------------------------------------------------------------- Ability unlocks
	/**
	 * UnlockRequirement clauses (comma / semicolon separated, all must pass):
	 *   "Level:5", "Quest:<QuestId>", "Mastery:<Track>:<Key>:<Level>" or "Mastery:<Key>:<Level>" (track inferred).
	 * Plus MasteryRequirement vs the ability's own track and RankRequirement vs the element's magic rank.
	 */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") bool IsAbilityUnlocked(const FMTAbilityData& Ability) const;
	/** Human readable reason the ability is locked (empty when unlocked). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Progression") FText GetAbilityLockReason(const FMTAbilityData& Ability) const;
	/**
	 * Which mastery track an ability feeds: character abilities -> Character track keyed by character id,
	 * race abilities -> Race track keyed by race name, element abilities -> Element track keyed by element name.
	 */
	bool GetAbilityMasteryTrack(const FMTAbilityData& Ability, EMTMasteryTrack& OutTrack, FName& OutKey) const;

	// ---------------------------------------------------------------- Settings
	const FMTSettingsSave& GetSettings() const { return Settings; }
	/** Clamps and stores. Apply to the world with UMTSaveSubsystem::ApplySettings. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Progression") void SetSettings(const FMTSettingsSave& S);

	// ---------------------------------------------------------------- Rolls (used by UMTRollSubsystem)
	TArray<FMTRollHistoryEntry>& GetRollHistoryMutable() { return RollHistory; }
	const TArray<FMTRollHistoryEntry>& GetRollHistory() const { return RollHistory; }
	FMTRollPityState& GetPityMutable(EMTRollCategory C) { return Pity.FindOrAdd(C); }
	FMTRollPityState GetPity(EMTRollCategory C) const { const FMTRollPityState* P = Pity.Find(C); return P ? *P : FMTRollPityState(); }

	// ---------------------------------------------------------------- Save
	void WriteToSave(FMTSaveData& Out) const;
	void ReadFromSave(const FMTSaveData& In);

	// ---------------------------------------------------------------- Curves / keys
	static int32 CalcXPToNextLevel(int32 InLevel);
	/** XP needed to go from InLevel to InLevel + 1: 50 * (InLevel + 1)^1.6. */
	static float CalcMasteryXPForLevel(int32 InLevel);
	static float GetMagicRankThreshold(EMTMagicRank Rank);
	static int32 GetAdventurerRankThreshold(EMTAdventurerRank Rank);
	static FName ElementKey(EMTElement Element) { return FName(*MTUtil::ElementToString(Element)); }
	/** Race mastery / roll key: the race row's RaceID, or the enum name when no row exists. */
	FName RaceKey(EMTRace Race) const;
	/** Display names resolved through the data registry (falls back to ids). */
	FText GetCharacterDisplayName(FName CharacterId) const;
	FText GetRaceDisplayName(EMTRace Race) const;
	FText GetElementDisplayName(EMTElement Element) const;

	// ---------------------------------------------------------------- Events
	UPROPERTY(BlueprintAssignable) FMTOnProgressionChanged OnBuildChanged;
	UPROPERTY(BlueprintAssignable) FMTOnProgressionLevelUp OnLevelUp;
	UPROPERTY(BlueprintAssignable) FMTOnProgressionMasteryLevelUp OnMasteryLevelUp;
	UPROPERTY(BlueprintAssignable) FMTOnProgressionRankUp OnAdventurerRankUp;
	UPROPERTY(BlueprintAssignable) FMTOnProgressionChanged OnInventoryChanged;
	UPROPERTY(BlueprintAssignable) FMTOnProgressionChanged OnSpinsChanged;

	/** Grant FMTEnemyData::XPReward when the local player kills an enemy (via UMTGameEvents::OnEnemyKilled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Progression") bool bGrantKillXP = true;

protected:
	UFUNCTION() void HandleEnemyKilled(FName EnemyId, FGameplayTagContainer EnemyTags, AActor* Killer);
	UFUNCTION() void HandleBossDefeated(FName BossId);
	UFUNCTION() void HandleLocationReached(FName LocationId);

private:
	UMTDataRegistry* GetRegistry() const;
	UMTGameEvents* GetEvents() const;
	void Notify(const FText& Message, const FLinearColor& Color) const;
	/** Broadcasts OnBuildChanged and re-applies to the local player pawn. */
	void BroadcastBuildChanged();
	void SanitizeBuild();
	void OnLevelReached(int32 NewLevel);
	FMTMasterySave* FindMastery(EMTMasteryTrack Track, FName Key);
	const FMTMasterySave* FindMastery(EMTMasteryTrack Track, FName Key) const;
	EMTMasteryTrack InferTrackFromKey(const FString& Key) const;
	bool EvaluateUnlock(const FMTAbilityData& Ability, FString* OutReason) const;

	// Build
	FName EquippedCharacter;
	EMTRace EquippedRace = EMTRace::Human;
	TArray<EMTElement> EquippedElements;
	int32 UnlockedElementSlots = 1;
	TSet<FName> OwnedCharacters;
	TSet<EMTElement> OwnedElements;
	TSet<EMTRace> OwnedRaces;

	// Progression
	int32 Level = 1;
	int32 XP = 0;
	int32 Gold = 0;
	int32 AdventurerPoints = 0;
	EMTAdventurerRank AdventurerRank = EMTAdventurerRank::F;
	TArray<FMTMasterySave> Masteries;
	TMap<EMTElement, float> MagicRankXP;
	TArray<FName> UnlockedAbilities;

	// Rolls
	TMap<EMTRollCategory, int32> Spins;
	TMap<EMTRollCategory, FMTRollPityState> Pity;
	TArray<FMTRollHistoryEntry> RollHistory;

	// Inventory / world
	TMap<FName, int32> Inventory;
	TSet<FName> DiscoveredLocations;
	TArray<FName> FastTravelUnlocked;
	TMap<FName, int32> BossKills;
	TArray<FName> AchievementsUnlocked;
	int64 LastDailyChallengeUnix = 0;
	TSet<FName> CompletedQuestIds;

	FMTSettingsSave Settings;
};

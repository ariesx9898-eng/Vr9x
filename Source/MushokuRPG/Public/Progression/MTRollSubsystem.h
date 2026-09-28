// Character / Element / Race rolls with displayed odds, soft + hard pity and duplicate protection.
// Spins are earned in gameplay only (quests, bosses, level-ups, achievements) - there is no purchase path.
// Tools/sim_roll.py mirrors this logic exactly; keep them in sync.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Math/RandomStream.h"
#include "Core/MTDataTypes.h"
#include "MTRollSubsystem.generated.h"

class UMTProgressionSubsystem;
class UMTDataRegistry;

USTRUCT(BlueprintType)
struct FMTRollResult
{
	GENERATED_BODY()

	/** False when no roll happened (no spins, empty pool, missing subsystems). */
	UPROPERTY(BlueprintReadOnly) bool bValid = false;
	UPROPERTY(BlueprintReadOnly) EMTRollCategory Category = EMTRollCategory::Character;
	/** Character id, element name ("Fire") or race id ("Human"). */
	UPROPERTY(BlueprintReadOnly) FName ResultId;
	UPROPERTY(BlueprintReadOnly) EMTRarity Rarity = EMTRarity::Common;
	/** A hard pity guarantee (Legendary+ or Mythic) decided the tier. */
	UPROPERTY(BlueprintReadOnly) bool bPityTriggered = false;
	/** The first pick was owned and duplicate protection re-rolled it inside the tier. */
	UPROPERTY(BlueprintReadOnly) bool bDuplicateRerolled = false;
	/** Ownership was granted by this roll (false = duplicate, compensation given). */
	UPROPERTY(BlueprintReadOnly) bool bWasNew = false;
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
};

/** One rollable entry (internal, also used by the HUD through GetPool). */
struct FMTRollPoolEntry
{
	FName Id;
	EMTRarity Rarity = EMTRarity::Common;
	float Weight = 1.f;
	FText DisplayName;
	EMTElement Element = EMTElement::None;
	EMTRace Race = EMTRace::Human;
};

UCLASS()
class MUSHOKURPG_API UMTRollSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 NumRarities = 5;
	/** Duplicate compensation (after duplicate protection could not find an unowned entry). */
	static constexpr int32 DuplicateGold = 150;
	static constexpr float DuplicateMasteryXP = 20.f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UMTRollSubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Roll") bool CanRoll(EMTRollCategory Category) const;

	/** Consumes a spin, rolls, grants ownership (or compensation), appends history (cap 100), updates pity. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Roll") FMTRollResult Roll(EMTRollCategory Category);

	/** Displayed base odds per entry (before pity), sorted by rarity (highest first). Probabilities sum to 1. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Roll")
	void GetPool(EMTRollCategory Category, TArray<FName>& OutIds, TArray<EMTRarity>& OutRarities, TArray<float>& OutProbabilities) const;

	/** 0..1 toward the Legendary+ hard pity (0 when the pool has no Legendary+ entries). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Roll") float GetLegendaryPityProgress(EMTRollCategory Category) const;
	/** Spins until Legendary+ is guaranteed (1 = the next spin). -1 when the pool has no Legendary+ entries. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Roll") int32 GetSpinsUntilHardPity(EMTRollCategory Category) const;
	/** Spins until Mythic is guaranteed (1 = the next spin). -1 when the pool has no Mythic entries. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Roll") int32 GetSpinsUntilMythicPity(EMTRollCategory Category) const;
	/** True when every entry in the pool is Legendary or Mythic (Legendary+ pity is then meaningless). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Roll") bool IsPoolAllLegendaryPlus(EMTRollCategory Category) const;
	/** Display name for a pool id of this category. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Roll") FText GetEntryDisplayName(EMTRollCategory Category, FName Id) const;
	/** Is this pool id already owned by the player? */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Roll") bool IsEntryOwned(EMTRollCategory Category, FName Id) const;

	/** Deterministic rolls for tests. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Roll") void SetSeed(int32 Seed) { Stream.Initialize(Seed); }

	/** Effective tuning: registry row (FindRollConfig) or code defaults. */
	FMTRollConfig GetEffectiveConfig(EMTRollCategory Category) const;
	void BuildPool(EMTRollCategory Category, TArray<FMTRollPoolEntry>& OutPool) const;

private:
	UMTProgressionSubsystem* GetProgression() const;
	UMTDataRegistry* GetRegistry() const;

	bool IsOwned(EMTRollCategory Category, const FMTRollPoolEntry& Entry) const;
	void GrantEntry(EMTRollCategory Category, const FMTRollPoolEntry& Entry) const;
	FName MasteryKeyFor(EMTRollCategory Category, const FMTRollPoolEntry& Entry) const;

	/** Base tier probabilities (index = EMTRarity) over tiers present in the pool. */
	static void ComputeBaseTierProbabilities(const TArray<FMTRollPoolEntry>& Pool, const FMTRollConfig& Config, TArray<float>& OutProb);
	/** Applies soft pity to base probabilities for the given pity state. */
	static void ApplySoftPity(const FMTRollConfig& Config, int32 SpinsSinceLegendary, TArray<float>& InOutProb);
	static int32 PickIndexByWeight(const TArray<float>& Weights, float U);

	mutable FRandomStream Stream;
};

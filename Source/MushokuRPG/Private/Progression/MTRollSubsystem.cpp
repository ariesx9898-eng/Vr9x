#include "Progression/MTRollSubsystem.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameEvents.h"
#include "Save/MTSaveTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"

#define LOCTEXT_NAMESPACE "MTRoll"

namespace MTRollPrivate
{
	// Code defaults used when Content/Data/RollConfigs.json has no row (or no weight for a tier).
	static const float DefaultRarityWeights[UMTRollSubsystem::NumRarities] = { 50.f, 30.f, 14.f, 5.f, 1.f };
	static constexpr int32 LegendaryIndex = static_cast<int32>(EMTRarity::Legendary);
	static constexpr int32 MythicIndex = static_cast<int32>(EMTRarity::Mythic);

	static void GetPresentTiers(const TArray<FMTRollPoolEntry>& Pool, bool (&OutPresent)[UMTRollSubsystem::NumRarities])
	{
		for (int32 i = 0; i < UMTRollSubsystem::NumRarities; ++i)
		{
			OutPresent[i] = false;
		}
		for (const FMTRollPoolEntry& Entry : Pool)
		{
			OutPresent[FMath::Clamp(static_cast<int32>(Entry.Rarity), 0, UMTRollSubsystem::NumRarities - 1)] = true;
		}
	}

	static FText CategoryText(EMTRollCategory C)
	{
		switch (C)
		{
		case EMTRollCategory::Character: return LOCTEXT("Character", "Character");
		case EMTRollCategory::Element: return LOCTEXT("Element", "Element");
		case EMTRollCategory::Race: return LOCTEXT("Race", "Race");
		}
		return FText::GetEmpty();
	}

	static EMTMasteryTrack TrackFor(EMTRollCategory C)
	{
		switch (C)
		{
		case EMTRollCategory::Element: return EMTMasteryTrack::Element;
		case EMTRollCategory::Race: return EMTMasteryTrack::Race;
		default: return EMTMasteryTrack::Character;
		}
	}
}

// ============================================================================ Lifecycle

void UMTRollSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMTDataRegistry::StaticClass());
	Collection.InitializeDependency(UMTProgressionSubsystem::StaticClass());
	Super::Initialize(Collection);

	Stream.Initialize(static_cast<int32>(FDateTime::Now().GetTicks() & 0x7FFFFFFF));
}

UMTRollSubsystem* UMTRollSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	if (const UGameInstance* GI = Cast<UGameInstance>(WorldContext))
	{
		return GI->GetSubsystem<UMTRollSubsystem>();
	}
	if (const UGameInstanceSubsystem* Sub = Cast<UGameInstanceSubsystem>(WorldContext))
	{
		const UGameInstance* GI = Sub->GetGameInstance();
		return GI ? GI->GetSubsystem<UMTRollSubsystem>() : nullptr;
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMTRollSubsystem>() : nullptr;
}

UMTProgressionSubsystem* UMTRollSubsystem::GetProgression() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMTProgressionSubsystem>() : nullptr;
}

UMTDataRegistry* UMTRollSubsystem::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMTDataRegistry>() : nullptr;
}

// ============================================================================ Config / pool

FMTRollConfig UMTRollSubsystem::GetEffectiveConfig(EMTRollCategory Category) const
{
	FMTRollConfig Config;
	Config.Category = Category;
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		if (const FMTRollConfig* Row = Registry->FindRollConfig(Category))
		{
			Config = *Row;
		}
	}
	for (int32 i = 0; i < NumRarities; ++i)
	{
		const EMTRarity Rarity = static_cast<EMTRarity>(i);
		if (!Config.RarityWeights.Contains(Rarity))
		{
			Config.RarityWeights.Add(Rarity, MTRollPrivate::DefaultRarityWeights[i]);
		}
	}
	Config.SoftPityStart = FMath::Max(0, Config.SoftPityStart);
	Config.SoftPityStep = FMath::Max(0.f, Config.SoftPityStep);
	Config.HardPity = FMath::Max(1, Config.HardPity);
	Config.MythicHardPity = FMath::Max(1, Config.MythicHardPity);
	return Config;
}

void UMTRollSubsystem::BuildPool(EMTRollCategory Category, TArray<FMTRollPoolEntry>& OutPool) const
{
	OutPool.Reset();
	const UMTDataRegistry* Registry = GetRegistry();
	const UMTProgressionSubsystem* Progression = GetProgression();
	if (!Registry)
	{
		return;
	}

	switch (Category)
	{
	case EMTRollCategory::Character:
		for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
		{
			FMTRollPoolEntry Entry;
			Entry.Id = Pair.Key;
			Entry.Rarity = Pair.Value.Rarity;
			Entry.Weight = Pair.Value.RollWeight;
			Entry.DisplayName = Pair.Value.DisplayName.IsEmpty() ? FText::FromName(Pair.Key) : Pair.Value.DisplayName;
			OutPool.Add(Entry);
		}
		break;

	case EMTRollCategory::Element:
		for (const TPair<EMTElement, FMTElementData>& Pair : Registry->GetElements())
		{
			// Rows without abilities (e.g. a kit-only school) cannot be equipped, so they are not rollable.
			if (Pair.Key == EMTElement::None || Pair.Value.Abilities.Num() == 0)
			{
				continue;
			}
			FMTRollPoolEntry Entry;
			Entry.Id = UMTProgressionSubsystem::ElementKey(Pair.Key);
			Entry.Element = Pair.Key;
			Entry.Rarity = Pair.Value.Rarity;
			Entry.Weight = Pair.Value.RollWeight;
			Entry.DisplayName = Pair.Value.DisplayName.IsEmpty() ? FText::FromName(Entry.Id) : Pair.Value.DisplayName;
			OutPool.Add(Entry);
		}
		break;

	case EMTRollCategory::Race:
		for (const TPair<EMTRace, FMTRaceData>& Pair : Registry->GetRaces())
		{
			if (!Pair.Value.bImplemented)
			{
				continue;
			}
			FMTRollPoolEntry Entry;
			Entry.Id = Progression ? Progression->RaceKey(Pair.Key) : Pair.Value.RaceID;
			Entry.Race = Pair.Key;
			Entry.Rarity = Pair.Value.Rarity;
			Entry.Weight = Pair.Value.RollWeight;
			Entry.DisplayName = Pair.Value.DisplayName.IsEmpty() ? FText::FromName(Entry.Id) : Pair.Value.DisplayName;
			OutPool.Add(Entry);
		}
		break;
	}

	// Deterministic order: rarity (highest first), then id.
	OutPool.Sort([](const FMTRollPoolEntry& A, const FMTRollPoolEntry& B)
	{
		if (A.Rarity != B.Rarity)
		{
			return static_cast<int32>(A.Rarity) > static_cast<int32>(B.Rarity);
		}
		return A.Id.ToString().Compare(B.Id.ToString(), ESearchCase::IgnoreCase) < 0;
	});
}

bool UMTRollSubsystem::IsOwned(EMTRollCategory Category, const FMTRollPoolEntry& Entry) const
{
	const UMTProgressionSubsystem* Progression = GetProgression();
	if (!Progression)
	{
		return false;
	}
	switch (Category)
	{
	case EMTRollCategory::Character: return Progression->OwnsCharacter(Entry.Id);
	case EMTRollCategory::Element: return Progression->OwnsElement(Entry.Element);
	case EMTRollCategory::Race: return Progression->OwnsRace(Entry.Race);
	}
	return false;
}

void UMTRollSubsystem::GrantEntry(EMTRollCategory Category, const FMTRollPoolEntry& Entry) const
{
	UMTProgressionSubsystem* Progression = GetProgression();
	if (!Progression)
	{
		return;
	}
	switch (Category)
	{
	case EMTRollCategory::Character: Progression->GrantCharacter(Entry.Id); break;
	case EMTRollCategory::Element: Progression->GrantElement(Entry.Element); break;
	case EMTRollCategory::Race: Progression->GrantRace(Entry.Race); break;
	}
}

FName UMTRollSubsystem::MasteryKeyFor(EMTRollCategory Category, const FMTRollPoolEntry& Entry) const
{
	// Pool ids already use the mastery key convention (character id / element name / race id).
	return Entry.Id;
}

// ============================================================================ Probability helpers

void UMTRollSubsystem::ComputeBaseTierProbabilities(const TArray<FMTRollPoolEntry>& Pool, const FMTRollConfig& Config, TArray<float>& OutProb)
{
	OutProb.Init(0.f, NumRarities);
	bool Present[NumRarities];
	MTRollPrivate::GetPresentTiers(Pool, Present);

	float Sum = 0.f;
	int32 PresentCount = 0;
	for (int32 i = 0; i < NumRarities; ++i)
	{
		if (!Present[i])
		{
			continue;
		}
		++PresentCount;
		const float* Weight = Config.RarityWeights.Find(static_cast<EMTRarity>(i));
		OutProb[i] = FMath::Max(0.f, Weight ? *Weight : MTRollPrivate::DefaultRarityWeights[i]);
		Sum += OutProb[i];
	}

	if (PresentCount == 0)
	{
		return;
	}
	for (int32 i = 0; i < NumRarities; ++i)
	{
		if (Present[i])
		{
			// Only tiers that have entries participate; renormalise (uniform if every weight is 0).
			OutProb[i] = Sum > 0.f ? OutProb[i] / Sum : 1.f / PresentCount;
		}
	}
}

void UMTRollSubsystem::ApplySoftPity(const FMTRollConfig& Config, int32 SpinsSinceLegendary, TArray<float>& InOutProb)
{
	using namespace MTRollPrivate;

	if (SpinsSinceLegendary < Config.SoftPityStart || Config.SoftPityStep <= 0.f)
	{
		return;
	}
	const float High = InOutProb[LegendaryIndex] + InOutProb[MythicIndex];
	const float Low = 1.f - High;
	if (High <= 0.f || Low <= KINDA_SMALL_NUMBER)
	{
		return; // no Legendary+ tier, or it is already certain
	}
	const float Bonus = static_cast<float>(SpinsSinceLegendary - Config.SoftPityStart + 1) * Config.SoftPityStep;
	const float NewHigh = FMath::Min(1.f, High + Bonus);
	const float HighScale = NewHigh / High;
	const float LowScale = (1.f - NewHigh) / Low;
	for (int32 i = 0; i < NumRarities; ++i)
	{
		InOutProb[i] *= (i >= LegendaryIndex) ? HighScale : LowScale;
	}
}

int32 UMTRollSubsystem::PickIndexByWeight(const TArray<float>& Weights, float U)
{
	float Total = 0.f;
	int32 LastPositive = INDEX_NONE;
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		if (Weights[i] > 0.f)
		{
			Total += Weights[i];
			LastPositive = i;
		}
	}
	if (LastPositive == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	const float Target = U * Total;
	float Accumulated = 0.f;
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		if (Weights[i] <= 0.f)
		{
			continue;
		}
		Accumulated += Weights[i];
		if (Target < Accumulated)
		{
			return i;
		}
	}
	return LastPositive;
}

// ============================================================================ Roll

bool UMTRollSubsystem::CanRoll(EMTRollCategory Category) const
{
	const UMTProgressionSubsystem* Progression = GetProgression();
	if (!Progression || Progression->GetSpins(Category) <= 0)
	{
		return false;
	}
	TArray<FMTRollPoolEntry> Pool;
	BuildPool(Category, Pool);
	return Pool.Num() > 0;
}

FMTRollResult UMTRollSubsystem::Roll(EMTRollCategory Category)
{
	using namespace MTRollPrivate;

	FMTRollResult Result;
	Result.Category = Category;

	UMTProgressionSubsystem* Progression = GetProgression();
	if (!Progression)
	{
		return Result;
	}
	TArray<FMTRollPoolEntry> Pool;
	BuildPool(Category, Pool);
	if (Pool.Num() == 0)
	{
		UE_LOG(LogMushoku, Warning, TEXT("Roll: pool for category %d is empty"), static_cast<int32>(Category));
		return Result;
	}
	if (!Progression->ConsumeSpin(Category))
	{
		return Result;
	}

	const FMTRollConfig Config = GetEffectiveConfig(Category);
	FMTRollPityState PityState = Progression->GetPity(Category);

	bool Present[NumRarities];
	GetPresentTiers(Pool, Present);
	const bool bHasHigh = Present[LegendaryIndex] || Present[MythicIndex];
	const bool bHasMythic = Present[MythicIndex];

	TArray<float> Prob;
	ComputeBaseTierProbabilities(Pool, Config, Prob);

	// ---- 1) Rarity tier (hard pity -> guaranteed; otherwise soft-pity adjusted odds).
	int32 Tier = INDEX_NONE;
	if (bHasMythic && PityState.SpinsSinceMythic + 1 >= Config.MythicHardPity)
	{
		Tier = MythicIndex;
		Result.bPityTriggered = true;
	}
	else if (bHasHigh && PityState.SpinsSinceLegendary + 1 >= Config.HardPity)
	{
		Result.bPityTriggered = true;
		if (!Present[MythicIndex])
		{
			Tier = LegendaryIndex;
		}
		else if (!Present[LegendaryIndex])
		{
			Tier = MythicIndex;
		}
		else
		{
			const float WL = Prob[LegendaryIndex];
			const float WM = Prob[MythicIndex];
			const float U = Stream.FRand();
			Tier = (WL + WM > 0.f) ? ((U * (WL + WM) < WM) ? MythicIndex : LegendaryIndex) : ((U < 0.5f) ? MythicIndex : LegendaryIndex);
		}
	}
	else
	{
		ApplySoftPity(Config, PityState.SpinsSinceLegendary, Prob);
		Tier = PickIndexByWeight(Prob, Stream.FRand());
	}
	if (Tier == INDEX_NONE)
	{
		for (int32 i = NumRarities - 1; i >= 0; --i)
		{
			if (Present[i]) { Tier = i; break; }
		}
	}

	// ---- 2) Entry inside the tier, weighted by RollWeight.
	TArray<int32> TierEntries;
	TArray<float> TierWeights;
	for (int32 i = 0; i < Pool.Num(); ++i)
	{
		if (static_cast<int32>(Pool[i].Rarity) == Tier)
		{
			TierEntries.Add(i);
			TierWeights.Add(FMath::Max(0.f, Pool[i].Weight));
		}
	}
	if (TierEntries.Num() == 0)
	{
		// Unreachable in practice (tiers come from the pool) - refund and bail.
		Progression->AddSpins(Category, 1);
		return Result;
	}
	int32 PickInTier = PickIndexByWeight(TierWeights, Stream.FRand());
	if (PickInTier == INDEX_NONE)
	{
		PickInTier = 0; // every weight 0: first entry
	}
	int32 PoolIndex = TierEntries[PickInTier];

	// ---- 3) Duplicate protection: one re-roll inside the same tier, excluding owned entries.
	if (Config.bDuplicateProtection && IsOwned(Category, Pool[PoolIndex]))
	{
		TArray<int32> Unowned;
		TArray<float> UnownedWeights;
		for (const int32 Index : TierEntries)
		{
			if (!IsOwned(Category, Pool[Index]))
			{
				Unowned.Add(Index);
				UnownedWeights.Add(FMath::Max(0.f, Pool[Index].Weight));
			}
		}
		if (Unowned.Num() > 0)
		{
			const int32 Pick = PickIndexByWeight(UnownedWeights, Stream.FRand());
			PoolIndex = Unowned[Pick == INDEX_NONE ? 0 : Pick];
			Result.bDuplicateRerolled = true;
		}
	}
	const FMTRollPoolEntry Entry = Pool[PoolIndex];

	// ---- 4) Pity + history are written before any broadcast (listeners may touch progression maps).
	PityState.TotalSpins += 1;
	PityState.SpinsSinceLegendary = (Tier >= LegendaryIndex) ? 0 : PityState.SpinsSinceLegendary + 1;
	PityState.SpinsSinceMythic = (Tier == MythicIndex) ? 0 : PityState.SpinsSinceMythic + 1;
	Progression->GetPityMutable(Category) = PityState;

	Result.bValid = true;
	Result.ResultId = Entry.Id;
	Result.Rarity = static_cast<EMTRarity>(Tier);
	Result.DisplayName = Entry.DisplayName;
	Result.bWasNew = !IsOwned(Category, Entry);

	{
		FMTRollHistoryEntry History;
		History.Category = Category;
		History.ResultId = Entry.Id;
		History.Rarity = Result.Rarity;
		History.TimeUnix = FDateTime::UtcNow().ToUnixTimestamp();
		History.bPityTriggered = Result.bPityTriggered;
		History.bDuplicateRerolled = Result.bDuplicateRerolled;
		History.bWasNew = Result.bWasNew;
		TArray<FMTRollHistoryEntry>& HistoryList = Progression->GetRollHistoryMutable();
		HistoryList.Add(History);
		if (HistoryList.Num() > UMTProgressionSubsystem::MaxRollHistory)
		{
			HistoryList.RemoveAt(0, HistoryList.Num() - UMTProgressionSubsystem::MaxRollHistory);
		}
	}

	// ---- 5) Ownership or duplicate compensation.
	FText Message;
	if (Result.bWasNew)
	{
		GrantEntry(Category, Entry);
		Message = FText::Format(LOCTEXT("RollNew", "{0} roll: {1} ({2}) - NEW!"),
			CategoryText(Category), Entry.DisplayName, FText::FromString(MTUtil::RarityToString(Result.Rarity)));
	}
	else
	{
		Progression->AddGold(DuplicateGold);
		Progression->AddMasteryXP(TrackFor(Category), MasteryKeyFor(Category, Entry), DuplicateMasteryXP);
		Message = FText::Format(LOCTEXT("RollDupe", "{0} roll: {1} ({2}) - duplicate: +{3} gold, +{4} mastery XP"),
			CategoryText(Category), Entry.DisplayName, FText::FromString(MTUtil::RarityToString(Result.Rarity)),
			FText::AsNumber(DuplicateGold), FText::AsNumber(FMath::RoundToInt(DuplicateMasteryXP)));
	}

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UMTGameEvents* Events = GI->GetSubsystem<UMTGameEvents>())
		{
			Events->Notify(Message, MTUtil::RarityColor(Result.Rarity));
		}
	}
	UE_LOG(LogMushoku, Log, TEXT("Roll: %s (pity=%d reroll=%d new=%d)"), *Message.ToString(),
		Result.bPityTriggered ? 1 : 0, Result.bDuplicateRerolled ? 1 : 0, Result.bWasNew ? 1 : 0);
	return Result;
}

// ============================================================================ Display helpers

void UMTRollSubsystem::GetPool(EMTRollCategory Category, TArray<FName>& OutIds, TArray<EMTRarity>& OutRarities, TArray<float>& OutProbabilities) const
{
	OutIds.Reset();
	OutRarities.Reset();
	OutProbabilities.Reset();

	TArray<FMTRollPoolEntry> Pool;
	BuildPool(Category, Pool);
	if (Pool.Num() == 0)
	{
		return;
	}
	TArray<float> TierProb;
	ComputeBaseTierProbabilities(Pool, GetEffectiveConfig(Category), TierProb);

	float TierWeightSum[NumRarities] = { 0.f, 0.f, 0.f, 0.f, 0.f };
	int32 TierCount[NumRarities] = { 0, 0, 0, 0, 0 };
	for (const FMTRollPoolEntry& Entry : Pool)
	{
		const int32 Tier = static_cast<int32>(Entry.Rarity);
		TierWeightSum[Tier] += FMath::Max(0.f, Entry.Weight);
		++TierCount[Tier];
	}
	for (const FMTRollPoolEntry& Entry : Pool)
	{
		const int32 Tier = static_cast<int32>(Entry.Rarity);
		const float InTier = TierWeightSum[Tier] > 0.f ? FMath::Max(0.f, Entry.Weight) / TierWeightSum[Tier] : 1.f / FMath::Max(1, TierCount[Tier]);
		OutIds.Add(Entry.Id);
		OutRarities.Add(Entry.Rarity);
		OutProbabilities.Add(TierProb[Tier] * InTier);
	}
}

float UMTRollSubsystem::GetLegendaryPityProgress(EMTRollCategory Category) const
{
	TArray<FMTRollPoolEntry> Pool;
	BuildPool(Category, Pool);
	bool Present[NumRarities];
	MTRollPrivate::GetPresentTiers(Pool, Present);
	const UMTProgressionSubsystem* Progression = GetProgression();
	if (!Progression || !(Present[MTRollPrivate::LegendaryIndex] || Present[MTRollPrivate::MythicIndex]))
	{
		return 0.f;
	}
	const FMTRollConfig Config = GetEffectiveConfig(Category);
	return FMath::Clamp(static_cast<float>(Progression->GetPity(Category).SpinsSinceLegendary) / static_cast<float>(Config.HardPity), 0.f, 1.f);
}

int32 UMTRollSubsystem::GetSpinsUntilHardPity(EMTRollCategory Category) const
{
	TArray<FMTRollPoolEntry> Pool;
	BuildPool(Category, Pool);
	bool Present[NumRarities];
	MTRollPrivate::GetPresentTiers(Pool, Present);
	const UMTProgressionSubsystem* Progression = GetProgression();
	if (!Progression || !(Present[MTRollPrivate::LegendaryIndex] || Present[MTRollPrivate::MythicIndex]))
	{
		return INDEX_NONE;
	}
	return FMath::Max(1, GetEffectiveConfig(Category).HardPity - Progression->GetPity(Category).SpinsSinceLegendary);
}

int32 UMTRollSubsystem::GetSpinsUntilMythicPity(EMTRollCategory Category) const
{
	TArray<FMTRollPoolEntry> Pool;
	BuildPool(Category, Pool);
	bool Present[NumRarities];
	MTRollPrivate::GetPresentTiers(Pool, Present);
	const UMTProgressionSubsystem* Progression = GetProgression();
	if (!Progression || !Present[MTRollPrivate::MythicIndex])
	{
		return INDEX_NONE;
	}
	return FMath::Max(1, GetEffectiveConfig(Category).MythicHardPity - Progression->GetPity(Category).SpinsSinceMythic);
}

bool UMTRollSubsystem::IsPoolAllLegendaryPlus(EMTRollCategory Category) const
{
	TArray<FMTRollPoolEntry> Pool;
	BuildPool(Category, Pool);
	if (Pool.Num() == 0)
	{
		return false;
	}
	for (const FMTRollPoolEntry& Entry : Pool)
	{
		if (static_cast<int32>(Entry.Rarity) < MTRollPrivate::LegendaryIndex)
		{
			return false;
		}
	}
	return true;
}

FText UMTRollSubsystem::GetEntryDisplayName(EMTRollCategory Category, FName Id) const
{
	TArray<FMTRollPoolEntry> Pool;
	BuildPool(Category, Pool);
	for (const FMTRollPoolEntry& Entry : Pool)
	{
		if (Entry.Id == Id)
		{
			return Entry.DisplayName;
		}
	}
	return FText::FromName(Id);
}

bool UMTRollSubsystem::IsEntryOwned(EMTRollCategory Category, FName Id) const
{
	TArray<FMTRollPoolEntry> Pool;
	BuildPool(Category, Pool);
	for (const FMTRollPoolEntry& Entry : Pool)
	{
		if (Entry.Id == Id)
		{
			return IsOwned(Category, Entry);
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE

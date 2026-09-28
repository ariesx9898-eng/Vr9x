#include "Progression/MTProgressionSubsystem.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameEvents.h"
#include "Character/MTCharacterBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "MTProgression"

namespace MTProgressionPrivate
{
	static const FName DefaultCharacterId(TEXT("Rudeus"));
	static constexpr EMTRace DefaultRace = EMTRace::Human;
	static constexpr EMTElement DefaultElement = EMTElement::Earth;

	// Cumulative XP thresholds.
	static const float MagicRankThresholds[] = { 0.f, 300.f, 1200.f, 4000.f, 12000.f, 30000.f, 80000.f };
	static const int32 AdventurerRankThresholds[] = { 0, 100, 300, 700, 1500, 3000, 6000 };
	static constexpr int32 NumRanks = 7;

	static const FLinearColor ColorLevel(1.f, 0.82f, 0.35f);
	static const FLinearColor ColorMastery(0.75f, 0.55f, 1.f);
	static const FLinearColor ColorRank(0.45f, 0.85f, 1.f);
	static const FLinearColor ColorReward(0.95f, 0.92f, 0.85f);

	static FString TrackToString(EMTMasteryTrack Track)
	{
		switch (Track)
		{
		case EMTMasteryTrack::Character: return TEXT("Character");
		case EMTMasteryTrack::Element: return TEXT("Element");
		case EMTMasteryTrack::Race: return TEXT("Race");
		}
		return TEXT("?");
	}

	static FString RaceEnumName(EMTRace Race)
	{
		const UEnum* Enum = StaticEnum<EMTRace>();
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Race)) : FString::FromInt(static_cast<int32>(Race));
	}

	static EMTRollCategory RewardCategoryForLevel(int32 NewLevel)
	{
		// Rotate level-up spins through the three categories.
		switch (NewLevel % 3)
		{
		case 0: return EMTRollCategory::Character;
		case 1: return EMTRollCategory::Element;
		default: return EMTRollCategory::Race;
		}
	}

	static FText RollCategoryText(EMTRollCategory C)
	{
		switch (C)
		{
		case EMTRollCategory::Character: return LOCTEXT("CatCharacter", "Character");
		case EMTRollCategory::Element: return LOCTEXT("CatElement", "Element");
		case EMTRollCategory::Race: return LOCTEXT("CatRace", "Race");
		}
		return FText::GetEmpty();
	}
}

// ============================================================================ Lifecycle

void UMTProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMTDataRegistry::StaticClass());
	Collection.InitializeDependency(UMTGameEvents::StaticClass());
	Super::Initialize(Collection);

	ResetToNewGame();

	if (UMTGameEvents* Events = GetEvents())
	{
		// Boss defeats are NOT bound: AMTBossCharacter calls RecordBossDefeated itself before broadcasting.
		Events->OnEnemyKilled.AddDynamic(this, &UMTProgressionSubsystem::HandleEnemyKilled);
		Events->OnLocationReached.AddDynamic(this, &UMTProgressionSubsystem::HandleLocationReached);
	}
}

void UMTProgressionSubsystem::Deinitialize()
{
	if (UMTGameEvents* Events = GetEvents())
	{
		Events->OnEnemyKilled.RemoveDynamic(this, &UMTProgressionSubsystem::HandleEnemyKilled);
		Events->OnLocationReached.RemoveDynamic(this, &UMTProgressionSubsystem::HandleLocationReached);
	}
	Super::Deinitialize();
}

UMTProgressionSubsystem* UMTProgressionSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	if (const UGameInstance* GI = Cast<UGameInstance>(WorldContext))
	{
		return GI->GetSubsystem<UMTProgressionSubsystem>();
	}
	if (const UGameInstanceSubsystem* Sub = Cast<UGameInstanceSubsystem>(WorldContext))
	{
		const UGameInstance* GI = Sub->GetGameInstance();
		return GI ? GI->GetSubsystem<UMTProgressionSubsystem>() : nullptr;
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMTProgressionSubsystem>() : nullptr;
}

void UMTProgressionSubsystem::ResetToNewGame()
{
	using namespace MTProgressionPrivate;

	OwnedCharacters.Reset();
	OwnedElements.Reset();
	OwnedRaces.Reset();
	Loadouts.Reset();
	OwnedCharacters.Add(DefaultCharacterId);
	// LA PLACE: both lineages are playable from the start (chosen in EDIT).
	if (const UMTDataRegistry* Registry = UMTDataRegistry::Get(this))
	{
		for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
		{
			OwnedCharacters.Add(Pair.Key);
		}
	}
	OwnedRaces.Add(DefaultRace);
	OwnedElements.Add(DefaultElement);

	EquippedCharacter = DefaultCharacterId;
	EquippedRace = DefaultRace;
	EquippedElements.Init(EMTElement::None, MaxElementSlots);
	EquippedElements[0] = DefaultElement;
	UnlockedElementSlots = 1;

	Level = 1;
	XP = 0;
	Gold = 0;
	AdventurerPoints = 0;
	AdventurerRank = EMTAdventurerRank::F;
	Masteries.Reset();
	MagicRankXP.Reset();
	UnlockedAbilities.Reset();

	Spins.Reset();
	Spins.Add(EMTRollCategory::Character, 3);
	Spins.Add(EMTRollCategory::Element, 3);
	Spins.Add(EMTRollCategory::Race, 2);
	Pity.Reset();
	RollHistory.Reset();

	Inventory.Reset();
	DiscoveredLocations.Reset();
	FastTravelUnlocked.Reset();
	BossKills.Reset();
	AchievementsUnlocked.Reset();
	LastDailyChallengeUnix = 0;
	CompletedQuestIds.Reset();

	Settings = FMTSettingsSave();
}

UMTDataRegistry* UMTProgressionSubsystem::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMTDataRegistry>() : nullptr;
}

UMTGameEvents* UMTProgressionSubsystem::GetEvents() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMTGameEvents>() : nullptr;
}

void UMTProgressionSubsystem::Notify(const FText& Message, const FLinearColor& Color) const
{
	if (UMTGameEvents* Events = GetEvents())
	{
		Events->Notify(Message, Color);
	}
	UE_LOG(LogMushoku, Log, TEXT("Progression: %s"), *Message.ToString());
}

// ============================================================================ Build

EMTElement UMTProgressionSubsystem::GetEquippedElement(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= UnlockedElementSlots || !EquippedElements.IsValidIndex(SlotIndex))
	{
		return EMTElement::None;
	}
	return EquippedElements[SlotIndex];
}

bool UMTProgressionSubsystem::EquipCharacter(FName CharacterId)
{
	if (CharacterId.IsNone() || !OwnsCharacter(CharacterId))
	{
		return false;
	}
	if (EquippedCharacter == CharacterId)
	{
		return true;
	}
	EquippedCharacter = CharacterId;
	BroadcastBuildChanged();
	return true;
}

bool UMTProgressionSubsystem::EquipRace(EMTRace Race)
{
	if (!OwnsRace(Race))
	{
		return false;
	}
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		const FMTRaceData* Row = Registry->FindRace(Race);
		if (Row && !Row->bImplemented)
		{
			return false; // data exists but the race is not playable yet
		}
	}
	if (EquippedRace == Race)
	{
		return true;
	}
	EquippedRace = Race;
	BroadcastBuildChanged();
	return true;
}

bool UMTProgressionSubsystem::EquipElement(int32 SlotIndex, EMTElement Element)
{
	if (SlotIndex < 0 || SlotIndex >= FMath::Min(UnlockedElementSlots, MaxElementSlots))
	{
		return false;
	}
	if (Element == EMTElement::None)
	{
		// Slot A must always hold an element; slot B may be cleared.
		if (SlotIndex == 0)
		{
			return false;
		}
	}
	else if (!OwnsElement(Element))
	{
		return false;
	}

	if (EquippedElements.Num() < MaxElementSlots)
	{
		EquippedElements.SetNum(MaxElementSlots);
	}
	if (EquippedElements[SlotIndex] == Element)
	{
		return true;
	}

	// Same element in the other slot: swap instead of duplicating.
	const int32 OtherSlot = 1 - SlotIndex;
	if (Element != EMTElement::None && EquippedElements[OtherSlot] == Element)
	{
		if (OtherSlot == 0 && EquippedElements[SlotIndex] == EMTElement::None)
		{
			return false; // would leave slot A empty
		}
		EquippedElements[OtherSlot] = EquippedElements[SlotIndex];
	}
	EquippedElements[SlotIndex] = Element;
	BroadcastBuildChanged();
	return true;
}

void UMTProgressionSubsystem::GrantCharacter(FName Id)
{
	if (!Id.IsNone() && !OwnedCharacters.Contains(Id))
	{
		OwnedCharacters.Add(Id);
		OnBuildChanged.Broadcast();
	}
}

void UMTProgressionSubsystem::GrantElement(EMTElement E)
{
	if (E != EMTElement::None && !OwnedElements.Contains(E))
	{
		OwnedElements.Add(E);
		OnBuildChanged.Broadcast();
	}
}

void UMTProgressionSubsystem::GrantRace(EMTRace R)
{
	if (!OwnedRaces.Contains(R))
	{
		OwnedRaces.Add(R);
		OnBuildChanged.Broadcast();
	}
}

void UMTProgressionSubsystem::ApplyBuildTo(AMTCharacterBase* Character) const
{
	if (!IsValid(Character))
	{
		return;
	}
	Character->ApplyCharacterLineage(EquippedCharacter);
	Character->ApplyRace(EquippedRace);
	const int32 Slots = FMath::Clamp(UnlockedElementSlots, 1, MaxElementSlots);
	for (int32 SlotIndex = 0; SlotIndex < Slots; ++SlotIndex)
	{
		Character->ApplyElementSlot(SlotIndex, GetEquippedElement(SlotIndex));
	}
	Character->ApplyLoadout(GetLoadout(EquippedCharacter));
}

TArray<FName> UMTProgressionSubsystem::GetLoadout(FName CharacterId) const
{
	TArray<FName> Out;
	if (const TArray<FName>* Saved = Loadouts.Find(CharacterId))
	{
		Out = *Saved;
	}
	else if (const UMTDataRegistry* Registry = UMTDataRegistry::Get(this))
	{
		if (const FMTCharacterData* Row = Registry->FindCharacter(CharacterId))
		{
			Out = Row->DefaultLoadout;
		}
	}
	Out.SetNum(4);
	return Out;
}

void UMTProgressionSubsystem::SetLoadoutSlot(FName CharacterId, int32 Index, FName AbilityId)
{
	if (Index < 0 || Index > 3 || CharacterId.IsNone())
	{
		return;
	}
	TArray<FName> Loadout = GetLoadout(CharacterId);
	const int32 Existing = AbilityId.IsNone() ? INDEX_NONE : Loadout.IndexOfByKey(AbilityId);
	if (Existing != INDEX_NONE && Existing != Index)
	{
		Loadout[Existing] = Loadout[Index];
	}
	Loadout[Index] = AbilityId;
	Loadouts.Add(CharacterId, Loadout);
	BroadcastBuildChanged();
}

TArray<FName> UMTProgressionSubsystem::GetLoadoutPool(FName CharacterId) const
{
	TArray<FName> Pool;
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Registry)
	{
		return Pool;
	}
	if (const FMTCharacterData* Row = Registry->FindCharacter(CharacterId))
	{
		for (const FName& Id : Row->Abilities)
		{
			Pool.AddUnique(Id);
		}
	}
	for (const TPair<EMTElement, FMTElementData>& Pair : Registry->GetElements())
	{
		for (const FName& Id : Pair.Value.Abilities)
		{
			Pool.AddUnique(Id);
		}
	}
	return Pool;
}

void UMTProgressionSubsystem::BroadcastBuildChanged()
{
	OnBuildChanged.Broadcast();

	const UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (World && World->IsGameWorld())
	{
		if (AMTCharacterBase* Pawn = Cast<AMTCharacterBase>(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			ApplyBuildTo(Pawn);
		}
	}
}

void UMTProgressionSubsystem::SanitizeBuild()
{
	using namespace MTProgressionPrivate;

	if (OwnedCharacters.Num() == 0) { OwnedCharacters.Add(DefaultCharacterId); }
	if (OwnedRaces.Num() == 0) { OwnedRaces.Add(DefaultRace); }
	if (OwnedElements.Num() == 0) { OwnedElements.Add(DefaultElement); }
	OwnedElements.Remove(EMTElement::None);

	if (!OwnedCharacters.Contains(EquippedCharacter))
	{
		EquippedCharacter = OwnedCharacters.Contains(DefaultCharacterId) ? DefaultCharacterId : *OwnedCharacters.CreateConstIterator();
	}
	if (!OwnedRaces.Contains(EquippedRace))
	{
		EquippedRace = OwnedRaces.Contains(DefaultRace) ? DefaultRace : *OwnedRaces.CreateConstIterator();
	}

	// Second slot unlock is level driven (it may have been unlocked in an older save before the rule).
	UnlockedElementSlots = FMath::Clamp(UnlockedElementSlots, 1, MaxElementSlots);
	if (Level >= SecondElementSlotLevel)
	{
		UnlockedElementSlots = MaxElementSlots;
	}

	EquippedElements.SetNum(MaxElementSlots);
	for (int32 i = 0; i < MaxElementSlots; ++i)
	{
		if (EquippedElements[i] != EMTElement::None && !OwnedElements.Contains(EquippedElements[i]))
		{
			EquippedElements[i] = EMTElement::None;
		}
	}
	if (EquippedElements[0] == EMTElement::None)
	{
		EquippedElements[0] = OwnedElements.Contains(DefaultElement) ? DefaultElement : *OwnedElements.CreateConstIterator();
	}
	if (EquippedElements[1] == EquippedElements[0])
	{
		EquippedElements[1] = EMTElement::None;
	}
}

// ============================================================================ Level / XP

int32 UMTProgressionSubsystem::CalcXPToNextLevel(int32 InLevel)
{
	const int32 L = FMath::Max(1, InLevel);
	return FMath::Max(1, FMath::RoundToInt(100.f * FMath::Pow(static_cast<float>(L), 1.5f)));
}

void UMTProgressionSubsystem::AddXP(int32 Amount)
{
	if (Amount <= 0 || Level >= MaxLevel)
	{
		return;
	}
	XP += Amount;
	while (Level < MaxLevel && XP >= CalcXPToNextLevel(Level))
	{
		XP -= CalcXPToNextLevel(Level);
		++Level;
		OnLevelReached(Level);
	}
	if (Level >= MaxLevel)
	{
		XP = 0;
	}
}

void UMTProgressionSubsystem::OnLevelReached(int32 NewLevel)
{
	using namespace MTProgressionPrivate;

	Notify(FText::Format(LOCTEXT("LevelUp", "Level up! You are now level {0}."), FText::AsNumber(NewLevel)), ColorLevel);

	// Every level-up grants one spin, rotating through the categories.
	const EMTRollCategory RewardCategory = RewardCategoryForLevel(NewLevel);
	AddSpins(RewardCategory, 1);
	Notify(FText::Format(LOCTEXT("LevelSpin", "+1 {0} spin"), RollCategoryText(RewardCategory)), ColorReward);

	if (NewLevel >= SecondElementSlotLevel && UnlockedElementSlots < MaxElementSlots)
	{
		UnlockedElementSlots = MaxElementSlots;
		Notify(LOCTEXT("SecondSlot", "Second element slot unlocked! Equip a second element in the Element menu."), ColorLevel);
		BroadcastBuildChanged();
	}

	OnLevelUp.Broadcast(NewLevel);
}

// ============================================================================ Mastery

float UMTProgressionSubsystem::CalcMasteryXPForLevel(int32 InLevel)
{
	const int32 L = FMath::Clamp(InLevel, 0, MaxMasteryLevel);
	return 50.f * FMath::Pow(static_cast<float>(L + 1), 1.6f);
}

FMTMasterySave* UMTProgressionSubsystem::FindMastery(EMTMasteryTrack Track, FName Key)
{
	return Masteries.FindByPredicate([Track, Key](const FMTMasterySave& M) { return M.Track == Track && M.Key == Key; });
}

const FMTMasterySave* UMTProgressionSubsystem::FindMastery(EMTMasteryTrack Track, FName Key) const
{
	return Masteries.FindByPredicate([Track, Key](const FMTMasterySave& M) { return M.Track == Track && M.Key == Key; });
}

void UMTProgressionSubsystem::AddMasteryXP(EMTMasteryTrack Track, FName Key, float InXP)
{
	using namespace MTProgressionPrivate;

	if (Key.IsNone() || InXP <= 0.f)
	{
		return;
	}
	FMTMasterySave* Entry = FindMastery(Track, Key);
	if (!Entry)
	{
		FMTMasterySave NewEntry;
		NewEntry.Track = Track;
		NewEntry.Key = Key;
		Entry = &Masteries.Add_GetRef(NewEntry);
	}
	if (Entry->Level >= MaxMasteryLevel)
	{
		return;
	}

	Entry->XP += InXP;
	while (Entry->Level < MaxMasteryLevel && Entry->XP >= CalcMasteryXPForLevel(Entry->Level))
	{
		Entry->XP -= CalcMasteryXPForLevel(Entry->Level);
		++Entry->Level;

		const int32 NewLevel = Entry->Level;
		Notify(FText::Format(LOCTEXT("MasteryUp", "{0} mastery ({1}) reached level {2}/{3}"),
			FText::FromName(Key), FText::FromString(TrackToString(Track)), FText::AsNumber(NewLevel), FText::AsNumber(MaxMasteryLevel)), ColorMastery);
		OnMasteryLevelUp.Broadcast(Track, Key, NewLevel);

		// Broadcast may have added entries (array realloc) - re-resolve the pointer.
		Entry = FindMastery(Track, Key);
		if (!Entry)
		{
			return;
		}
	}
	if (Entry->Level >= MaxMasteryLevel)
	{
		Entry->XP = 0.f;
	}
}

int32 UMTProgressionSubsystem::GetMasteryLevel(EMTMasteryTrack Track, FName Key) const
{
	const FMTMasterySave* Entry = FindMastery(Track, Key);
	return Entry ? Entry->Level : 0;
}

float UMTProgressionSubsystem::GetMasteryProgress(EMTMasteryTrack Track, FName Key) const
{
	const FMTMasterySave* Entry = FindMastery(Track, Key);
	if (!Entry)
	{
		return 0.f;
	}
	if (Entry->Level >= MaxMasteryLevel)
	{
		return 1.f;
	}
	return FMath::Clamp(Entry->XP / CalcMasteryXPForLevel(Entry->Level), 0.f, 1.f);
}

// ============================================================================ Magic rank

float UMTProgressionSubsystem::GetMagicRankThreshold(EMTMagicRank Rank)
{
	return MTProgressionPrivate::MagicRankThresholds[FMath::Clamp(static_cast<int32>(Rank), 0, MTProgressionPrivate::NumRanks - 1)];
}

void UMTProgressionSubsystem::AddMagicRankXP(EMTElement Element, float InXP)
{
	using namespace MTProgressionPrivate;

	if (Element == EMTElement::None || InXP <= 0.f)
	{
		return;
	}
	const EMTMagicRank Before = GetMagicRank(Element);
	MagicRankXP.FindOrAdd(Element) += InXP;
	const EMTMagicRank After = GetMagicRank(Element);
	if (After != Before)
	{
		Notify(FText::Format(LOCTEXT("MagicRankUp", "{0} magic rank: {1}"),
			GetElementDisplayName(Element), FText::FromString(MTUtil::MagicRankToString(After))), MTUtil::ElementColor(Element));
	}
}

EMTMagicRank UMTProgressionSubsystem::GetMagicRank(EMTElement Element) const
{
	const float Value = GetMagicRankXP(Element);
	int32 Rank = 0;
	for (int32 i = 0; i < MTProgressionPrivate::NumRanks; ++i)
	{
		if (Value >= MTProgressionPrivate::MagicRankThresholds[i])
		{
			Rank = i;
		}
	}
	return static_cast<EMTMagicRank>(Rank);
}

float UMTProgressionSubsystem::GetMagicRankXP(EMTElement Element) const
{
	const float* Value = MagicRankXP.Find(Element);
	return Value ? *Value : 0.f;
}

float UMTProgressionSubsystem::GetMagicRankProgress(EMTElement Element) const
{
	const EMTMagicRank Rank = GetMagicRank(Element);
	if (Rank == EMTMagicRank::God)
	{
		return 1.f;
	}
	const float Low = GetMagicRankThreshold(Rank);
	const float High = GetMagicRankThreshold(static_cast<EMTMagicRank>(static_cast<int32>(Rank) + 1));
	return High > Low ? FMath::Clamp((GetMagicRankXP(Element) - Low) / (High - Low), 0.f, 1.f) : 1.f;
}

// ============================================================================ Adventurer rank

int32 UMTProgressionSubsystem::GetAdventurerRankThreshold(EMTAdventurerRank Rank)
{
	return MTProgressionPrivate::AdventurerRankThresholds[FMath::Clamp(static_cast<int32>(Rank), 0, MTProgressionPrivate::NumRanks - 1)];
}

void UMTProgressionSubsystem::AddAdventurerPoints(int32 Points)
{
	using namespace MTProgressionPrivate;

	if (Points <= 0)
	{
		return;
	}
	AdventurerPoints += Points;
	while (static_cast<int32>(AdventurerRank) < NumRanks - 1
		&& AdventurerPoints >= AdventurerRankThresholds[static_cast<int32>(AdventurerRank) + 1])
	{
		AdventurerRank = static_cast<EMTAdventurerRank>(static_cast<int32>(AdventurerRank) + 1);
		Notify(FText::Format(LOCTEXT("RankUp", "Adventurer rank up! You are now rank {0}. (+1 Character spin)"),
			FText::FromString(MTUtil::AdventurerRankToString(AdventurerRank))), ColorRank);
		AddSpins(EMTRollCategory::Character, 1);
		OnAdventurerRankUp.Broadcast(AdventurerRank);
	}
}

// ============================================================================ Spins

int32 UMTProgressionSubsystem::GetSpins(EMTRollCategory C) const
{
	const int32* Value = Spins.Find(C);
	return Value ? *Value : 0;
}

void UMTProgressionSubsystem::AddSpins(EMTRollCategory C, int32 N)
{
	if (N == 0)
	{
		return;
	}
	int32& Value = Spins.FindOrAdd(C);
	Value = FMath::Max(0, Value + N);
	OnSpinsChanged.Broadcast();
}

bool UMTProgressionSubsystem::ConsumeSpin(EMTRollCategory C)
{
	int32* Value = Spins.Find(C);
	if (!Value || *Value <= 0)
	{
		return false;
	}
	--(*Value);
	OnSpinsChanged.Broadcast();
	return true;
}

// ============================================================================ Inventory / gold

void UMTProgressionSubsystem::AddItem(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0)
	{
		return;
	}
	int32 MaxStack = MAX_int32;
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		if (const FMTItemData* Item = Registry->FindItem(ItemId))
		{
			MaxStack = Item->MaxStack > 0 ? Item->MaxStack : MAX_int32;
		}
	}
	int32& Value = Inventory.FindOrAdd(ItemId);
	Value = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Value) + Count, MaxStack));
	OnInventoryChanged.Broadcast();
}

bool UMTProgressionSubsystem::RemoveItem(FName ItemId, int32 Count)
{
	int32* Value = Inventory.Find(ItemId);
	if (Count <= 0 || !Value || *Value < Count)
	{
		return false;
	}
	*Value -= Count;
	if (*Value <= 0)
	{
		Inventory.Remove(ItemId);
	}
	OnInventoryChanged.Broadcast();
	return true;
}

int32 UMTProgressionSubsystem::GetItemCount(FName ItemId) const
{
	const int32* Value = Inventory.Find(ItemId);
	return Value ? *Value : 0;
}

void UMTProgressionSubsystem::AddGold(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	Gold = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Gold) + Amount, MAX_int32));
	OnInventoryChanged.Broadcast();
}

bool UMTProgressionSubsystem::SpendGold(int32 Amount)
{
	if (Amount < 0 || Gold < Amount)
	{
		return false;
	}
	Gold -= Amount;
	OnInventoryChanged.Broadcast();
	return true;
}

// ============================================================================ World

void UMTProgressionSubsystem::DiscoverLocation(FName LocationId)
{
	if (LocationId.IsNone() || DiscoveredLocations.Contains(LocationId))
	{
		return;
	}
	DiscoveredLocations.Add(LocationId);

	FText Name = FText::FromName(LocationId);
	bool bFastTravel = false;
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		if (const FMTLocationData* Row = Registry->FindLocation(LocationId))
		{
			Name = Row->DisplayName.IsEmpty() ? Name : Row->DisplayName;
			bFastTravel = Row->bFastTravel;
		}
	}
	if (bFastTravel)
	{
		FastTravelUnlocked.AddUnique(LocationId);
	}
	Notify(bFastTravel
		? FText::Format(LOCTEXT("DiscoveredFT", "Discovered {0} (fast travel unlocked)"), Name)
		: FText::Format(LOCTEXT("Discovered", "Discovered {0}"), Name), MTProgressionPrivate::ColorReward);
}

void UMTProgressionSubsystem::RecordBossDefeated(FName BossId)
{
	if (BossId.IsNone())
	{
		return;
	}
	++BossKills.FindOrAdd(BossId);
}

int32 UMTProgressionSubsystem::GetBossKills(FName BossId) const
{
	const int32* Kills = BossKills.Find(BossId);
	return Kills ? *Kills : 0;
}

void UMTProgressionSubsystem::MarkQuestCompleted(FName QuestId)
{
	if (!QuestId.IsNone())
	{
		CompletedQuestIds.Add(QuestId);
	}
}

void UMTProgressionSubsystem::HandleEnemyKilled(FName EnemyId, FGameplayTagContainer EnemyTags, AActor* Killer)
{
	if (!bGrantKillXP || !Killer)
	{
		return;
	}
	const UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	APawn* PlayerPawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	// Credit the player for their own kills and for kills by their spawned actors (projectiles, zones).
	if (!PlayerPawn || (Killer != PlayerPawn && Killer->GetOwner() != PlayerPawn && Killer->GetInstigator() != PlayerPawn))
	{
		return;
	}
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		if (const FMTEnemyData* Enemy = Registry->FindEnemy(EnemyId))
		{
			AddXP(Enemy->XPReward);
		}
	}
}

void UMTProgressionSubsystem::HandleLocationReached(FName LocationId)
{
	DiscoverLocation(LocationId);
}

// ============================================================================ Ability unlocks

FName UMTProgressionSubsystem::RaceKey(EMTRace Race) const
{
	return FName(*MTProgressionPrivate::RaceEnumName(Race));
}

FText UMTProgressionSubsystem::GetCharacterDisplayName(FName CharacterId) const
{
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		if (const FMTCharacterData* Row = Registry->FindCharacter(CharacterId))
		{
			if (!Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName;
			}
		}
	}
	return FText::FromName(CharacterId);
}

FText UMTProgressionSubsystem::GetRaceDisplayName(EMTRace Race) const
{
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		if (const FMTRaceData* Row = Registry->FindRace(Race))
		{
			if (!Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName;
			}
		}
	}
	return FText::FromName(RaceKey(Race));
}

FText UMTProgressionSubsystem::GetElementDisplayName(EMTElement Element) const
{
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		if (const FMTElementData* Row = Registry->FindElement(Element))
		{
			if (!Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName;
			}
		}
	}
	return FText::FromString(MTUtil::ElementToString(Element));
}

EMTMasteryTrack UMTProgressionSubsystem::InferTrackFromKey(const FString& Key) const
{
	const UMTDataRegistry* Registry = GetRegistry();
	if (Registry && Registry->FindCharacter(FName(*Key)))
	{
		return EMTMasteryTrack::Character;
	}
	for (int32 i = 0; i <= static_cast<int32>(EMTElement::Arcane); ++i)
	{
		if (Key.Equals(MTUtil::ElementToString(static_cast<EMTElement>(i)), ESearchCase::IgnoreCase))
		{
			return EMTMasteryTrack::Element;
		}
	}
	for (int32 i = 0; i <= static_cast<int32>(EMTRace::SeaRace); ++i)
	{
		if (Key.Equals(RaceKey(static_cast<EMTRace>(i)).ToString(), ESearchCase::IgnoreCase))
		{
			return EMTMasteryTrack::Race;
		}
	}
	return EMTMasteryTrack::Character;
}

bool UMTProgressionSubsystem::GetAbilityMasteryTrack(const FMTAbilityData& Ability, EMTMasteryTrack& OutTrack, FName& OutKey) const
{
	const FName Id = Ability.AbilityID;
	const UMTDataRegistry* Registry = GetRegistry();

	// 1) Character abilities.
	if (!Ability.CharacterRequirement.IsNone())
	{
		OutTrack = EMTMasteryTrack::Character;
		OutKey = Ability.CharacterRequirement;
		return true;
	}
	if (Registry && !Id.IsNone())
	{
		for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
		{
			const FMTCharacterData& Row = Pair.Value;
			if (Row.BasicAbility == Id || Row.SpecialAbility == Id || Row.AwakeningAbility == Id || Row.Abilities.Contains(Id))
			{
				OutTrack = EMTMasteryTrack::Character;
				OutKey = Pair.Key;
				return true;
			}
		}
	}

	// 2) Race abilities.
	if (Ability.bRequiresRace)
	{
		OutTrack = EMTMasteryTrack::Race;
		OutKey = RaceKey(Ability.RaceRequirement);
		return true;
	}
	if (Registry && !Id.IsNone())
	{
		for (const TPair<EMTRace, FMTRaceData>& Pair : Registry->GetRaces())
		{
			if (Pair.Value.ActiveAbility == Id || Pair.Value.TransformationAbility == Id)
			{
				OutTrack = EMTMasteryTrack::Race;
				OutKey = RaceKey(Pair.Key);
				return true;
			}
		}
	}

	// 3) Element abilities.
	if (Registry && !Id.IsNone())
	{
		for (const TPair<EMTElement, FMTElementData>& Pair : Registry->GetElements())
		{
			if (Pair.Value.Abilities.Contains(Id))
			{
				OutTrack = EMTMasteryTrack::Element;
				OutKey = ElementKey(Pair.Key);
				return true;
			}
		}
	}
	const EMTElement Element = Ability.ElementRequirement != EMTElement::None ? Ability.ElementRequirement : Ability.Element;
	if (Element != EMTElement::None)
	{
		OutTrack = EMTMasteryTrack::Element;
		OutKey = ElementKey(Element);
		return true;
	}
	return false;
}

bool UMTProgressionSubsystem::EvaluateUnlock(const FMTAbilityData& Ability, FString* OutReason) const
{
	TArray<FString> Missing;

	// ---- UnlockRequirement clauses.
	FString Requirement = Ability.UnlockRequirement;
	Requirement.ReplaceInline(TEXT(";"), TEXT(","));
	Requirement.ReplaceInline(TEXT("&"), TEXT(","));
	TArray<FString> Clauses;
	Requirement.ParseIntoArray(Clauses, TEXT(","), true);

	for (FString Clause : Clauses)
	{
		Clause.TrimStartAndEndInline();
		if (Clause.IsEmpty())
		{
			continue;
		}
		TArray<FString> Parts;
		Clause.ParseIntoArray(Parts, TEXT(":"), true);
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
		}
		if (Parts.Num() == 0)
		{
			continue;
		}

		const FString& Kind = Parts[0];
		if (Kind.Equals(TEXT("Level"), ESearchCase::IgnoreCase) && Parts.Num() >= 2)
		{
			const int32 Needed = FCString::Atoi(*Parts[1]);
			if (Level < Needed)
			{
				Missing.Add(FString::Printf(TEXT("Level %d"), Needed));
			}
		}
		else if (Kind.Equals(TEXT("Quest"), ESearchCase::IgnoreCase) && Parts.Num() >= 2)
		{
			const FName QuestId(*Parts[1]);
			if (!IsQuestCompleted(QuestId))
			{
				FString QuestName = Parts[1];
				if (const UMTDataRegistry* Registry = GetRegistry())
				{
					if (const FMTQuestData* Quest = Registry->FindQuest(QuestId))
					{
						QuestName = Quest->Title.IsEmpty() ? QuestName : Quest->Title.ToString();
					}
				}
				Missing.Add(FString::Printf(TEXT("Quest \"%s\""), *QuestName));
			}
		}
		else if (Kind.Equals(TEXT("Mastery"), ESearchCase::IgnoreCase) && Parts.Num() >= 3)
		{
			EMTMasteryTrack Track;
			FString Key;
			int32 Needed;
			if (Parts.Num() >= 4)
			{
				Key = Parts[2];
				Needed = FCString::Atoi(*Parts[3]);
				if (Parts[1].Equals(TEXT("Character"), ESearchCase::IgnoreCase)) { Track = EMTMasteryTrack::Character; }
				else if (Parts[1].Equals(TEXT("Element"), ESearchCase::IgnoreCase)) { Track = EMTMasteryTrack::Element; }
				else if (Parts[1].Equals(TEXT("Race"), ESearchCase::IgnoreCase)) { Track = EMTMasteryTrack::Race; }
				else { Track = InferTrackFromKey(Key); }
			}
			else
			{
				Key = Parts[1];
				Needed = FCString::Atoi(*Parts[2]);
				Track = InferTrackFromKey(Key);
			}
			if (GetMasteryLevel(Track, FName(*Key)) < Needed)
			{
				Missing.Add(FString::Printf(TEXT("%s mastery %d"), *Key, Needed));
			}
		}
		else
		{
			UE_LOG(LogMushoku, Warning, TEXT("Progression: unknown unlock requirement '%s' on %s (treated as met)"),
				*Clause, *Ability.AbilityID.ToString());
		}
	}

	// ---- Mastery of the ability's own track.
	if (Ability.MasteryRequirement > 0)
	{
		EMTMasteryTrack Track;
		FName Key;
		if (GetAbilityMasteryTrack(Ability, Track, Key) && GetMasteryLevel(Track, Key) < Ability.MasteryRequirement)
		{
			Missing.Add(FString::Printf(TEXT("%s mastery %d"), *Key.ToString(), Ability.MasteryRequirement));
		}
	}

	// ---- Magic rank of the ability's element.
	if (Ability.RankRequirement != EMTMagicRank::Beginner)
	{
		const EMTElement Element = Ability.ElementRequirement != EMTElement::None ? Ability.ElementRequirement : Ability.Element;
		if (Element != EMTElement::None && GetMagicRank(Element) < Ability.RankRequirement)
		{
			Missing.Add(FString::Printf(TEXT("%s rank %s"), *MTUtil::ElementToString(Element), *MTUtil::MagicRankToString(Ability.RankRequirement)));
		}
	}

	if (OutReason)
	{
		*OutReason = Missing.Num() > 0 ? FString(TEXT("Requires ")) + FString::Join(Missing, TEXT(", ")) : FString();
	}
	return Missing.Num() == 0;
}

bool UMTProgressionSubsystem::IsAbilityUnlocked(const FMTAbilityData& Ability) const
{
	// LA PLACE: every ability can be equipped and used (the loadout is the player's choice); mastery still levels up
	// and is shown, it just no longer locks anything. EvaluateUnlock remains for the requirement text.
	return true;
}

FText UMTProgressionSubsystem::GetAbilityLockReason(const FMTAbilityData& Ability) const
{
	FString Reason;
	EvaluateUnlock(Ability, &Reason);
	return FText::FromString(Reason);
}

// ============================================================================ Settings

void UMTProgressionSubsystem::SetSettings(const FMTSettingsSave& S)
{
	Settings = S;
	Settings.MouseSensitivity = FMath::Clamp(Settings.MouseSensitivity, 0.05f, 10.f);
	Settings.GamepadSensitivity = FMath::Clamp(Settings.GamepadSensitivity, 0.05f, 10.f);
	Settings.FieldOfView = FMath::Clamp(Settings.FieldOfView, 60.f, 120.f);
	Settings.CameraShakeScale = FMath::Clamp(Settings.CameraShakeScale, 0.f, 1.f);
	Settings.MasterVolume = FMath::Clamp(Settings.MasterVolume, 0.f, 1.f);
	Settings.GraphicsQuality = FMath::Clamp(Settings.GraphicsQuality, 0, 3);
}

// ============================================================================ Save

void UMTProgressionSubsystem::WriteToSave(FMTSaveData& Out) const
{
	Out.EquippedCharacter = EquippedCharacter;
	Out.EquippedRace = EquippedRace;
	Out.EquippedElements = EquippedElements;
	Out.UnlockedElementSlots = UnlockedElementSlots;
	Out.Loadouts.Reset();
	for (const TPair<FName, TArray<FName>>& Pair : Loadouts)
	{
		FMTLoadoutSave& Entry = Out.Loadouts.AddDefaulted_GetRef();
		Entry.CharacterId = Pair.Key;
		Entry.Abilities = Pair.Value;
	}
	Out.OwnedCharacters = OwnedCharacters.Array();
	Out.OwnedElements = OwnedElements.Array();
	Out.OwnedRaces = OwnedRaces.Array();

	Out.Level = Level;
	Out.XP = XP;
	Out.Gold = Gold;
	Out.AdventurerPoints = AdventurerPoints;
	Out.AdventurerRank = AdventurerRank;
	Out.Masteries = Masteries;
	Out.MagicRankXP = MagicRankXP;
	Out.UnlockedAbilities = UnlockedAbilities;

	Out.Spins = Spins;
	Out.Pity = Pity;
	Out.RollHistory = RollHistory;

	Out.Inventory = Inventory;
	Out.DiscoveredLocations = DiscoveredLocations.Array();
	Out.FastTravelUnlocked = FastTravelUnlocked;
	Out.BossKills = BossKills;
	Out.AchievementsUnlocked = AchievementsUnlocked;
	Out.LastDailyChallengeUnix = LastDailyChallengeUnix;

	Out.Settings = Settings;
}

void UMTProgressionSubsystem::ReadFromSave(const FMTSaveData& In)
{
	EquippedCharacter = In.EquippedCharacter;
	EquippedRace = In.EquippedRace;
	EquippedElements = In.EquippedElements;
	UnlockedElementSlots = In.UnlockedElementSlots;
	OwnedCharacters.Reset();
	for (const FName& Id : In.OwnedCharacters) { OwnedCharacters.Add(Id); }
	Loadouts.Reset();
	for (const FMTLoadoutSave& Entry : In.Loadouts)
	{
		if (!Entry.CharacterId.IsNone())
		{
			Loadouts.Add(Entry.CharacterId, Entry.Abilities);
		}
	}
	// Saves from before LA PLACE only owned Rudeus: every lineage is playable now.
	if (const UMTDataRegistry* Registry = UMTDataRegistry::Get(this))
	{
		for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
		{
			OwnedCharacters.Add(Pair.Key);
		}
	}
	OwnedElements.Reset();
	for (const EMTElement E : In.OwnedElements) { OwnedElements.Add(E); }
	OwnedRaces.Reset();
	for (const EMTRace R : In.OwnedRaces) { OwnedRaces.Add(R); }

	Level = FMath::Clamp(In.Level, 1, MaxLevel);
	XP = FMath::Max(0, In.XP);
	Gold = FMath::Max(0, In.Gold);
	AdventurerPoints = FMath::Max(0, In.AdventurerPoints);
	AdventurerRank = In.AdventurerRank;
	Masteries = In.Masteries;
	for (FMTMasterySave& M : Masteries)
	{
		M.Level = FMath::Clamp(M.Level, 0, MaxMasteryLevel);
		M.XP = FMath::Max(0.f, M.XP);
	}
	MagicRankXP = In.MagicRankXP;
	UnlockedAbilities = In.UnlockedAbilities;

	Spins = In.Spins;
	Pity = In.Pity;
	RollHistory = In.RollHistory;
	if (RollHistory.Num() > MaxRollHistory)
	{
		RollHistory.RemoveAt(0, RollHistory.Num() - MaxRollHistory);
	}

	Inventory = In.Inventory;
	DiscoveredLocations.Reset();
	for (const FName& Id : In.DiscoveredLocations) { DiscoveredLocations.Add(Id); }
	FastTravelUnlocked = In.FastTravelUnlocked;
	BossKills = In.BossKills;
	AchievementsUnlocked = In.AchievementsUnlocked;
	LastDailyChallengeUnix = In.LastDailyChallengeUnix;

	// Completed quests are mirrored from the quest save so unlock checks work before the quest system loads.
	CompletedQuestIds.Reset();
	for (const FMTQuestSaveState& Quest : In.Quests)
	{
		if (Quest.bCompleted || Quest.TimesCompleted > 0)
		{
			CompletedQuestIds.Add(Quest.QuestId);
		}
	}

	SetSettings(In.Settings);
	SanitizeBuild();

	BroadcastBuildChanged();
	OnInventoryChanged.Broadcast();
	OnSpinsChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE

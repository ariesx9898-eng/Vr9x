// AMTHUD menus: journal pages, NPC quest dialog and the roll screen (split from MTHUD.cpp for size).
#include "UI/MTHUD.h"
#include "Abilities/MTAbilityComponent.h"
#include "Character/MTCharacterBase.h"
#include "Core/MTDataRegistry.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Progression/MTRollSubsystem.h"
#include "Quests/MTQuestSubsystem.h"
#include "Save/MTSaveSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "MTHUDMenus"

namespace MTHUDMenusPrivate
{
	struct FPageInfo
	{
		EMTMenuPage Page;
		const TCHAR* Label;
	};

	static const FPageInfo Pages[] =
	{
		{ EMTMenuPage::Character, TEXT("CHARACTER") },
		{ EMTMenuPage::Element, TEXT("ELEMENT") },
		{ EMTMenuPage::Race, TEXT("RACE") },
		{ EMTMenuPage::Mastery, TEXT("MASTERY") },
		{ EMTMenuPage::Inventory, TEXT("INVENTORY") },
		{ EMTMenuPage::Quests, TEXT("QUESTS") },
		{ EMTMenuPage::Map, TEXT("MAP") },
		{ EMTMenuPage::Party, TEXT("PARTY") },
		{ EMTMenuPage::Settings, TEXT("SETTINGS") },
		{ EMTMenuPage::Roll, TEXT("ROLL") },
	};

	static FName MakeId(const FString& Str) { return FName(*Str); }

	static FString CategoryLabel(EMTRollCategory C)
	{
		switch (C)
		{
		case EMTRollCategory::Character: return TEXT("CHARACTER");
		case EMTRollCategory::Element: return TEXT("ELEMENT");
		case EMTRollCategory::Race: return TEXT("RACE");
		}
		return TEXT("?");
	}

	static FString QuestTypeLabel(EMTQuestType Type)
	{
		switch (Type)
		{
		case EMTQuestType::Basic: return TEXT("Basic");
		case EMTQuestType::Combat: return TEXT("Combat");
		case EMTQuestType::Elite: return TEXT("Elite");
		case EMTQuestType::Boss: return TEXT("Boss");
		case EMTQuestType::Story: return TEXT("Story");
		case EMTQuestType::WorldEvent: return TEXT("World Event");
		}
		return TEXT("Quest");
	}

	static FString RewardSummary(const FMTQuestReward& Reward)
	{
		TArray<FString> Parts;
		if (Reward.XP > 0) { Parts.Add(FString::Printf(TEXT("%d XP"), Reward.XP)); }
		if (Reward.Gold > 0) { Parts.Add(FString::Printf(TEXT("%d gold"), Reward.Gold)); }
		if (Reward.AdventurerPoints > 0) { Parts.Add(FString::Printf(TEXT("%d adventurer pts"), Reward.AdventurerPoints)); }
		if (Reward.CharacterSpins > 0) { Parts.Add(FString::Printf(TEXT("+%d Character spin"), Reward.CharacterSpins)); }
		if (Reward.ElementSpins > 0) { Parts.Add(FString::Printf(TEXT("+%d Element spin"), Reward.ElementSpins)); }
		if (Reward.RaceSpins > 0) { Parts.Add(FString::Printf(TEXT("+%d Race spin"), Reward.RaceSpins)); }
		if (Reward.MasteryXP > 0.f) { Parts.Add(FString::Printf(TEXT("%d mastery XP"), FMath::RoundToInt(Reward.MasteryXP))); }
		for (const TPair<FName, int32>& Item : Reward.Items)
		{
			Parts.Add(FString::Printf(TEXT("%s x%d"), *Item.Key.ToString(), Item.Value));
		}
		return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : FString(TEXT("-"));
	}

	static const TCHAR* GraphicsLabel(int32 Quality)
	{
		static const TCHAR* Labels[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
		return Labels[FMath::Clamp(Quality, 0, 3)];
	}
}

// ============================================================================ Page switching

void AMTHUD::OpenPageInternal(EMTMenuPage Page)
{
	if (OpenPage == EMTMenuPage::Roll && Page != EMTMenuPage::Roll)
	{
		FinishRollAnimation();
	}
	OpenPage = Page;
	FocusedButton = NAME_None;
}

FString AMTHUD::GetQuestTitle(FName QuestId) const
{
	if (const UMTDataRegistry* Registry = UMTDataRegistry::Get(this))
	{
		if (const FMTQuestData* Quest = Registry->FindQuest(QuestId))
		{
			if (!Quest->Title.IsEmpty())
			{
				return Quest->Title.ToString();
			}
		}
	}
	return QuestId.ToString();
}

// ============================================================================ Frame

void AMTHUD::DrawMenu()
{
	const float SW = Canvas->ClipX;
	const float SH = Canvas->ClipY;
	FillRect(0.f, 0.f, SW, SH, FLinearColor(0.f, 0.f, 0.02f, 0.55f));

	const float Margin = FMath::Min(Sc(60.f), SW * 0.04f);
	const float X = Margin;
	const float Y = Sc(36.f);
	const float W = SW - 2.f * Margin;
	const float H = SH - Sc(72.f);
	DrawPanelBox(X, Y, W, H, 0.9f, true);

	// Header: title + progression summary.
	DrawStr(TEXT("ADVENTURER'S JOURNAL"), X + Sc(24.f), Y + Sc(16.f), Gold(), LargeFont(), UIScale * 0.7f);
	if (const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this))
	{
		const FString Stats = FString::Printf(TEXT("Lv %d     XP %d / %d     Gold %d     Adventurer Rank %s (%d pts)"),
			Prog->GetLevel(), Prog->GetXP(), Prog->GetXPToNextLevel(), Prog->GetGold(),
			*MTUtil::AdventurerRankToString(Prog->GetAdventurerRank()), Prog->GetAdventurerPoints());
		DrawStr(Stats, X + W - Sc(24.f), Y + Sc(18.f), Parchment(), SmallFont(), UIScale * 1.05f, 1.f, 0.f);
		const float XPFrac = SafeFraction(static_cast<float>(Prog->GetXP()), static_cast<float>(Prog->GetXPToNextLevel()));
		DrawMeter(X + W - Sc(24.f) - Sc(320.f), Y + Sc(44.f), Sc(320.f), Sc(5.f), XPFrac, XPFrac, Gold(), Gold());
	}

	// Tabs.
	const int32 NumTabs = UE_ARRAY_COUNT(MTHUDMenusPrivate::Pages);
	const float TabY = Y + Sc(62.f);
	const float TabH = Sc(34.f);
	const float TabGap = Sc(6.f);
	const float TabW = (W - Sc(48.f) - TabGap * (NumTabs - 1)) / NumTabs;
	for (int32 i = 0; i < NumTabs; ++i)
	{
		const MTHUDMenusPrivate::FPageInfo& Info = MTHUDMenusPrivate::Pages[i];
		DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("Tab:%d"), static_cast<int32>(Info.Page))), Info.Label,
			X + Sc(24.f) + i * (TabW + TabGap), TabY, TabW, TabH, true, OpenPage == Info.Page);
	}

	// Content.
	const float CX = X + Sc(24.f);
	const float CY = TabY + TabH + Sc(18.f);
	const float CW = W - Sc(48.f);
	const float CH = Y + H - CY - Sc(42.f);
	switch (OpenPage)
	{
	case EMTMenuPage::Character: DrawCharacterPage(CX, CY, CW, CH); break;
	case EMTMenuPage::Element: DrawElementPage(CX, CY, CW, CH); break;
	case EMTMenuPage::Race: DrawRacePage(CX, CY, CW, CH); break;
	case EMTMenuPage::Mastery: DrawMasteryPage(CX, CY, CW, CH); break;
	case EMTMenuPage::Inventory: DrawInventoryPage(CX, CY, CW, CH); break;
	case EMTMenuPage::Quests: DrawQuestsPage(CX, CY, CW, CH); break;
	case EMTMenuPage::Map: DrawMapPage(CX, CY, CW, CH); break;
	case EMTMenuPage::Party: DrawPartyPage(CX, CY, CW, CH); break;
	case EMTMenuPage::Settings: DrawSettingsPage(CX, CY, CW, CH); break;
	case EMTMenuPage::Roll: DrawRollPage(CX, CY, CW, CH); break;
	default: break;
	}

	DrawStr(TEXT("Esc: close     Tab keys: switch page     Arrows / stick: move     Enter / click: select"),
		X + W * 0.5f, Y + H - Sc(20.f), Dim(), SmallFont(), UIScale, 0.5f, 0.5f);
}

float AMTHUD::DrawAbilityLine(FName AbilityId, const FString& KeyLabel, float X, float Y, float W)
{
	if (AbilityId.IsNone())
	{
		return 0.f;
	}
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	const FMTAbilityData* Ability = Registry ? Registry->FindAbility(AbilityId) : nullptr;

	const float RowH = Sc(42.f);
	FillRect(X, Y, W, RowH - Sc(4.f), PanelColor(0.55f));
	StrokeRect(X, Y, W, RowH - Sc(4.f), Gold(0.18f));
	DrawStr(KeyLabel, X + Sc(10.f), Y + (RowH - Sc(4.f)) * 0.5f, Gold(), SmallFont(), UIScale, 0.f, 0.5f);

	if (!Ability)
	{
		DrawStr(AbilityId.ToString(), X + Sc(60.f), Y + Sc(10.f), Dim(), SmallFont(), UIScale);
		return RowH;
	}
	const FLinearColor ElemColor = Ability->Element == EMTElement::None ? Parchment() : MTUtil::ElementColor(Ability->Element);
	DrawStr(Ability->DisplayName.IsEmpty() ? AbilityId.ToString() : Ability->DisplayName.ToString(), X + Sc(60.f), Y + Sc(3.f), ElemColor, SmallFont(), UIScale * 1.05f);
	const FString Sub = FString::Printf(TEXT("%s   |   %d mana   |   %.1fs cooldown%s"),
		*MTUtil::ElementToString(Ability->Element), FMath::RoundToInt(Ability->ManaCost), Ability->Cooldown,
		Ability->bChargeable ? TEXT("   |   hold to charge") : TEXT(""));
	DrawStr(Sub, X + Sc(60.f), Y + Sc(20.f), Dim(), SmallFont(), UIScale * 0.85f);

	const bool bUnlocked = !Prog || Prog->IsAbilityUnlocked(*Ability);
	const FString Status = bUnlocked ? FString(TEXT("UNLOCKED")) : Prog->GetAbilityLockReason(*Ability).ToString();
	DrawStr(FitStr(Status, W * 0.42f, SmallFont(), UIScale * 0.9f), X + W - Sc(10.f), Y + (RowH - Sc(4.f)) * 0.5f,
		bUnlocked ? Good() : Bad(), SmallFont(), UIScale * 0.9f, 1.f, 0.5f);
	return RowH;
}

// ============================================================================ CHARACTER

void AMTHUD::DrawCharacterPage(float X, float Y, float W, float H)
{
	UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Prog || !Registry)
	{
		DrawStr(TEXT("Character data unavailable."), X, Y, Dim(), SmallFont(), UIScale);
		return;
	}

	TArray<FName> Owned;
	for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
	{
		if (Prog->OwnsCharacter(Pair.Key))
		{
			Owned.Add(Pair.Key);
		}
	}
	Owned.Sort([Registry](const FName& A, const FName& B)
	{
		const FMTCharacterData* DA = Registry->FindCharacter(A);
		const FMTCharacterData* DB = Registry->FindCharacter(B);
		const int32 RA = DA ? static_cast<int32>(DA->Rarity) : 0;
		const int32 RB = DB ? static_cast<int32>(DB->Rarity) : 0;
		return RA != RB ? RA > RB : A.LexicalLess(B);
	});
	if (!Owned.Contains(SelCharacter))
	{
		SelCharacter = Owned.Contains(Prog->GetEquippedCharacter()) ? Prog->GetEquippedCharacter() : (Owned.Num() > 0 ? Owned[0] : NAME_None);
	}

	const float ListW = FMath::Min(Sc(330.f), W * 0.3f);
	DrawStr(TEXT("OWNED CHARACTERS"), X, Y, Gold(), SmallFont(), UIScale);
	float LY = Y + Sc(26.f);
	for (const FName& Id : Owned)
	{
		const FMTCharacterData* Data = Registry->FindCharacter(Id);
		const bool bEquipped = Prog->GetEquippedCharacter() == Id;
		const FString Label = (Data && !Data->DisplayName.IsEmpty() ? Data->DisplayName.ToString() : Id.ToString()) + (bEquipped ? TEXT("   [EQUIPPED]") : TEXT(""));
		DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("SelChar:") + Id.ToString()), Label, X, LY, ListW, Sc(38.f), true, Id == SelCharacter,
			Data ? MTUtil::RarityColor(Data->Rarity) : Gold());
		LY += Sc(44.f);
	}
	DrawWrapped(TEXT("More characters come from Character rolls (ROLL tab). Spins are earned through quests, bosses and level-ups."),
		X, LY + Sc(8.f), ListW, Dim(), SmallFont(), UIScale * 0.9f, 4);

	const FMTCharacterData* Data = Registry->FindCharacter(SelCharacter);
	if (!Data)
	{
		return;
	}
	const float DX = X + ListW + Sc(28.f);
	const float DW = W - ListW - Sc(28.f);
	float DY = Y;

	DrawStr(Data->DisplayName.IsEmpty() ? SelCharacter.ToString() : Data->DisplayName.ToString(), DX, DY, MTUtil::RarityColor(Data->Rarity), LargeFont(), UIScale * 0.75f);
	DY += LineHeight(LargeFont(), UIScale * 0.75f) + Sc(2.f);
	DrawStr(Data->Title.ToString() + TEXT("     ") + MTUtil::RarityToString(Data->Rarity).ToUpper(), DX, DY, Gold(), SmallFont(), UIScale);
	DY += LineHeight(SmallFont(), UIScale) + Sc(8.f);
	DY += DrawWrapped(Data->Description.ToString(), DX, DY, DW, Parchment(), SmallFont(), UIScale, 4) + Sc(10.f);

	DrawStr(TEXT("PASSIVE: ") + Data->Passive.ToString(), DX, DY, Gold(), SmallFont(), UIScale);
	DY += LineHeight(SmallFont(), UIScale) + Sc(2.f);
	DY += DrawWrapped(Data->PassiveDescription.ToString(), DX, DY, DW, Parchment(0.9f), SmallFont(), UIScale, 3) + Sc(10.f);

	const int32 MasteryLevel = Prog->GetMasteryLevel(EMTMasteryTrack::Character, SelCharacter);
	const float MasteryProgress = Prog->GetMasteryProgress(EMTMasteryTrack::Character, SelCharacter);
	DrawStr(FString::Printf(TEXT("MASTERY  %d / %d"), MasteryLevel, UMTProgressionSubsystem::MaxMasteryLevel), DX, DY, Gold(), SmallFont(), UIScale);
	DrawMeter(DX + Sc(170.f), DY + Sc(4.f), Sc(260.f), Sc(8.f), MasteryProgress, MasteryProgress, Gold(), Gold());
	DY += Sc(26.f);

	DrawStr(TEXT("ABILITIES"), DX, DY, Gold(), SmallFont(), UIScale);
	DY += Sc(22.f);
	DY += DrawAbilityLine(Data->BasicAbility, UMTAbilityComponent::SlotToKeyLabel(EMTAbilitySlot::Basic), DX, DY, DW);
	for (int32 i = 0; i < Data->Abilities.Num() && i < 3; ++i)
	{
		const EMTAbilitySlot Slot = static_cast<EMTAbilitySlot>(static_cast<int32>(EMTAbilitySlot::Character1) + i);
		DY += DrawAbilityLine(Data->Abilities[i], UMTAbilityComponent::SlotToKeyLabel(Slot), DX, DY, DW);
	}
	DY += DrawAbilityLine(Data->SpecialAbility, UMTAbilityComponent::SlotToKeyLabel(EMTAbilitySlot::Special), DX, DY, DW);
	DY += DrawAbilityLine(Data->AwakeningAbility, UMTAbilityComponent::SlotToKeyLabel(EMTAbilitySlot::Awakening), DX, DY, DW);

	const bool bEquipped = Prog->GetEquippedCharacter() == SelCharacter;
	DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("EquipChar:") + SelCharacter.ToString()), bEquipped ? TEXT("EQUIPPED") : TEXT("EQUIP"),
		DX + DW - Sc(200.f), Y + H - Sc(42.f), Sc(200.f), Sc(40.f), !bEquipped, bEquipped);
}

// ============================================================================ ELEMENT

void AMTHUD::DrawElementPage(float X, float Y, float W, float H)
{
	UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Prog || !Registry)
	{
		DrawStr(TEXT("Element data unavailable."), X, Y, Dim(), SmallFont(), UIScale);
		return;
	}

	TArray<EMTElement> Owned;
	for (int32 i = static_cast<int32>(EMTElement::Fire); i <= static_cast<int32>(EMTElement::Arcane); ++i)
	{
		const EMTElement E = static_cast<EMTElement>(i);
		if (Prog->OwnsElement(E))
		{
			Owned.Add(E);
		}
	}
	if (!Owned.Contains(SelElement))
	{
		SelElement = Owned.Contains(Prog->GetEquippedElement(0)) ? Prog->GetEquippedElement(0) : (Owned.Num() > 0 ? Owned[0] : EMTElement::None);
	}

	const float ListW = FMath::Min(Sc(300.f), W * 0.28f);
	DrawStr(TEXT("OWNED ELEMENTS"), X, Y, Gold(), SmallFont(), UIScale);
	float LY = Y + Sc(26.f);
	for (const EMTElement E : Owned)
	{
		FString Tag;
		if (Prog->GetEquippedElement(0) == E) { Tag = TEXT("   [A]"); }
		else if (Prog->GetEquippedElement(1) == E) { Tag = TEXT("   [B]"); }
		DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("SelElem:%d"), static_cast<int32>(E))),
			Prog->GetElementDisplayName(E).ToString() + Tag, X, LY, ListW, Sc(38.f), true, E == SelElement, MTUtil::ElementColor(E));
		LY += Sc(44.f);
	}
	const FString Loadout = FString::Printf(TEXT("Slot A: %s\nSlot B: %s"),
		*Prog->GetElementDisplayName(Prog->GetEquippedElement(0)).ToString(),
		Prog->GetUnlockedElementSlots() >= 2
			? *(Prog->GetEquippedElement(1) == EMTElement::None ? FString(TEXT("(empty)")) : Prog->GetElementDisplayName(Prog->GetEquippedElement(1)).ToString())
			: *FString::Printf(TEXT("(unlocks at Lv %d)"), UMTProgressionSubsystem::SecondElementSlotLevel));
	DrawWrapped(Loadout, X, LY + Sc(8.f), ListW, Parchment(0.9f), SmallFont(), UIScale, 3);

	if (SelElement == EMTElement::None)
	{
		return;
	}
	const FMTElementData* Data = Registry->FindElement(SelElement);
	const float DX = X + ListW + Sc(28.f);
	const float DW = W - ListW - Sc(28.f);
	float DY = Y;

	DrawStr(Prog->GetElementDisplayName(SelElement).ToString(), DX, DY, MTUtil::ElementColor(SelElement), LargeFont(), UIScale * 0.75f);
	DY += LineHeight(LargeFont(), UIScale * 0.75f) + Sc(2.f);
	if (Data)
	{
		DrawStr(MTUtil::RarityToString(Data->Rarity).ToUpper(), DX, DY, MTUtil::RarityColor(Data->Rarity), SmallFont(), UIScale);
		DY += LineHeight(SmallFont(), UIScale) + Sc(6.f);
		DY += DrawWrapped(Data->Description.ToString(), DX, DY, DW, Parchment(), SmallFont(), UIScale, 4) + Sc(10.f);
	}

	const EMTMagicRank Rank = Prog->GetMagicRank(SelElement);
	const float RankProgress = Prog->GetMagicRankProgress(SelElement);
	DrawStr(FString::Printf(TEXT("MAGIC RANK: %s"), *MTUtil::MagicRankToString(Rank).ToUpper()), DX, DY, Gold(), SmallFont(), UIScale);
	DrawMeter(DX + Sc(250.f), DY + Sc(4.f), Sc(240.f), Sc(8.f), RankProgress, RankProgress, MTUtil::ElementColor(SelElement), Gold());
	if (Rank != EMTMagicRank::God)
	{
		const EMTMagicRank NextRank = static_cast<EMTMagicRank>(static_cast<int32>(Rank) + 1);
		DrawStr(FString::Printf(TEXT("%d / %d  (%s)"), FMath::FloorToInt(Prog->GetMagicRankXP(SelElement)),
			FMath::RoundToInt(UMTProgressionSubsystem::GetMagicRankThreshold(NextRank)), *MTUtil::MagicRankToString(NextRank)),
			DX + Sc(500.f), DY, Dim(), SmallFont(), UIScale * 0.9f);
	}
	DY += Sc(24.f);

	const FName MasteryKey = UMTProgressionSubsystem::ElementKey(SelElement);
	const int32 MasteryLevel = Prog->GetMasteryLevel(EMTMasteryTrack::Element, MasteryKey);
	const float MasteryProgress = Prog->GetMasteryProgress(EMTMasteryTrack::Element, MasteryKey);
	DrawStr(FString::Printf(TEXT("MASTERY  %d / %d"), MasteryLevel, UMTProgressionSubsystem::MaxMasteryLevel), DX, DY, Gold(), SmallFont(), UIScale);
	DrawMeter(DX + Sc(250.f), DY + Sc(4.f), Sc(240.f), Sc(8.f), MasteryProgress, MasteryProgress, Gold(), Gold());
	DY += Sc(28.f);

	DrawStr(TEXT("ABILITIES"), DX, DY, Gold(), SmallFont(), UIScale);
	DY += Sc(22.f);
	const bool bInA = Prog->GetEquippedElement(0) == SelElement;
	const bool bInB = Prog->GetEquippedElement(1) == SelElement;
	if (Data)
	{
		for (int32 i = 0; i < Data->Abilities.Num() && i < 3; ++i)
		{
			FString Key = TEXT("-");
			if (bInA) { Key = UMTAbilityComponent::SlotToKeyLabel(static_cast<EMTAbilitySlot>(static_cast<int32>(EMTAbilitySlot::ElementA1) + i)); }
			else if (bInB) { Key = UMTAbilityComponent::SlotToKeyLabel(static_cast<EMTAbilitySlot>(static_cast<int32>(EMTAbilitySlot::ElementB1) + i)); }
			DY += DrawAbilityLine(Data->Abilities[i], Key, DX, DY, DW);
		}
	}

	const float BtnW = Sc(220.f);
	const float BtnY = Y + H - Sc(42.f);
	const bool bSlotBUnlocked = Prog->GetUnlockedElementSlots() >= 2;
	DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("EquipElem:0:%d"), static_cast<int32>(SelElement))),
		bInA ? TEXT("IN SLOT A") : TEXT("EQUIP TO SLOT A"), DX + DW - 2.f * BtnW - Sc(12.f), BtnY, BtnW, Sc(40.f), !bInA, bInA);
	DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("EquipElem:1:%d"), static_cast<int32>(SelElement))),
		!bSlotBUnlocked ? FString::Printf(TEXT("SLOT B: LV %d"), UMTProgressionSubsystem::SecondElementSlotLevel) : (bInB ? FString(TEXT("IN SLOT B")) : FString(TEXT("EQUIP TO SLOT B"))),
		DX + DW - BtnW, BtnY, BtnW, Sc(40.f), bSlotBUnlocked && !bInB, bInB);
}

// ============================================================================ RACE

void AMTHUD::DrawRacePage(float X, float Y, float W, float H)
{
	UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Prog || !Registry)
	{
		DrawStr(TEXT("Race data unavailable."), X, Y, Dim(), SmallFont(), UIScale);
		return;
	}

	TArray<EMTRace> Owned;
	for (int32 i = 0; i <= static_cast<int32>(EMTRace::SeaRace); ++i)
	{
		const EMTRace R = static_cast<EMTRace>(i);
		if (Prog->OwnsRace(R))
		{
			Owned.Add(R);
		}
	}
	if (!bSelRaceValid || !Owned.Contains(SelRace))
	{
		SelRace = Owned.Contains(Prog->GetEquippedRace()) ? Prog->GetEquippedRace() : (Owned.Num() > 0 ? Owned[0] : EMTRace::Human);
		bSelRaceValid = Owned.Num() > 0;
	}

	const float ListW = FMath::Min(Sc(300.f), W * 0.28f);
	DrawStr(TEXT("OWNED RACES"), X, Y, Gold(), SmallFont(), UIScale);
	float LY = Y + Sc(26.f);
	for (const EMTRace R : Owned)
	{
		const FMTRaceData* Row = Registry->FindRace(R);
		const bool bEquipped = Prog->GetEquippedRace() == R;
		DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("SelRace:%d"), static_cast<int32>(R))),
			Prog->GetRaceDisplayName(R).ToString() + (bEquipped ? TEXT("   [EQUIPPED]") : TEXT("")), X, LY, ListW, Sc(38.f), true, R == SelRace,
			Row ? MTUtil::RarityColor(Row->Rarity) : Gold());
		LY += Sc(44.f);
	}

	const FMTRaceData* Row = Registry->FindRace(SelRace);
	const float DX = X + ListW + Sc(28.f);
	const float DW = W - ListW - Sc(28.f);
	float DY = Y;
	DrawStr(Prog->GetRaceDisplayName(SelRace).ToString(), DX, DY, Row ? MTUtil::RarityColor(Row->Rarity) : Parchment(), LargeFont(), UIScale * 0.75f);
	DY += LineHeight(LargeFont(), UIScale * 0.75f) + Sc(2.f);
	if (!Row)
	{
		DrawStr(TEXT("No race data row."), DX, DY, Dim(), SmallFont(), UIScale);
		return;
	}
	DrawStr(MTUtil::RarityToString(Row->Rarity).ToUpper() + (Row->bImplemented ? TEXT("") : TEXT("     NOT YET PLAYABLE")), DX, DY,
		Row->bImplemented ? MTUtil::RarityColor(Row->Rarity) : Bad(), SmallFont(), UIScale);
	DY += LineHeight(SmallFont(), UIScale) + Sc(6.f);
	DY += DrawWrapped(Row->Description.ToString(), DX, DY, DW, Parchment(), SmallFont(), UIScale, 4) + Sc(10.f);

	// Canon label badge.
	auto DrawBadge = [this](const FString& Label, float BX, float BY)
	{
		const bool bCanon = Label.Contains(TEXT("CANON")) && !Label.Contains(TEXT("ORIGINAL"));
		const FLinearColor Color = bCanon ? FLinearColor(0.45f, 0.8f, 1.f) : FLinearColor(0.95f, 0.7f, 0.35f);
		const FVector2D Size = MeasureStr(Label, SmallFont(), UIScale * 0.8f);
		FillRect(BX, BY, static_cast<float>(Size.X) + Sc(12.f), static_cast<float>(Size.Y) + Sc(4.f), WithAlpha(Color, 0.18f));
		StrokeRect(BX, BY, static_cast<float>(Size.X) + Sc(12.f), static_cast<float>(Size.Y) + Sc(4.f), WithAlpha(Color, 0.8f));
		DrawStr(Label, BX + Sc(6.f), BY + Sc(2.f), Color, SmallFont(), UIScale * 0.8f);
	};

	const FString PassiveTitle = TEXT("PASSIVE: ") + Row->PassiveName.ToString();
	DrawStr(PassiveTitle, DX, DY, Gold(), SmallFont(), UIScale);
	DrawBadge(Row->PassiveCanonStatus.ToUpper(), DX + static_cast<float>(MeasureStr(PassiveTitle, SmallFont(), UIScale).X) + Sc(12.f), DY - Sc(1.f));
	DY += LineHeight(SmallFont(), UIScale) + Sc(4.f);
	DY += DrawWrapped(Row->PassiveDescription.ToString(), DX, DY, DW, Parchment(0.9f), SmallFont(), UIScale, 3) + Sc(10.f);

	// FMTRaceData has no canon field for the active ability: racial actives are labelled as gameplay originals.
	DrawStr(TEXT("ACTIVE"), DX, DY, Gold(), SmallFont(), UIScale);
	DrawBadge(TEXT("GAMEPLAY ORIGINAL"), DX + Sc(80.f), DY - Sc(1.f));
	DY += Sc(22.f);
	DY += DrawAbilityLine(Row->ActiveAbility, UMTAbilityComponent::SlotToKeyLabel(EMTAbilitySlot::RaceActive), DX, DY, DW);

	DrawStr(TEXT("TRANSFORMATION"), DX, DY + Sc(4.f), Gold(), SmallFont(), UIScale);
	DrawBadge(Row->TransformationCanonStatus.ToUpper(), DX + Sc(160.f), DY + Sc(3.f));
	DY += Sc(26.f);
	DY += DrawAbilityLine(Row->TransformationAbility, UMTAbilityComponent::SlotToKeyLabel(EMTAbilitySlot::RaceTransformation), DX, DY, DW);

	if (!Row->LoreNote.IsEmpty())
	{
		DY += Sc(6.f);
		DrawWrapped(TEXT("Lore: ") + Row->LoreNote, DX, DY, DW, Dim(), SmallFont(), UIScale * 0.9f, 3);
	}

	const bool bEquipped = Prog->GetEquippedRace() == SelRace;
	DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("EquipRace:%d"), static_cast<int32>(SelRace))),
		bEquipped ? TEXT("EQUIPPED") : (Row->bImplemented ? TEXT("EQUIP") : TEXT("NOT PLAYABLE")),
		DX + DW - Sc(200.f), Y + H - Sc(42.f), Sc(200.f), Sc(40.f), !bEquipped && Row->bImplemented, bEquipped);
}

// ============================================================================ MASTERY

void AMTHUD::DrawMasteryPage(float X, float Y, float W, float H)
{
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Prog || !Registry)
	{
		DrawStr(TEXT("Mastery data unavailable."), X, Y, Dim(), SmallFont(), UIScale);
		return;
	}

	struct FRow { FString Name; EMTMasteryTrack Track; FName Key; FLinearColor Color; };
	TArray<FRow> Columns[3];
	for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
	{
		if (Prog->OwnsCharacter(Pair.Key))
		{
			Columns[0].Add({ Pair.Value.DisplayName.IsEmpty() ? Pair.Key.ToString() : Pair.Value.DisplayName.ToString(), EMTMasteryTrack::Character, Pair.Key, MTUtil::RarityColor(Pair.Value.Rarity) });
		}
	}
	for (int32 i = static_cast<int32>(EMTElement::Fire); i <= static_cast<int32>(EMTElement::Arcane); ++i)
	{
		const EMTElement E = static_cast<EMTElement>(i);
		const FName Key = UMTProgressionSubsystem::ElementKey(E);
		if (Prog->OwnsElement(E) || Prog->GetMasteryLevel(EMTMasteryTrack::Element, Key) > 0)
		{
			Columns[1].Add({ Prog->GetElementDisplayName(E).ToString(), EMTMasteryTrack::Element, Key, MTUtil::ElementColor(E) });
		}
	}
	for (int32 i = 0; i <= static_cast<int32>(EMTRace::SeaRace); ++i)
	{
		const EMTRace R = static_cast<EMTRace>(i);
		if (Prog->OwnsRace(R))
		{
			Columns[2].Add({ Prog->GetRaceDisplayName(R).ToString(), EMTMasteryTrack::Race, Prog->RaceKey(R), Parchment() });
		}
	}

	static const TCHAR* Headers[] = { TEXT("CHARACTER MASTERY"), TEXT("ELEMENT MASTERY"), TEXT("RACE MASTERY") };
	const float Gap = Sc(24.f);
	const float ColW = (W - 2.f * Gap) / 3.f;
	for (int32 c = 0; c < 3; ++c)
	{
		const float CX = X + c * (ColW + Gap);
		DrawPanelBox(CX, Y, ColW, H - Sc(60.f), 0.5f, false);
		DrawStr(Headers[c], CX + Sc(14.f), Y + Sc(12.f), Gold(), SmallFont(), UIScale);
		float RY = Y + Sc(42.f);
		for (const FRow& Row : Columns[c])
		{
			const int32 Level = Prog->GetMasteryLevel(Row.Track, Row.Key);
			const float Progress = Prog->GetMasteryProgress(Row.Track, Row.Key);
			DrawStr(Row.Name, CX + Sc(14.f), RY, Row.Color, SmallFont(), UIScale);
			DrawStr(FString::Printf(TEXT("Lv %d / %d"), Level, UMTProgressionSubsystem::MaxMasteryLevel), CX + ColW - Sc(14.f), RY, Parchment(), SmallFont(), UIScale, 1.f);
			DrawMeter(CX + Sc(14.f), RY + Sc(22.f), ColW - Sc(28.f), Sc(8.f), Progress, Progress, Level >= UMTProgressionSubsystem::MaxMasteryLevel ? Good() : Gold(), Gold());
			RY += Sc(48.f);
		}
		if (Columns[c].Num() == 0)
		{
			DrawStr(TEXT("Nothing owned yet."), CX + Sc(14.f), RY, Dim(), SmallFont(), UIScale);
		}
	}
	DrawWrapped(TEXT("Mastery grows by using that character's, element's or race's abilities in combat. Higher mastery unlocks stronger techniques; awakenings and transformations require high mastery."),
		X, Y + H - Sc(48.f), W, Dim(), SmallFont(), UIScale * 0.9f, 2);
}

// ============================================================================ INVENTORY

void AMTHUD::DrawInventoryPage(float X, float Y, float W, float H)
{
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Prog)
	{
		DrawStr(TEXT("Inventory unavailable."), X, Y, Dim(), SmallFont(), UIScale);
		return;
	}
	DrawStr(FString::Printf(TEXT("GOLD  %d"), Prog->GetGold()), X, Y, Gold(), MediumFont(), UIScale * 0.75f);

	TArray<FName> Items;
	for (const TPair<FName, int32>& Pair : Prog->GetInventory())
	{
		if (Pair.Value > 0)
		{
			Items.Add(Pair.Key);
		}
	}
	Items.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	if (Items.Num() == 0)
	{
		DrawStr(TEXT("Your bag is empty. Materials and quest items you collect appear here."), X, Y + Sc(40.f), Dim(), SmallFont(), UIScale);
		return;
	}

	const int32 Cols = 3;
	const float Gap = Sc(14.f);
	const float CardW = (W - Gap * (Cols - 1)) / Cols;
	const float CardH = Sc(92.f);
	const float StartY = Y + Sc(40.f);
	const int32 MaxRows = FMath::Max(1, FMath::FloorToInt((H - Sc(40.f)) / (CardH + Gap)));
	for (int32 i = 0; i < Items.Num() && i < MaxRows * Cols; ++i)
	{
		const float CX = X + (i % Cols) * (CardW + Gap);
		const float CY = StartY + (i / Cols) * (CardH + Gap);
		const FMTItemData* Item = Registry ? Registry->FindItem(Items[i]) : nullptr;
		const FLinearColor Color = Item ? MTUtil::RarityColor(Item->Rarity) : Parchment();
		DrawPanelBox(CX, CY, CardW, CardH, 0.6f, false);
		FillRect(CX, CY, Sc(4.f), CardH, Color);
		DrawStr(FitStr(Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName.ToString() : Items[i].ToString(), CardW - Sc(90.f), SmallFont(), UIScale * 1.05f),
			CX + Sc(14.f), CY + Sc(8.f), Color, SmallFont(), UIScale * 1.05f);
		DrawStr(FString::Printf(TEXT("x%d"), Prog->GetItemCount(Items[i])), CX + CardW - Sc(12.f), CY + Sc(8.f), Parchment(), MediumFont(), UIScale * 0.7f, 1.f);
		if (Item)
		{
			DrawWrapped(Item->Description.ToString(), CX + Sc(14.f), CY + Sc(32.f), CardW - Sc(28.f), Parchment(0.85f), SmallFont(), UIScale * 0.85f, 2);
			if (Item->bQuestItem)
			{
				DrawStr(TEXT("QUEST ITEM"), CX + CardW - Sc(12.f), CY + CardH - Sc(8.f), Gold(), SmallFont(), UIScale * 0.8f, 1.f, 1.f);
			}
		}
	}
	if (Items.Num() > MaxRows * Cols)
	{
		DrawStr(FString::Printf(TEXT("+%d more"), Items.Num() - MaxRows * Cols), X + W, Y, Dim(), SmallFont(), UIScale, 1.f);
	}
}

// ============================================================================ QUESTS

void AMTHUD::DrawQuestsPage(float X, float Y, float W, float H)
{
	UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Quests)
	{
		DrawStr(TEXT("Quest log unavailable."), X, Y, Dim(), SmallFont(), UIScale);
		return;
	}
	const TArray<FName> Active = Quests->GetActiveQuestIds();
	const TArray<FName> Completed = Quests->GetCompletedQuestIds();
	const FName Tracked = Quests->GetTrackedQuest();
	if (SelQuest.IsNone() || (!Active.Contains(SelQuest) && !Completed.Contains(SelQuest)))
	{
		SelQuest = Active.Contains(Tracked) ? Tracked : (Active.Num() > 0 ? Active[0] : (Completed.Num() > 0 ? Completed[0] : NAME_None));
	}

	const float ListW = FMath::Min(Sc(360.f), W * 0.32f);
	const float RowH = Sc(34.f);
	float LY = Y;
	DrawStr(FString::Printf(TEXT("ACTIVE (%d)"), Active.Num()), X, LY, Gold(), SmallFont(), UIScale);
	LY += Sc(24.f);
	for (const FName& QuestId : Active)
	{
		if (LY > Y + H * 0.6f)
		{
			break;
		}
		DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("SelQuest:") + QuestId.ToString()), (QuestId == Tracked ? TEXT("> ") : TEXT("")) + GetQuestTitle(QuestId),
			X, LY, ListW, RowH, true, QuestId == SelQuest, QuestId == Tracked ? Gold() : FLinearColor::Transparent);
		LY += RowH + Sc(4.f);
	}
	if (Active.Num() == 0)
	{
		DrawStr(TEXT("No active quests. Talk to villagers and the guild."), X, LY, Dim(), SmallFont(), UIScale * 0.9f);
		LY += Sc(24.f);
	}
	LY += Sc(12.f);
	DrawStr(FString::Printf(TEXT("COMPLETED (%d)"), Completed.Num()), X, LY, Gold(), SmallFont(), UIScale);
	LY += Sc(24.f);
	for (const FName& QuestId : Completed)
	{
		if (LY + RowH > Y + H)
		{
			break;
		}
		DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("SelQuest:") + QuestId.ToString()), GetQuestTitle(QuestId), X, LY, ListW, RowH, true, QuestId == SelQuest, Good(0.6f));
		LY += RowH + Sc(4.f);
	}

	if (SelQuest.IsNone())
	{
		return;
	}
	const FMTQuestData* Quest = Registry ? Registry->FindQuest(SelQuest) : nullptr;
	const bool bActive = Active.Contains(SelQuest);
	const float DX = X + ListW + Sc(28.f);
	const float DW = W - ListW - Sc(28.f);
	float DY = Y;
	DrawStr(GetQuestTitle(SelQuest), DX, DY, Gold(), LargeFont(), UIScale * 0.7f);
	DY += LineHeight(LargeFont(), UIScale * 0.7f) + Sc(2.f);
	if (!Quest)
	{
		return;
	}
	DrawStr(FString::Printf(TEXT("%s quest   |   %s   |   Rank %s+%s"), *MTHUDMenusPrivate::QuestTypeLabel(Quest->Type), *Quest->Region.ToString(),
		*MTUtil::AdventurerRankToString(Quest->RequiredRank), bActive ? TEXT("") : TEXT("   |   COMPLETED")), DX, DY, Dim(), SmallFont(), UIScale);
	DY += LineHeight(SmallFont(), UIScale) + Sc(8.f);
	DY += DrawWrapped(Quest->Summary.ToString(), DX, DY, DW, Parchment(), SmallFont(), UIScale, 5) + Sc(12.f);

	DrawStr(TEXT("OBJECTIVES"), DX, DY, Gold(), SmallFont(), UIScale);
	DY += Sc(24.f);
	const FMTQuestSaveState* State = Quests->GetQuestState(SelQuest);
	for (int32 i = 0; i < Quest->Objectives.Num(); ++i)
	{
		const bool bHidden = bActive && Quest->bSequentialObjectives && State && i > State->Stage;
		const bool bDone = !bActive || (State && State->Progress.IsValidIndex(i) && State->Progress[i] >= Quest->Objectives[i].Count);
		const FString Line = bHidden ? FString(TEXT("???")) : (bActive ? Quests->GetObjectiveText(SelQuest, i).ToString() : Quest->Objectives[i].Description.ToString());
		const float Box = Sc(10.f);
		StrokeRect(DX, DY + Sc(4.f), Box, Box, Gold(0.8f));
		if (bDone)
		{
			FillRect(DX + Sc(2.f), DY + Sc(6.f), Box - Sc(4.f), Box - Sc(4.f), Good());
		}
		DrawStr(FitStr(Line, DW - Sc(24.f), SmallFont(), UIScale), DX + Sc(18.f), DY, bDone ? Dim() : Parchment(), SmallFont(), UIScale);
		DY += Sc(24.f);
	}
	DY += Sc(10.f);
	DrawStr(TEXT("REWARDS"), DX, DY, Gold(), SmallFont(), UIScale);
	DY += Sc(22.f);
	DrawWrapped(MTHUDMenusPrivate::RewardSummary(Quest->Reward), DX, DY, DW, Parchment(), SmallFont(), UIScale, 3);

	if (bActive)
	{
		const bool bIsTracked = SelQuest == Tracked;
		DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("Track:") + SelQuest.ToString()), bIsTracked ? TEXT("TRACKING") : TEXT("TRACK"),
			DX + DW - Sc(200.f), Y + H - Sc(42.f), Sc(200.f), Sc(40.f), !bIsTracked, bIsTracked);
	}
}

// ============================================================================ MAP

void AMTHUD::DrawMapPage(float X, float Y, float W, float H)
{
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const APawn* Pawn = GetOwningPawn();
	if (!Prog || !Registry)
	{
		DrawStr(TEXT("Map unavailable."), X, Y, Dim(), SmallFont(), UIScale);
		return;
	}

	const float MapW = W * 0.66f;
	const float MapH = H;
	DrawPanelBox(X, Y, MapW, MapH, 0.6f, false);
	for (int32 i = 1; i < 8; ++i)
	{
		FillRect(X + MapW * i / 8.f, Y + 1.f, 1.f, MapH - 2.f, Gold(0.06f));
		FillRect(X + 1.f, Y + MapH * i / 8.f, MapW - 2.f, 1.f, Gold(0.06f));
	}

	// Discovered locations + player define the bounds.
	TArray<const FMTLocationData*> Known;
	int32 UndiscoveredTravel = 0;
	for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
	{
		if (Prog->IsLocationDiscovered(Pair.Key))
		{
			Known.Add(&Pair.Value);
		}
		else if (Pair.Value.bFastTravel)
		{
			++UndiscoveredTravel;
		}
	}
	FBox2D Bounds(ForceInit);
	for (const FMTLocationData* Loc : Known)
	{
		Bounds += FVector2D(Loc->WorldLocation.X, Loc->WorldLocation.Y);
	}
	if (Pawn)
	{
		Bounds += FVector2D(Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y);
	}

	if (Known.Num() == 0 && !Pawn)
	{
		DrawStr(TEXT("No locations discovered yet."), X + MapW * 0.5f, Y + MapH * 0.5f, Dim(), SmallFont(), UIScale, 0.5f, 0.5f);
	}
	else
	{
		const FVector2D Center = Bounds.GetCenter();
		const double RangeNorth = FMath::Max(5000.0, Bounds.Max.X - Bounds.Min.X);
		const double RangeEast = FMath::Max(5000.0, Bounds.Max.Y - Bounds.Min.Y);
		const float Pad = Sc(50.f);
		const double MapScale = FMath::Min((MapW - 2.f * Pad) / RangeEast, (MapH - 2.f * Pad) / RangeNorth);
		const float MX = X + MapW * 0.5f;
		const float MY = Y + MapH * 0.5f;
		// North-up: world +X -> screen up, world +Y -> screen right.
		auto ToScreen = [&](const FVector& World) -> FVector2D
		{
			return FVector2D(MX + (World.Y - Center.Y) * MapScale, MY - (World.X - Center.X) * MapScale);
		};

		for (const FMTLocationData* Loc : Known)
		{
			const FVector2D P = ToScreen(Loc->WorldLocation);
			const FLinearColor Color = Loc->bFastTravel ? FLinearColor(0.45f, 0.85f, 1.f) : Parchment(0.85f);
			DrawDiamondShape(P, Sc(6.f), Color, true);
			DrawStr(Loc->DisplayName.IsEmpty() ? Loc->LocationID.ToString() : Loc->DisplayName.ToString(),
				static_cast<float>(P.X) + Sc(10.f), static_cast<float>(P.Y), Color, SmallFont(), UIScale * 0.9f, 0.f, 0.5f);
		}

		if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
		{
			FVector MarkerLoc;
			const FName Tracked = Quests->GetTrackedQuest();
			if (!Tracked.IsNone() && Quests->GetObjectiveMarker(Tracked, MarkerLoc))
			{
				FVector2D P = ToScreen(MarkerLoc);
				P.X = FMath::Clamp(P.X, static_cast<double>(X + Sc(8.f)), static_cast<double>(X + MapW - Sc(8.f)));
				P.Y = FMath::Clamp(P.Y, static_cast<double>(Y + Sc(8.f)), static_cast<double>(Y + MapH - Sc(8.f)));
				DrawDiamondShape(P, Sc(8.f), Gold(0.7f + 0.3f * FMath::Sin(HudTime * 4.f)), true);
				DrawStr(TEXT("QUEST"), static_cast<float>(P.X), static_cast<float>(P.Y) - Sc(10.f), Gold(), SmallFont(), UIScale * 0.8f, 0.5f, 1.f);
			}
		}

		if (Pawn)
		{
			const FVector2D P = ToScreen(Pawn->GetActorLocation());
			const float Yaw = FMath::DegreesToRadians(Pawn->GetActorRotation().Yaw);
			const FVector2D Fwd(FMath::Sin(Yaw), -FMath::Cos(Yaw));
			const FVector2D Right(-Fwd.Y, Fwd.X);
			const FVector2D Tip = P + Fwd * Sc(12.f);
			const FVector2D L = P - Fwd * Sc(8.f) - Right * Sc(7.f);
			const FVector2D R = P - Fwd * Sc(8.f) + Right * Sc(7.f);
			DrawLine(Tip.X, Tip.Y, L.X, L.Y, Good(), 2.f);
			DrawLine(Tip.X, Tip.Y, R.X, R.Y, Good(), 2.f);
			DrawLine(L.X, L.Y, R.X, R.Y, Good(), 2.f);
			DrawStr(TEXT("YOU"), static_cast<float>(P.X), static_cast<float>(P.Y) + Sc(12.f), Good(), SmallFont(), UIScale * 0.8f, 0.5f, 0.f);
		}
		DrawStr(TEXT("N"), X + MapW - Sc(20.f), Y + Sc(12.f), Gold(), MediumFont(), UIScale * 0.7f, 0.5f, 0.f);
	}

	// Fast travel list.
	const float LX = X + MapW + Sc(24.f);
	const float LW = W - MapW - Sc(24.f);
	float LY = Y;
	DrawStr(TEXT("FAST TRAVEL"), LX, LY, Gold(), SmallFont(), UIScale);
	LY += Sc(26.f);
	Known.Sort([](const FMTLocationData& A, const FMTLocationData& B) { return A.DisplayName.ToString() < B.DisplayName.ToString(); });
	int32 Listed = 0;
	for (const FMTLocationData* Loc : Known)
	{
		if (!Loc->bFastTravel)
		{
			continue;
		}
		const bool bRankOk = Prog->GetAdventurerRank() >= Loc->RequiredRank;
		const FString Label = (Loc->DisplayName.IsEmpty() ? Loc->LocationID.ToString() : Loc->DisplayName.ToString())
			+ (bRankOk ? FString::Printf(TEXT("  (%s)"), *Loc->Region.ToString()) : FString::Printf(TEXT("  (Rank %s)"), *MTUtil::AdventurerRankToString(Loc->RequiredRank)));
		DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("Travel:") + Loc->LocationID.ToString()), Label, LX, LY, LW, Sc(36.f), bRankOk && Pawn != nullptr, false,
			FLinearColor(0.45f, 0.85f, 1.f));
		LY += Sc(42.f);
		++Listed;
		if (LY > Y + H - Sc(60.f))
		{
			break;
		}
	}
	if (Listed == 0)
	{
		DrawWrapped(TEXT("Discover waystones to unlock fast travel."), LX, LY, LW, Dim(), SmallFont(), UIScale, 2);
		LY += Sc(40.f);
	}
	if (UndiscoveredTravel > 0)
	{
		DrawStr(FString::Printf(TEXT("%d undiscovered travel point(s)"), UndiscoveredTravel), LX, LY + Sc(8.f), Dim(), SmallFont(), UIScale * 0.9f);
	}
}

// ============================================================================ PARTY

void AMTHUD::DrawPartyPage(float X, float Y, float W, float H)
{
	DrawStr(TEXT("PARTY"), X, Y, Gold(), LargeFont(), UIScale * 0.7f);
	DrawWrapped(TEXT("Co-op and party play are planned but NOT implemented in this build. This page is a placeholder: there is currently no multiplayer, matchmaking, companions or party system.\n\nWhen it lands, this page will list party members, their builds and shared quest progress."),
		X, Y + Sc(48.f), FMath::Min(W, Sc(900.f)), Parchment(), SmallFont(), UIScale * 1.05f, 10);
}

// ============================================================================ SETTINGS

void AMTHUD::DrawSettingsPage(float X, float Y, float W, float H)
{
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	if (!Prog)
	{
		DrawStr(TEXT("Settings unavailable."), X, Y, Dim(), SmallFont(), UIScale);
		return;
	}
	const FMTSettingsSave& S = Prog->GetSettings();

	struct FSettingRow { const TCHAR* Key; FString Label; FString Value; bool bToggle; };
	const FSettingRow Rows[] =
	{
		{ TEXT("Sens"), TEXT("Mouse sensitivity"), FString::Printf(TEXT("%.2f"), S.MouseSensitivity), false },
		{ TEXT("InvertY"), TEXT("Invert Y axis"), S.bInvertY ? TEXT("On") : TEXT("Off"), true },
		{ TEXT("FOV"), TEXT("Field of view"), FString::Printf(TEXT("%.0f"), S.FieldOfView), false },
		{ TEXT("Shake"), TEXT("Camera shake"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S.CameraShakeScale * 100.f)), false },
		{ TEXT("SkipRoll"), TEXT("Skip roll animation"), S.bSkipRollAnimation ? TEXT("On") : TEXT("Off"), true },
		{ TEXT("ToggleSprint"), TEXT("Toggle sprint"), S.bToggleSprint ? TEXT("On") : TEXT("Off"), true },
		{ TEXT("Graphics"), TEXT("Graphics quality"), MTHUDMenusPrivate::GraphicsLabel(S.GraphicsQuality), false },
	};

	const float RowW = FMath::Min(W, Sc(760.f));
	const float RowH = Sc(44.f);
	float RY = Y;
	DrawStr(TEXT("SETTINGS"), X, RY, Gold(), SmallFont(), UIScale);
	RY += Sc(28.f);
	for (const FSettingRow& Row : Rows)
	{
		FillRect(X, RY, RowW, RowH - Sc(6.f), PanelColor(0.5f));
		DrawStr(Row.Label, X + Sc(14.f), RY + (RowH - Sc(6.f)) * 0.5f, Parchment(), SmallFont(), UIScale * 1.05f, 0.f, 0.5f);
		DrawStr(Row.Value, X + RowW * 0.62f, RY + (RowH - Sc(6.f)) * 0.5f, Gold(), SmallFont(), UIScale * 1.05f, 0.5f, 0.5f);
		const float BtnH = RowH - Sc(14.f);
		const float BtnY = RY + Sc(4.f);
		if (Row.bToggle)
		{
			DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("Set:%s:0"), Row.Key)), TEXT("TOGGLE"), X + RowW - Sc(130.f), BtnY, Sc(120.f), BtnH);
		}
		else
		{
			DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("Set:%s:-1"), Row.Key)), TEXT("-"), X + RowW - Sc(130.f), BtnY, Sc(56.f), BtnH);
			DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("Set:%s:1"), Row.Key)), TEXT("+"), X + RowW - Sc(66.f), BtnY, Sc(56.f), BtnH);
		}
		RY += RowH;
	}

	RY += Sc(16.f);
	const UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(this);
	DrawButtonBox(TEXT("Save"), TEXT("SAVE GAME"), X, RY, Sc(200.f), Sc(40.f), Save != nullptr);
	DrawButtonBox(TEXT("Load"), TEXT("LOAD GAME"), X + Sc(212.f), RY, Sc(200.f), Sc(40.f), Save && Save->HasSave(Save->GetCurrentSlot()));
	if (Save)
	{
		DrawStr(Save->HasSave(Save->GetCurrentSlot()) ? FString::Printf(TEXT("Slot \"%s\" has a save. Autosave every %d s."), *Save->GetCurrentSlot(), FMath::RoundToInt(Save->AutosaveInterval))
			: FString::Printf(TEXT("No save in slot \"%s\" yet."), *Save->GetCurrentSlot()),
			X + Sc(430.f), RY + Sc(20.f), Dim(), SmallFont(), UIScale, 0.f, 0.5f);
	}
	RY += Sc(56.f);
	DrawWrapped(TEXT("Field of view and graphics quality apply immediately. Sensitivity, invert Y, camera shake and toggle sprint are read by the player controls. Settings are stored in the save file."),
		X, RY, RowW, Dim(), SmallFont(), UIScale * 0.9f, 3);
}

void AMTHUD::AdjustSetting(const FString& Key, int32 Dir)
{
	UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	if (!Prog)
	{
		return;
	}
	FMTSettingsSave S = Prog->GetSettings();
	bool bGraphics = false;
	if (Key == TEXT("Sens")) { S.MouseSensitivity = FMath::RoundToFloat((S.MouseSensitivity + 0.1f * Dir) * 20.f) / 20.f; }
	else if (Key == TEXT("InvertY")) { S.bInvertY = !S.bInvertY; }
	else if (Key == TEXT("FOV")) { S.FieldOfView += 5.f * Dir; }
	else if (Key == TEXT("Shake")) { S.CameraShakeScale = FMath::RoundToFloat((S.CameraShakeScale + 0.1f * Dir) * 10.f) / 10.f; }
	else if (Key == TEXT("SkipRoll")) { S.bSkipRollAnimation = !S.bSkipRollAnimation; }
	else if (Key == TEXT("ToggleSprint")) { S.bToggleSprint = !S.bToggleSprint; }
	else if (Key == TEXT("Graphics")) { S.GraphicsQuality += Dir; bGraphics = true; }
	Prog->SetSettings(S); // clamps
	if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(this))
	{
		Save->ApplySettings(bGraphics);
	}
}

// ============================================================================ ROLL

void AMTHUD::DrawRollPage(float X, float Y, float W, float H)
{
	UMTRollSubsystem* RollSys = UMTRollSubsystem::Get(this);
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	if (!RollSys || !Prog)
	{
		DrawStr(TEXT("Rolls unavailable."), X, Y, Dim(), SmallFont(), UIScale);
		return;
	}

	// Category tabs.
	const float TabW = Sc(230.f);
	for (int32 i = 0; i < 3; ++i)
	{
		const EMTRollCategory C = static_cast<EMTRollCategory>(i);
		DrawButtonBox(MTHUDMenusPrivate::MakeId(FString::Printf(TEXT("RollCat:%d"), i)),
			FString::Printf(TEXT("%s  (%d)"), *MTHUDMenusPrivate::CategoryLabel(C), Prog->GetSpins(C)),
			X + i * (TabW + Sc(8.f)), Y, TabW, Sc(36.f), !bRollAnimating || C == RollCategory, C == RollCategory);
	}
	const float TopY = Y + Sc(52.f);

	// ---- Pool with displayed odds.
	TArray<FName> Ids;
	TArray<EMTRarity> Rarities;
	TArray<float> Odds;
	RollSys->GetPool(RollCategory, Ids, Rarities, Odds);
	const float PoolW = W * 0.42f;
	DrawPanelBox(X, TopY, PoolW, H - (TopY - Y), 0.5f, false);
	DrawStr(TEXT("POOL  -  displayed odds (base rates, before pity)"), X + Sc(14.f), TopY + Sc(10.f), Gold(), SmallFont(), UIScale);
	float PY = TopY + Sc(38.f);
	for (int32 i = 0; i < Ids.Num(); ++i)
	{
		if (PY > TopY + (H - (TopY - Y)) - Sc(30.f))
		{
			break;
		}
		const FLinearColor Color = MTUtil::RarityColor(Rarities[i]);
		FillRect(X + Sc(14.f), PY, Sc(4.f), Sc(24.f), Color);
		DrawStr(RollSys->GetEntryDisplayName(RollCategory, Ids[i]).ToString(), X + Sc(26.f), PY + Sc(12.f), Color, SmallFont(), UIScale * 1.05f, 0.f, 0.5f);
		DrawStr(MTUtil::RarityToString(Rarities[i]), X + PoolW * 0.5f, PY + Sc(12.f), Dim(), SmallFont(), UIScale * 0.9f, 0.f, 0.5f);
		DrawStr(FString::Printf(TEXT("%.2f%%"), Odds[i] * 100.f), X + PoolW * 0.78f, PY + Sc(12.f), Parchment(), SmallFont(), UIScale, 1.f, 0.5f);
		if (RollSys->IsEntryOwned(RollCategory, Ids[i]))
		{
			DrawStr(TEXT("OWNED"), X + PoolW - Sc(14.f), PY + Sc(12.f), Good(), SmallFont(), UIScale * 0.8f, 1.f, 0.5f);
		}
		PY += Sc(30.f);
	}
	if (Ids.Num() == 0)
	{
		DrawWrapped(TEXT("Nothing can be rolled in this category yet."), X + Sc(14.f), PY, PoolW - Sc(28.f), Dim(), SmallFont(), UIScale, 2);
	}

	// ---- Right column: spins, pity, roll button, result card, history.
	const float RX = X + PoolW + Sc(24.f);
	const float RW = W - PoolW - Sc(24.f);
	float RY = TopY;
	const int32 Spins = Prog->GetSpins(RollCategory);
	DrawStr(FString::Printf(TEXT("%s SPINS: %d"), *MTHUDMenusPrivate::CategoryLabel(RollCategory), Spins), RX, RY, Gold(), MediumFont(), UIScale * 0.8f);
	RY += LineHeight(MediumFont(), UIScale * 0.8f) + Sc(6.f);

	const FMTRollConfig Config = RollSys->GetEffectiveConfig(RollCategory);
	const FMTRollPityState Pity = Prog->GetPity(RollCategory);
	const int32 UntilLegendary = RollSys->GetSpinsUntilHardPity(RollCategory);
	const int32 UntilMythic = RollSys->GetSpinsUntilMythicPity(RollCategory);
	const float BarW = FMath::Min(RW, Sc(420.f));
	if (RollSys->IsPoolAllLegendaryPlus(RollCategory))
	{
		DrawStr(TEXT("Every result in this pool is Legendary or better."), RX, RY, Parchment(), SmallFont(), UIScale);
		RY += Sc(22.f);
	}
	else if (UntilLegendary > 0)
	{
		const float Progress = RollSys->GetLegendaryPityProgress(RollCategory);
		DrawMeter(RX, RY, BarW, Sc(10.f), Progress, Progress, MTUtil::RarityColor(EMTRarity::Legendary), Gold());
		DrawStr(FString::Printf(TEXT("Guaranteed Legendary+ in %d"), UntilLegendary), RX, RY + Sc(14.f), Parchment(), SmallFont(), UIScale);
		if (Pity.SpinsSinceLegendary >= Config.SoftPityStart)
		{
			DrawStr(TEXT("Soft pity active: Legendary+ odds rising"), RX + BarW, RY + Sc(14.f), MTUtil::RarityColor(EMTRarity::Legendary), SmallFont(), UIScale * 0.9f, 1.f);
		}
		RY += Sc(40.f);
	}
	else
	{
		DrawStr(TEXT("No Legendary+ entries in this pool."), RX, RY, Dim(), SmallFont(), UIScale);
		RY += Sc(22.f);
	}
	if (UntilMythic > 0)
	{
		const float Progress = FMath::Clamp(static_cast<float>(Pity.SpinsSinceMythic) / FMath::Max(1, Config.MythicHardPity), 0.f, 1.f);
		DrawMeter(RX, RY, BarW, Sc(6.f), Progress, Progress, MTUtil::RarityColor(EMTRarity::Mythic), Gold());
		DrawStr(FString::Printf(TEXT("Guaranteed Mythic in %d"), UntilMythic), RX, RY + Sc(10.f), Parchment(0.85f), SmallFont(), UIScale * 0.9f);
		RY += Sc(34.f);
	}

	// Roll button (never forced to watch: SKIP while animating).
	const bool bCanRoll = RollSys->CanRoll(RollCategory);
	const FString RollLabel = bRollAnimating ? FString(TEXT("SKIP")) : (bCanRoll ? FString(TEXT("ROLL  (1 spin)")) : FString(TEXT("NO SPINS")));
	DrawButtonBox(TEXT("Roll"), RollLabel, RX, RY, Sc(240.f), Sc(44.f), bRollAnimating || bCanRoll, bRollAnimating);
	if (!bCanRoll && !bRollAnimating)
	{
		DrawWrapped(TEXT("Spins are earned by playing: quests, bosses, level-ups, rank-ups and achievements."), RX + Sc(256.f), RY + Sc(4.f), RW - Sc(256.f), Dim(), SmallFont(), UIScale * 0.9f, 2);
	}
	RY += Sc(58.f);

	// Result card.
	const float CardH = Sc(120.f);
	const float CardW = FMath::Min(RW, Sc(520.f));
	if (bRollAnimating && RollReel.Num() > 0)
	{
		const float T = FMath::Clamp(RollAnimTime / RollAnimDuration, 0.f, 1.f);
		const float Eased = 1.f - FMath::Pow(1.f - T, 3.f);
		const int32 ReelStep = FMath::Min(RollReelTicks, FMath::FloorToInt(Eased * RollReelTicks));
		const FString& Name = RollReel[(RollReelStart + ReelStep) % RollReel.Num()];
		// Whole card is a skip target (registered first so the card art draws on top of it).
		DrawButtonBox(TEXT("RollSkip"), TEXT(""), RX, RY, CardW, CardH, true, false);
		DrawPanelBox(RX, RY, CardW, CardH, 0.9f, true);
		const float Jitter = (1.f - Eased) * Sc(3.f) * FMath::Sin(HudTime * 60.f);
		DrawStr(Name, RX + CardW * 0.5f + Jitter, RY + CardH * 0.45f, Parchment(), LargeFont(), UIScale * 0.8f, 0.5f, 0.5f);
		DrawStr(TEXT("click / confirm to skip"), RX + CardW * 0.5f, RY + CardH - Sc(14.f), Dim(), SmallFont(), UIScale * 0.8f, 0.5f, 0.5f);
	}
	else if (bHasLastRoll && LastRoll.Category == RollCategory)
	{
		const FLinearColor Color = MTUtil::RarityColor(LastRoll.Rarity);
		DrawPanelBox(RX, RY, CardW, CardH, 0.9f, true);
		StrokeRect(RX + Sc(3.f), RY + Sc(3.f), CardW - Sc(6.f), CardH - Sc(6.f), Color, Sc(2.f));
		DrawStr(LastRoll.DisplayName.ToString(), RX + CardW * 0.5f, RY + Sc(38.f), Color, LargeFont(), UIScale * 0.8f, 0.5f, 0.5f);
		DrawStr(MTUtil::RarityToString(LastRoll.Rarity).ToUpper(), RX + CardW * 0.5f, RY + Sc(66.f), Color, SmallFont(), UIScale, 0.5f, 0.5f);
		const FString Status = LastRoll.bWasNew
			? FString(TEXT("NEW!"))
			: FString::Printf(TEXT("DUPLICATE  (+%d gold, +%d mastery XP)"), UMTRollSubsystem::DuplicateGold, FMath::RoundToInt(UMTRollSubsystem::DuplicateMasteryXP));
		DrawStr(Status, RX + CardW * 0.5f, RY + Sc(88.f), LastRoll.bWasNew ? Good() : Parchment(0.85f), SmallFont(), UIScale, 0.5f, 0.5f);
		FString Flags;
		if (LastRoll.bPityTriggered) { Flags += TEXT("PITY GUARANTEE   "); }
		if (LastRoll.bDuplicateRerolled) { Flags += TEXT("DUPLICATE PROTECTION"); }
		DrawStr(Flags, RX + CardW * 0.5f, RY + Sc(106.f), Gold(), SmallFont(), UIScale * 0.8f, 0.5f, 0.5f);

		// Reveal flash in the rarity colour.
		if (RollRevealAge < 0.6f)
		{
			FillRect(RX, RY, CardW, CardH, WithAlpha(Color, 0.75f * (1.f - RollRevealAge / 0.6f)));
		}
	}
	else
	{
		DrawPanelBox(RX, RY, CardW, CardH, 0.5f, false);
		DrawStr(TEXT("Your result appears here."), RX + CardW * 0.5f, RY + CardH * 0.5f, Dim(), SmallFont(), UIScale, 0.5f, 0.5f);
	}
	RY += CardH + Sc(18.f);

	// History (last 10 of this category, newest first).
	DrawStr(TEXT("HISTORY"), RX, RY, Gold(), SmallFont(), UIScale);
	RY += Sc(22.f);
	const TArray<FMTRollHistoryEntry>& History = Prog->GetRollHistory();
	int32 Shown = 0;
	for (int32 i = History.Num() - 1; i >= 0 && Shown < 10; --i)
	{
		const FMTRollHistoryEntry& Entry = History[i];
		if (Entry.Category != RollCategory)
		{
			continue;
		}
		// Hide the result being revealed so the history does not spoil the animation.
		if (bRollAnimating && Shown == 0 && i == History.Num() - 1)
		{
			++Shown;
			continue;
		}
		if (RY > Y + H - Sc(20.f))
		{
			break;
		}
		FString Line = FString::Printf(TEXT("%s  -  %s%s%s"), *RollSys->GetEntryDisplayName(Entry.Category, Entry.ResultId).ToString(),
			*MTUtil::RarityToString(Entry.Rarity), Entry.bWasNew ? TEXT("  -  NEW") : TEXT(""), Entry.bPityTriggered ? TEXT("  -  pity") : TEXT(""));
		DrawStr(Line, RX, RY, MTUtil::RarityColor(Entry.Rarity), SmallFont(), UIScale * 0.9f);
		RY += Sc(20.f);
		++Shown;
	}
	if (Shown == 0)
	{
		DrawStr(TEXT("No rolls yet."), RX, RY, Dim(), SmallFont(), UIScale * 0.9f);
	}
}

void AMTHUD::DoRoll()
{
	if (bRollAnimating)
	{
		FinishRollAnimation();
		return;
	}
	UMTRollSubsystem* RollSys = UMTRollSubsystem::Get(this);
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	if (!RollSys || !Prog)
	{
		return;
	}
	if (!RollSys->CanRoll(RollCategory))
	{
		ShowCenterMessage(LOCTEXT("NoSpins", "No spins left - earn more by questing"), 2.f);
		return;
	}

	// The roll resolves (and is saved in progression) immediately; the animation is presentation only.
	const FMTRollResult Result = RollSys->Roll(RollCategory);
	if (!Result.bValid)
	{
		return;
	}
	LastRoll = Result;
	bHasLastRoll = true;

	if (Prog->GetSettings().bSkipRollAnimation)
	{
		bRollAnimating = false;
		RollRevealAge = 0.f;
		return;
	}

	TArray<FName> Ids;
	TArray<EMTRarity> Rarities;
	TArray<float> Odds;
	RollSys->GetPool(RollCategory, Ids, Rarities, Odds);
	RollReel.Reset();
	for (const FName& Id : Ids)
	{
		RollReel.Add(RollSys->GetEntryDisplayName(RollCategory, Id).ToString());
	}
	int32 ResultIndex = Ids.IndexOfByKey(Result.ResultId);
	if (ResultIndex == INDEX_NONE)
	{
		ResultIndex = RollReel.Add(Result.DisplayName.ToString());
	}
	const int32 N = FMath::Max(1, RollReel.Num());
	// The reel lands exactly on the result after RollReelTicks steps.
	RollReelStart = ((ResultIndex - RollReelTicks) % N + N) % N;
	RollAnimTime = 0.f;
	bRollAnimating = true;
}

void AMTHUD::FinishRollAnimation()
{
	if (bRollAnimating)
	{
		bRollAnimating = false;
		RollRevealAge = 0.f;
	}
}

// ============================================================================ Fast travel

void AMTHUD::FastTravelTo(FName LocationId)
{
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	APawn* Pawn = GetOwningPawn();
	const FMTLocationData* Loc = Registry ? Registry->FindLocation(LocationId) : nullptr;
	if (!Loc || !Prog || !Pawn || !Loc->bFastTravel || !Prog->IsLocationDiscovered(LocationId) || Prog->GetAdventurerRank() < Loc->RequiredRank)
	{
		return;
	}
	const FVector Destination = Loc->WorldLocation + FVector(0.f, 0.f, 200.f);
	if (!Pawn->TeleportTo(Destination, Pawn->GetActorRotation()))
	{
		Pawn->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	}
	CloseAll();
	ShowCenterMessage(FText::Format(LOCTEXT("Travelled", "Travelled to {0}"), Loc->DisplayName.IsEmpty() ? FText::FromName(LocationId) : Loc->DisplayName), 2.f);
}

// ============================================================================ NPC dialog

void AMTHUD::DrawNPCDialog()
{
	HitBoxPriority = 10;
	UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const TArray<FName> Offers = Quests ? Quests->GetAvailableQuestsForNPC(DialogNpcId) : TArray<FName>();
	const TArray<FName> TurnIns = Quests ? Quests->GetTurnInQuestsForNPC(DialogNpcId) : TArray<FName>();
	if (!Offers.Contains(DialogQuestId) && !TurnIns.Contains(DialogQuestId))
	{
		DialogQuestId = TurnIns.Num() > 0 ? TurnIns[0] : (Offers.Num() > 0 ? Offers[0] : NAME_None);
	}

	const float W = FMath::Min(Canvas->ClipX - Sc(120.f), Sc(1150.f));
	const float H = FMath::Min(Canvas->ClipY * 0.5f, Sc(400.f));
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY - H - Sc(36.f);
	FillRect(0.f, 0.f, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.f, 0.f, 0.02f, 0.3f));
	DrawPanelBox(X, Y, W, H, 0.92f, true);

	// Name plate.
	const FString Name = DialogNpcName.ToString();
	const FVector2D NameSize = MeasureStr(Name, MediumFont(), UIScale * 0.8f);
	FillRect(X + Sc(24.f), Y - Sc(18.f), static_cast<float>(NameSize.X) + Sc(32.f), Sc(36.f), PanelColor(1.f));
	StrokeRect(X + Sc(24.f), Y - Sc(18.f), static_cast<float>(NameSize.X) + Sc(32.f), Sc(36.f), Gold());
	DrawStr(Name, X + Sc(40.f), Y, Gold(), MediumFont(), UIScale * 0.8f, 0.f, 0.5f);

	const float Inner = Sc(24.f);
	const float ListW = W * 0.3f;
	float LY = Y + Sc(34.f);

	if (!DialogQuestId.IsNone())
	{
		for (const FName& QuestId : TurnIns)
		{
			DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("DlgSel:") + QuestId.ToString()), TEXT("[Turn in] ") + GetQuestTitle(QuestId),
				X + Inner, LY, ListW, Sc(34.f), true, QuestId == DialogQuestId, Good());
			LY += Sc(40.f);
		}
		for (const FName& QuestId : Offers)
		{
			DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("DlgSel:") + QuestId.ToString()), TEXT("[New] ") + GetQuestTitle(QuestId),
				X + Inner, LY, ListW, Sc(34.f), true, QuestId == DialogQuestId, Gold());
			LY += Sc(40.f);
		}
	}

	const float DX = DialogQuestId.IsNone() ? X + Inner : X + Inner + ListW + Sc(24.f);
	const float DW = X + W - Inner - DX;
	float DY = Y + Sc(34.f);
	const float BtnY = Y + H - Sc(56.f);
	const float BtnW = Sc(170.f);

	if (DialogQuestId.IsNone())
	{
		DrawWrapped(TEXT("\"Nothing I need help with right now, adventurer. Check back later - and mind the roads.\""), DX, DY, DW, Parchment(), SmallFont(), UIScale * 1.1f, 4);
		DrawButtonBox(TEXT("DlgClose"), TEXT("FAREWELL"), X + W - Inner - BtnW, BtnY, BtnW, Sc(40.f));
		HitBoxPriority = 0;
		return;
	}

	const bool bTurnIn = TurnIns.Contains(DialogQuestId);
	const FMTQuestData* Quest = Registry ? Registry->FindQuest(DialogQuestId) : nullptr;
	DrawStr(GetQuestTitle(DialogQuestId), DX, DY, Gold(), MediumFont(), UIScale * 0.75f);
	DY += LineHeight(MediumFont(), UIScale * 0.75f) + Sc(6.f);
	if (Quest)
	{
		const TArray<FText>& Lines = bTurnIn ? Quest->CompleteDialogue : Quest->OfferDialogue;
		FString Dialogue;
		for (const FText& Line : Lines)
		{
			Dialogue += (Dialogue.IsEmpty() ? TEXT("") : TEXT("\n")) + FString(TEXT("\"")) + Line.ToString() + TEXT("\"");
		}
		if (Dialogue.IsEmpty())
		{
			Dialogue = Quest->Summary.ToString();
		}
		DY += DrawWrapped(Dialogue, DX, DY, DW, Parchment(), SmallFont(), UIScale * 1.05f, 5) + Sc(8.f);
		if (!bTurnIn)
		{
			FString Objectives;
			for (const FMTQuestObjective& Objective : Quest->Objectives)
			{
				Objectives += (Objectives.IsEmpty() ? TEXT("- ") : TEXT("\n- ")) + Objective.Description.ToString();
				if (Quest->bSequentialObjectives)
				{
					break; // sequential stories reveal one step at a time
				}
			}
			DY += DrawWrapped(Objectives, DX, DY, DW, Dim(), SmallFont(), UIScale, 3) + Sc(6.f);
		}
		DrawWrapped(TEXT("Rewards: ") + MTHUDMenusPrivate::RewardSummary(Quest->Reward), DX, DY, DW, Gold(0.9f), SmallFont(), UIScale, 2);
	}

	if (bTurnIn)
	{
		DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("DlgTurnIn:") + DialogQuestId.ToString()), TEXT("TURN IN"), X + W - Inner - 2.f * BtnW - Sc(12.f), BtnY, BtnW, Sc(40.f), true, true);
		DrawButtonBox(TEXT("DlgClose"), TEXT("LATER"), X + W - Inner - BtnW, BtnY, BtnW, Sc(40.f));
	}
	else
	{
		DrawButtonBox(MTHUDMenusPrivate::MakeId(TEXT("DlgAccept:") + DialogQuestId.ToString()), TEXT("ACCEPT"), X + W - Inner - 2.f * BtnW - Sc(12.f), BtnY, BtnW, Sc(40.f), true, true);
		DrawButtonBox(TEXT("DlgDecline"), TEXT("DECLINE"), X + W - Inner - BtnW, BtnY, BtnW, Sc(40.f));
	}
	HitBoxPriority = 0;
}

// ============================================================================ Button dispatch

void AMTHUD::HandleButton(FName Id)
{
	TArray<FString> Parts;
	Id.ToString().ParseIntoArray(Parts, TEXT(":"), false);
	if (Parts.Num() == 0)
	{
		return;
	}
	const FString& Cmd = Parts[0];
	const FString Arg1 = Parts.IsValidIndex(1) ? Parts[1] : FString();
	const FString Arg2 = Parts.IsValidIndex(2) ? Parts[2] : FString();
	UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this);

	if (Cmd == TEXT("Tab"))
	{
		const int32 Page = FMath::Clamp(FCString::Atoi(*Arg1), static_cast<int32>(EMTMenuPage::Character), static_cast<int32>(EMTMenuPage::Roll));
		OpenPageInternal(static_cast<EMTMenuPage>(Page));
	}
	else if (Cmd == TEXT("SelChar")) { SelCharacter = FName(*Arg1); }
	else if (Cmd == TEXT("SelElem")) { SelElement = static_cast<EMTElement>(FCString::Atoi(*Arg1)); }
	else if (Cmd == TEXT("SelRace")) { SelRace = static_cast<EMTRace>(FCString::Atoi(*Arg1)); bSelRaceValid = true; }
	else if (Cmd == TEXT("SelQuest")) { SelQuest = FName(*Arg1); }
	else if (Cmd == TEXT("EquipChar"))
	{
		if (Prog && Prog->EquipCharacter(FName(*Arg1)))
		{
			ShowCenterMessage(FText::Format(LOCTEXT("EquippedChar", "{0} equipped"), Prog->GetCharacterDisplayName(FName(*Arg1))), 1.5f);
		}
	}
	else if (Cmd == TEXT("EquipElem"))
	{
		const int32 SlotIndex = FCString::Atoi(*Arg1);
		const EMTElement Element = static_cast<EMTElement>(FCString::Atoi(*Arg2));
		if (Prog && Prog->EquipElement(SlotIndex, Element))
		{
			ShowCenterMessage(FText::Format(LOCTEXT("EquippedElem", "{0} set in slot {1}"), Prog->GetElementDisplayName(Element),
				FText::FromString(SlotIndex == 0 ? TEXT("A") : TEXT("B"))), 1.5f);
		}
	}
	else if (Cmd == TEXT("EquipRace"))
	{
		const EMTRace Race = static_cast<EMTRace>(FCString::Atoi(*Arg1));
		if (Prog && Prog->EquipRace(Race))
		{
			ShowCenterMessage(FText::Format(LOCTEXT("EquippedRace", "{0} equipped"), Prog->GetRaceDisplayName(Race)), 1.5f);
		}
	}
	else if (Cmd == TEXT("Track")) { if (Quests) { Quests->SetTrackedQuest(FName(*Arg1)); } }
	else if (Cmd == TEXT("Travel")) { FastTravelTo(FName(*Arg1)); }
	else if (Cmd == TEXT("Set")) { AdjustSetting(Arg1, FCString::Atoi(*Arg2)); }
	else if (Cmd == TEXT("Save"))
	{
		UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(this);
		const bool bOk = Save && Save->SaveGame(Save->GetCurrentSlot());
		ShowCenterMessage(bOk ? LOCTEXT("Saved", "Game saved") : LOCTEXT("SaveFail", "Save failed"), 1.5f);
	}
	else if (Cmd == TEXT("Load"))
	{
		UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(this);
		const bool bOk = Save && Save->LoadGame(Save->GetCurrentSlot());
		ShowCenterMessage(bOk ? LOCTEXT("Loaded", "Game loaded") : LOCTEXT("LoadFail", "No save to load"), 1.5f);
	}
	else if (Cmd == TEXT("RollCat"))
	{
		if (!bRollAnimating)
		{
			RollCategory = static_cast<EMTRollCategory>(FMath::Clamp(FCString::Atoi(*Arg1), 0, 2));
		}
	}
	else if (Cmd == TEXT("Roll")) { DoRoll(); }
	else if (Cmd == TEXT("RollSkip")) { FinishRollAnimation(); }
	else if (Cmd == TEXT("DlgSel")) { DialogQuestId = FName(*Arg1); }
	else if (Cmd == TEXT("DlgAccept"))
	{
		const FName QuestId(*Arg1);
		if (Quests && Quests->AcceptQuest(QuestId))
		{
			ShowCenterMessage(FText::Format(LOCTEXT("Accepted", "Quest accepted: {0}"), FText::FromString(GetQuestTitle(QuestId))), 2.f);
			if (Quests->GetTrackedQuest().IsNone())
			{
				Quests->SetTrackedQuest(QuestId);
			}
		}
		DialogQuestId = NAME_None;
	}
	else if (Cmd == TEXT("DlgTurnIn"))
	{
		const FName QuestId(*Arg1);
		if (Quests && Quests->TurnInQuest(QuestId))
		{
			ShowCenterMessage(FText::Format(LOCTEXT("TurnedIn", "Quest complete: {0}"), FText::FromString(GetQuestTitle(QuestId))), 2.f);
		}
		DialogQuestId = NAME_None;
	}
	else if (Cmd == TEXT("DlgDecline") || Cmd == TEXT("DlgClose"))
	{
		bDialogOpen = false;
		DialogNpcId = NAME_None;
		DialogQuestId = NAME_None;
		FocusedButton = NAME_None;
	}
}

#undef LOCTEXT_NAMESPACE

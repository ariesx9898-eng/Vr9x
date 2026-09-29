#include "UI/LaPlace/SMTJournal.h"
#include "UI/LaPlace/MTUIStyle.h"
#include "UI/LaPlace/MTFrontEndSubsystem.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTDataTypes.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Progression/MTRollSubsystem.h"
#include "Quests/MTQuestSubsystem.h"
#include "Save/MTSaveSubsystem.h"
#include "Save/MTSaveTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/SlateBrush.h"

#define LOCTEXT_NAMESPACE "MTJournal"

// Helpers live in a named namespace: the module is built as a unity build, and anonymous-namespace names from the
// other LA PLACE widgets would clash with these.
namespace MTJ
{
	// Content area of every page, in design units.
	constexpr float Left = 60.f;
	constexpr float Top = 188.f;
	constexpr float Bottom = 1006.f;

	struct FPageInfo
	{
		EMTMenuPage Page;
		const TCHAR* Label;
		const TCHAR* Key;
	};

	static const FPageInfo Pages[] = {
		{ EMTMenuPage::Character, TEXT("CHARACTER"), TEXT("C") },
		{ EMTMenuPage::Element, TEXT("ELEMENT"), TEXT("") },
		{ EMTMenuPage::Race, TEXT("RACE"), TEXT("") },
		{ EMTMenuPage::Mastery, TEXT("MASTERY"), TEXT("") },
		{ EMTMenuPage::Inventory, TEXT("INVENTORY"), TEXT("I") },
		{ EMTMenuPage::Quests, TEXT("QUESTS"), TEXT("J") },
		{ EMTMenuPage::Map, TEXT("MAP"), TEXT("M") },
		{ EMTMenuPage::Party, TEXT("PARTY"), TEXT("") },
		{ EMTMenuPage::Settings, TEXT("SETTINGS"), TEXT("") },
		{ EMTMenuPage::Roll, TEXT("ROLL"), TEXT("K") },
	};

	static const TCHAR* const Sep = TEXT("  ·  ");

	static UMTProgressionSubsystem* GetProgression(const TWeakObjectPtr<APlayerController>& PC)
	{
		return PC.IsValid() ? UMTProgressionSubsystem::Get(PC.Get()) : nullptr;
	}

	static const UMTDataRegistry* GetRegistry(const TWeakObjectPtr<APlayerController>& PC)
	{
		return PC.IsValid() ? UMTDataRegistry::Get(PC.Get()) : nullptr;
	}

	static UMTQuestSubsystem* GetQuests(const TWeakObjectPtr<APlayerController>& PC)
	{
		return PC.IsValid() ? UMTQuestSubsystem::Get(PC.Get()) : nullptr;
	}

	static UMTRollSubsystem* GetRolls(const TWeakObjectPtr<APlayerController>& PC)
	{
		return PC.IsValid() ? UMTRollSubsystem::Get(PC.Get()) : nullptr;
	}

	static UMTSaveSubsystem* GetSave(const TWeakObjectPtr<APlayerController>& PC)
	{
		return PC.IsValid() ? UMTSaveSubsystem::Get(PC.Get()) : nullptr;
	}

	static FString BehaviorLabel(EMTAbilityBehavior B)
	{
		switch (B)
		{
		case EMTAbilityBehavior::Projectile: return TEXT("Projectile");
		case EMTAbilityBehavior::Zone: return TEXT("Area");
		case EMTAbilityBehavior::Sequence: return TEXT("Barrage");
		case EMTAbilityBehavior::Dash: return TEXT("Movement");
		case EMTAbilityBehavior::Counter: return TEXT("Counter");
		case EMTAbilityBehavior::Buff: return TEXT("Empowerment");
		case EMTAbilityBehavior::Structure: return TEXT("Barrier");
		case EMTAbilityBehavior::Melee: return TEXT("Martial");
		}
		return TEXT("Technique");
	}

	static FString QuestTypeLabel(EMTQuestType Type)
	{
		switch (Type)
		{
		case EMTQuestType::Basic: return TEXT("Errand");
		case EMTQuestType::Combat: return TEXT("Hunt");
		case EMTQuestType::Elite: return TEXT("Elite hunt");
		case EMTQuestType::Boss: return TEXT("Boss");
		case EMTQuestType::Story: return TEXT("Story");
		case EMTQuestType::WorldEvent: return TEXT("World event");
		}
		return TEXT("Quest");
	}

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

	static TArray<FString> RewardParts(const FMTQuestReward& Reward, const UMTDataRegistry* Registry)
	{
		TArray<FString> Parts;
		if (Reward.XP > 0) { Parts.Add(FString::Printf(TEXT("%d XP"), Reward.XP)); }
		if (Reward.Gold > 0) { Parts.Add(FString::Printf(TEXT("%d gold"), Reward.Gold)); }
		if (Reward.AdventurerPoints > 0) { Parts.Add(FString::Printf(TEXT("%d adventurer points"), Reward.AdventurerPoints)); }
		if (Reward.CharacterSpins > 0) { Parts.Add(FString::Printf(TEXT("%d character spin%s"), Reward.CharacterSpins, Reward.CharacterSpins > 1 ? TEXT("s") : TEXT(""))); }
		if (Reward.ElementSpins > 0) { Parts.Add(FString::Printf(TEXT("%d element spin%s"), Reward.ElementSpins, Reward.ElementSpins > 1 ? TEXT("s") : TEXT(""))); }
		if (Reward.RaceSpins > 0) { Parts.Add(FString::Printf(TEXT("%d race spin%s"), Reward.RaceSpins, Reward.RaceSpins > 1 ? TEXT("s") : TEXT(""))); }
		if (Reward.MasteryXP > 0.f) { Parts.Add(FString::Printf(TEXT("%d mastery XP"), FMath::RoundToInt(Reward.MasteryXP))); }
		for (const TPair<FName, int32>& Item : Reward.Items)
		{
			const FMTItemData* Data = Registry ? Registry->FindItem(Item.Key) : nullptr;
			const FString Name = Data && !Data->DisplayName.IsEmpty() ? Data->DisplayName.ToString() : Item.Key.ToString();
			Parts.Add(FString::Printf(TEXT("%s x%d"), *Name, Item.Value));
		}
		return Parts;
	}

	static FString QuestTitle(const UMTDataRegistry* Registry, FName QuestId)
	{
		const FMTQuestData* Quest = Registry ? Registry->FindQuest(QuestId) : nullptr;
		return Quest && !Quest->Title.IsEmpty() ? Quest->Title.ToString() : QuestId.ToString();
	}

	static FString ShortAbilityName(FString Name)
	{
		Name.RemoveFromStart(TEXT("Dragon God Style: "));
		Name.RemoveFromStart(TEXT("Awakening: "));
		return Name;
	}

	static FString Initials(const FString& Name)
	{
		TArray<FString> Words;
		Name.ParseIntoArrayWS(Words);
		FString Out;
		for (const FString& Word : Words)
		{
			if (Out.Len() < 2 && Word.Len() > 0 && FChar::IsAlpha(Word[0]))
			{
				Out.AppendChar(FChar::ToUpper(Word[0]));
			}
		}
		return Out.IsEmpty() ? FString(TEXT("?")) : Out;
	}

	/** "DragonGodKnowledge" -> "Dragon God Knowledge". */
	static FString SpaceWords(const FString& Id)
	{
		FString Out;
		for (int32 i = 0; i < Id.Len(); ++i)
		{
			const TCHAR C = Id[i];
			if (i > 0 && FChar::IsUpper(C) && !FChar::IsUpper(Id[i - 1]) && Id[i - 1] != TEXT('_'))
			{
				Out.AppendChar(TEXT(' '));
			}
			Out.AppendChar(C == TEXT('_') ? TEXT(' ') : C);
		}
		return Out;
	}

	/**
	 * Passive texts read "Name (note): effect" or "Name: effect". Splits them so the page shows a readable name instead of
	 * the id (DragonGodKnowledge), the note as a quiet aside and the effect as the body.
	 */
	static void SplitPassive(FName PassiveId, const FString& Description, FString& OutName, FString& OutNote, FString& OutBody)
	{
		const FString Id = PassiveId.ToString();
		OutName = SpaceWords(Id);
		OutNote.Reset();
		OutBody = Description;
		int32 Colon = INDEX_NONE;
		if (!Description.FindChar(TEXT(':'), Colon) || Colon <= 0 || Colon > 100)
		{
			return;
		}
		FString Head = Description.Left(Colon).TrimStartAndEnd();
		FString Note;
		int32 Paren = INDEX_NONE;
		if (Head.FindChar(TEXT('('), Paren) && Head.EndsWith(TEXT(")")))
		{
			Note = Head.Mid(Paren + 1, Head.Len() - Paren - 2).TrimStartAndEnd();
			Head = Head.Left(Paren).TrimStartAndEnd();
		}
		// Only treat the prefix as the passive's name when it matches the id ("Dragon ..." for DragonGodKnowledge).
		TArray<FString> Words;
		Head.ParseIntoArrayWS(Words);
		if (Words.Num() == 0 || !Id.StartsWith(Words[0].Replace(TEXT("'"), TEXT(""))))
		{
			return;
		}
		OutName = Head;
		OutNote = Note;
		OutBody = Description.Mid(Colon + 1).TrimStartAndEnd();
		if (OutBody.Len() > 0)
		{
			OutBody[0] = FChar::ToUpper(OutBody[0]);
		}
	}

	/** A bright UI colour darkened so it reads as ink on parchment (linear value ~0.12, about 0.38 on screen). */
	static FLinearColor Ink(const FLinearColor& Color)
	{
		const FLinearColor HSV = Color.LinearRGBToHSV();
		FLinearColor Out = FLinearColor(HSV.R, FMath::Min(1.f, HSV.G * 1.15f), HSV.B * 0.13f, 1.f).HSVToLinearRGB();
		Out.A = Color.A;
		return Out;
	}

	static FString FitText(const FString& Text, const FSlateFontInfo& Font, float MaxWidth)
	{
		if (MaxWidth <= 0.f || MTUI::Measure(Text, Font).X <= MaxWidth)
		{
			return Text;
		}
		FString Out = Text;
		while (Out.Len() > 1 && MTUI::Measure(Out + TEXT("..."), Font).X > MaxWidth)
		{
			Out.LeftChopInline(1);
		}
		return Out.TrimEnd() + TEXT("...");
	}

	static FString AbilityMeta(const FMTAbilityData& A)
	{
		TArray<FString> Parts;
		Parts.Add(A.Element == EMTElement::None ? FString(TEXT("TECHNIQUE")) : MTUtil::ElementToString(A.Element).ToUpper());
		Parts.Add(BehaviorLabel(A.Behavior).ToUpper());
		Parts.Add(A.ManaCost > 0.5f ? FString::Printf(TEXT("%d MANA"), FMath::RoundToInt(A.ManaCost)) : FString(TEXT("NO MANA")));
		Parts.Add(FString::Printf(TEXT("COOLDOWN %.1f S"), A.Cooldown));
		if (A.bChargeable)
		{
			Parts.Add(TEXT("HOLD TO CHARGE"));
		}
		return FString::Join(Parts, Sep);
	}

	static const FSlateBrush* Portrait(const FMTCharacterData* C)
	{
		if (!C)
		{
			return nullptr;
		}
		if (const FSlateBrush* B = MTUI::UIBrush(TEXT("Art"), FString::Printf(TEXT("T_Portrait_%s"), *C->CharacterID.ToString())))
		{
			return B;
		}
		if (const FSlateBrush* B = MTUI::Brush(C->Portrait.ToString()))
		{
			return B;
		}
		return MTUI::AbilityIcon(C->AwakeningAbility);
	}

	/** Draws Brush over the whole box, cropping the image (no letterbox). */
	static void DrawCover(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FSlateBrush* Brush, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Tint)
	{
		if (!Brush || Size.X <= 1.0 || Size.Y <= 1.0)
		{
			return;
		}
		const float ImageAspect = Brush->ImageSize.X / FMath::Max(1.f, (float)Brush->ImageSize.Y);
		const float BoxAspect = (float)(Size.X / Size.Y);
		FSlateBrush Cropped = *Brush;
		if (ImageAspect > BoxAspect)
		{
			const float U = BoxAspect / ImageAspect;
			Cropped.SetUVRegion(FBox2f(FVector2f((1.f - U) * 0.5f, 0.f), FVector2f((1.f + U) * 0.5f, 1.f)));
		}
		else
		{
			const float V = ImageAspect / FMath::Max(0.01f, BoxAspect);
			Cropped.SetUVRegion(FBox2f(FVector2f(0.f, (1.f - V) * 0.5f), FVector2f(1.f, (1.f + V) * 0.5f)));
		}
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Pos))), &Cropped, ESlateDrawEffect::None, Tint);
	}

	/** Draws Brush inside the box at its own aspect ratio (letterboxed). */
	static void DrawContain(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FSlateBrush* Brush, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Tint)
	{
		if (!Brush)
		{
			return;
		}
		const float Aspect = Brush->ImageSize.X / FMath::Max(1.f, (float)Brush->ImageSize.Y);
		FVector2D ImageSize(Size.Y * Aspect, Size.Y);
		if (ImageSize.X > Size.X)
		{
			ImageSize = FVector2D(Size.X, Size.X / Aspect);
		}
		MTUI::Box(Out, Layer, G, Pos + (Size - ImageSize) * 0.5f, ImageSize, Tint, Brush);
	}

	static void Outline(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color, float Thickness = 1.f)
	{
		TArray<FVector2f> Points = { FVector2f(Pos), FVector2f(Pos.X + Size.X, Pos.Y), FVector2f(Pos + Size), FVector2f(Pos.X, Pos.Y + Size.Y), FVector2f(Pos) };
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	static void DiamondOutline(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness = 1.5f)
	{
		TArray<FVector2f> Points = {
			FVector2f(Center.X, Center.Y - Radius), FVector2f(Center.X + Radius, Center.Y), FVector2f(Center.X, Center.Y + Radius),
			FVector2f(Center.X - Radius, Center.Y), FVector2f(Center.X, Center.Y - Radius) };
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	/** Solid diamond (two triangles through the renderer, like MTUI::RadialWipe). */
	static void FillDiamond(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Center, float Radius, const FLinearColor& Color)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*MTUI::White());
		const FSlateRenderTransform& T = G.GetAccumulatedRenderTransform();
		const FColor C = Color.ToFColor(true);
		const FVector2f P(Center);
		TArray<FSlateVertex> Verts;
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, P + FVector2f(0.f, -Radius), FVector2f(0.5f, 0.5f), C));
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, P + FVector2f(Radius, 0.f), FVector2f(0.5f, 0.5f), C));
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, P + FVector2f(0.f, Radius), FVector2f(0.5f, 0.5f), C));
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, P + FVector2f(-Radius, 0.f), FVector2f(0.5f, 0.5f), C));
		TArray<SlateIndex> Indexes;
		Indexes.Add(0);
		Indexes.Add(1);
		Indexes.Add(2);
		Indexes.Add(0);
		Indexes.Add(2);
		Indexes.Add(3);
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Handle, Verts, Indexes, nullptr, 0, 0);
	}

	static void Chevron(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Center, float Half, bool bUp, const FLinearColor& Color, float Thickness)
	{
		const float Dir = bUp ? -1.f : 1.f;
		TArray<FVector2f> Points = {
			FVector2f(Center.X - Half, Center.Y - Dir * Half * 0.5f), FVector2f(Center.X, Center.Y + Dir * Half * 0.5f), FVector2f(Center.X + Half, Center.Y - Dir * Half * 0.5f) };
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	static TArray<FName> OwnedCharacters(const UMTProgressionSubsystem* Prog, const UMTDataRegistry* Registry)
	{
		TArray<FName> Owned;
		for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
		{
			if (Prog->OwnsCharacter(Pair.Key))
			{
				Owned.Add(Pair.Key);
			}
		}
		// Rarest first, then by name.
		Owned.Sort([Registry](const FName& A, const FName& B)
		{
			const FMTCharacterData* DA = Registry->FindCharacter(A);
			const FMTCharacterData* DB = Registry->FindCharacter(B);
			const int32 RA = DA ? (int32)DA->Rarity : 0;
			const int32 RB = DB ? (int32)DB->Rarity : 0;
			return RA != RB ? RA > RB : A.LexicalLess(B);
		});
		return Owned;
	}

	static TArray<EMTRace> OwnedRaces(const UMTProgressionSubsystem* Prog)
	{
		TArray<EMTRace> Owned;
		for (int32 i = 0; i <= (int32)EMTRace::SeaRace; ++i)
		{
			if (Prog->OwnsRace((EMTRace)i))
			{
				Owned.Add((EMTRace)i);
			}
		}
		return Owned;
	}

	/** World position (cm) to 0..1 on the painted world map (Docs/LaPlace/Spec.md section 2). */
	static FVector2D WorldToMapUV(const FVector& World)
	{
		return FVector2D(World.X / 1828800.0 + 0.5, World.Y / 1371600.0 + 0.5);
	}

	static bool InsideUnit(const FVector2D& UV)
	{
		return UV.X >= 0.0 && UV.X <= 1.0 && UV.Y >= 0.0 && UV.Y <= 1.0;
	}
}

// ---------------------------------------------------------------------------------------------
// Setup, state and input
// ---------------------------------------------------------------------------------------------

void SMTJournal::Construct(const FArguments& InArgs)
{
	PlayerController = InArgs._PlayerController;
	HUD = InArgs._HUD;
	SetVisibility(EVisibility::Collapsed);
	for (int32 i = 0; i < 36; ++i)
	{
		FMote M;
		M.Pos = FVector2D(FMath::FRand() * 1920.f, FMath::FRand() * 1080.f);
		M.Vel = FVector2D(FMath::FRandRange(-8.f, 8.f), FMath::FRandRange(-30.f, -12.f));
		M.Size = FMath::FRandRange(2.f, 5.f);
		M.MaxLife = FMath::FRandRange(5.f, 10.f);
		M.Life = FMath::FRand() * M.MaxLife;
		M.Hue = FMath::FRand();
		Motes.Add(M);
	}
}

double SMTJournal::SlateNow() const
{
	// The widget does not tick while it is collapsed, so read Slate's clock directly when (re)opening.
	return FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCurrentTime() : Now;
}

void SMTJournal::OpenPage(EMTMenuPage NewPage)
{
	if (NewPage == EMTMenuPage::None)
	{
		CloseAll();
		return;
	}
	const bool bWasOpen = IsOpen();
	if (NewPage != EMTMenuPage::Roll)
	{
		StopRollReveal();
	}
	bDialogOpen = false;
	DialogNpcId = NAME_None;
	DialogQuestId = NAME_None;
	const bool bChanged = NewPage != Page;
	Page = NewPage;
	Now = SlateNow();
	if (bChanged || !bWasOpen)
	{
		PageSince = Now;
		FocusKey = NAME_None;
		FocusHit = FHit();
		HoverHit = FHit();
		Hover.Reset();
		Scroll.Reset();
		DraggingSetting = NAME_None;
	}
	if (!bWasOpen)
	{
		OpenedAt = Now;
		MTUI::Sound(TEXT("ui_open"), 0.5f);
	}
	else if (bChanged)
	{
		MTUI::Sound(TEXT("ui_tab"), 0.45f);
	}
	ValidateSelections();
	SetVisibility(EVisibility::Visible);
}

void SMTJournal::OpenDialog(FName NpcId, const FText& NpcName)
{
	StopRollReveal();
	const bool bWasOpen = IsOpen();
	Page = EMTMenuPage::None;
	bDialogOpen = true;
	DialogNpcId = NpcId;
	DialogNpcName = NpcName.IsEmpty() ? FText::FromName(NpcId) : NpcName;
	DialogQuestId = NAME_None;
	FocusKey = NAME_None;
	FocusHit = FHit();
	HoverHit = FHit();
	Hover.Reset();
	Now = SlateNow();
	PageSince = Now;
	if (!bWasOpen)
	{
		OpenedAt = Now;
	}
	MTUI::Sound(TEXT("ui_open"), 0.45f);
	ValidateSelections();
	SetVisibility(EVisibility::Visible);
}

void SMTJournal::StopRollReveal()
{
	// Leaving the roll page never replays the reveal; the result is already saved and shows the next time it opens.
	if (bRollAnimating)
	{
		bRollAnimating = false;
		RollRevealAt = SlateNow() - 10.0;
	}
}

void SMTJournal::CloseAll()
{
	StopRollReveal();
	Page = EMTMenuPage::None;
	bDialogOpen = false;
	DialogNpcId = NAME_None;
	DialogQuestId = NAME_None;
	FocusKey = NAME_None;
	FocusHit = FHit();
	HoveredKey = NAME_None;
	HoverHit = FHit();
	DraggingSetting = NAME_None;
	bShowFocus = false;
	Hits.Reset();
	ScrollRegions.Reset();
	SetVisibility(EVisibility::Collapsed);
}

FName SMTJournal::HoverKey(EAction Action, FName Param, int32 Index)
{
	return FName(*FString::Printf(TEXT("%d|%s|%d"), (int32)Action, *Param.ToString(), Index));
}

const SMTJournal::FHit* SMTJournal::FindHit(FName Key) const
{
	if (Key.IsNone())
	{
		return nullptr;
	}
	for (const FHit& Hit : Hits)
	{
		if (HoverKey(Hit.Action, Hit.Param, Hit.Index) == Key)
		{
			return &Hit;
		}
	}
	return nullptr;
}

void SMTJournal::AddHit(const FVector2D& Pos, const FVector2D& Size, EAction Action, FName Param, int32 Index, bool bEnabled) const
{
	FHit Hit;
	Hit.Rect = FSlateRect(Pos.X, Pos.Y, Pos.X + Size.X, Pos.Y + Size.Y);
	Hit.Action = Action;
	Hit.Param = Param;
	Hit.Index = Index;
	Hit.bEnabled = bEnabled;
	Hits.Add(Hit);
}

float SMTJournal::HoverAlpha(EAction Action, FName Param, int32 Index) const
{
	const float* A = Hover.Find(HoverKey(Action, Param, Index));
	return A ? *A : 0.f;
}

float SMTJournal::DS(const FGeometry& G) const
{
	const FVector2D Size = G.GetLocalSize();
	return FMath::Min(Size.X / 1920.f, Size.Y / 1080.f);
}

FVector2D SMTJournal::D(const FGeometry& G, float X, float Y) const
{
	const FVector2D Size = G.GetLocalSize();
	const float S = DS(G);
	return FVector2D((Size.X - 1920.f * S) * 0.5f + X * S, (Size.Y - 1080.f * S) * 0.5f + (Y + SlideY) * S);
}

void SMTJournal::Navigate(FIntPoint Direction)
{
	if (!IsOpen() || (Direction.X == 0 && Direction.Y == 0))
	{
		return;
	}
	bShowFocus = true;
	const FHit* Current = FindHit(FocusKey);
	if (!Current || !Current->bEnabled)
	{
		// Start inside the page rather than on the tabs.
		const FHit* First = nullptr;
		for (const FHit& Hit : Hits)
		{
			if (Hit.bEnabled && Hit.Action != EAction::Tab && Hit.Action != EAction::Close)
			{
				First = &Hit;
				break;
			}
		}
		for (int32 i = 0; !First && i < Hits.Num(); ++i)
		{
			First = Hits[i].bEnabled ? &Hits[i] : nullptr;
		}
		if (First)
		{
			FocusKey = HoverKey(First->Action, First->Param, First->Index);
			FocusHit = *First;
			MTUI::Sound(TEXT("ui_hover"), 0.25f);
		}
		return;
	}
	const FVector2D From((Current->Rect.Left + Current->Rect.Right) * 0.5f, (Current->Rect.Top + Current->Rect.Bottom) * 0.5f);
	const FVector2D Dir = FVector2D((double)Direction.X, (double)Direction.Y).GetSafeNormal();
	double BestScore = TNumericLimits<double>::Max();
	const FHit* Best = nullptr;
	for (const FHit& Hit : Hits)
	{
		if (!Hit.bEnabled || &Hit == Current)
		{
			continue;
		}
		const FVector2D To((Hit.Rect.Left + Hit.Rect.Right) * 0.5f, (Hit.Rect.Top + Hit.Rect.Bottom) * 0.5f);
		const FVector2D Delta = To - From;
		const double Along = FVector2D::DotProduct(Delta, Dir);
		if (Along <= 1.0)
		{
			continue;
		}
		const double Perp = FMath::Abs(FVector2D::CrossProduct(Delta, Dir));
		const double Score = Along + Perp * 2.5;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = &Hit;
		}
	}
	if (Best)
	{
		FocusKey = HoverKey(Best->Action, Best->Param, Best->Index);
		FocusHit = *Best;
		MTUI::Sound(TEXT("ui_hover"), 0.25f);
	}
}

void SMTJournal::Confirm()
{
	if (!IsOpen())
	{
		return;
	}
	if (bRollAnimating)
	{
		FinishRollAnimation();
		return;
	}
	const FHit* Focused = FindHit(FocusKey);
	if (Focused && Focused->bEnabled)
	{
		const FHit Hit = *Focused;
		MTUI::Sound(TEXT("ui_click"), 0.55f);
		HandleAction(Hit);
	}
}

void SMTJournal::Back()
{
	if (!IsOpen())
	{
		return;
	}
	if (bRollAnimating)
	{
		FinishRollAnimation();
		return;
	}
	MTUI::Sound(TEXT("ui_close"), 0.5f);
	CloseAll();
}

void SMTJournal::NextTab(int32 Dir)
{
	if (bDialogOpen || Page == EMTMenuPage::None)
	{
		return;
	}
	const int32 Count = UE_ARRAY_COUNT(MTJ::Pages);
	int32 Current = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		if (MTJ::Pages[i].Page == Page)
		{
			Current = i;
		}
	}
	const int32 Next = ((Current + (Dir >= 0 ? 1 : -1)) % Count + Count) % Count;
	OpenPage(MTJ::Pages[Next].Page);
}

void SMTJournal::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = InCurrentTime;
	if (!IsOpen())
	{
		return;
	}
	ValidateSelections();

	// Hover eases toward the entry under the mouse and, after keyboard / gamepad navigation, the focused one.
	FName Under;
	HoverHit = FHit();
	for (int32 i = Hits.Num() - 1; i >= 0; --i)
	{
		if (Hits[i].Rect.ContainsPoint(Mouse))
		{
			Under = HoverKey(Hits[i].Action, Hits[i].Param, Hits[i].Index);
			HoverHit = Hits[i];
			break;
		}
	}
	if (Under != HoveredKey && !Under.IsNone() && HoverHit.bEnabled)
	{
		MTUI::Sound(TEXT("ui_hover"), 0.25f);
	}
	HoveredKey = Under;
	if (!Under.IsNone() && HoverHit.bEnabled)
	{
		Hover.FindOrAdd(Under);
	}
	if (bShowFocus && !FocusKey.IsNone())
	{
		Hover.FindOrAdd(FocusKey);
	}
	for (auto It = Hover.CreateIterator(); It; ++It)
	{
		const bool bOn = (It.Key() == Under && HoverHit.bEnabled) || (bShowFocus && It.Key() == FocusKey);
		It.Value() = FMath::FInterpTo(It.Value(), bOn ? 1.f : 0.f, InDeltaTime, 12.f);
		if (!bOn && It.Value() < 0.002f)
		{
			It.RemoveCurrent();
		}
	}

	for (FMote& M : Motes)
	{
		M.Life += InDeltaTime;
		M.Pos += M.Vel * InDeltaTime;
		M.Pos.X += FMath::Sin((M.Life + M.Hue * 10.f) * 0.9f) * 6.f * InDeltaTime;
		if (M.Life >= M.MaxLife || M.Pos.Y < -20.f)
		{
			M.Pos = FVector2D(FMath::FRand() * 1920.f, 1080.f + 10.f);
			M.Vel = FVector2D(FMath::FRandRange(-8.f, 8.f), FMath::FRandRange(-45.f, -15.f));
			M.Life = 0.f;
			M.MaxLife = FMath::FRandRange(6.f, 12.f);
			M.Size = FMath::FRandRange(2.f, 5.f);
			M.Hue = FMath::FRand();
		}
	}

	// Roll reveal: a soft tick per name on the reel, then the result.
	if (bRollAnimating)
	{
		const float T = FMath::Clamp(float(Now - RollStart) / RollAnimDuration, 0.f, 1.f);
		const float Eased = 1.f - FMath::Pow(1.f - T, 3.f);
		const int32 Step = FMath::Min(RollReelTicks, FMath::FloorToInt(Eased * RollReelTicks));
		if (Step != LastReelStep)
		{
			LastReelStep = Step;
			MTUI::Sound(TEXT("ui_tab"), 0.18f);
		}
		if (T >= 1.f)
		{
			FinishRollAnimation();
		}
	}
}

FReply SMTJournal::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D NewMouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (!NewMouse.Equals(Mouse, 0.5))
	{
		bShowFocus = false; // the mouse takes over the highlight until the next key press
	}
	Mouse = NewMouse;
	if (!DraggingSetting.IsNone())
	{
		if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			for (const FHit& Hit : Hits)
			{
				if (Hit.Action == EAction::Slider && Hit.Param == DraggingSetting)
				{
					SetSettingAlpha(Hit.Param, FMath::Clamp((Mouse.X - Hit.Rect.Left) / FMath::Max(1.f, Hit.Rect.Right - Hit.Rect.Left), 0.f, 1.f));
				}
			}
		}
		else
		{
			DraggingSetting = NAME_None;
		}
	}
	return FReply::Handled();
}

FReply SMTJournal::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Every click is handled here while the journal is open, so none reaches the game (no attack under the menu).
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}
	Mouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	bool bSkippedReveal = false;
	if (bRollAnimating)
	{
		FinishRollAnimation(); // any click skips the reveal
		bSkippedReveal = true;
	}
	for (int32 i = Hits.Num() - 1; i >= 0; --i)
	{
		if (!Hits[i].Rect.ContainsPoint(Mouse))
		{
			continue;
		}
		const FHit Hit = Hits[i];
		if (bSkippedReveal && (Hit.Action == EAction::Roll || Hit.Action == EAction::RollSkip))
		{
			return FReply::Handled(); // the click that skips must not start another roll
		}
		if (!Hit.bEnabled)
		{
			MTUI::Sound(TEXT("ui_error"), 0.3f);
			return FReply::Handled();
		}
		FocusKey = HoverKey(Hit.Action, Hit.Param, Hit.Index);
		FocusHit = Hit;
		if (Hit.Action == EAction::Slider)
		{
			DraggingSetting = Hit.Param;
			SetSettingAlpha(Hit.Param, FMath::Clamp((Mouse.X - Hit.Rect.Left) / FMath::Max(1.f, Hit.Rect.Right - Hit.Rect.Left), 0.f, 1.f));
			return FReply::Handled();
		}
		MTUI::Sound(TEXT("ui_click"), 0.55f);
		HandleAction(Hit);
		return FReply::Handled();
	}
	return FReply::Handled();
}

FReply SMTJournal::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	DraggingSetting = NAME_None;
	return FReply::Handled();
}

FReply SMTJournal::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Mouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	for (const FScrollRegion& Region : ScrollRegions)
	{
		if (Region.Rect.ContainsPoint(Mouse))
		{
			int32& Value = Scroll.FindOrAdd(Region.List);
			Value = FMath::Clamp(Value - (MouseEvent.GetWheelDelta() > 0.f ? 1 : -1), 0, Region.Max);
			break;
		}
	}
	return FReply::Handled();
}

void SMTJournal::ValidateSelections()
{
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	if (!Prog || !Registry)
	{
		return;
	}
	if (bDialogOpen)
	{
		UMTQuestSubsystem* Quests = MTJ::GetQuests(PlayerController);
		const TArray<FName> Offers = Quests ? Quests->GetAvailableQuestsForNPC(DialogNpcId) : TArray<FName>();
		const TArray<FName> TurnIns = Quests ? Quests->GetTurnInQuestsForNPC(DialogNpcId) : TArray<FName>();
		if (!Offers.Contains(DialogQuestId) && !TurnIns.Contains(DialogQuestId))
		{
			DialogQuestId = TurnIns.Num() > 0 ? TurnIns[0] : (Offers.Num() > 0 ? Offers[0] : NAME_None);
		}
		return;
	}
	switch (Page)
	{
	case EMTMenuPage::Character:
	{
		const TArray<FName> Owned = MTJ::OwnedCharacters(Prog, Registry);
		if (!Owned.Contains(SelCharacter))
		{
			SelCharacter = Owned.Contains(Prog->GetEquippedCharacter()) ? Prog->GetEquippedCharacter() : (Owned.Num() > 0 ? Owned[0] : NAME_None);
			InspectAbility = NAME_None;
		}
		break;
	}
	case EMTMenuPage::Element:
		if (!Registry->FindElement(SelElement))
		{
			SelElement = Registry->FindElement(Prog->GetEquippedElement(0)) ? Prog->GetEquippedElement(0) : EMTElement::None;
			for (const TPair<EMTElement, FMTElementData>& Pair : Registry->GetElements())
			{
				if (SelElement == EMTElement::None)
				{
					SelElement = Pair.Key;
				}
			}
		}
		break;
	case EMTMenuPage::Race:
	{
		const TArray<EMTRace> Owned = MTJ::OwnedRaces(Prog);
		if (!bSelRaceValid || !Owned.Contains(SelRace))
		{
			SelRace = Owned.Contains(Prog->GetEquippedRace()) ? Prog->GetEquippedRace() : (Owned.Num() > 0 ? Owned[0] : EMTRace::Human);
			bSelRaceValid = Owned.Num() > 0;
		}
		break;
	}
	case EMTMenuPage::Quests:
		if (UMTQuestSubsystem* Quests = MTJ::GetQuests(PlayerController))
		{
			const TArray<FName> Active = Quests->GetActiveQuestIds();
			const TArray<FName> Completed = Quests->GetCompletedQuestIds();
			if (SelQuest.IsNone() || (!Active.Contains(SelQuest) && !Completed.Contains(SelQuest)))
			{
				const FName Tracked = Quests->GetTrackedQuest();
				SelQuest = Active.Contains(Tracked) ? Tracked : (Active.Num() > 0 ? Active[0] : (Completed.Num() > 0 ? Completed[0] : NAME_None));
			}
		}
		break;
	case EMTMenuPage::Map:
		if (SelLocation.IsNone() || !Prog->IsLocationDiscovered(SelLocation) || !Registry->FindLocation(SelLocation))
		{
			// The discovered waystone nearest to the player.
			const APawn* Pawn = PlayerController.IsValid() ? PlayerController->GetPawn() : nullptr;
			double Best = TNumericLimits<double>::Max();
			SelLocation = NAME_None;
			for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
			{
				if (!Pair.Value.bFastTravel || !Prog->IsLocationDiscovered(Pair.Key))
				{
					continue;
				}
				const double Dist = Pawn ? FVector::DistSquared2D(Pawn->GetActorLocation(), Pair.Value.WorldLocation) : 0.0;
				if (Dist < Best)
				{
					Best = Dist;
					SelLocation = Pair.Key;
				}
			}
		}
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// Building blocks
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::Button(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FVector2D& Size,
	const FString& Label, EAction Action, FName Param, int32 Index, bool bPrimary, bool bSelected, float FontSize, bool bEnabled) const
{
	const float S = DS(G);
	const float H = bEnabled ? HoverAlpha(Action, Param, Index) : 0.f;
	const bool bLit = bPrimary && bEnabled;
	FLinearColor Fill = bLit ? FLinearColor(0.32f + 0.12f * H, 0.19f + 0.08f * H, 0.05f, 0.95f) : FLinearColor(0.04f + 0.05f * H, 0.035f + 0.04f * H, 0.03f, 0.88f);
	if (bSelected && !bLit)
	{
		Fill = FLinearColor(0.15f, 0.095f, 0.03f, 0.92f);
	}
	if (H > 0.01f || bLit)
	{
		MTUI::Glow(Out, Layer, G, Pos + Size * 0.5f, FMath::Max(Size.X, Size.Y) * (0.55f + 0.05f * H), MTUI::Gold.CopyWithNewOpacity((bLit ? 0.18f : 0.f) + 0.22f * H));
	}
	MTUI::Box(Out, Layer + 1, G, Pos, Size, Fill);
	MTUI::Box(Out, Layer + 1, G, Pos, FVector2D(Size.X, Size.Y * 0.45f), FLinearColor(1.f, 0.9f, 0.7f, 0.04f + 0.04f * H));
	MTUI::GoldFrame(Out, Layer + 2, G, Pos, Size, bSelected ? 1.f : (bEnabled ? 0.7f + 0.3f * H : 0.35f), false);
	FLinearColor TextColor = FLinearColor::LerpUsingHSV(MTUI::TextLight, MTUI::GoldBright, H);
	if (bSelected)
	{
		TextColor = MTUI::GoldBright.CopyWithNewOpacity(bEnabled ? 1.f : 0.85f);
	}
	else if (!bEnabled)
	{
		TextColor = MTUI::TextLight.CopyWithNewOpacity(0.45f);
	}
	const FSlateFontInfo Font = MTUI::Heading(FontSize * S);
	MTUI::TextAligned(Out, Layer + 3, G, MTJ::FitText(Label, Font, Size.X - 18.f * S), Font, Pos, Size, FVector2D(0.5f, 0.5f), TextColor);
	AddHit(Pos, Size, Action, Param, Index, bEnabled);
	return Layer + 5;
}

int32 SMTJournal::ListCard(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FVector2D& Size, const FSlateBrush* Icon, bool bIconSlot,
	const FString& Title, const FString& Subtitle, const FLinearColor& Accent, EAction Action, FName Param, int32 Index, bool bSelected, const FString& Tag) const
{
	const float S = DS(G);
	const float H = HoverAlpha(Action, Param, Index);
	if (bSelected || H > 0.01f)
	{
		MTUI::Glow(Out, Layer, G, Pos + Size * 0.5f, Size.X * 0.56f, MTUI::Gold.CopyWithNewOpacity(bSelected ? 0.15f + 0.05f * H : 0.12f * H));
	}
	MTUI::Box(Out, Layer + 1, G, Pos, Size, bSelected ? FLinearColor(0.1f, 0.068f, 0.03f, 0.93f) : FLinearColor(0.02f + 0.035f * H, 0.02f + 0.028f * H, 0.026f, 0.84f));
	MTUI::Box(Out, Layer + 1, G, Pos, FVector2D(Size.X, Size.Y * 0.45f), FLinearColor(1.f, 0.9f, 0.7f, 0.03f + 0.03f * H));
	MTUI::Box(Out, Layer + 2, G, Pos, FVector2D(4.f * S, Size.Y), Accent);

	const bool bLarge = Size.Y >= 90.f * S;
	float TX = Pos.X + 20.f * S;
	if (bIconSlot)
	{
		const float Pad = (bLarge ? 12.f : 9.f) * S;
		const float IconSize = Size.Y - 2.f * Pad;
		const FVector2D IconPos(Pos.X + 16.f * S, Pos.Y + Pad);
		MTUI::Box(Out, Layer + 2, G, IconPos, FVector2D(IconSize), FLinearColor(0.f, 0.f, 0.f, 0.8f));
		if (Icon)
		{
			MTJ::DrawCover(Out, Layer + 3, G, Icon, IconPos + FVector2D(2.f * S), FVector2D(IconSize - 4.f * S), FLinearColor::White);
		}
		else
		{
			MTUI::Box(Out, Layer + 3, G, IconPos + FVector2D(2.f * S), FVector2D(IconSize - 4.f * S), (Accent * 0.35f).CopyWithNewOpacity(1.f));
			MTUI::TextAligned(Out, Layer + 4, G, MTJ::Initials(Title), MTUI::Heading(IconSize * 0.34f), IconPos, FVector2D(IconSize), FVector2D(0.5f, 0.5f), MTUI::TextLight);
		}
		MTUI::GoldFrame(Out, Layer + 5, G, IconPos, FVector2D(IconSize), bSelected ? 1.f : 0.65f + 0.35f * H, false);
		TX = IconPos.X + IconSize + 16.f * S;
	}
	const float Right = Pos.X + Size.X - 14.f * S;
	if (!Tag.IsEmpty())
	{
		// The tag sits on the card's top edge, like the key tags on hotbar slots, so it never shortens the title.
		const float TagW = ChipWidth(G, Tag, 11.f);
		Chip(G, Out, Layer + 6, FVector2D(Right - TagW, Pos.Y - 10.f * S), Tag, MTUI::GoldBright, false, 11.f);
	}
	const FSlateFontInfo TitleFont = MTUI::Heading((bLarge ? 22.f : 18.f) * S);
	const FSlateFontInfo SubFont = MTUI::BodyItalic((bLarge ? 19.f : 16.f) * S);
	const float TitleH = MTUI::Measure(TEXT("Ag"), TitleFont).Y;
	const float SubH = Subtitle.IsEmpty() ? 0.f : (float)MTUI::Measure(TEXT("Ag"), SubFont).Y;
	const float TextTop = Pos.Y + (Size.Y - TitleH - SubH * 0.95f) * 0.5f;
	const FLinearColor TitleColor = bSelected ? MTUI::GoldBright : FLinearColor::LerpUsingHSV(MTUI::TextLight, MTUI::GoldBright, H);
	MTUI::Text(Out, Layer + 6, G, MTJ::FitText(Title, TitleFont, Right - TX), TitleFont, FVector2D(TX, TextTop), TitleColor);
	if (!Subtitle.IsEmpty())
	{
		MTUI::Text(Out, Layer + 6, G, MTJ::FitText(Subtitle, SubFont, Pos.X + Size.X - 14.f * S - TX), SubFont, FVector2D(TX, TextTop + TitleH * 0.98f),
			MTUI::TextLight.CopyWithNewOpacity(0.78f));
	}
	MTUI::GoldFrame(Out, Layer + 5, G, Pos, Size, bSelected ? 1.f : 0.4f + 0.5f * H, false);
	AddHit(Pos, Size, Action, Param, Index, true);
	return Layer + 8;
}

float SMTJournal::ChipWidth(const FGeometry& G, const FString& Label, float FontSize) const
{
	const float S = DS(G);
	return MTUI::Measure(Label, MTUI::Heading(FontSize * S)).X + 14.f * S;
}

float SMTJournal::Chip(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FString& Label, const FLinearColor& Color, bool bOnParchment, float FontSize) const
{
	const float S = DS(G);
	const FSlateFontInfo Font = MTUI::Heading(FontSize * S);
	const FVector2D TextSize = MTUI::Measure(Label, Font);
	const FVector2D Size(TextSize.X + 14.f * S, TextSize.Y + 3.f * S);
	MTUI::Box(Out, Layer, G, Pos, Size, bOnParchment ? Color.CopyWithNewOpacity(0.12f) : FLinearColor(0.035f, 0.025f, 0.015f, 0.92f));
	MTJ::Outline(Out, Layer + 1, G, Pos, Size, Color.CopyWithNewOpacity(0.85f), 1.f);
	MTUI::TextAligned(Out, Layer + 1, G, Label, Font, Pos, Size, FVector2D(0.5f, 0.5f), Color, false);
	return Size.X;
}

void SMTJournal::Section(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, float Width, const FString& Label, bool bOnParchment, float FontSize) const
{
	const float S = DS(G);
	const FSlateFontInfo Font = MTUI::Heading(FontSize * S);
	MTUI::Text(Out, Layer, G, Label, Font, Pos, bOnParchment ? MTUI::InkSoft : MTUI::GoldBright, !bOnParchment);
	const FVector2D TextSize = MTUI::Measure(Label, Font);
	if (Width > TextSize.X + 40.f * S)
	{
		const float LineY = Pos.Y + TextSize.Y * 0.52f;
		const FVector2D From(Pos.X + TextSize.X + 14.f * S, LineY);
		const FVector2D To(Pos.X + Width, LineY);
		MTUI::Line(Out, Layer, G, From, To, (bOnParchment ? MTUI::InkSoft : MTUI::Gold).CopyWithNewOpacity(bOnParchment ? 0.35f : 0.5f), 1.f);
		MTJ::FillDiamond(Out, Layer + 1, G, To, 3.5f * S, bOnParchment ? MTUI::GoldDim : MTUI::GoldBright);
	}
}

void SMTJournal::AbilityIcon(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, float Size, FName AbilityId, float FrameOpacity) const
{
	const float Inset = FMath::Max(2.f, Size * 0.035f);
	MTUI::Box(Out, Layer, G, Pos, FVector2D(Size), FLinearColor(0.f, 0.f, 0.f, 0.8f));
	if (AbilityId.IsNone())
	{
		MTUI::TextAligned(Out, Layer + 1, G, TEXT("-"), MTUI::Heading(Size * 0.3f), Pos, FVector2D(Size), FVector2D(0.5f, 0.5f), MTUI::GoldDim);
		MTUI::GoldFrame(Out, Layer + 3, G, Pos, FVector2D(Size), FrameOpacity * 0.6f, false);
		return;
	}
	if (const FSlateBrush* Icon = MTUI::AbilityIcon(AbilityId))
	{
		MTUI::Box(Out, Layer + 1, G, Pos + FVector2D(Inset), FVector2D(Size - 2.f * Inset), FLinearColor::White, Icon);
	}
	else
	{
		const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
		const FMTAbilityData* A = Registry ? Registry->FindAbility(AbilityId) : nullptr;
		const FLinearColor Tile = MTUtil::ElementColor(A ? A->Element : EMTElement::None) * 0.45f;
		MTUI::Box(Out, Layer + 1, G, Pos + FVector2D(Inset), FVector2D(Size - 2.f * Inset), Tile.CopyWithNewOpacity(1.f));
		const FString Name = A ? MTJ::ShortAbilityName(A->DisplayName.ToString()) : AbilityId.ToString();
		MTUI::TextAligned(Out, Layer + 2, G, MTJ::Initials(Name), MTUI::Heading(Size * 0.3f), Pos, FVector2D(Size), FVector2D(0.5f, 0.5f), MTUI::TextLight);
	}
	MTUI::GoldFrame(Out, Layer + 3, G, Pos, FVector2D(Size), FrameOpacity, false);
}

void SMTJournal::KeyTag(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& SlotPos, const FString& Key) const
{
	const float S = DS(G);
	const FSlateFontInfo KeyFont = MTUI::Heading(13.f * S);
	const FVector2D KeySize = MTUI::Measure(Key, KeyFont) + FVector2D(10.f * S, 3.f * S);
	const FVector2D KeyPos = SlotPos + FVector2D(-6.f * S, -9.f * S);
	MTUI::Box(Out, Layer, G, KeyPos, KeySize, FLinearColor(0.05f, 0.035f, 0.02f, 0.95f));
	MTUI::GoldFrame(Out, Layer + 1, G, KeyPos, KeySize, 0.9f, false);
	MTUI::TextAligned(Out, Layer + 2, G, Key, KeyFont, KeyPos, KeySize, FVector2D(0.5f, 0.5f), MTUI::GoldBright, false);
}

float SMTJournal::AbilityRow(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, float Width, FName AbilityId, const FString& Tag, int32 MaxLines) const
{
	const float S = DS(G);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	const FMTAbilityData* A = Registry ? Registry->FindAbility(AbilityId) : nullptr;
	const float IconSize = 72.f * S;
	AbilityIcon(G, Out, Layer, Pos, IconSize, AbilityId, 0.9f);
	const float TX = Pos.X + IconSize + 18.f * S;
	const float TW = Pos.X + Width - TX;
	float NameW = TW;
	if (!Tag.IsEmpty())
	{
		const float TagW = ChipWidth(G, Tag, 12.f);
		Chip(G, Out, Layer + 4, FVector2D(Pos.X + Width - TagW, Pos.Y + 2.f * S), Tag, MTJ::Ink(MTUI::Gold), true, 12.f);
		NameW -= TagW + 12.f * S;
	}
	if (!A)
	{
		MTUI::Text(Out, Layer + 4, G, AbilityId.ToString(), MTUI::Heading(19.f * S), FVector2D(TX, Pos.Y), MTUI::InkSoft, false);
		return IconSize + 16.f * S;
	}
	const FSlateFontInfo NameFont = MTUI::Heading(20.f * S);
	const FSlateFontInfo MetaFont = MTUI::Heading(12.f * S);
	MTUI::Text(Out, Layer + 4, G, MTJ::FitText(A->DisplayName.ToString(), NameFont, NameW), NameFont, FVector2D(TX, Pos.Y - 1.f * S), MTUI::Ink, false);
	MTUI::Text(Out, Layer + 4, G, MTJ::FitText(MTJ::AbilityMeta(*A), MetaFont, TW), MetaFont, FVector2D(TX, Pos.Y + 27.f * S), MTUI::InkSoft, false);
	const float DescH = Paragraph(G, Out, Layer + 4, A->Description.ToString(), MTUI::Body(17.f * S), FVector2D(TX, Pos.Y + 45.f * S), TW, MTUI::Ink, MaxLines, 1.f);
	return FMath::Max(IconSize, 45.f * S + DescH) + 16.f * S;
}

float SMTJournal::Paragraph(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FString& Text, const FSlateFontInfo& Font, const FVector2D& Pos, float Width,
	const FLinearColor& Color, int32 MaxLines, float LineSpacing) const
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	TArray<FString> Lines;
	FString Current;
	for (const FString& Word : Words)
	{
		const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
		if (!Current.IsEmpty() && MTUI::Measure(Candidate, Font).X > Width)
		{
			Lines.Add(Current);
			Current = Word;
		}
		else
		{
			Current = Candidate;
		}
	}
	if (!Current.IsEmpty())
	{
		Lines.Add(Current);
	}
	if (MaxLines > 0 && Lines.Num() > MaxLines)
	{
		Lines.SetNum(MaxLines);
		FString& Last = Lines.Last();
		while (Last.Len() > 1 && MTUI::Measure(Last + TEXT("..."), Font).X > Width)
		{
			Last.LeftChopInline(1);
		}
		Last = Last.TrimEnd() + TEXT("...");
	}
	// Light text sits on dark panels and gets a shadow; ink on parchment does not.
	const bool bShadow = Color.GetLuminance() > 0.45f;
	const float LineHeight = MTUI::Measure(TEXT("Ag"), Font).Y * LineSpacing;
	float Y = Pos.Y;
	for (const FString& Line : Lines)
	{
		MTUI::Text(Out, Layer, G, Line, Font, FVector2D(Pos.X, Y), Color, bShadow);
		Y += LineHeight;
	}
	return Y - Pos.Y;
}

int32 SMTJournal::ScrollList(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, FName List, int32 Count, int32 Visible, const FVector2D& Pos, const FVector2D& Size, bool bOnParchment) const
{
	const int32 Max = FMath::Max(0, Count - Visible);
	FScrollRegion& Region = ScrollRegions.AddDefaulted_GetRef();
	Region.Rect = FSlateRect(Pos.X, Pos.Y, Pos.X + Size.X, Pos.Y + Size.Y);
	Region.List = List;
	Region.Max = Max;
	const int32 First = FMath::Clamp(Scroll.FindRef(List), 0, Max);
	if (Max <= 0)
	{
		return First;
	}
	// Arrows under the list, right-aligned, with the visible range beside them.
	const float S = DS(G);
	const FVector2D ArrowSize(40.f * S, 30.f * S);
	const FVector2D DownPos(Pos.X + Size.X - ArrowSize.X, Pos.Y + Size.Y + 8.f * S);
	const FVector2D UpPos(DownPos.X - ArrowSize.X - 8.f * S, DownPos.Y);
	for (int32 i = 0; i < 2; ++i)
	{
		const bool bUp = i == 0;
		const FVector2D P = bUp ? UpPos : DownPos;
		const bool bEnabled = bUp ? First > 0 : First < Max;
		const float H = bEnabled ? HoverAlpha(EAction::Scroll, List, bUp ? -1 : 1) : 0.f;
		MTUI::Box(Out, Layer, G, P, ArrowSize, FLinearColor(0.04f + 0.08f * H, 0.03f + 0.05f * H, 0.02f, bEnabled ? 0.9f : 0.5f));
		MTUI::GoldFrame(Out, Layer + 1, G, P, ArrowSize, bEnabled ? 0.6f + 0.4f * H : 0.3f, false);
		MTJ::Chevron(Out, Layer + 2, G, P + ArrowSize * 0.5f, 7.f * S, bUp, (bEnabled ? MTUI::GoldBright : MTUI::GoldDim).CopyWithNewOpacity(bEnabled ? 1.f : 0.6f), 2.f * S);
		AddHit(P, ArrowSize, EAction::Scroll, List, bUp ? -1 : 1, bEnabled);
	}
	const FString Range = FString::Printf(TEXT("%d-%d of %d"), First + 1, FMath::Min(Count, First + Visible), Count);
	const FSlateFontInfo Font = MTUI::BodyItalic(17.f * S);
	const FVector2D RangeSize = MTUI::Measure(Range, Font);
	MTUI::Text(Out, Layer + 1, G, Range, Font, FVector2D(UpPos.X - RangeSize.X - 12.f * S, UpPos.Y + (ArrowSize.Y - RangeSize.Y) * 0.5f),
		bOnParchment ? MTUI::InkSoft : MTUI::TextLight.CopyWithNewOpacity(0.75f), !bOnParchment);
	return First;
}

// ---------------------------------------------------------------------------------------------
// Painting: frame
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	Hits.Reset();
	ScrollRegions.Reset();
	SlideY = 0.f;
	if (!IsOpen())
	{
		return LayerId;
	}
	int32 Layer = LayerId;
	if (bDialogOpen)
	{
		Layer = PaintDialog(G, Out, Layer);
		return PaintNotice(G, Out, Layer + 1, D(G, 960.f, 574.f));
	}
	Layer = PaintFrame(G, Out, Layer);
	// Page content rises a little as it appears.
	const float T = FMath::Clamp(float(Now - PageSince) / 0.3f, 0.f, 1.f);
	SlideY = 18.f * FMath::Pow(1.f - T, 3.f);
	switch (Page)
	{
	case EMTMenuPage::Character: Layer = PaintCharacter(G, Out, Layer); break;
	case EMTMenuPage::Element: Layer = PaintElement(G, Out, Layer); break;
	case EMTMenuPage::Race: Layer = PaintRace(G, Out, Layer); break;
	case EMTMenuPage::Mastery: Layer = PaintMastery(G, Out, Layer); break;
	case EMTMenuPage::Inventory: Layer = PaintInventory(G, Out, Layer); break;
	case EMTMenuPage::Quests: Layer = PaintQuests(G, Out, Layer); break;
	case EMTMenuPage::Map: Layer = PaintMap(G, Out, Layer); break;
	case EMTMenuPage::Party: Layer = PaintParty(G, Out, Layer); break;
	case EMTMenuPage::Settings: Layer = PaintSettings(G, Out, Layer); break;
	case EMTMenuPage::Roll: Layer = PaintRoll(G, Out, Layer); break;
	default: break;
	}
	SlideY = 0.f;
	return PaintNotice(G, Out, Layer + 1, D(G, 960.f, 1046.f));
}

int32 SMTJournal::PaintMotes(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, float Opacity) const
{
	const float S = DS(G);
	for (const FMote& M : Motes)
	{
		const float A = FMath::Sin(PI * FMath::Clamp(M.Life / M.MaxLife, 0.f, 1.f));
		const FLinearColor C = M.Hue < 0.7f ? MTUI::GoldBright : FLinearColor(0.45f, 0.8f, 1.f);
		MTUI::Glow(Out, Layer, G, D(G, M.Pos.X, M.Pos.Y), M.Size * 3.f * S, C.CopyWithNewOpacity(0.4f * A * Opacity));
	}
	return Layer + 1;
}

int32 SMTJournal::PaintFrame(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const FVector2D Screen = G.GetLocalSize();
	const float Open = FMath::Clamp(float(Now - OpenedAt) / 0.25f, 0.f, 1.f);

	// The world stays visible behind, dimmed, with darker bands behind the header and the key hints.
	MTUI::Box(Out, Layer, G, FVector2D::ZeroVector, Screen, FLinearColor(0.004f, 0.005f, 0.012f, 0.66f * Open));
	TArray<FSlateGradientStop> TopBand;
	TopBand.Add(FSlateGradientStop(FVector2D(0.f, 0.f), FLinearColor(0.f, 0.f, 0.f, 0.6f * Open)));
	TopBand.Add(FSlateGradientStop(FVector2D(0.f, Screen.Y * 0.2f), FLinearColor(0.f, 0.f, 0.f, 0.f)));
	FSlateDrawElement::MakeGradient(Out, Layer, G.ToPaintGeometry(), TopBand, Orient_Horizontal);
	TArray<FSlateGradientStop> BottomBand;
	BottomBand.Add(FSlateGradientStop(FVector2D(0.f, Screen.Y * 0.86f), FLinearColor(0.f, 0.f, 0.f, 0.f)));
	BottomBand.Add(FSlateGradientStop(FVector2D(0.f, Screen.Y), FLinearColor(0.f, 0.f, 0.f, 0.6f * Open)));
	FSlateDrawElement::MakeGradient(Out, Layer, G.ToPaintGeometry(), BottomBand, Orient_Horizontal);
	Layer = PaintMotes(G, Out, Layer + 1, Open);

	// Title.
	MTUI::TextAligned(Out, Layer, G, TEXT("ADVENTURER'S JOURNAL"), MTUI::Title(38.f * S), D(G, 0.f, 16.f), FVector2D(1920.f, 56.f) * S, FVector2D(0.5f, 0.f), MTUI::GoldBright);
	MTUI::Divider(Out, Layer, G, D(G, 960.f, 88.f), 420.f * S);

	// Status plates: level and XP on the left, gold and adventurer rank on the right.
	if (const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController))
	{
		const FVector2D LeftPos = D(G, MTJ::Left, 22.f);
		const FVector2D PlateSize = FVector2D(360.f, 74.f) * S;
		MTUI::DarkPanel(Out, Layer, G, LeftPos, PlateSize, 0.9f, false);
		MTUI::Text(Out, Layer + 3, G, FString::Printf(TEXT("LEVEL %d"), Prog->GetLevel()), MTUI::Heading(22.f * S), LeftPos + FVector2D(18.f, 10.f) * S, MTUI::GoldBright);
		MTUI::TextAligned(Out, Layer + 3, G, FString::Printf(TEXT("%d / %d XP"), Prog->GetXP(), Prog->GetXPToNextLevel()), MTUI::Heading(13.f * S),
			LeftPos + FVector2D(0.f, 16.f) * S, FVector2D(342.f, 20.f) * S, FVector2D(1.f, 0.f), MTUI::TextLight.CopyWithNewOpacity(0.8f));
		const float XPFrac = Prog->GetXPToNextLevel() > 0 ? FMath::Clamp((float)Prog->GetXP() / (float)Prog->GetXPToNextLevel(), 0.f, 1.f) : 0.f;
		MTUI::Bar(Out, Layer + 3, G, LeftPos + FVector2D(18.f, 48.f) * S, FVector2D(324.f, 9.f) * S, XPFrac, XPFrac, MTUI::Gold);

		const FVector2D RightPos = D(G, 1860.f - 360.f, 22.f);
		MTUI::DarkPanel(Out, Layer, G, RightPos, PlateSize, 0.9f, false);
		MTUI::Glow(Out, Layer + 2, G, RightPos + FVector2D(34.f, 37.f) * S, 16.f * S, MTUI::GoldBright.CopyWithNewOpacity(0.85f));
		MTUI::Glow(Out, Layer + 2, G, RightPos + FVector2D(34.f, 37.f) * S, 7.f * S, FLinearColor(1.f, 0.97f, 0.85f, 1.f));
		MTUI::Text(Out, Layer + 3, G, TEXT("GOLD"), MTUI::Heading(12.f * S), RightPos + FVector2D(60.f, 12.f) * S, MTUI::TextLight.CopyWithNewOpacity(0.7f));
		MTUI::Text(Out, Layer + 3, G, FString::FromInt(Prog->GetGold()), MTUI::Heading(24.f * S), RightPos + FVector2D(60.f, 30.f) * S, MTUI::GoldBright);
		MTUI::Text(Out, Layer + 3, G, TEXT("ADVENTURER RANK"), MTUI::Heading(12.f * S), RightPos + FVector2D(178.f, 12.f) * S, MTUI::TextLight.CopyWithNewOpacity(0.7f));
		const FString Rank = MTUtil::AdventurerRankToString(Prog->GetAdventurerRank());
		MTUI::Text(Out, Layer + 3, G, Rank, MTUI::Heading(24.f * S), RightPos + FVector2D(178.f, 30.f) * S, MTUI::GoldBright);
		const float RankW = MTUI::Measure(Rank, MTUI::Heading(24.f * S)).X;
		MTUI::Text(Out, Layer + 3, G, FString::Printf(TEXT("%d pts"), Prog->GetAdventurerPoints()), MTUI::BodyItalic(18.f * S),
			RightPos + FVector2D(178.f, 36.f) * S + FVector2D(RankW + 10.f * S, 0.f), MTUI::TextLight.CopyWithNewOpacity(0.8f));
	}

	// Tabs. The page hotkey (C, I, J, M, K) sits beside the label.
	const int32 NumTabs = UE_ARRAY_COUNT(MTJ::Pages);
	const float TabW = 1800.f / NumTabs;
	for (int32 i = 0; i < NumTabs; ++i)
	{
		const MTJ::FPageInfo& Info = MTJ::Pages[i];
		const bool bActive = Info.Page == Page;
		const float H = HoverAlpha(EAction::Tab, NAME_None, (int32)Info.Page);
		const FVector2D P = D(G, MTJ::Left + i * TabW, 124.f);
		const FVector2D Size = FVector2D(TabW, 42.f) * S;
		if (bActive)
		{
			MTUI::Glow(Out, Layer + 1, G, P + Size * 0.5f, Size.X * 0.5f, MTUI::Gold.CopyWithNewOpacity(0.2f));
		}
		else if (H > 0.01f)
		{
			MTUI::Glow(Out, Layer + 1, G, P + Size * 0.5f, Size.X * 0.46f, MTUI::Gold.CopyWithNewOpacity(0.13f * H));
		}
		const FSlateFontInfo Font = MTUI::Heading((bActive ? 19.f : 17.f) * S);
		const FLinearColor Color = bActive ? MTUI::GoldBright : FLinearColor::LerpUsingHSV(MTUI::TextLight.CopyWithNewOpacity(0.7f), MTUI::GoldBright, H);
		MTUI::TextAligned(Out, Layer + 2, G, Info.Label, Font, P, FVector2D(Size.X, Size.Y - 4.f * S), FVector2D(0.5f, 0.5f), Color);
		if (*Info.Key)
		{
			const FVector2D LabelSize = MTUI::Measure(Info.Label, Font);
			MTUI::Text(Out, Layer + 2, G, Info.Key, MTUI::Heading(11.f * S), P + FVector2D((Size.X + LabelSize.X) * 0.5f + 5.f * S, 6.f * S),
				(bActive ? MTUI::Gold : MTUI::GoldDim).CopyWithNewOpacity(0.9f));
		}
		if (bActive)
		{
			MTUI::Line(Out, Layer + 2, G, P + FVector2D(22.f * S, Size.Y - 3.f * S), P + FVector2D(Size.X - 22.f * S, Size.Y - 3.f * S), MTUI::GoldBright.CopyWithNewOpacity(0.9f), 2.f);
			MTJ::FillDiamond(Out, Layer + 3, G, P + FVector2D(Size.X * 0.5f, Size.Y - 3.f * S), 5.f * S, MTUI::GoldBright);
		}
		AddHit(P, Size, EAction::Tab, NAME_None, (int32)Info.Page);
	}
	MTUI::Line(Out, Layer, G, D(G, MTJ::Left, 167.f), D(G, 1860.f, 167.f), MTUI::Gold.CopyWithNewOpacity(0.3f), 1.f);

	// Key hints (fade while a notice is shown in their place). "ESC close" is also a button.
	const float NoticeT = float(Now - NoticeAt);
	const float HintAlpha = (NoticeT >= 0.f && NoticeT < 2.8f && !NoticeText.IsEmpty()) ? FMath::Clamp(1.f - FMath::Min(NoticeT / 0.15f, (2.8f - NoticeT) / 0.5f), 0.f, 1.f) : 1.f;
	struct FHint { const TCHAR* Key; const TCHAR* Label; };
	const FHint Hints[] = {
		{ TEXT("ESC"), TEXT("close") },
		{ TEXT("Q  /  E"), TEXT("switch page") },
		{ TEXT("ARROWS"), TEXT("move") },
		{ TEXT("ENTER"), TEXT("select") },
	};
	const FSlateFontInfo KeyFont = MTUI::Heading(12.f * S);
	const FSlateFontInfo LabelFont = MTUI::Body(19.f * S);
	float Total = 0.f;
	for (const FHint& Hint : Hints)
	{
		Total += MTUI::Measure(Hint.Key, KeyFont).X + 12.f * S + 8.f * S + MTUI::Measure(Hint.Label, LabelFont).X + 40.f * S;
	}
	float HX = D(G, 960.f, 0.f).X - (Total - 40.f * S) * 0.5f;
	const float HY = D(G, 0.f, 1046.f).Y;
	const int32 NumHints = UE_ARRAY_COUNT(Hints);
	for (int32 i = 0; i < NumHints; ++i)
	{
		const FVector2D KeySize = MTUI::Measure(Hints[i].Key, KeyFont) + FVector2D(12.f * S, 4.f * S);
		const FVector2D LabelSize = MTUI::Measure(Hints[i].Label, LabelFont);
		const float H = i == 0 ? HoverAlpha(EAction::Close) : 0.f;
		const FVector2D KeyPos(HX, HY - KeySize.Y * 0.5f);
		MTUI::Box(Out, Layer, G, KeyPos, KeySize, FLinearColor(0.05f, 0.035f, 0.02f, 0.9f * HintAlpha));
		MTUI::GoldFrame(Out, Layer + 1, G, KeyPos, KeySize, (0.7f + 0.3f * H) * HintAlpha, false);
		MTUI::TextAligned(Out, Layer + 2, G, Hints[i].Key, KeyFont, KeyPos, KeySize, FVector2D(0.5f, 0.5f), MTUI::GoldBright.CopyWithNewOpacity(HintAlpha), false);
		const FLinearColor LabelColor = FLinearColor::LerpUsingHSV(MTUI::TextLight.CopyWithNewOpacity(0.72f), MTUI::GoldBright, H);
		MTUI::Text(Out, Layer + 1, G, Hints[i].Label, LabelFont, FVector2D(KeyPos.X + KeySize.X + 8.f * S, HY - LabelSize.Y * 0.55f), LabelColor.CopyWithNewOpacity(LabelColor.A * HintAlpha));
		if (i == 0 && HintAlpha > 0.5f)
		{
			AddHit(KeyPos - FVector2D(6.f * S), FVector2D(KeySize.X + 8.f * S + LabelSize.X + 12.f * S, KeySize.Y + 12.f * S), EAction::Close);
		}
		HX += KeySize.X + 8.f * S + LabelSize.X + 40.f * S;
	}
	return Layer + 6;
}

int32 SMTJournal::PaintNotice(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Center) const
{
	const float T = float(Now - NoticeAt);
	if (NoticeText.IsEmpty() || T < 0.f || T > 2.8f)
	{
		return Layer;
	}
	const float S = DS(G);
	const float A = FMath::Clamp(FMath::Min(T / 0.15f, (2.8f - T) / 0.5f), 0.f, 1.f);
	const FString Message = NoticeText.ToString();
	const FSlateFontInfo Font = MTUI::Heading(19.f * S);
	const FVector2D TextSize = MTUI::Measure(Message, Font);
	const FVector2D Size(TextSize.X + 72.f * S, TextSize.Y + 18.f * S);
	const FVector2D Pos = Center - Size * 0.5f;
	MTUI::Glow(Out, Layer, G, Center, Size.X * 0.6f, NoticeColor.CopyWithNewOpacity(0.16f * A));
	MTUI::DarkPanel(Out, Layer + 1, G, Pos, Size, A, false);
	MTUI::TextAligned(Out, Layer + 4, G, Message, Font, Pos, Size, FVector2D(0.5f, 0.5f), NoticeColor.CopyWithNewOpacity(A));
	return Layer + 6;
}

int32 SMTJournal::PaintUnavailable(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FString& Message) const
{
	const float S = DS(G);
	const FVector2D P = D(G, 560.f, 420.f);
	const FVector2D Size = FVector2D(800.f, 240.f) * S;
	MTUI::ParchmentPanel(Out, Layer, G, P, Size, 1.f);
	MTUI::TextAligned(Out, Layer + 6, G, Message, MTUI::BodyItalic(26.f * S), P, Size, FVector2D(0.5f, 0.5f), MTUI::InkSoft, false);
	return Layer + 8;
}

// ---------------------------------------------------------------------------------------------
// Character
// ---------------------------------------------------------------------------------------------

FName SMTJournal::GetDetailAbility(FName Fallback) const
{
	if (HoverHit.Action == EAction::Inspect && HoverHit.bEnabled && !HoverHit.Param.IsNone())
	{
		return HoverHit.Param;
	}
	if (bShowFocus && FocusHit.Action == EAction::Inspect && !FocusHit.Param.IsNone())
	{
		return FocusHit.Param;
	}
	return InspectAbility.IsNone() ? Fallback : InspectAbility;
}

int32 SMTJournal::PaintCharacter(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	if (!Prog || !Registry)
	{
		return PaintUnavailable(G, Out, Layer, TEXT("Character data is unavailable."));
	}
	const TArray<FName> Owned = MTJ::OwnedCharacters(Prog, Registry);
	const FMTCharacterData* Data = Registry->FindCharacter(SelCharacter);
	const bool bPlaying = Data && Prog->GetEquippedCharacter() == SelCharacter;

	// Left: the lineages you own, and what to do with the selected one.
	Section(G, Out, Layer, D(G, MTJ::Left, MTJ::Top), 380.f * S, TEXT("LINEAGES"), false);
	float Y = MTJ::Top + 36.f;
	for (const FName& Id : Owned)
	{
		const FMTCharacterData* C = Registry->FindCharacter(Id);
		if (!C || Y > 700.f)
		{
			continue;
		}
		ListCard(G, Out, Layer + 2, D(G, MTJ::Left, Y), FVector2D(380.f, 112.f) * S, MTJ::Portrait(C), true, C->DisplayName.ToString(), C->Title.ToString(),
			MTUtil::RarityColor(C->Rarity), EAction::SelChar, Id, INDEX_NONE, Id == SelCharacter, Prog->GetEquippedCharacter() == Id ? TEXT("PLAYING") : TEXT(""));
		Y += 126.f;
	}
	Paragraph(G, Out, Layer + 2, TEXT("A lineage brings its own techniques, vitals and awakening."), MTUI::BodyItalic(19.f * S), D(G, MTJ::Left + 4.f, Y + 4.f), 372.f * S,
		MTUI::TextLight.CopyWithNewOpacity(0.72f), 3);
	if (Data)
	{
		FString FirstName = Data->DisplayName.ToString();
		int32 Space = INDEX_NONE;
		if (FirstName.FindChar(TEXT(' '), Space))
		{
			FirstName = FirstName.Left(Space);
		}
		Button(G, Out, Layer + 12, D(G, MTJ::Left, 872.f), FVector2D(380.f, 60.f) * S,
			bPlaying ? FString(TEXT("NOW PLAYING")) : FString::Printf(TEXT("PLAY AS %s"), *FirstName.ToUpper()),
			EAction::EquipChar, SelCharacter, INDEX_NONE, !bPlaying, bPlaying, 20.f, !bPlaying);
	}
	// The ABILITIES page edits the lineage you are playing, so this only opens for that one.
	Button(G, Out, Layer + 12, D(G, MTJ::Left, 946.f), FVector2D(380.f, 60.f) * S, TEXT("CHANGE HOTBAR"), EAction::Hotbar, NAME_None, INDEX_NONE, false, false, 20.f, bPlaying);

	// Right: the dossier.
	const FVector2D CardPos = D(G, 470.f, MTJ::Top);
	const FVector2D CardSize = FVector2D(1390.f, MTJ::Bottom - MTJ::Top) * S;
	MTUI::ParchmentPanel(Out, Layer, G, CardPos, CardSize, 1.f);
	const int32 L = Layer + 6;
	if (!Data)
	{
		MTUI::TextAligned(Out, L, G, TEXT("No lineage selected."), MTUI::BodyItalic(26.f * S), CardPos, CardSize, FVector2D(0.5f, 0.5f), MTUI::InkSoft, false);
		return L + 4;
	}

	// Portrait in a gilded frame over a soft light in the lineage's rarity colour, vitals under it.
	const FVector2D PortraitPos = D(G, 514.f, 230.f);
	const FVector2D PortraitSize = FVector2D(310.f, 356.f) * S;
	MTUI::Box(Out, L, G, PortraitPos, PortraitSize, FLinearColor(0.035f, 0.03f, 0.04f, 1.f));
	MTUI::Glow(Out, L + 1, G, PortraitPos + PortraitSize * FVector2D(0.5f, 0.42f), PortraitSize.X * 0.62f, MTUtil::RarityColor(Data->Rarity).CopyWithNewOpacity(0.22f));
	MTJ::DrawContain(Out, L + 2, G, MTJ::Portrait(Data), PortraitPos + FVector2D(4.f * S), PortraitSize - FVector2D(8.f * S), FLinearColor::White);
	MTUI::GoldFrame(Out, L + 3, G, PortraitPos, PortraitSize, 1.f, true, 34.f * S);

	float MaxHealth = 1.f, MaxMana = 1.f, MaxPoise = 1.f, MaxSpeed = 1.f;
	for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
	{
		MaxHealth = FMath::Max(MaxHealth, Pair.Value.MaxHealth);
		MaxMana = FMath::Max(MaxMana, Pair.Value.MaxMana);
		MaxPoise = FMath::Max(MaxPoise, Pair.Value.MaxPoise);
		MaxSpeed = FMath::Max(MaxSpeed, Pair.Value.SprintSpeed);
	}
	struct FStat { const TCHAR* Name; float Value; float Max; FLinearColor Color; };
	const FStat Stats[] = {
		{ TEXT("VITALITY"), Data->MaxHealth, MaxHealth, MTUI::Health },
		{ TEXT("MANA"), Data->MaxMana, MaxMana, MTUI::Mana },
		{ TEXT("POISE"), Data->MaxPoise, MaxPoise, FLinearColor(0.7f, 0.55f, 0.3f, 1.f) },
		{ TEXT("SPEED"), Data->SprintSpeed, MaxSpeed, MTUI::Stamina },
	};
	float StatY = 602.f;
	for (const FStat& Stat : Stats)
	{
		const float Fill = Stat.Value / Stat.Max;
		MTUI::Text(Out, L + 3, G, Stat.Name, MTUI::Heading(12.f * S), D(G, 514.f, StatY), MTUI::InkSoft, false);
		MTUI::Bar(Out, L + 3, G, D(G, 604.f, StatY + 3.f), FVector2D(220.f, 10.f) * S, Fill, Fill, Stat.Color);
		StatY += 24.f;
	}

	// Name, epithet, rarity, lore.
	const float TX = 866.f;
	const float TW = 950.f;
	float TY = 214.f;
	MTUI::Text(Out, L + 3, G, Data->DisplayName.ToString(), MTUI::Title(42.f * S), D(G, TX, TY), MTUI::Ink, false);
	TY += 56.f;
	const FString Epithet = Data->Title.ToString();
	const FSlateFontInfo EpithetFont = MTUI::BodyItalic(26.f * S);
	MTUI::Text(Out, L + 3, G, Epithet, EpithetFont, D(G, TX, TY), MTUI::InkSoft, false);
	const float EpithetW = MTUI::Measure(Epithet, EpithetFont).X / S;
	Chip(G, Out, L + 3, D(G, TX + EpithetW + 18.f, TY + 7.f), MTUtil::RarityToString(Data->Rarity).ToUpper(), MTJ::Ink(MTUtil::RarityColor(Data->Rarity)), true, 13.f);
	TY += 48.f;
	MTUI::Divider(Out, L + 3, G, D(G, TX + TW * 0.5f, TY), 380.f * S, 0.9f);
	TY += 28.f;
	TY += Paragraph(G, Out, L + 3, Data->Description.ToString(), MTUI::Body(18.f * S), D(G, TX, TY), TW * S, MTUI::Ink, 7) / S + 12.f;

	// Passive: a readable name (not the data id), its note, then the effect.
	FString PassiveName, PassiveNote, PassiveBody;
	MTJ::SplitPassive(Data->Passive, Data->PassiveDescription.ToString(), PassiveName, PassiveNote, PassiveBody);
	MTUI::Text(Out, L + 3, G, TEXT("PASSIVE"), MTUI::Heading(13.f * S), D(G, TX, TY + 5.f), MTUI::InkSoft, false);
	const FSlateFontInfo PassiveFont = MTUI::Heading(20.f * S);
	MTUI::Text(Out, L + 3, G, PassiveName, PassiveFont, D(G, TX + 92.f, TY), MTUI::Ink, false);
	if (!PassiveNote.IsEmpty())
	{
		const float NoteX = TX + 92.f + MTUI::Measure(PassiveName, PassiveFont).X / S + 14.f;
		const FSlateFontInfo NoteFont = MTUI::BodyItalic(17.f * S);
		MTUI::Text(Out, L + 3, G, MTJ::FitText(PassiveNote, NoteFont, (TX + TW - NoteX) * S), NoteFont, D(G, NoteX, TY + 4.f), MTUI::InkSoft, false);
	}
	TY += 32.f;
	Paragraph(G, Out, L + 3, PassiveBody, MTUI::Body(18.f * S), D(G, TX, TY), TW * S, MTUI::Ink, 2);

	// Hotbar as it plays: LMB, the four keys you choose, then F and G.
	const float HY = 724.f;
	Section(G, Out, L + 3, D(G, 514.f, HY), 764.f * S, TEXT("HOTBAR"), true);
	const int32 MasteryLevel = Prog->GetMasteryLevel(EMTMasteryTrack::Character, SelCharacter);
	const float MasteryProgress = Prog->GetMasteryProgress(EMTMasteryTrack::Character, SelCharacter);
	MTUI::Text(Out, L + 3, G, FString::Printf(TEXT("MASTERY  %d / %d"), MasteryLevel, UMTProgressionSubsystem::MaxMasteryLevel), MTUI::Heading(14.f * S), D(G, 1330.f, HY + 1.f), MTUI::InkSoft, false);
	MTUI::Bar(Out, L + 3, G, D(G, 1506.f, HY + 6.f), FVector2D(310.f, 10.f) * S, MasteryProgress, MasteryProgress, MTUI::Gold);

	const TArray<FName> Loadout = Prog->GetLoadout(SelCharacter);
	struct FSlotDef { const TCHAR* Key; FName Id; };
	const FSlotDef Slots[7] = {
		{ TEXT("LMB"), Data->BasicAbility },
		{ TEXT("1"), Loadout.IsValidIndex(0) ? Loadout[0] : NAME_None },
		{ TEXT("2"), Loadout.IsValidIndex(1) ? Loadout[1] : NAME_None },
		{ TEXT("3"), Loadout.IsValidIndex(2) ? Loadout[2] : NAME_None },
		{ TEXT("4"), Loadout.IsValidIndex(3) ? Loadout[3] : NAME_None },
		{ TEXT("F"), Data->SpecialAbility },
		{ TEXT("G"), Data->AwakeningAbility },
	};
	const FName Detail = GetDetailAbility(Data->BasicAbility);
	const float Tile = 84.f;
	for (int32 i = 0; i < 7; ++i)
	{
		const FSlotDef& Slot = Slots[i];
		const float X = 522.f + i * (Tile + 22.f) + (i >= 5 ? 26.f : 0.f);
		const FVector2D P = D(G, X, HY + 50.f);
		const FVector2D Size = FVector2D(Tile, Tile) * S;
		const float H = HoverAlpha(EAction::Inspect, Slot.Id, i);
		const bool bShown = !Slot.Id.IsNone() && Slot.Id == Detail;
		if (bShown || H > 0.01f)
		{
			MTUI::Glow(Out, L + 3, G, P + Size * 0.5f, Size.X * 0.85f, MTUI::Gold.CopyWithNewOpacity(bShown ? 0.35f : 0.25f * H));
		}
		AbilityIcon(G, Out, L + 4, P, Size.X, Slot.Id, bShown ? 1.f : 0.6f + 0.4f * H);
		KeyTag(G, Out, L + 8, P, Slot.Key);
		if (const FMTAbilityData* A = Registry->FindAbility(Slot.Id))
		{
			const FSlateFontInfo NameFont = MTUI::Body(16.f * S);
			MTUI::TextAligned(Out, L + 8, G, MTJ::FitText(MTJ::ShortAbilityName(A->DisplayName.ToString()), NameFont, (Tile + 20.f) * S), NameFont,
				P + FVector2D(-10.f * S, Size.Y + 3.f * S), FVector2D((Tile + 20.f) * S, 20.f * S), FVector2D(0.5f, 0.f), MTUI::Ink, false);
		}
		AddHit(P, Size, EAction::Inspect, Slot.Id, i, !Slot.Id.IsNone());
	}
	Paragraph(G, Out, L + 3, TEXT("LMB, F and G come with the lineage. Keys 1-4 are yours: CHANGE HOTBAR."),
		MTUI::BodyItalic(18.f * S), D(G, 516.f, HY + 172.f), 760.f * S, MTUI::InkSoft, 2);

	// The hovered, focused or last clicked slot, described.
	if (const FMTAbilityData* A = Registry->FindAbility(Detail))
	{
		const float DX = 1330.f;
		const float DW = 486.f;
		float DY = HY + 44.f;
		const FSlateFontInfo NameFont = MTUI::Title(24.f * S);
		MTUI::Text(Out, L + 3, G, MTJ::FitText(MTJ::ShortAbilityName(A->DisplayName.ToString()), NameFont, DW * S), NameFont, D(G, DX, DY), MTUI::Ink, false);
		DY += 38.f;
		DY += Paragraph(G, Out, L + 3, MTJ::AbilityMeta(*A), MTUI::Heading(12.f * S), D(G, DX, DY), DW * S, MTUI::InkSoft, 2, 1.15f) / S + 8.f;
		Paragraph(G, Out, L + 3, A->Description.ToString(), MTUI::Body(17.f * S), D(G, DX, DY), DW * S, MTUI::Ink, 5, 1.f);
	}
	return L + 14;
}

// ---------------------------------------------------------------------------------------------
// Element (the schools of magic)
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintElement(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	if (!Prog || !Registry)
	{
		return PaintUnavailable(G, Out, Layer, TEXT("Element data is unavailable."));
	}
	// LA PLACE: every school's spells can go on keys 1-4, so this page lists all of them, not only rolled elements.
	const TArray<FName> Loadout = Prog->GetLoadout(Prog->GetEquippedCharacter());

	Section(G, Out, Layer, D(G, MTJ::Left, MTJ::Top), 380.f * S, TEXT("SCHOOLS OF MAGIC"), false);
	float Y = MTJ::Top + 36.f;
	for (const TPair<EMTElement, FMTElementData>& Pair : Registry->GetElements())
	{
		if (Y > 760.f)
		{
			break;
		}
		const EMTElement E = Pair.Key;
		const FMTElementData& Row = Pair.Value;
		int32 OnBar = 0;
		for (const FName& Id : Row.Abilities)
		{
			OnBar += Loadout.Contains(Id) ? 1 : 0;
		}
		const FString Sub = FString::Printf(TEXT("%s%smastery %d"), *MTUtil::MagicRankToString(Prog->GetMagicRank(E)), MTJ::Sep,
			Prog->GetMasteryLevel(EMTMasteryTrack::Element, UMTProgressionSubsystem::ElementKey(E)));
		ListCard(G, Out, Layer + 2, D(G, MTJ::Left, Y), FVector2D(380.f, 100.f) * S, Row.Abilities.Num() > 0 ? MTUI::AbilityIcon(Row.Abilities[0]) : nullptr, true,
			Prog->GetElementDisplayName(E).ToString(), Sub, MTUtil::ElementColor(E), EAction::SelElem, NAME_None, (int32)E, E == SelElement,
			OnBar > 0 ? FString::Printf(TEXT("%d ON HOTBAR"), OnBar) : FString());
		Y += 112.f;
	}
	Paragraph(G, Out, Layer + 2, TEXT("Every spell of every school can go on keys 1-4."), MTUI::BodyItalic(19.f * S), D(G, MTJ::Left + 4.f, Y + 4.f), 372.f * S,
		MTUI::TextLight.CopyWithNewOpacity(0.72f), 2);
	Button(G, Out, Layer + 12, D(G, MTJ::Left, 946.f), FVector2D(380.f, 60.f) * S, TEXT("CHANGE HOTBAR"), EAction::Hotbar, NAME_None, INDEX_NONE, false, false, 20.f);

	const FVector2D CardPos = D(G, 470.f, MTJ::Top);
	const FVector2D CardSize = FVector2D(1390.f, MTJ::Bottom - MTJ::Top) * S;
	MTUI::ParchmentPanel(Out, Layer, G, CardPos, CardSize, 1.f);
	const int32 L = Layer + 6;
	const FMTElementData* Data = Registry->FindElement(SelElement);
	if (!Data)
	{
		MTUI::TextAligned(Out, L, G, TEXT("Choose a school of magic."), MTUI::BodyItalic(26.f * S), CardPos, CardSize, FVector2D(0.5f, 0.5f), MTUI::InkSoft, false);
		return L + 4;
	}
	const FLinearColor ElemColor = MTUtil::ElementColor(SelElement);
	float TY = 218.f;
	MTUI::Glow(Out, L, G, D(G, 536.f, TY + 30.f), 30.f * S, ElemColor.CopyWithNewOpacity(0.55f));
	MTJ::FillDiamond(Out, L + 1, G, D(G, 536.f, TY + 30.f), 11.f * S, MTJ::Ink(ElemColor));
	const FString Title = FString::Printf(TEXT("%s Magic"), *Prog->GetElementDisplayName(SelElement).ToString());
	const FSlateFontInfo TitleFont = MTUI::Title(42.f * S);
	MTUI::Text(Out, L + 3, G, Title, TitleFont, D(G, 566.f, TY), MTUI::Ink, false);
	Chip(G, Out, L + 3, D(G, 566.f + MTUI::Measure(Title, TitleFont).X / S + 20.f, TY + 16.f), MTUtil::RarityToString(Data->Rarity).ToUpper(),
		MTJ::Ink(MTUtil::RarityColor(Data->Rarity)), true, 13.f);
	TY += 66.f;
	TY += Paragraph(G, Out, L + 3, Data->Description.ToString(), MTUI::Body(20.f * S), D(G, 514.f, TY), 1300.f * S, MTUI::Ink, 3) / S + 14.f;

	// Magic rank and mastery.
	const EMTMagicRank Rank = Prog->GetMagicRank(SelElement);
	const float RankProgress = Prog->GetMagicRankProgress(SelElement);
	MTUI::Text(Out, L + 3, G, TEXT("MAGIC RANK"), MTUI::Heading(14.f * S), D(G, 514.f, TY + 4.f), MTUI::InkSoft, false);
	MTUI::Text(Out, L + 3, G, MTUtil::MagicRankToString(Rank), MTUI::Heading(21.f * S), D(G, 680.f, TY), MTUI::Ink, false);
	MTUI::Bar(Out, L + 3, G, D(G, 900.f, TY + 8.f), FVector2D(480.f, 12.f) * S, RankProgress, RankProgress, ElemColor);
	if (Rank != EMTMagicRank::God)
	{
		const EMTMagicRank NextRank = (EMTMagicRank)((int32)Rank + 1);
		MTUI::Text(Out, L + 3, G, FString::Printf(TEXT("%d / %d to %s"), FMath::FloorToInt(Prog->GetMagicRankXP(SelElement)),
			FMath::RoundToInt(UMTProgressionSubsystem::GetMagicRankThreshold(NextRank)), *MTUtil::MagicRankToString(NextRank)),
			MTUI::BodyItalic(18.f * S), D(G, 1400.f, TY + 2.f), MTUI::InkSoft, false);
	}
	TY += 40.f;
	const FName MasteryKey = UMTProgressionSubsystem::ElementKey(SelElement);
	const float MasteryProgress = Prog->GetMasteryProgress(EMTMasteryTrack::Element, MasteryKey);
	MTUI::Text(Out, L + 3, G, TEXT("MASTERY"), MTUI::Heading(14.f * S), D(G, 514.f, TY + 4.f), MTUI::InkSoft, false);
	MTUI::Text(Out, L + 3, G, FString::Printf(TEXT("%d / %d"), Prog->GetMasteryLevel(EMTMasteryTrack::Element, MasteryKey), UMTProgressionSubsystem::MaxMasteryLevel),
		MTUI::Heading(21.f * S), D(G, 680.f, TY), MTUI::Ink, false);
	MTUI::Bar(Out, L + 3, G, D(G, 900.f, TY + 8.f), FVector2D(480.f, 12.f) * S, MasteryProgress, MasteryProgress, MTUI::Gold);
	TY += 50.f;

	// Spells, with the key each one sits on.
	Section(G, Out, L + 3, D(G, 514.f, TY), 1300.f * S, TEXT("SPELLS"), true);
	TY += 40.f;
	for (const FName& Id : Data->Abilities)
	{
		if (TY > 860.f)
		{
			break;
		}
		const int32 Key = Loadout.IndexOfByKey(Id);
		TY += AbilityRow(G, Out, L + 3, D(G, 514.f, TY), 1300.f * S, Id, Key != INDEX_NONE ? FString::Printf(TEXT("ON KEY %d"), Key + 1) : FString(), 2) / S;
	}
	Paragraph(G, Out, L + 3, TEXT("Magic rank and mastery rise as you cast this school's spells in battle."), MTUI::BodyItalic(18.f * S), D(G, 514.f, FMath::Min(TY + 6.f, 890.f)),
		1300.f * S, MTUI::InkSoft, 1);
	return L + 14;
}

// ---------------------------------------------------------------------------------------------
// Race
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintRace(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	if (!Prog || !Registry)
	{
		return PaintUnavailable(G, Out, Layer, TEXT("Race data is unavailable."));
	}
	const TArray<EMTRace> Owned = MTJ::OwnedRaces(Prog);

	Section(G, Out, Layer, D(G, MTJ::Left, MTJ::Top), 380.f * S, TEXT("BLOODLINES"), false);
	const int32 Visible = 5;
	const int32 First = ScrollList(G, Out, Layer + 20, TEXT("Races"), Owned.Num(), Visible, D(G, MTJ::Left, MTJ::Top + 36.f), FVector2D(380.f, Visible * 112.f - 12.f) * S, false);
	float Y = MTJ::Top + 36.f;
	for (int32 i = First; i < Owned.Num() && i < First + Visible; ++i)
	{
		const EMTRace R = Owned[i];
		const FMTRaceData* Row = Registry->FindRace(R);
		const FString Sub = Row ? MTUtil::RarityToString(Row->Rarity) + (Row->bImplemented ? FString() : FString::Printf(TEXT("%snot yet playable"), MTJ::Sep)) : FString();
		ListCard(G, Out, Layer + 2, D(G, MTJ::Left, Y), FVector2D(380.f, 100.f) * S, nullptr, true, Prog->GetRaceDisplayName(R).ToString(), Sub,
			Row ? MTUtil::RarityColor(Row->Rarity) : MTUI::Gold, EAction::SelRace, NAME_None, (int32)R, bSelRaceValid && R == SelRace,
			Prog->GetEquippedRace() == R ? TEXT("EQUIPPED") : TEXT(""));
		Y += 112.f;
	}

	const FMTRaceData* Row = bSelRaceValid ? Registry->FindRace(SelRace) : nullptr;
	if (Row)
	{
		const bool bEquipped = Prog->GetEquippedRace() == SelRace;
		const bool bCanEquip = !bEquipped && Row->bImplemented;
		const FString Label = bEquipped ? FString(TEXT("EQUIPPED"))
			: (Row->bImplemented ? FString::Printf(TEXT("EQUIP %s"), *Prog->GetRaceDisplayName(SelRace).ToString().ToUpper()) : FString(TEXT("NOT YET PLAYABLE")));
		Button(G, Out, Layer + 12, D(G, MTJ::Left, 946.f), FVector2D(380.f, 60.f) * S, Label, EAction::EquipRace, NAME_None, (int32)SelRace, bCanEquip, bEquipped, 20.f, bCanEquip);
	}

	const FVector2D CardPos = D(G, 470.f, MTJ::Top);
	const FVector2D CardSize = FVector2D(1390.f, MTJ::Bottom - MTJ::Top) * S;
	MTUI::ParchmentPanel(Out, Layer, G, CardPos, CardSize, 1.f);
	const int32 L = Layer + 6;
	if (!Row)
	{
		MTUI::TextAligned(Out, L, G, TEXT("No race selected."), MTUI::BodyItalic(26.f * S), CardPos, CardSize, FVector2D(0.5f, 0.5f), MTUI::InkSoft, false);
		return L + 4;
	}
	auto CanonColor = [](const FString& Status)
	{
		const bool bCanon = Status.Contains(TEXT("CANON")) && !Status.Contains(TEXT("ORIGINAL"));
		return MTJ::Ink(bCanon ? FLinearColor(0.45f, 0.8f, 1.f, 1.f) : FLinearColor(0.95f, 0.7f, 0.35f, 1.f));
	};

	float TY = 218.f;
	const FString Name = Prog->GetRaceDisplayName(SelRace).ToString();
	const FSlateFontInfo NameFont = MTUI::Title(42.f * S);
	MTUI::Text(Out, L + 3, G, Name, NameFont, D(G, 514.f, TY), MTUI::Ink, false);
	float ChipX = 514.f + MTUI::Measure(Name, NameFont).X / S + 20.f;
	ChipX += Chip(G, Out, L + 3, D(G, ChipX, TY + 16.f), MTUtil::RarityToString(Row->Rarity).ToUpper(), MTJ::Ink(MTUtil::RarityColor(Row->Rarity)), true, 13.f) / S + 10.f;
	if (!Row->bImplemented)
	{
		Chip(G, Out, L + 3, D(G, ChipX, TY + 16.f), TEXT("NOT YET PLAYABLE"), MTJ::Ink(MTUI::Danger), true, 13.f);
	}
	TY += 66.f;
	TY += Paragraph(G, Out, L + 3, Row->Description.ToString(), MTUI::Body(20.f * S), D(G, 514.f, TY), 1300.f * S, MTUI::Ink, 3) / S + 10.f;
	MTUI::Divider(Out, L + 3, G, D(G, 1165.f, TY + 18.f), 380.f * S, 0.9f);
	TY += 46.f;

	// Passive (always applies while equipped).
	MTUI::Text(Out, L + 3, G, TEXT("PASSIVE"), MTUI::Heading(13.f * S), D(G, 514.f, TY + 6.f), MTUI::InkSoft, false);
	const FString PassiveName = Row->PassiveName.ToString();
	const FSlateFontInfo PassiveFont = MTUI::Heading(21.f * S);
	MTUI::Text(Out, L + 3, G, PassiveName, PassiveFont, D(G, 606.f, TY), MTUI::Ink, false);
	if (!Row->PassiveCanonStatus.IsEmpty())
	{
		Chip(G, Out, L + 3, D(G, 606.f + MTUI::Measure(PassiveName, PassiveFont).X / S + 14.f, TY + 5.f), Row->PassiveCanonStatus.ToUpper(), CanonColor(Row->PassiveCanonStatus.ToUpper()), true, 11.f);
	}
	TY += 34.f;
	TY += Paragraph(G, Out, L + 3, Row->PassiveDescription.ToString(), MTUI::Body(19.f * S), D(G, 514.f, TY), 1300.f * S, MTUI::Ink, 2) / S + 16.f;

	// Racial techniques (not on the hotbar in LA PLACE).
	Section(G, Out, L + 3, D(G, 514.f, TY), 1300.f * S, TEXT("RACIAL TECHNIQUES"), true);
	TY += 40.f;
	if (!Row->ActiveAbility.IsNone())
	{
		TY += AbilityRow(G, Out, L + 3, D(G, 514.f, TY), 1300.f * S, Row->ActiveAbility, FString::Printf(TEXT("ACTIVE%sGAMEPLAY ORIGINAL"), MTJ::Sep), 2) / S;
	}
	if (!Row->TransformationAbility.IsNone())
	{
		TY += AbilityRow(G, Out, L + 3, D(G, 514.f, TY), 1300.f * S, Row->TransformationAbility,
			FString::Printf(TEXT("TRANSFORMATION%s%s"), MTJ::Sep, *Row->TransformationCanonStatus.ToUpper()), 2) / S;
	}
	TY += Paragraph(G, Out, L + 3, TEXT("Racial techniques are not on the LA PLACE hotbar yet; the passive applies while this race is equipped."),
		MTUI::BodyItalic(18.f * S), D(G, 514.f, TY + 2.f), 1300.f * S, MTUI::InkSoft, 2) / S + 10.f;
	if (!Row->LoreNote.IsEmpty() && TY < 866.f)
	{
		Paragraph(G, Out, L + 3, Row->LoreNote, MTUI::BodyItalic(16.f * S), D(G, 514.f, TY), 1300.f * S, MTUI::InkSoft.CopyWithNewOpacity(0.85f), 2);
	}
	return Layer + 24;
}

// ---------------------------------------------------------------------------------------------
// Mastery
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintMastery(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	if (!Prog || !Registry)
	{
		return PaintUnavailable(G, Out, Layer, TEXT("Mastery data is unavailable."));
	}
	struct FRow { FString Name; EMTMasteryTrack Track; FName Key; FLinearColor Color; };
	TArray<FRow> Columns[3];
	for (const FName& Id : MTJ::OwnedCharacters(Prog, Registry))
	{
		const FMTCharacterData* C = Registry->FindCharacter(Id);
		Columns[0].Add({ C && !C->DisplayName.IsEmpty() ? C->DisplayName.ToString() : Id.ToString(), EMTMasteryTrack::Character, Id, C ? MTUtil::RarityColor(C->Rarity) : MTUI::Gold });
	}
	for (const TPair<EMTElement, FMTElementData>& Pair : Registry->GetElements())
	{
		Columns[1].Add({ Prog->GetElementDisplayName(Pair.Key).ToString(), EMTMasteryTrack::Element, UMTProgressionSubsystem::ElementKey(Pair.Key), MTUtil::ElementColor(Pair.Key) });
	}
	for (const EMTRace R : MTJ::OwnedRaces(Prog))
	{
		const FMTRaceData* Row = Registry->FindRace(R);
		Columns[2].Add({ Prog->GetRaceDisplayName(R).ToString(), EMTMasteryTrack::Race, Prog->RaceKey(R), Row ? MTUtil::RarityColor(Row->Rarity) : MTUI::Gold });
	}

	static const TCHAR* Headers[] = { TEXT("LINEAGES"), TEXT("ELEMENTS"), TEXT("RACES") };
	for (int32 c = 0; c < 3; ++c)
	{
		const float CX = MTJ::Left + c * 610.f;
		MTUI::ParchmentPanel(Out, Layer, G, D(G, CX, MTJ::Top), FVector2D(580.f, 732.f) * S, 1.f);
		const int32 L = Layer + 6;
		MTUI::TextAligned(Out, L, G, Headers[c], MTUI::Title(28.f * S), D(G, CX, MTJ::Top + 30.f), FVector2D(580.f, 40.f) * S, FVector2D(0.5f, 0.f), MTUI::Ink, false);
		MTUI::Divider(Out, L, G, D(G, CX + 290.f, MTJ::Top + 98.f), 300.f * S, 0.9f);
		float RY = MTJ::Top + 138.f;
		for (int32 i = 0; i < Columns[c].Num(); ++i)
		{
			if (RY > MTJ::Top + 640.f)
			{
				MTUI::TextAligned(Out, L, G, FString::Printf(TEXT("and %d more"), Columns[c].Num() - i), MTUI::BodyItalic(19.f * S), D(G, CX, RY), FVector2D(580.f, 26.f) * S,
					FVector2D(0.5f, 0.f), MTUI::InkSoft, false);
				break;
			}
			const FRow& Row = Columns[c][i];
			const int32 Level = Prog->GetMasteryLevel(Row.Track, Row.Key);
			const float Progress = Prog->GetMasteryProgress(Row.Track, Row.Key);
			const bool bMax = Level >= UMTProgressionSubsystem::MaxMasteryLevel;
			MTJ::FillDiamond(Out, L, G, D(G, CX + 50.f, RY + 13.f), 7.f * S, MTJ::Ink(Row.Color));
			MTUI::Text(Out, L, G, MTJ::FitText(Row.Name, MTUI::Heading(20.f * S), 360.f * S), MTUI::Heading(20.f * S), D(G, CX + 68.f, RY), MTUI::Ink, false);
			MTUI::TextAligned(Out, L, G, bMax ? FString(TEXT("MASTERED")) : FString::Printf(TEXT("%d / %d"), Level, UMTProgressionSubsystem::MaxMasteryLevel), MTUI::Heading(17.f * S),
				D(G, CX, RY + 2.f), FVector2D(536.f, 24.f) * S, FVector2D(1.f, 0.f), bMax ? MTJ::Ink(MTUI::Stamina) : MTUI::InkSoft, false);
			MTUI::Bar(Out, L, G, D(G, CX + 44.f, RY + 34.f), FVector2D(492.f, 10.f) * S, Progress, Progress, bMax ? MTUI::Stamina : MTUI::Gold);
			RY += 70.f;
		}
		if (Columns[c].Num() == 0)
		{
			MTUI::TextAligned(Out, L, G, TEXT("Nothing yet."), MTUI::BodyItalic(21.f * S), D(G, CX, RY), FVector2D(580.f, 30.f) * S, FVector2D(0.5f, 0.f), MTUI::InkSoft, false);
		}
	}
	// LA PLACE: mastery is shown and levels up, but it no longer locks techniques.
	MTUI::TextAligned(Out, Layer + 6, G, TEXT("Mastery grows as you use techniques in battle. It no longer locks anything: every technique is yours from the start."),
		MTUI::BodyItalic(21.f * S), D(G, 0.f, MTJ::Top + 758.f), FVector2D(1920.f, 30.f) * S, FVector2D(0.5f, 0.f), MTUI::TextLight.CopyWithNewOpacity(0.8f));
	return Layer + 14;
}

// ---------------------------------------------------------------------------------------------
// Inventory
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintInventory(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	if (!Prog)
	{
		return PaintUnavailable(G, Out, Layer, TEXT("The satchel is unavailable."));
	}
	const FVector2D CardPos = D(G, MTJ::Left, MTJ::Top);
	const FVector2D CardSize = FVector2D(1800.f, MTJ::Bottom - MTJ::Top) * S;
	MTUI::ParchmentPanel(Out, Layer, G, CardPos, CardSize, 1.f);
	const int32 L = Layer + 6;
	MTUI::Text(Out, L, G, TEXT("SATCHEL"), MTUI::Title(36.f * S), D(G, 110.f, 214.f), MTUI::Ink, false);
	const FString GoldText = FString::Printf(TEXT("%d GOLD"), Prog->GetGold());
	const FSlateFontInfo GoldFont = MTUI::Heading(24.f * S);
	const float GoldW = MTUI::Measure(GoldText, GoldFont).X / S;
	MTUI::Glow(Out, L, G, D(G, 1810.f - GoldW - 26.f, 236.f), 18.f * S, MTUI::GoldBright.CopyWithNewOpacity(0.9f));
	MTUI::Glow(Out, L + 1, G, D(G, 1810.f - GoldW - 26.f, 236.f), 7.f * S, FLinearColor(1.f, 0.97f, 0.85f, 1.f));
	MTUI::Text(Out, L, G, GoldText, GoldFont, D(G, 1810.f - GoldW, 220.f), MTUI::Ink, false);
	MTUI::Divider(Out, L, G, D(G, 960.f, 292.f), 380.f * S, 0.9f);

	TArray<FName> Items;
	for (const TPair<FName, int32>& Pair : Prog->GetInventory())
	{
		if (Pair.Value > 0)
		{
			Items.Add(Pair.Key);
		}
	}
	// Rarest first, then by name.
	Items.Sort([Registry](const FName& A, const FName& B)
	{
		const FMTItemData* IA = Registry ? Registry->FindItem(A) : nullptr;
		const FMTItemData* IB = Registry ? Registry->FindItem(B) : nullptr;
		const int32 RA = IA ? (int32)IA->Rarity : 0;
		const int32 RB = IB ? (int32)IB->Rarity : 0;
		return RA != RB ? RA > RB : A.LexicalLess(B);
	});
	if (Items.Num() == 0)
	{
		MTUI::TextAligned(Out, L, G, TEXT("Your satchel is empty. Herbs, materials and quest items you gather appear here."), MTUI::BodyItalic(25.f * S),
			D(G, MTJ::Left, 300.f), FVector2D(1800.f, 500.f) * S, FVector2D(0.5f, 0.5f), MTUI::InkSoft, false);
		return L + 4;
	}

	const int32 Cols = 3;
	const int32 Rows = (Items.Num() + Cols - 1) / Cols;
	const int32 VisibleRows = 3;
	const float CellW = 540.f;
	const float CellH = 150.f;
	const float GapX = 30.f;
	const float GapY = 22.f;
	const float GridX = 110.f;
	const float GridY = 330.f;
	const int32 FirstRow = ScrollList(G, Out, L + 20, TEXT("Items"), Rows, VisibleRows, D(G, GridX, GridY),
		FVector2D(Cols * CellW + (Cols - 1) * GapX, VisibleRows * (CellH + GapY) - GapY) * S, true);
	for (int32 i = FirstRow * Cols; i < Items.Num() && i < (FirstRow + VisibleRows) * Cols; ++i)
	{
		const int32 Local = i - FirstRow * Cols;
		const float CX = GridX + (Local % Cols) * (CellW + GapX);
		const float CY = GridY + (Local / Cols) * (CellH + GapY);
		const FVector2D P = D(G, CX, CY);
		const FVector2D Size = FVector2D(CellW, CellH) * S;
		const FMTItemData* Item = Registry ? Registry->FindItem(Items[i]) : nullptr;
		const FLinearColor Rarity = Item ? MTUtil::RarityColor(Item->Rarity) : MTUI::Gold;
		const FString Name = Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName.ToString() : Items[i].ToString();
		MTUI::Box(Out, L, G, P, Size, FLinearColor(0.35f, 0.2f, 0.06f, 0.07f));
		MTJ::Outline(Out, L + 1, G, P, Size, MTUI::InkSoft.CopyWithNewOpacity(0.3f), 1.f);
		MTUI::Box(Out, L + 1, G, P, FVector2D(4.f * S, Size.Y), MTJ::Ink(Rarity));

		const FVector2D IconPos = P + FVector2D(18.f, 18.f) * S;
		const float IconSize = 114.f * S;
		MTUI::Box(Out, L + 1, G, IconPos, FVector2D(IconSize), FLinearColor(0.f, 0.f, 0.f, 0.8f));
		if (const FSlateBrush* Icon = Item ? MTUI::Brush(Item->Icon.ToString()) : nullptr)
		{
			MTJ::DrawCover(Out, L + 2, G, Icon, IconPos + FVector2D(3.f * S), FVector2D(IconSize - 6.f * S), FLinearColor::White);
		}
		else
		{
			MTUI::Glow(Out, L + 2, G, IconPos + FVector2D(IconSize * 0.5f), IconSize * 0.42f, Rarity.CopyWithNewOpacity(0.45f));
			MTUI::TextAligned(Out, L + 3, G, MTJ::Initials(Name), MTUI::Title(IconSize * 0.3f), IconPos, FVector2D(IconSize), FVector2D(0.5f, 0.5f), MTUI::TextLight);
		}
		MTUI::GoldFrame(Out, L + 4, G, IconPos, FVector2D(IconSize), 0.85f, false);

		const float TX = CX + 150.f;
		const float TW = CellW - 150.f - 16.f;
		const FString Count = FString::Printf(TEXT("x%d"), Prog->GetItemCount(Items[i]));
		const FSlateFontInfo NameFont = MTUI::Heading(20.f * S);
		const float CountW = MTUI::Measure(Count, NameFont).X / S;
		MTUI::Text(Out, L + 3, G, MTJ::FitText(Name, NameFont, (TW - CountW - 12.f) * S), NameFont, D(G, TX, CY + 14.f), MTUI::Ink, false);
		MTUI::Text(Out, L + 3, G, Count, NameFont, D(G, CX + CellW - 16.f - CountW, CY + 14.f), MTUI::InkSoft, false);
		if (Item)
		{
			MTUI::Text(Out, L + 3, G, MTUtil::RarityToString(Item->Rarity), MTUI::BodyItalic(17.f * S), D(G, TX, CY + 42.f), MTJ::Ink(Rarity), false);
			if (Item->bQuestItem)
			{
				const float ChipW = ChipWidth(G, TEXT("QUEST ITEM"), 11.f) / S;
				Chip(G, Out, L + 3, D(G, CX + CellW - 16.f - ChipW, CY + 46.f), TEXT("QUEST ITEM"), MTJ::Ink(MTUI::Gold), true, 11.f);
			}
			Paragraph(G, Out, L + 3, Item->Description.ToString(), MTUI::Body(17.f * S), D(G, TX, CY + 70.f), TW * S, MTUI::Ink, 2, 1.f);
		}
	}
	return L + 30;
}

// ---------------------------------------------------------------------------------------------
// Quests
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintQuests(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	UMTQuestSubsystem* Quests = MTJ::GetQuests(PlayerController);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	if (!Quests)
	{
		return PaintUnavailable(G, Out, Layer, TEXT("The quest log is unavailable."));
	}
	const TArray<FName> Active = Quests->GetActiveQuestIds();
	const TArray<FName> Completed = Quests->GetCompletedQuestIds();
	const FName Tracked = Quests->GetTrackedQuest();
	TArray<FName> Entries = Active;
	Entries.Append(Completed);

	// Left: one log, active quests first.
	Section(G, Out, Layer, D(G, MTJ::Left, MTJ::Top), 440.f * S, FString::Printf(TEXT("QUEST LOG  (%d ACTIVE)"), Active.Num()), false);
	const int32 Visible = 8;
	const float RowStep = 86.f;
	const int32 First = ScrollList(G, Out, Layer + 20, TEXT("Quests"), Entries.Num(), Visible, D(G, MTJ::Left, MTJ::Top + 36.f), FVector2D(440.f, Visible * RowStep - 10.f) * S, false);
	float Y = MTJ::Top + 36.f;
	for (int32 i = First; i < Entries.Num() && i < First + Visible; ++i)
	{
		const FName Id = Entries[i];
		const bool bDone = !Active.Contains(Id);
		const FMTQuestData* Quest = Registry ? Registry->FindQuest(Id) : nullptr;
		const FString Sub = bDone ? FString(TEXT("Completed"))
			: (Quest ? FString::Printf(TEXT("%s%s%s"), *MTJ::QuestTypeLabel(Quest->Type), MTJ::Sep, *Quest->Region.ToString()) : FString());
		const FLinearColor Accent = Id == Tracked ? MTUI::GoldBright : (bDone ? MTUI::Stamina : MTUI::GoldDim);
		ListCard(G, Out, Layer + 2, D(G, MTJ::Left, Y), FVector2D(440.f, 76.f) * S, nullptr, false, MTJ::QuestTitle(Registry, Id), Sub, Accent,
			EAction::SelQuest, Id, INDEX_NONE, Id == SelQuest, Id == Tracked ? TEXT("TRACKED") : (bDone ? TEXT("DONE") : TEXT("")));
		Y += RowStep;
	}
	if (Entries.Num() == 0)
	{
		Paragraph(G, Out, Layer + 2, TEXT("No quests yet. Villagers and the Adventurers' Guild have work for you."), MTUI::BodyItalic(20.f * S), D(G, MTJ::Left + 4.f, Y + 4.f),
			430.f * S, MTUI::TextLight.CopyWithNewOpacity(0.75f), 3);
	}

	// Right: the selected quest.
	const FVector2D CardPos = D(G, 530.f, MTJ::Top);
	const FVector2D CardSize = FVector2D(1330.f, MTJ::Bottom - MTJ::Top) * S;
	MTUI::ParchmentPanel(Out, Layer, G, CardPos, CardSize, 1.f);
	const int32 L = Layer + 6;
	const FMTQuestData* Quest = Registry && !SelQuest.IsNone() ? Registry->FindQuest(SelQuest) : nullptr;
	if (!Quest)
	{
		MTUI::TextAligned(Out, L, G, Entries.Num() == 0 ? TEXT("Your quest log is empty.") : TEXT("Select a quest."), MTUI::BodyItalic(26.f * S), CardPos, CardSize,
			FVector2D(0.5f, 0.5f), MTUI::InkSoft, false);
		return L + 4;
	}
	const bool bActive = Active.Contains(SelQuest);
	const float X = 576.f;
	const float W = 1238.f;
	float TY = 216.f;
	const FSlateFontInfo TitleFont = MTUI::Title(34.f * S);
	MTUI::Text(Out, L, G, MTJ::FitText(MTJ::QuestTitle(Registry, SelQuest), TitleFont, W * S), TitleFont, D(G, X, TY), MTUI::Ink, false);
	TY += 52.f;
	const FString Meta = FString::Printf(TEXT("%s QUEST%s%s%sRANK %s+"), *MTJ::QuestTypeLabel(Quest->Type).ToUpper(), MTJ::Sep, *Quest->Region.ToString().ToUpper(),
		MTJ::Sep, *MTUtil::AdventurerRankToString(Quest->RequiredRank));
	const FSlateFontInfo MetaFont = MTUI::Heading(13.f * S);
	MTUI::Text(Out, L, G, Meta, MetaFont, D(G, X, TY), MTUI::InkSoft, false);
	if (!bActive)
	{
		Chip(G, Out, L, D(G, X + MTUI::Measure(Meta, MetaFont).X / S + 16.f, TY - 2.f), TEXT("COMPLETED"), MTJ::Ink(MTUI::Stamina), true, 12.f);
	}
	TY += 32.f;
	TY += Paragraph(G, Out, L, Quest->Summary.ToString(), MTUI::Body(21.f * S), D(G, X, TY), W * S, MTUI::Ink, 4) / S + 12.f;
	MTUI::Divider(Out, L, G, D(G, X + W * 0.5f, TY + 18.f), 380.f * S, 0.9f);
	TY += 46.f;

	Section(G, Out, L, D(G, X, TY), W * S, TEXT("OBJECTIVES"), true);
	TY += 38.f;
	const FMTQuestSaveState* State = Quests->GetQuestState(SelQuest);
	for (int32 i = 0; i < Quest->Objectives.Num() && TY < 780.f; ++i)
	{
		const bool bHidden = bActive && Quest->bSequentialObjectives && State && i > State->Stage;
		const bool bDone = !bActive || Quests->IsObjectiveComplete(SelQuest, i);
		const FString Line = bHidden ? FString(TEXT("???")) : (bActive ? Quests->GetObjectiveText(SelQuest, i).ToString() : Quest->Objectives[i].Description.ToString());
		const FVector2D Mark = D(G, X + 10.f, TY + 13.f);
		MTJ::DiamondOutline(Out, L, G, Mark, 8.f * S, MTUI::InkSoft, 1.5f);
		if (bDone)
		{
			MTJ::FillDiamond(Out, L + 1, G, Mark, 5.f * S, MTJ::Ink(MTUI::Stamina));
		}
		const FSlateFontInfo Font = MTUI::Body(20.f * S);
		MTUI::Text(Out, L, G, MTJ::FitText(Line, Font, (W - 34.f) * S), Font, D(G, X + 32.f, TY), bDone || bHidden ? MTUI::InkSoft : MTUI::Ink, false);
		TY += 32.f;
	}
	TY += 12.f;
	Section(G, Out, L, D(G, X, TY), 940.f * S, TEXT("REWARDS"), true);
	TY += 38.f;
	const TArray<FString> Rewards = MTJ::RewardParts(Quest->Reward, Registry);
	Paragraph(G, Out, L, Rewards.Num() > 0 ? FString::Join(Rewards, MTJ::Sep) : FString(TEXT("None")), MTUI::Body(20.f * S), D(G, X, TY), 940.f * S, MTUI::Ink, 2);
	if (bActive)
	{
		const bool bIsTracked = SelQuest == Tracked;
		Button(G, Out, L + 4, D(G, 1540.f, 924.f), FVector2D(236.f, 56.f) * S, bIsTracked ? TEXT("TRACKING") : TEXT("TRACK"), EAction::Track, SelQuest, INDEX_NONE,
			!bIsTracked, bIsTracked, 20.f, !bIsTracked);
	}
	return Layer + 24;
}

// ---------------------------------------------------------------------------------------------
// Map
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintMap(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	if (!Prog || !Registry)
	{
		return PaintUnavailable(G, Out, Layer, TEXT("The map is unavailable."));
	}
	const APawn* Pawn = PlayerController.IsValid() ? PlayerController->GetPawn() : nullptr;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 3.0);

	// The painted world map (the same art as the PLAY screen).
	const FVector2D MapPos = D(G, MTJ::Left, MTJ::Top);
	const FVector2D MapSize = FVector2D(1088.f, 816.f) * S;
	MTUI::Box(Out, Layer, G, MapPos + FVector2D(10.f, 12.f) * S, MapSize, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	if (const FSlateBrush* MapArt = MTUI::UIBrush(TEXT("Art"), TEXT("T_WorldMap")))
	{
		MTUI::Box(Out, Layer + 1, G, MapPos, MapSize, FLinearColor::White, MapArt);
	}
	else
	{
		MTUI::ParchmentPanel(Out, Layer + 1, G, MapPos, MapSize, 1.f);
		MTUI::TextAligned(Out, Layer + 6, G, TEXT("The cartographers are still drawing this world..."), MTUI::BodyItalic(26.f * S), MapPos, MapSize, FVector2D(0.5f, 0.5f), MTUI::Ink, false);
	}
	MTUI::GoldFrame(Out, Layer + 6, G, MapPos, MapSize, 1.f, true, 56.f * S);
	MTUI::Glow(Out, Layer + 7, G, MapPos + FVector2D(MapSize.X - 52.f * S, 40.f * S), 26.f * S, FLinearColor(0.f, 0.f, 0.f, 0.45f));
	MTUI::TextAligned(Out, Layer + 7, G, TEXT("N"), MTUI::Title(26.f * S), MapPos + FVector2D(MapSize.X - 72.f * S, 22.f * S), FVector2D(40.f, 36.f) * S, FVector2D(0.5f, 0.5f), MTUI::GoldBright);

	// Places you have discovered: waystones and spawn points as pins, the rest as small marks.
	const int32 PinLayer = Layer + 9;
	TArray<const FMTLocationData*> Waystones;
	int32 Undiscovered = 0;
	for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
	{
		const FMTLocationData& Loc = Pair.Value;
		if (!Prog->IsLocationDiscovered(Pair.Key))
		{
			Undiscovered += Loc.bFastTravel ? 1 : 0;
			continue;
		}
		if (Loc.bFastTravel)
		{
			Waystones.Add(&Loc);
		}
		const FVector2D At = MapPos + MapSize * Loc.MapUV;
		const bool bMajor = Loc.bFastTravel || Loc.bSpawnPoint;
		const bool bSel = Pair.Key == SelLocation;
		const float H = HoverAlpha(EAction::SelLoc, Pair.Key);
		const float R = (bMajor ? 8.f + 4.f * H + (bSel ? 3.f : 0.f) : 4.f + 2.f * H) * S;
		MTUI::Glow(Out, PinLayer, G, At, R * (bSel ? 4.f + 1.2f * Pulse : 3.f),
			(bSel ? MTUI::GoldBright : MTUI::Gold).CopyWithNewOpacity(bSel ? 0.55f : (bMajor ? 0.35f : 0.2f) + 0.3f * H));
		MTUI::Glow(Out, PinLayer + 1, G, At, R, bMajor ? FLinearColor(1.f, 0.95f, 0.8f, 1.f) : FLinearColor(0.95f, 0.85f, 0.6f, 0.85f));
		if (bSel)
		{
			MTUI::Arc(Out, PinLayer + 1, G, At, R * 2.6f + 4.f * Pulse * S, 1.f, MTUI::GoldBright.CopyWithNewOpacity(0.8f), 2.f);
		}
		if (bSel || H > 0.05f)
		{
			const FString Name = Loc.DisplayName.IsEmpty() ? Pair.Key.ToString() : Loc.DisplayName.ToString();
			const FSlateFontInfo Font = MTUI::Heading(17.f * S);
			const FVector2D TextSize = MTUI::Measure(Name, Font);
			const FVector2D LabelPos = At + FVector2D(-TextSize.X * 0.5f, -R - TextSize.Y - 10.f * S);
			MTUI::Box(Out, PinLayer + 2, G, LabelPos - FVector2D(8.f, 3.f) * S, TextSize + FVector2D(16.f, 6.f) * S, FLinearColor(0.03f, 0.02f, 0.01f, 0.82f));
			MTUI::GoldFrame(Out, PinLayer + 3, G, LabelPos - FVector2D(8.f, 3.f) * S, TextSize + FVector2D(16.f, 6.f) * S, 0.8f, false);
			MTUI::Text(Out, PinLayer + 4, G, Name, Font, LabelPos, MTUI::GoldBright);
		}
		AddHit(At - FVector2D(R + 10.f * S), FVector2D(2.f * (R + 10.f * S)), EAction::SelLoc, Pair.Key);
	}

	// Tracked quest objective and the player (world to map as in Docs/LaPlace/Spec.md section 2).
	if (UMTQuestSubsystem* Quests = MTJ::GetQuests(PlayerController))
	{
		FVector MarkerLoc;
		const FName TrackedQuest = Quests->GetTrackedQuest();
		if (!TrackedQuest.IsNone() && Quests->GetObjectiveMarker(TrackedQuest, MarkerLoc))
		{
			const FVector2D UV = MTJ::WorldToMapUV(MarkerLoc);
			if (MTJ::InsideUnit(UV))
			{
				const FVector2D At = MapPos + MapSize * UV;
				MTUI::Glow(Out, PinLayer + 5, G, At, 22.f * S, MTUI::GoldBright.CopyWithNewOpacity(0.35f + 0.25f * Pulse));
				MTJ::FillDiamond(Out, PinLayer + 6, G, At, 8.f * S, MTUI::GoldBright);
				MTJ::DiamondOutline(Out, PinLayer + 7, G, At, 11.f * S, FLinearColor(0.1f, 0.05f, 0.f, 0.9f), 1.5f);
				MTUI::TextAligned(Out, PinLayer + 7, G, TEXT("QUEST"), MTUI::Heading(12.f * S), At - FVector2D(40.f, 34.f) * S, FVector2D(80.f, 16.f) * S, FVector2D(0.5f, 0.5f), MTUI::GoldBright);
			}
		}
	}
	if (Pawn)
	{
		const FVector2D UV = MTJ::WorldToMapUV(Pawn->GetActorLocation());
		if (MTJ::InsideUnit(UV))
		{
			const FVector2D At = MapPos + MapSize * UV;
			// World +X is east (map right) and +Y south (map down), so yaw maps straight onto the image.
			const float Yaw = FMath::DegreesToRadians(Pawn->GetActorRotation().Yaw);
			const FVector2D Forward(FMath::Cos(Yaw), FMath::Sin(Yaw));
			MTUI::Glow(Out, PinLayer + 8, G, At, 22.f * S, FLinearColor(0.45f, 1.f, 0.6f, 0.4f + 0.2f * Pulse));
			MTUI::Line(Out, PinLayer + 9, G, At, At + Forward * 22.f * S, FLinearColor(0.85f, 1.f, 0.88f, 1.f), 3.f);
			MTUI::Glow(Out, PinLayer + 9, G, At, 7.f * S, FLinearColor(0.9f, 1.f, 0.92f, 1.f));
			MTUI::Text(Out, PinLayer + 9, G, TEXT("YOU"), MTUI::Heading(12.f * S), At + FVector2D(12.f, 6.f) * S, FLinearColor(0.75f, 1.f, 0.8f, 1.f));
		}
	}

	// Right: waystones you can travel to, and the selected place.
	const FVector2D CardPos = D(G, 1180.f, MTJ::Top);
	const FVector2D CardSize = FVector2D(680.f, MTJ::Bottom - MTJ::Top) * S;
	MTUI::ParchmentPanel(Out, Layer, G, CardPos, CardSize, 1.f);
	const int32 L = Layer + 6;
	const float X = 1224.f;
	const float W = 592.f;
	Section(G, Out, L, D(G, X, 212.f), W * S, TEXT("FAST TRAVEL"), true);
	Waystones.Sort([](const FMTLocationData& A, const FMTLocationData& B) { return A.DisplayName.ToString() < B.DisplayName.ToString(); });
	const int32 Visible = 5;
	const float RowH = 50.f;
	const int32 First = ScrollList(G, Out, L + 10, TEXT("Waystones"), Waystones.Num(), Visible, D(G, X, 248.f), FVector2D(W, Visible * RowH) * S, true);
	for (int32 i = First; i < Waystones.Num() && i < First + Visible; ++i)
	{
		const FMTLocationData* Loc = Waystones[i];
		const FVector2D P = D(G, X, 248.f + (i - First) * RowH);
		const FVector2D Size = FVector2D(W, RowH - 6.f) * S;
		const bool bSel = Loc->LocationID == SelLocation;
		const float H = HoverAlpha(EAction::SelLoc, Loc->LocationID, 1);
		if (bSel || H > 0.01f)
		{
			MTUI::Box(Out, L, G, P, Size, FLinearColor(0.45f, 0.26f, 0.06f, bSel ? 0.2f : 0.1f * H));
		}
		if (bSel)
		{
			MTUI::Box(Out, L + 1, G, P, FVector2D(4.f * S, Size.Y), MTJ::Ink(MTUI::Gold));
		}
		const bool bRankOk = Prog->GetAdventurerRank() >= Loc->RequiredRank;
		const FString Name = Loc->DisplayName.IsEmpty() ? Loc->LocationID.ToString() : Loc->DisplayName.ToString();
		MTUI::Text(Out, L + 1, G, MTJ::FitText(Name, MTUI::Heading(18.f * S), (W - 200.f) * S), MTUI::Heading(18.f * S), P + FVector2D(16.f, 9.f) * S, MTUI::Ink, false);
		const FString Where = bRankOk ? Loc->Region.ToString() : FString::Printf(TEXT("Rank %s"), *MTUtil::AdventurerRankToString(Loc->RequiredRank));
		MTUI::TextAligned(Out, L + 1, G, Where, MTUI::BodyItalic(17.f * S), P, Size - FVector2D(14.f * S, 0.f), FVector2D(1.f, 0.5f), bRankOk ? MTUI::InkSoft : MTJ::Ink(MTUI::Danger), false);
		MTUI::Line(Out, L, G, P + FVector2D(0.f, Size.Y + 3.f * S), P + FVector2D(Size.X, Size.Y + 3.f * S), MTUI::InkSoft.CopyWithNewOpacity(0.2f), 1.f);
		AddHit(P, Size, EAction::SelLoc, Loc->LocationID, 1);
	}
	if (Waystones.Num() == 0)
	{
		Paragraph(G, Out, L, TEXT("Discover waystones to unlock fast travel."), MTUI::BodyItalic(19.f * S), D(G, X, 256.f), W * S, MTUI::InkSoft, 2);
	}
	if (Undiscovered > 0)
	{
		MTUI::Text(Out, L, G, FString::Printf(TEXT("%d waystone%s still undiscovered"), Undiscovered, Undiscovered > 1 ? TEXT("s") : TEXT("")),
			MTUI::BodyItalic(17.f * S), D(G, X, 548.f), MTUI::InkSoft, false);
	}
	MTUI::Divider(Out, L, G, D(G, X + W * 0.5f, 588.f), 300.f * S, 0.9f);

	const FMTLocationData* Sel = Registry->FindLocation(SelLocation);
	if (!Sel || !Prog->IsLocationDiscovered(SelLocation))
	{
		Paragraph(G, Out, L, TEXT("Select a place on the map."), MTUI::BodyItalic(22.f * S), D(G, X, 640.f), W * S, MTUI::InkSoft, 2);
		return PinLayer + 12;
	}
	float TY = 612.f;
	const FVector2D ArtPos = D(G, X, TY);
	const FVector2D ArtSize = FVector2D(W, 160.f) * S;
	if (const FSlateBrush* Art = MTUI::UIBrush(TEXT("Art"), FString::Printf(TEXT("T_Location_%s"), *SelLocation.ToString())))
	{
		MTJ::DrawCover(Out, L, G, Art, ArtPos, ArtSize, FLinearColor::White);
	}
	else
	{
		MTUI::Box(Out, L, G, ArtPos, ArtSize, FLinearColor(0.1f, 0.08f, 0.06f, 1.f));
		MTUI::TextAligned(Out, L + 1, G, Sel->Biome.ToString(), MTUI::BodyItalic(22.f * S), ArtPos, ArtSize, FVector2D(0.5f, 0.5f), MTUI::TextLight.CopyWithNewOpacity(0.6f));
	}
	MTUI::GoldFrame(Out, L + 1, G, ArtPos, ArtSize, 1.f, false);
	TY += 172.f;
	const FSlateFontInfo NameFont = MTUI::Title(28.f * S);
	MTUI::Text(Out, L + 2, G, MTJ::FitText(Sel->DisplayName.IsEmpty() ? SelLocation.ToString() : Sel->DisplayName.ToString(), NameFont, W * S), NameFont, D(G, X, TY), MTUI::Ink, false);
	TY += 42.f;
	FString Where = Sel->Continent.ToString();
	if (!Sel->Region.IsNone())
	{
		Where += FString(Where.IsEmpty() ? TEXT("") : MTJ::Sep) + Sel->Region.ToString();
	}
	MTUI::Text(Out, L + 2, G, Where.ToUpper(), MTUI::Heading(13.f * S), D(G, X, TY), MTUI::InkSoft, false);
	// Danger: five diamonds, right-aligned.
	MTUI::Text(Out, L + 2, G, TEXT("DANGER"), MTUI::Heading(12.f * S), D(G, X + W - 186.f, TY + 1.f), MTUI::InkSoft, false);
	for (int32 i = 0; i < 5; ++i)
	{
		const FVector2D C = D(G, X + W - 96.f + i * 22.f, TY + 9.f);
		if (i < FMath::Clamp(Sel->Difficulty, 1, 5))
		{
			MTJ::FillDiamond(Out, L + 2, G, C, 7.f * S, FLinearColor(0.62f, 0.1f, 0.04f, 1.f));
		}
		else
		{
			MTJ::DiamondOutline(Out, L + 2, G, C, 7.f * S, MTUI::InkSoft.CopyWithNewOpacity(0.5f), 1.2f);
		}
	}
	TY += 26.f;
	Paragraph(G, Out, L + 2, Sel->Description.ToString(), MTUI::Body(18.f * S), D(G, X, TY), W * S, MTUI::Ink, 2, 1.f);

	const bool bRankOk = Prog->GetAdventurerRank() >= Sel->RequiredRank;
	const bool bCanTravel = Sel->bFastTravel && bRankOk && Pawn != nullptr;
	FString Label = TEXT("TRAVEL");
	if (!Sel->bFastTravel)
	{
		Label = TEXT("NO WAYSTONE HERE");
	}
	else if (!bRankOk)
	{
		Label = FString::Printf(TEXT("RANK %s REQUIRED"), *MTUtil::AdventurerRankToString(Sel->RequiredRank));
	}
	Button(G, Out, L + 4, D(G, X + 30.f, 936.f), FVector2D(W - 60.f, 56.f) * S, Label, EAction::Travel, SelLocation, INDEX_NONE, bCanTravel, false, 22.f, bCanTravel);
	return PinLayer + 12;
}

// ---------------------------------------------------------------------------------------------
// Party
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintParty(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	MTUI::ParchmentPanel(Out, Layer, G, D(G, 460.f, 250.f), FVector2D(1000.f, 560.f) * S, 1.f);
	const int32 L = Layer + 6;
	MTUI::TextAligned(Out, L, G, TEXT("PARTY"), MTUI::Title(40.f * S), D(G, 460.f, 284.f), FVector2D(1000.f, 56.f) * S, FVector2D(0.5f, 0.f), MTUI::Ink, false);
	MTUI::Divider(Out, L, G, D(G, 960.f, 368.f), 380.f * S, 0.9f);
	float Y = 414.f;
	Y += Paragraph(G, Out, L, TEXT("Co-op and party play are planned but not in this build yet: there is no multiplayer, matchmaking, companion or party system."),
		MTUI::Body(24.f * S), D(G, 530.f, Y), 860.f * S, MTUI::Ink, 4) / S + 22.f;
	Paragraph(G, Out, L, TEXT("When it arrives, this page will list your companions, their builds and the quests you share."), MTUI::BodyItalic(22.f * S),
		D(G, 530.f, Y), 860.f * S, MTUI::InkSoft, 3);
	return L + 4;
}

// ---------------------------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintSettings(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	if (!Prog)
	{
		return PaintUnavailable(G, Out, Layer, TEXT("Settings are unavailable."));
	}
	const FMTSettingsSave& Settings = Prog->GetSettings();
	MTUI::ParchmentPanel(Out, Layer, G, D(G, 410.f, MTJ::Top), FVector2D(1100.f, MTJ::Bottom - MTJ::Top) * S, 1.f);
	const int32 L = Layer + 6;
	MTUI::TextAligned(Out, L, G, TEXT("SETTINGS"), MTUI::Title(40.f * S), D(G, 410.f, 208.f), FVector2D(1100.f, 56.f) * S, FVector2D(0.5f, 0.f), MTUI::Ink, false);
	MTUI::Divider(Out, L, G, D(G, 960.f, 290.f), 380.f * S, 0.9f);

	// Sliders: drag the track, or use - and + (keyboard / gamepad).
	const float LX = 470.f;
	const float CX = 924.f;
	const float CW = 380.f;
	float Y = 326.f;
	struct FSlider { FName Id; const TCHAR* Label; float Alpha; FString Value; };
	const FSlider Sliders[] = {
		{ TEXT("Volume"), TEXT("MASTER VOLUME"), Settings.MasterVolume, FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings.MasterVolume * 100.f)) },
		{ TEXT("Mouse"), TEXT("MOUSE SENSITIVITY"), (Settings.MouseSensitivity - 0.2f) / 2.8f, FString::Printf(TEXT("%.2f"), Settings.MouseSensitivity) },
		{ TEXT("FOV"), TEXT("FIELD OF VIEW"), (Settings.FieldOfView - 70.f) / 40.f, FString::Printf(TEXT("%d"), FMath::RoundToInt(Settings.FieldOfView)) },
		{ TEXT("Shake"), TEXT("CAMERA SHAKE"), Settings.CameraShakeScale, FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings.CameraShakeScale * 100.f)) },
	};
	for (const FSlider& Slider : Sliders)
	{
		const float Alpha = FMath::Clamp(Slider.Alpha, 0.f, 1.f);
		const float H = FMath::Max(HoverAlpha(EAction::Slider, Slider.Id), DraggingSetting == Slider.Id ? 1.f : 0.f);
		MTUI::Text(Out, L, G, Slider.Label, MTUI::Heading(19.f * S), D(G, LX, Y), MTUI::Ink, false);
		Button(G, Out, L, D(G, CX - 58.f, Y - 6.f), FVector2D(42.f, 38.f) * S, TEXT("-"), EAction::Step, Slider.Id, -1, false, false, 20.f, Alpha > 0.001f);
		MTUI::Bar(Out, L, G, D(G, CX, Y + 9.f), FVector2D(CW, 10.f) * S, Alpha, 0.f, MTUI::Gold);
		const FVector2D Knob = D(G, CX + CW * Alpha, Y + 14.f);
		MTUI::Glow(Out, L + 5, G, Knob, (22.f + 6.f * H) * S, MTUI::GoldBright.CopyWithNewOpacity(0.55f + 0.2f * H));
		const FVector2D Stud = FVector2D(12.f, 26.f) * S;
		MTUI::Box(Out, L + 6, G, Knob - Stud * 0.5f, Stud, FLinearColor(0.35f, 0.21f, 0.05f, 1.f));
		MTUI::GoldFrame(Out, L + 7, G, Knob - Stud * 0.5f, Stud, 1.f, false);
		Button(G, Out, L, D(G, CX + CW + 16.f, Y - 6.f), FVector2D(42.f, 38.f) * S, TEXT("+"), EAction::Step, Slider.Id, 1, false, false, 20.f, Alpha < 0.999f);
		MTUI::Text(Out, L, G, Slider.Value, MTUI::Heading(18.f * S), D(G, CX + CW + 76.f, Y + 1.f), MTUI::InkSoft, false);
		AddHit(D(G, CX, Y - 8.f), FVector2D(CW, 44.f) * S, EAction::Slider, Slider.Id);
		Y += 60.f;
	}
	struct FToggle { FName Id; const TCHAR* Label; bool bOn; };
	const FToggle Toggles[] = {
		{ TEXT("InvertY"), TEXT("INVERT CAMERA Y"), Settings.bInvertY },
		{ TEXT("ToggleSprint"), TEXT("TOGGLE SPRINT"), Settings.bToggleSprint },
		{ TEXT("SkipRoll"), TEXT("SKIP ROLL REVEAL"), Settings.bSkipRollAnimation },
	};
	for (const FToggle& Toggle : Toggles)
	{
		MTUI::Text(Out, L, G, Toggle.Label, MTUI::Heading(19.f * S), D(G, LX, Y), MTUI::Ink, false);
		Button(G, Out, L, D(G, CX - 58.f, Y - 8.f), FVector2D(160.f, 44.f) * S, Toggle.bOn ? TEXT("ON") : TEXT("OFF"), EAction::Toggle, Toggle.Id, INDEX_NONE, Toggle.bOn, Toggle.bOn, 18.f);
		Y += 60.f;
	}
	MTUI::Text(Out, L, G, TEXT("GRAPHICS"), MTUI::Heading(19.f * S), D(G, LX, Y), MTUI::Ink, false);
	const TCHAR* Qualities[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bOn = Settings.GraphicsQuality == i;
		Button(G, Out, L, D(G, CX - 58.f + i * 128.f, Y - 8.f), FVector2D(118.f, 44.f) * S, Qualities[i], EAction::Quality, NAME_None, i, bOn, bOn, 16.f);
	}
	Y += 72.f;

	// Save and load (only here: the pause menu has no save buttons).
	const UMTSaveSubsystem* Save = MTJ::GetSave(PlayerController);
	const bool bHasSave = Save && Save->HasSave(Save->GetCurrentSlot());
	Button(G, Out, L, D(G, LX, Y), FVector2D(250.f, 56.f) * S, TEXT("SAVE GAME"), EAction::Save, NAME_None, INDEX_NONE, false, false, 19.f, Save != nullptr);
	Button(G, Out, L, D(G, LX + 266.f, Y), FVector2D(250.f, 56.f) * S, TEXT("LOAD GAME"), EAction::Load, NAME_None, INDEX_NONE, false, false, 19.f, bHasSave);
	if (Save)
	{
		const FString Status = bHasSave
			? FString::Printf(TEXT("Slot \"%s\" holds a save. Autosave every %d s."), *Save->GetCurrentSlot(), FMath::RoundToInt(Save->AutosaveInterval))
			: FString::Printf(TEXT("No save in slot \"%s\" yet."), *Save->GetCurrentSlot());
		Paragraph(G, Out, L, Status, MTUI::BodyItalic(18.f * S), D(G, LX + 540.f, Y + 4.f), 420.f * S, MTUI::InkSoft, 2);
	}
	Y += 80.f;
	Paragraph(G, Out, L, TEXT("Field of view and graphics apply at once. Settings are kept in your save."), MTUI::BodyItalic(17.f * S), D(G, LX, Y), 980.f * S, MTUI::InkSoft, 1);
	return L + 10;
}

// ---------------------------------------------------------------------------------------------
// Roll
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintRoll(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	UMTRollSubsystem* Rolls = MTJ::GetRolls(PlayerController);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	if (!Rolls || !Prog)
	{
		return PaintUnavailable(G, Out, Layer, TEXT("Rolls are unavailable."));
	}
	for (int32 i = 0; i < 3; ++i)
	{
		const EMTRollCategory C = (EMTRollCategory)i;
		Button(G, Out, Layer, D(G, MTJ::Left + i * 316.f, MTJ::Top), FVector2D(300.f, 56.f) * S, FString::Printf(TEXT("%s%s%d"), *MTJ::CategoryLabel(C), MTJ::Sep, Prog->GetSpins(C)),
			EAction::RollCat, NAME_None, i, false, C == RollCategory, 18.f, !bRollAnimating || C == RollCategory);
	}

	// Left: what can come out, with the displayed odds.
	MTUI::ParchmentPanel(Out, Layer, G, D(G, MTJ::Left, 262.f), FVector2D(840.f, MTJ::Bottom - 262.f) * S, 1.f);
	const int32 L = Layer + 6;
	MTUI::Text(Out, L, G, TEXT("THE POOL"), MTUI::Title(28.f * S), D(G, 104.f, 288.f), MTUI::Ink, false);
	MTUI::Text(Out, L, G, TEXT("Displayed odds are base rates, before pity."), MTUI::BodyItalic(18.f * S), D(G, 104.f, 330.f), MTUI::InkSoft, false);
	MTUI::Divider(Out, L, G, D(G, 480.f, 386.f), 300.f * S, 0.9f);
	TArray<FName> Ids;
	TArray<EMTRarity> Rarities;
	TArray<float> Odds;
	Rolls->GetPool(RollCategory, Ids, Rarities, Odds);
	const int32 Visible = 11;
	const int32 First = ScrollList(G, Out, L + 10, TEXT("Pool"), Ids.Num(), Visible, D(G, 104.f, 410.f), FVector2D(752.f, Visible * 44.f) * S, true);
	for (int32 i = First; i < Ids.Num() && i < First + Visible && Rarities.IsValidIndex(i) && Odds.IsValidIndex(i); ++i)
	{
		const float RY = 410.f + (i - First) * 44.f;
		const FLinearColor Color = MTUtil::RarityColor(Rarities[i]);
		MTUI::Glow(Out, L, G, D(G, 116.f, RY + 15.f), 13.f * S, Color.CopyWithNewOpacity(0.55f));
		MTJ::FillDiamond(Out, L + 1, G, D(G, 116.f, RY + 15.f), 6.f * S, MTJ::Ink(Color));
		const FSlateFontInfo NameFont = MTUI::Heading(19.f * S);
		MTUI::Text(Out, L + 1, G, MTJ::FitText(Rolls->GetEntryDisplayName(RollCategory, Ids[i]).ToString(), NameFont, 360.f * S), NameFont, D(G, 138.f, RY + 2.f), MTUI::Ink, false);
		MTUI::Text(Out, L + 1, G, MTUtil::RarityToString(Rarities[i]), MTUI::BodyItalic(18.f * S), D(G, 510.f, RY + 3.f), MTJ::Ink(Color), false);
		if (Rolls->IsEntryOwned(RollCategory, Ids[i]))
		{
			Chip(G, Out, L + 1, D(G, 640.f, RY + 5.f), TEXT("OWNED"), MTJ::Ink(MTUI::Stamina), true, 11.f);
		}
		MTUI::TextAligned(Out, L + 1, G, FString::Printf(TEXT("%.2f%%"), Odds[i] * 100.f), MTUI::Heading(18.f * S), D(G, 104.f, RY + 3.f), FVector2D(752.f, 24.f) * S,
			FVector2D(1.f, 0.f), MTUI::Ink, false);
		MTUI::Line(Out, L, G, D(G, 104.f, RY + 38.f), D(G, 856.f, RY + 38.f), MTUI::InkSoft.CopyWithNewOpacity(0.15f), 1.f);
	}
	if (Ids.Num() == 0)
	{
		Paragraph(G, Out, L, TEXT("Nothing can be rolled in this category yet."), MTUI::BodyItalic(21.f * S), D(G, 104.f, 412.f), 752.f * S, MTUI::InkSoft, 2);
	}

	// Right: spins, pity, the roll itself, the result and the history.
	const float RX = 944.f;
	MTUI::Text(Out, Layer, G, FString::Printf(TEXT("%s SPINS"), *MTJ::CategoryLabel(RollCategory)), MTUI::Heading(16.f * S), D(G, RX, 262.f), MTUI::TextLight.CopyWithNewOpacity(0.75f));
	MTUI::Text(Out, Layer, G, FString::FromInt(Prog->GetSpins(RollCategory)), MTUI::Title(54.f * S), D(G, RX, 282.f), MTUI::GoldBright);

	const FMTRollConfig Config = Rolls->GetEffectiveConfig(RollCategory);
	const FMTRollPityState Pity = Prog->GetPity(RollCategory);
	const int32 UntilLegendary = Rolls->GetSpinsUntilHardPity(RollCategory);
	const int32 UntilMythic = Rolls->GetSpinsUntilMythicPity(RollCategory);
	const float PX = 1180.f;
	const float PW = 676.f;
	float PY = 266.f;
	if (Rolls->IsPoolAllLegendaryPlus(RollCategory))
	{
		MTUI::Text(Out, Layer, G, TEXT("Every result in this pool is Legendary or better."), MTUI::Body(19.f * S), D(G, PX, PY), MTUI::TextLight);
		PY += 40.f;
	}
	else if (UntilLegendary > 0)
	{
		const float Progress = Rolls->GetLegendaryPityProgress(RollCategory);
		MTUI::Text(Out, Layer, G, FString::Printf(TEXT("Guaranteed Legendary+ in %d"), UntilLegendary), MTUI::Body(19.f * S), D(G, PX, PY), MTUI::TextLight);
		if (Pity.SpinsSinceLegendary >= Config.SoftPityStart)
		{
			MTUI::TextAligned(Out, Layer, G, TEXT("Soft pity: odds rising"), MTUI::BodyItalic(17.f * S), D(G, PX, PY + 2.f), FVector2D(PW, 24.f) * S, FVector2D(1.f, 0.f),
				MTUtil::RarityColor(EMTRarity::Legendary));
		}
		MTUI::Bar(Out, Layer, G, D(G, PX, PY + 30.f), FVector2D(PW, 12.f) * S, Progress, Progress, MTUtil::RarityColor(EMTRarity::Legendary));
		PY += 58.f;
	}
	else
	{
		MTUI::Text(Out, Layer, G, TEXT("No Legendary+ entries in this pool."), MTUI::Body(19.f * S), D(G, PX, PY), MTUI::TextLight.CopyWithNewOpacity(0.7f));
		PY += 40.f;
	}
	if (UntilMythic > 0)
	{
		const float Progress = FMath::Clamp((float)Pity.SpinsSinceMythic / (float)FMath::Max(1, Config.MythicHardPity), 0.f, 1.f);
		MTUI::Text(Out, Layer, G, FString::Printf(TEXT("Guaranteed Mythic in %d"), UntilMythic), MTUI::Body(19.f * S), D(G, PX, PY), MTUI::TextLight.CopyWithNewOpacity(0.85f));
		MTUI::Bar(Out, Layer, G, D(G, PX, PY + 30.f), FVector2D(PW, 8.f) * S, Progress, Progress, MTUtil::RarityColor(EMTRarity::Mythic));
	}

	// The roll resolves (and is saved) at once; the reel is presentation and can always be skipped.
	const bool bCanRoll = Rolls->CanRoll(RollCategory);
	const FString RollLabel = bRollAnimating ? FString(TEXT("SKIP")) : (bCanRoll ? FString::Printf(TEXT("ROLL%s1 SPIN"), MTJ::Sep) : FString(TEXT("NO SPINS")));
	Button(G, Out, Layer, D(G, RX, 420.f), FVector2D(440.f, 76.f) * S, RollLabel, EAction::Roll, NAME_None, INDEX_NONE, bCanRoll && !bRollAnimating, bRollAnimating, 26.f,
		bRollAnimating || bCanRoll);
	if (!bCanRoll && !bRollAnimating)
	{
		Paragraph(G, Out, Layer, TEXT("Spins come from quests, bosses, level-ups and rank-ups."), MTUI::BodyItalic(19.f * S), D(G, RX + 464.f, 428.f), 450.f * S,
			MTUI::TextLight.CopyWithNewOpacity(0.75f), 2);
	}

	const FVector2D CardPos = D(G, RX, 522.f);
	const FVector2D CardSize = FVector2D(916.f, 250.f) * S;
	const FVector2D Center = CardPos + CardSize * 0.5f;
	if (bRollAnimating && RollReel.Num() > 0)
	{
		const float T = FMath::Clamp(float(Now - RollStart) / RollAnimDuration, 0.f, 1.f);
		const float Eased = 1.f - FMath::Pow(1.f - T, 3.f);
		const int32 Step = FMath::Min(RollReelTicks, FMath::FloorToInt(Eased * RollReelTicks));
		const FString& Name = RollReel[(RollReelStart + Step) % RollReel.Num()];
		MTUI::Glow(Out, Layer, G, Center, CardSize.X * 0.45f, MTUI::Gold.CopyWithNewOpacity(0.12f + 0.08f * FMath::Sin(Now * 18.0)));
		MTUI::DarkPanel(Out, Layer + 1, G, CardPos, CardSize, 1.f, true);
		const float Jitter = (1.f - Eased) * 4.f * S * FMath::Sin(Now * 60.0);
		MTUI::TextAligned(Out, Layer + 4, G, Name, MTUI::Title(46.f * S), CardPos + FVector2D(Jitter, -12.f * S), CardSize, FVector2D(0.5f, 0.5f), MTUI::TextLight);
		MTUI::TextAligned(Out, Layer + 4, G, TEXT("click or press ENTER to skip"), MTUI::BodyItalic(18.f * S), CardPos + FVector2D(0.f, CardSize.Y - 46.f * S),
			FVector2D(CardSize.X, 30.f * S), FVector2D(0.5f, 0.5f), MTUI::TextLight.CopyWithNewOpacity(0.6f));
		AddHit(CardPos, CardSize, EAction::RollSkip);
	}
	else if (bHasLastRoll && LastRoll.Category == RollCategory)
	{
		const FLinearColor Color = MTUtil::RarityColor(LastRoll.Rarity);
		const float Reveal = FMath::Clamp(float(Now - RollRevealAt) / 0.9f, 0.f, 1.f);
		// The rarity's light bursts out, then settles into a glow.
		MTUI::Glow(Out, Layer, G, Center, CardSize.X * (0.35f + 0.4f * Reveal), Color.CopyWithNewOpacity(0.18f + 0.5f * (1.f - Reveal)));
		MTUI::DarkPanel(Out, Layer + 1, G, CardPos, CardSize, 1.f, true);
		MTJ::Outline(Out, Layer + 3, G, CardPos + FVector2D(10.f * S), CardSize - FVector2D(20.f * S), Color.CopyWithNewOpacity(0.8f), 1.5f);
		MTUI::TextAligned(Out, Layer + 4, G, LastRoll.DisplayName.ToString(), MTUI::Title(48.f * S), CardPos + FVector2D(0.f, 26.f * S), FVector2D(CardSize.X, 66.f * S),
			FVector2D(0.5f, 0.5f), Color);
		MTUI::TextAligned(Out, Layer + 4, G, MTUtil::RarityToString(LastRoll.Rarity).ToUpper(), MTUI::Heading(19.f * S), CardPos + FVector2D(0.f, 100.f * S), FVector2D(CardSize.X, 28.f * S),
			FVector2D(0.5f, 0.5f), Color);
		const FString Status = LastRoll.bWasNew ? FString(TEXT("NEW!"))
			: FString::Printf(TEXT("DUPLICATE%s+%d GOLD, +%d MASTERY XP"), MTJ::Sep, UMTRollSubsystem::DuplicateGold, FMath::RoundToInt(UMTRollSubsystem::DuplicateMasteryXP));
		MTUI::TextAligned(Out, Layer + 4, G, Status, MTUI::Heading(17.f * S), CardPos + FVector2D(0.f, 140.f * S), FVector2D(CardSize.X, 26.f * S), FVector2D(0.5f, 0.5f),
			LastRoll.bWasNew ? FLinearColor(0.55f, 1.f, 0.6f, 1.f) : MTUI::TextLight);
		TArray<FString> Flags;
		if (LastRoll.bPityTriggered)
		{
			Flags.Add(TEXT("PITY GUARANTEE"));
		}
		if (LastRoll.bDuplicateRerolled)
		{
			Flags.Add(TEXT("DUPLICATE PROTECTION"));
		}
		MTUI::TextAligned(Out, Layer + 4, G, FString::Join(Flags, MTJ::Sep), MTUI::Heading(14.f * S), CardPos + FVector2D(0.f, 176.f * S), FVector2D(CardSize.X, 24.f * S),
			FVector2D(0.5f, 0.5f), MTUI::Gold);
		if (Reveal < 1.f)
		{
			MTUI::Box(Out, Layer + 6, G, CardPos, CardSize, Color.CopyWithNewOpacity(0.6f * (1.f - Reveal) * (1.f - Reveal)));
		}
	}
	else
	{
		MTUI::DarkPanel(Out, Layer + 1, G, CardPos, CardSize, 0.75f, true);
		MTUI::TextAligned(Out, Layer + 4, G, TEXT("Your fortune appears here."), MTUI::BodyItalic(26.f * S), CardPos, CardSize, FVector2D(0.5f, 0.5f),
			MTUI::TextLight.CopyWithNewOpacity(0.6f));
	}

	Section(G, Out, Layer, D(G, RX, 800.f), 916.f * S, TEXT("HISTORY"), false);
	const TArray<FMTRollHistoryEntry>& History = Prog->GetRollHistory();
	float HY = 838.f;
	int32 Shown = 0;
	for (int32 i = History.Num() - 1; i >= 0 && Shown < 5; --i)
	{
		const FMTRollHistoryEntry& Entry = History[i];
		// The result being revealed stays off the list until the reel stops.
		if (Entry.Category != RollCategory || (bRollAnimating && i == History.Num() - 1))
		{
			continue;
		}
		const FLinearColor Color = MTUtil::RarityColor(Entry.Rarity);
		MTJ::FillDiamond(Out, Layer, G, D(G, RX + 8.f, HY + 13.f), 5.f * S, Color);
		FString Line = Rolls->GetEntryDisplayName(Entry.Category, Entry.ResultId).ToString() + MTJ::Sep + MTUtil::RarityToString(Entry.Rarity);
		if (Entry.bWasNew)
		{
			Line += FString(MTJ::Sep) + TEXT("NEW");
		}
		if (Entry.bPityTriggered)
		{
			Line += FString(MTJ::Sep) + TEXT("pity");
		}
		MTUI::Text(Out, Layer, G, Line, MTUI::Body(19.f * S), D(G, RX + 24.f, HY), Color);
		HY += 30.f;
		++Shown;
	}
	if (Shown == 0)
	{
		MTUI::Text(Out, Layer, G, TEXT("No rolls yet."), MTUI::BodyItalic(19.f * S), D(G, RX, HY), MTUI::TextLight.CopyWithNewOpacity(0.6f));
	}
	return Layer + 20;
}

// ---------------------------------------------------------------------------------------------
// NPC quest dialog
// ---------------------------------------------------------------------------------------------

int32 SMTJournal::PaintDialog(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const FVector2D Screen = G.GetLocalSize();
	const float Open = FMath::Clamp(float(Now - OpenedAt) / 0.25f, 0.f, 1.f);
	UMTQuestSubsystem* Quests = MTJ::GetQuests(PlayerController);
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	const TArray<FName> Offers = Quests ? Quests->GetAvailableQuestsForNPC(DialogNpcId) : TArray<FName>();
	const TArray<FName> TurnIns = Quests ? Quests->GetTurnInQuestsForNPC(DialogNpcId) : TArray<FName>();

	// The scene stays visible; only the lower part darkens behind the dialog.
	TArray<FSlateGradientStop> Shade;
	Shade.Add(FSlateGradientStop(FVector2D(0.f, Screen.Y * 0.35f), FLinearColor(0.f, 0.f, 0.f, 0.f)));
	Shade.Add(FSlateGradientStop(FVector2D(0.f, Screen.Y), FLinearColor(0.f, 0.f, 0.01f, 0.72f * Open)));
	FSlateDrawElement::MakeGradient(Out, Layer, G.ToPaintGeometry(), Shade, Orient_Horizontal);

	SlideY = 24.f * FMath::Pow(1.f - Open, 3.f);
	MTUI::ParchmentPanel(Out, Layer + 1, G, D(G, 240.f, 640.f), FVector2D(1440.f, 372.f) * S, 1.f);
	int32 L = Layer + 8;

	// Name plate.
	const FString Name = DialogNpcName.ToString();
	const FSlateFontInfo NameFont = MTUI::Title(26.f * S);
	const FVector2D PlatePos = D(G, 290.f, 604.f);
	const FVector2D PlateSize(MTUI::Measure(Name, NameFont).X + 64.f * S, 60.f * S);
	MTUI::DarkPanel(Out, L, G, PlatePos, PlateSize, 1.f, false);
	MTUI::TextAligned(Out, L + 3, G, Name, NameFont, PlatePos, PlateSize, FVector2D(0.5f, 0.5f), MTUI::GoldBright);
	L += 6;

	const FVector2D ButtonSize = FVector2D(220.f, 56.f) * S;
	const FVector2D RightButton = D(G, 1636.f - 220.f, 946.f);
	const FVector2D LeftButton = D(G, 1636.f - 452.f, 946.f);
	if (DialogQuestId.IsNone())
	{
		Paragraph(G, Out, L, TEXT("\"Nothing I need help with right now, adventurer. Check back later - and mind the roads.\""), MTUI::BodyItalic(25.f * S),
			D(G, 300.f, 690.f), 1320.f * S, MTUI::Ink, 3);
		Button(G, Out, L, RightButton, ButtonSize, TEXT("FAREWELL"), EAction::DlgClose, NAME_None, INDEX_NONE, false, false, 20.f);
		SlideY = 0.f;
		return L + 8;
	}

	// Quests this person offers or can take back.
	float ListY = 686.f;
	auto Choice = [&](FName QuestId, bool bTurnIn)
	{
		if (ListY > 900.f)
		{
			return;
		}
		ListCard(G, Out, L, D(G, 290.f, ListY), FVector2D(400.f, 64.f) * S, nullptr, false, MTJ::QuestTitle(Registry, QuestId),
			bTurnIn ? TEXT("Ready to turn in") : TEXT("New quest"), bTurnIn ? MTUI::Stamina : MTUI::GoldBright, EAction::DlgSel, QuestId, INDEX_NONE,
			QuestId == DialogQuestId, bTurnIn ? TEXT("DONE") : TEXT("NEW"));
		ListY += 72.f;
	};
	for (const FName& Id : TurnIns)
	{
		Choice(Id, true);
	}
	for (const FName& Id : Offers)
	{
		Choice(Id, false);
	}

	const bool bTurnIn = TurnIns.Contains(DialogQuestId);
	const FMTQuestData* Quest = Registry ? Registry->FindQuest(DialogQuestId) : nullptr;
	const float TX = 730.f;
	const float TW = 906.f;
	float TY = 676.f;
	const FSlateFontInfo TitleFont = MTUI::Title(28.f * S);
	MTUI::Text(Out, L, G, MTJ::FitText(MTJ::QuestTitle(Registry, DialogQuestId), TitleFont, TW * S), TitleFont, D(G, TX, TY), MTUI::Ink, false);
	TY += 46.f;
	if (Quest)
	{
		const TArray<FText>& Lines = bTurnIn ? Quest->CompleteDialogue : Quest->OfferDialogue;
		FString Dialogue;
		for (const FText& Line : Lines)
		{
			Dialogue += (Dialogue.IsEmpty() ? FString() : FString(TEXT(" "))) + TEXT("\"") + Line.ToString() + TEXT("\"");
		}
		if (Dialogue.IsEmpty())
		{
			Dialogue = Quest->Summary.ToString();
		}
		TY += Paragraph(G, Out, L, Dialogue, MTUI::BodyItalic(21.f * S), D(G, TX, TY), TW * S, MTUI::Ink, 3) / S + 6.f;
		if (!bTurnIn)
		{
			FString Objectives;
			for (const FMTQuestObjective& Objective : Quest->Objectives)
			{
				Objectives += (Objectives.IsEmpty() ? FString() : FString(TEXT("   "))) + TEXT("- ") + Objective.Description.ToString();
				if (Quest->bSequentialObjectives)
				{
					break; // sequential stories reveal one step at a time
				}
			}
			Paragraph(G, Out, L, Objectives, MTUI::Body(18.f * S), D(G, TX, TY), TW * S, MTUI::InkSoft, 1);
		}
		const TArray<FString> Rewards = MTJ::RewardParts(Quest->Reward, Registry);
		MTUI::Text(Out, L, G, TEXT("REWARDS"), MTUI::Heading(13.f * S), D(G, TX, 906.f), MTUI::InkSoft, false);
		MTUI::Text(Out, L, G, MTJ::FitText(Rewards.Num() > 0 ? FString::Join(Rewards, MTJ::Sep) : FString(TEXT("None")), MTUI::Body(19.f * S), (TW - 110.f) * S),
			MTUI::Body(19.f * S), D(G, TX + 104.f, 900.f), MTUI::Ink, false);
	}
	if (bTurnIn)
	{
		Button(G, Out, L, LeftButton, ButtonSize, TEXT("TURN IN"), EAction::DlgTurnIn, DialogQuestId, INDEX_NONE, true, false, 20.f);
		Button(G, Out, L, RightButton, ButtonSize, TEXT("LATER"), EAction::DlgClose, NAME_None, INDEX_NONE, false, false, 20.f);
	}
	else
	{
		Button(G, Out, L, LeftButton, ButtonSize, TEXT("ACCEPT"), EAction::DlgAccept, DialogQuestId, INDEX_NONE, true, false, 20.f);
		Button(G, Out, L, RightButton, ButtonSize, TEXT("DECLINE"), EAction::DlgClose, NAME_None, INDEX_NONE, false, false, 20.f);
	}
	SlideY = 0.f;
	return L + 8;
}

// ---------------------------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------------------------

void SMTJournal::ShowNotice(const FText& Message, const FLinearColor& Color)
{
	NoticeText = Message;
	NoticeColor = Color;
	NoticeAt = SlateNow();
}

void SMTJournal::HandleAction(const FHit& Hit)
{
	UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	UMTQuestSubsystem* Quests = MTJ::GetQuests(PlayerController);
	switch (Hit.Action)
	{
	case EAction::Tab:
		OpenPage((EMTMenuPage)Hit.Index);
		break;
	case EAction::Close:
	case EAction::DlgClose:
		MTUI::Sound(TEXT("ui_close"), 0.5f);
		CloseAll();
		break;
	case EAction::SelChar:
		if (SelCharacter != Hit.Param)
		{
			SelCharacter = Hit.Param;
			InspectAbility = NAME_None;
		}
		break;
	case EAction::EquipChar:
		if (Prog && Prog->EquipCharacter(Hit.Param))
		{
			MTUI::Sound(TEXT("ui_confirm"), 0.6f);
			ShowNotice(FText::Format(LOCTEXT("EquippedChar", "Now playing as {0}"), Prog->GetCharacterDisplayName(Hit.Param)), MTUI::GoldBright);
		}
		break;
	case EAction::Hotbar:
	{
		// The loadout editor lives in the pause menu's ABILITIES page; BACK there returns to the pause menu.
		APlayerController* PC = PlayerController.Get();
		if (UMTFrontEndSubsystem* FrontEnd = PC ? UMTFrontEndSubsystem::Get(PC) : nullptr)
		{
			CloseAll();
			FrontEnd->OpenAbilities(PC);
		}
		break;
	}
	case EAction::Inspect:
		InspectAbility = Hit.Param;
		break;
	case EAction::SelElem:
		SelElement = (EMTElement)Hit.Index;
		break;
	case EAction::SelRace:
		SelRace = (EMTRace)Hit.Index;
		bSelRaceValid = true;
		break;
	case EAction::EquipRace:
		if (Prog && Prog->EquipRace((EMTRace)Hit.Index))
		{
			MTUI::Sound(TEXT("ui_confirm"), 0.6f);
			ShowNotice(FText::Format(LOCTEXT("EquippedRace", "Your race is now {0}"), Prog->GetRaceDisplayName((EMTRace)Hit.Index)), MTUI::GoldBright);
		}
		break;
	case EAction::SelQuest:
		SelQuest = Hit.Param;
		break;
	case EAction::Track:
		if (Quests)
		{
			Quests->SetTrackedQuest(Hit.Param);
			ShowNotice(FText::Format(LOCTEXT("Tracking", "Tracking: {0}"), FText::FromString(MTJ::QuestTitle(MTJ::GetRegistry(PlayerController), Hit.Param))), MTUI::GoldBright);
		}
		break;
	case EAction::SelLoc:
		SelLocation = Hit.Param;
		MTUI::Sound(TEXT("map_select"), 0.5f);
		break;
	case EAction::Travel:
		FastTravelTo(Hit.Param);
		break;
	case EAction::Scroll:
	{
		int32 Max = 0;
		for (const FScrollRegion& Region : ScrollRegions)
		{
			Max = Region.List == Hit.Param ? Region.Max : Max;
		}
		int32& Value = Scroll.FindOrAdd(Hit.Param);
		Value = FMath::Clamp(Value + Hit.Index, 0, Max);
		break;
	}
	case EAction::Step:
		StepSetting(Hit.Param, Hit.Index);
		break;
	case EAction::Toggle:
		if (Prog)
		{
			FMTSettingsSave Settings = Prog->GetSettings();
			if (Hit.Param == TEXT("InvertY")) { Settings.bInvertY = !Settings.bInvertY; }
			if (Hit.Param == TEXT("ToggleSprint")) { Settings.bToggleSprint = !Settings.bToggleSprint; }
			if (Hit.Param == TEXT("SkipRoll")) { Settings.bSkipRollAnimation = !Settings.bSkipRollAnimation; }
			Prog->SetSettings(Settings);
			if (UMTSaveSubsystem* Save = MTJ::GetSave(PlayerController))
			{
				Save->ApplySettings(false);
			}
		}
		break;
	case EAction::Quality:
		if (Prog)
		{
			FMTSettingsSave Settings = Prog->GetSettings();
			Settings.GraphicsQuality = FMath::Clamp(Hit.Index, 0, 3);
			Prog->SetSettings(Settings);
			if (UMTSaveSubsystem* Save = MTJ::GetSave(PlayerController))
			{
				Save->ApplySettings(true);
			}
		}
		break;
	case EAction::Save:
	{
		UMTSaveSubsystem* Save = MTJ::GetSave(PlayerController);
		const bool bOk = Save && Save->SaveGame(Save->GetCurrentSlot());
		MTUI::Sound(bOk ? TEXT("ui_confirm") : TEXT("ui_error"), 0.5f);
		ShowNotice(bOk ? LOCTEXT("Saved", "Game saved") : LOCTEXT("SaveFail", "Save failed"), bOk ? MTUI::GoldBright : MTUI::Danger);
		break;
	}
	case EAction::Load:
	{
		UMTSaveSubsystem* Save = MTJ::GetSave(PlayerController);
		const bool bOk = Save && Save->LoadGame(Save->GetCurrentSlot());
		MTUI::Sound(bOk ? TEXT("ui_confirm") : TEXT("ui_error"), 0.5f);
		ShowNotice(bOk ? LOCTEXT("Loaded", "Game loaded") : LOCTEXT("LoadFail", "No save to load"), bOk ? MTUI::GoldBright : MTUI::Danger);
		break;
	}
	case EAction::RollCat:
		if (!bRollAnimating)
		{
			RollCategory = (EMTRollCategory)FMath::Clamp(Hit.Index, 0, 2);
			Scroll.Remove(TEXT("Pool"));
		}
		break;
	case EAction::Roll:
		DoRoll();
		break;
	case EAction::RollSkip:
		FinishRollAnimation();
		break;
	case EAction::DlgSel:
		DialogQuestId = Hit.Param;
		break;
	case EAction::DlgAccept:
		if (Quests && Quests->AcceptQuest(Hit.Param))
		{
			MTUI::Sound(TEXT("ui_confirm"), 0.6f);
			ShowNotice(FText::Format(LOCTEXT("Accepted", "Quest accepted: {0}"), FText::FromString(MTJ::QuestTitle(MTJ::GetRegistry(PlayerController), Hit.Param))), MTUI::GoldBright);
			if (Quests->GetTrackedQuest().IsNone())
			{
				Quests->SetTrackedQuest(Hit.Param);
			}
		}
		DialogQuestId = NAME_None;
		break;
	case EAction::DlgTurnIn:
		if (Quests && Quests->TurnInQuest(Hit.Param))
		{
			MTUI::Sound(TEXT("ui_confirm"), 0.6f);
			ShowNotice(FText::Format(LOCTEXT("TurnedIn", "Quest complete: {0}"), FText::FromString(MTJ::QuestTitle(MTJ::GetRegistry(PlayerController), Hit.Param))), MTUI::GoldBright);
		}
		DialogQuestId = NAME_None;
		break;
	default:
		break;
	}
}

void SMTJournal::DoRoll()
{
	if (bRollAnimating)
	{
		FinishRollAnimation();
		return;
	}
	UMTRollSubsystem* Rolls = MTJ::GetRolls(PlayerController);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	if (!Rolls || !Prog)
	{
		return;
	}
	if (!Rolls->CanRoll(RollCategory))
	{
		MTUI::Sound(TEXT("ui_error"), 0.5f);
		ShowNotice(LOCTEXT("NoSpins", "No spins left - earn more by questing"), MTUI::Danger);
		return;
	}
	// The roll resolves (and is saved in progression) now; the reel below is presentation only.
	const FMTRollResult Result = Rolls->Roll(RollCategory);
	if (!Result.bValid)
	{
		return;
	}
	LastRoll = Result;
	bHasLastRoll = true;
	if (Prog->GetSettings().bSkipRollAnimation)
	{
		bRollAnimating = true;
		FinishRollAnimation();
		return;
	}
	TArray<FName> Ids;
	TArray<EMTRarity> Rarities;
	TArray<float> Odds;
	Rolls->GetPool(RollCategory, Ids, Rarities, Odds);
	RollReel.Reset();
	for (const FName& Id : Ids)
	{
		RollReel.Add(Rolls->GetEntryDisplayName(RollCategory, Id).ToString());
	}
	int32 ResultIndex = Ids.IndexOfByKey(Result.ResultId);
	if (ResultIndex == INDEX_NONE)
	{
		ResultIndex = RollReel.Add(Result.DisplayName.ToString());
	}
	const int32 N = FMath::Max(1, RollReel.Num());
	// The reel lands exactly on the result after RollReelTicks steps.
	RollReelStart = ((ResultIndex - RollReelTicks) % N + N) % N;
	RollStart = SlateNow();
	LastReelStep = -1;
	bRollAnimating = true;
}

void SMTJournal::FinishRollAnimation()
{
	if (!bRollAnimating)
	{
		return;
	}
	bRollAnimating = false;
	RollRevealAt = SlateNow();
	if (bHasLastRoll)
	{
		MTUI::Sound(LastRoll.Rarity >= EMTRarity::Legendary ? TEXT("ui_spawn") : TEXT("ui_confirm"), 0.6f);
	}
}

void SMTJournal::StepSetting(FName Setting, int32 Dir)
{
	UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	if (!Prog)
	{
		return;
	}
	// Same ranges as the sliders (and the front end's settings page).
	FMTSettingsSave Settings = Prog->GetSettings();
	if (Setting == TEXT("Volume"))
	{
		Settings.MasterVolume = FMath::Clamp(FMath::RoundToFloat((Settings.MasterVolume + 0.05f * Dir) * 20.f) / 20.f, 0.f, 1.f);
	}
	else if (Setting == TEXT("Mouse"))
	{
		Settings.MouseSensitivity = FMath::Clamp(FMath::RoundToFloat((Settings.MouseSensitivity + 0.1f * Dir) * 20.f) / 20.f, 0.2f, 3.f);
	}
	else if (Setting == TEXT("FOV"))
	{
		Settings.FieldOfView = FMath::Clamp(Settings.FieldOfView + 5.f * Dir, 70.f, 110.f);
	}
	else if (Setting == TEXT("Shake"))
	{
		Settings.CameraShakeScale = FMath::Clamp(FMath::RoundToFloat((Settings.CameraShakeScale + 0.1f * Dir) * 10.f) / 10.f, 0.f, 1.f);
	}
	Prog->SetSettings(Settings);
	if (UMTSaveSubsystem* Save = MTJ::GetSave(PlayerController))
	{
		Save->ApplySettings(false);
	}
}

void SMTJournal::SetSettingAlpha(FName Setting, float Alpha)
{
	UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	if (!Prog)
	{
		return;
	}
	FMTSettingsSave Settings = Prog->GetSettings();
	if (Setting == TEXT("Volume")) { Settings.MasterVolume = Alpha; }
	else if (Setting == TEXT("Mouse")) { Settings.MouseSensitivity = FMath::Lerp(0.2f, 3.f, Alpha); }
	else if (Setting == TEXT("FOV")) { Settings.FieldOfView = FMath::RoundToFloat(FMath::Lerp(70.f, 110.f, Alpha)); }
	else if (Setting == TEXT("Shake")) { Settings.CameraShakeScale = Alpha; }
	Prog->SetSettings(Settings);
	if (UMTSaveSubsystem* Save = MTJ::GetSave(PlayerController))
	{
		Save->ApplySettings(false);
	}
}

void SMTJournal::FastTravelTo(FName LocationId)
{
	const UMTDataRegistry* Registry = MTJ::GetRegistry(PlayerController);
	const UMTProgressionSubsystem* Prog = MTJ::GetProgression(PlayerController);
	APawn* Pawn = PlayerController.IsValid() ? PlayerController->GetPawn() : nullptr;
	const FMTLocationData* Loc = Registry ? Registry->FindLocation(LocationId) : nullptr;
	if (!Loc || !Prog || !Pawn || !Loc->bFastTravel || !Prog->IsLocationDiscovered(LocationId) || Prog->GetAdventurerRank() < Loc->RequiredRank)
	{
		MTUI::Sound(TEXT("ui_error"), 0.4f);
		return;
	}
	const FVector Destination = Loc->WorldLocation + FVector(0.f, 0.f, 200.f);
	if (!Pawn->TeleportTo(Destination, Pawn->GetActorRotation()))
	{
		Pawn->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	}
	CloseAll();
	MTUI::Sound(TEXT("ui_spawn"), 0.6f);
	if (AMTHUD* Owner = HUD.Get())
	{
		Owner->ShowCenterMessage(FText::Format(LOCTEXT("Travelled", "Travelled to {0}"), Loc->DisplayName.IsEmpty() ? FText::FromName(LocationId) : Loc->DisplayName), 2.f);
	}
}

#undef LOCTEXT_NAMESPACE

// LA PLACE in-game journal (Slate): the Adventurer's Journal pages (character, element, race, mastery, inventory,
// quests, map, party, settings, roll) and the NPC quest dialog, painted like the front end (dimmed world, gold
// filigree, parchment cards, Cinzel / Cormorant Garamond, painted icons and portraits). AMTHUD owns it and forwards
// the keyboard / gamepad menu input the player character receives; the mouse is handled here. Layout is in
// 1920x1080 design units.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/MTHUD.h"
#include "Core/MTTypes.h"
#include "Progression/MTRollSubsystem.h"

class APlayerController;
struct FSlateBrush;

class MUSHOKURPG_API SMTJournal : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMTJournal) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PlayerController)
		SLATE_ARGUMENT(TWeakObjectPtr<AMTHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens a journal page (closes the NPC dialog). None closes everything. */
	void OpenPage(EMTMenuPage NewPage);
	void OpenDialog(FName NpcId, const FText& NpcName);
	void CloseAll();
	bool IsOpen() const { return Page != EMTMenuPage::None || bDialogOpen; }
	EMTMenuPage GetPage() const { return Page; }
	bool IsDialogOpen() const { return bDialogOpen; }

	// Keyboard / gamepad, forwarded by AMTHUD. Screen space: X = right, Y = down.
	void Navigate(FIntPoint Direction);
	/** Activates the focused entry (or skips a running roll reveal). */
	void Confirm();
	/** Skips a roll reveal, else closes the dialog, else closes the journal. */
	void Back();
	void NextTab(int32 Dir);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.f, 1080.f); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	enum class EAction : uint8
	{
		None,
		Tab,
		Close,
		SelChar,
		EquipChar,
		Hotbar,
		Inspect,
		SelElem,
		SelRace,
		EquipRace,
		SelQuest,
		Track,
		SelLoc,
		Travel,
		Scroll,
		Slider,
		Step,
		Toggle,
		Quality,
		Save,
		Load,
		RollCat,
		Roll,
		RollSkip,
		DlgSel,
		DlgAccept,
		DlgTurnIn,
		DlgClose,
	};

	/** A clickable / focusable region registered while painting. */
	struct FHit
	{
		FSlateRect Rect;
		EAction Action = EAction::None;
		FName Param;
		int32 Index = INDEX_NONE;
		bool bEnabled = true;
	};

	/** A list that scrolls with the mouse wheel while the cursor is over it. */
	struct FScrollRegion
	{
		FSlateRect Rect;
		FName List;
		int32 Max = 0;
	};

	struct FMote
	{
		FVector2D Pos;
		FVector2D Vel;
		float Size = 3.f;
		float Life = 0.f;
		float MaxLife = 6.f;
		float Hue = 0.f;
	};

	// Frame and pages.
	int32 PaintFrame(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintCharacter(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintElement(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintRace(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintMastery(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintInventory(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintQuests(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintMap(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintParty(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintSettings(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintRoll(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintDialog(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintMotes(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, float Opacity) const;
	int32 PaintNotice(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Center) const;
	/** "Nothing to show" card used when a subsystem is missing. */
	int32 PaintUnavailable(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FString& Message) const;

	// Building blocks (positions in local units, sizes already scaled).
	int32 Button(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FVector2D& Size, const FString& Label,
		EAction Action, FName Param = NAME_None, int32 Index = INDEX_NONE, bool bPrimary = false, bool bSelected = false, float FontSize = 22.f, bool bEnabled = true) const;
	/** Dark list card: icon (or initials), title, subtitle, accent stripe and an optional tag; selectable. */
	int32 ListCard(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FVector2D& Size, const FSlateBrush* Icon, bool bIconSlot,
		const FString& Title, const FString& Subtitle, const FLinearColor& Accent, EAction Action, FName Param, int32 Index, bool bSelected, const FString& Tag = FString()) const;
	/** Small framed label (rarity, canon status, "TRACKED"); returns its width. */
	float Chip(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FString& Label, const FLinearColor& Color, bool bOnParchment, float FontSize = 13.f) const;
	float ChipWidth(const FGeometry& G, const FString& Label, float FontSize = 13.f) const;
	/** Section heading with a hairline to the right. */
	void Section(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, float Width, const FString& Label, bool bOnParchment, float FontSize = 15.f) const;
	/** Ability icon with a gold frame (element-coloured tile with initials when there is no painted icon). */
	void AbilityIcon(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, float Size, FName AbilityId, float FrameOpacity = 0.85f) const;
	/** Hotbar-style key tag (LMB, 1-4, F, G) at the top-left corner of a slot. */
	void KeyTag(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& SlotPos, const FString& Key) const;
	/** Ability row on parchment: icon, name, element / kind / mana / cooldown, description; returns the height used. */
	float AbilityRow(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, float Width, FName AbilityId, const FString& Tag, int32 MaxLines = 2) const;
	/** Word-wrapped paragraph clipped to MaxLines (the last line ends with "..."); returns the height used. */
	float Paragraph(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FString& Text, const FSlateFontInfo& Font, const FVector2D& Pos, float Width,
		const FLinearColor& Color, int32 MaxLines, float LineSpacing = 1.08f) const;
	/** Registers a wheel-scrollable list and draws its arrows when it overflows; returns the first visible index. */
	int32 ScrollList(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, FName List, int32 Count, int32 Visible, const FVector2D& Pos, const FVector2D& Size, bool bOnParchment) const;

	void AddHit(const FVector2D& Pos, const FVector2D& Size, EAction Action, FName Param = NAME_None, int32 Index = INDEX_NONE, bool bEnabled = true) const;
	float HoverAlpha(EAction Action, FName Param = NAME_None, int32 Index = INDEX_NONE) const;
	static FName HoverKey(EAction Action, FName Param, int32 Index);
	const FHit* FindHit(FName Key) const;

	/** Design space (1920x1080, centred, plus the page slide) to local widget space. */
	FVector2D D(const FGeometry& G, float X, float Y) const;
	float DS(const FGeometry& G) const;
	double SlateNow() const;

	// State and actions.
	void ValidateSelections();
	void HandleAction(const FHit& Hit);
	void DoRoll();
	/** Ends the reel and reveals the result (with its sound). */
	void FinishRollAnimation();
	/** Ends the reel without the reveal (leaving the page). */
	void StopRollReveal();
	void StepSetting(FName Setting, int32 Dir);
	void SetSettingAlpha(FName Setting, float Alpha);
	void FastTravelTo(FName LocationId);
	void ShowNotice(const FText& Message, const FLinearColor& Color);
	/** The ability the character page describes: hovered or focused hotbar slot, else the last one clicked. */
	FName GetDetailAbility(FName Fallback) const;

	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<AMTHUD> HUD;

	EMTMenuPage Page = EMTMenuPage::None;
	double Now = 0.0;
	double PageSince = 0.0;
	double OpenedAt = 0.0;
	mutable TArray<FHit> Hits;
	mutable TArray<FScrollRegion> ScrollRegions;
	/** Added to every design-space Y while a page's content is painted (slide-in). */
	mutable float SlideY = 0.f;
	FVector2D Mouse = FVector2D(-1.f, -1.f);
	FName HoveredKey;
	FHit HoverHit;
	FName FocusKey;
	FHit FocusHit;
	bool bShowFocus = false;
	TMap<FName, float> Hover;
	TMap<FName, int32> Scroll;
	TArray<FMote> Motes;

	// Page selections (kept valid by ValidateSelections every tick).
	FName SelCharacter;
	FName InspectAbility;
	EMTElement SelElement = EMTElement::None;
	EMTRace SelRace = EMTRace::Human;
	bool bSelRaceValid = false;
	FName SelQuest;
	FName SelLocation;
	FName DraggingSetting;

	// NPC dialog.
	bool bDialogOpen = false;
	FName DialogNpcId;
	FText DialogNpcName;
	FName DialogQuestId;

	// Roll presentation (the roll itself resolves at once and is saved; this is only the reveal).
	EMTRollCategory RollCategory = EMTRollCategory::Character;
	FMTRollResult LastRoll;
	bool bHasLastRoll = false;
	bool bRollAnimating = false;
	double RollStart = 0.0;
	double RollRevealAt = -100.0;
	int32 LastReelStep = -1;
	TArray<FString> RollReel;
	int32 RollReelStart = 0;
	static constexpr float RollAnimDuration = 1.6f;
	static constexpr int32 RollReelTicks = 26;

	// Feedback line (equipped, saved, quest accepted...).
	FText NoticeText;
	FLinearColor NoticeColor = FLinearColor::White;
	double NoticeAt = -100.0;
};

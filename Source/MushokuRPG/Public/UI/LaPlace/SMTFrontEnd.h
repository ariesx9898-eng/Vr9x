// LA PLACE front end (Slate): title screen (PLAY / EDIT / ABILITIES / SETTINGS), the world map where the player picks a
// spawn location, character selection, the 1-4 ability loadout, settings and the in-game pause menu. Everything is
// painted by hand (parchment, gold filigree, painted art, mana motes) and animated; layout is in 1920x1080 design units.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class APlayerController;

enum class EMTFrontPage : uint8
{
	None,
	Title,
	Map,
	Edit,
	Abilities,
	Settings,
	Pause,
};

DECLARE_DELEGATE_OneParam(FMTOnSpawnRequested, FName /*LocationId*/);

class MUSHOKURPG_API SMTFrontEnd : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMTFrontEnd) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PlayerController)
		SLATE_ARGUMENT(EMTFrontPage, InitialPage)
		SLATE_EVENT(FMTOnSpawnRequested, OnSpawn)
		SLATE_EVENT(FSimpleDelegate, OnResume)
		SLATE_EVENT(FSimpleDelegate, OnReturnToTitle)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void ShowPage(EMTFrontPage Page);
	EMTFrontPage GetPage() const { return Page; }
	/** Fades to black then fires OnSpawn (keeps painting the black until removed). */
	void BeginSpawnTransition(FName LocationId);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.f, 1080.f); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	/** A clickable region registered while painting. */
	struct FHit
	{
		FSlateRect Rect;
		FName Action;
		FName Param;
		int32 Index = INDEX_NONE;
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

	// Painting per page.
	int32 PaintTitle(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintMap(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintEdit(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintAbilities(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintSettings(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintPause(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintMotes(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintBackdrop(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const TCHAR* Art, float Dim) const;

	/** A gold-framed menu button; returns the layer after it. Registers the hit. */
	int32 Button(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FVector2D& Size, const FString& Label,
		FName Action, FName Param = NAME_None, bool bPrimary = false, bool bSelected = false, float FontSize = 26.f) const;
	/** Plain text menu entry (title screen): glows and slides on hover. */
	int32 TextEntry(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FString& Label, FName Action, float FontSize) const;
	void AddHit(const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, FName Action, FName Param = NAME_None, int32 Index = INDEX_NONE) const;
	float HoverAlpha(FName Action, FName Param, int32 Index = INDEX_NONE) const;
	static FName HoverKey(FName Action, FName Param, int32 Index);

	/** Design-space (1920x1080, centred) to local widget space. */
	FVector2D D(const FGeometry& G, float X, float Y) const;
	float DS(const FGeometry& G) const;

	void HandleAction(const FHit& Hit);
	void Back();
	void RefreshLocations();
	void ApplySettingsChange(FName Setting, float Value);

	TWeakObjectPtr<APlayerController> PlayerController;
	FMTOnSpawnRequested OnSpawn;
	FSimpleDelegate OnResume;
	FSimpleDelegate OnReturnToTitle;
	FSimpleDelegate OnQuit;

	EMTFrontPage Page = EMTFrontPage::Title;
	EMTFrontPage ReturnPage = EMTFrontPage::Title;
	double Now = 0.0;
	double PageSince = 0.0;
	mutable TArray<FHit> Hits;
	FVector2D Mouse = FVector2D(-1.f, -1.f);
	FName HoveredKey;
	TMap<FName, float> Hover;
	TArray<FMote> Motes;

	// Map page.
	TArray<FName> SpawnLocations;
	FName SelectedLocation;
	// Abilities page.
	int32 SelectedSlot = 0;
	FName InspectAbility;
	// Edit page.
	FName InspectCharacter;
	// Settings slider being dragged.
	FName DraggingSetting;

	// Spawn transition.
	bool bSpawning = false;
	double SpawnStart = 0.0;
	FName SpawnTarget;
	bool bSpawnFired = false;
};

// In-game HUD (Slate): the ability hotbar (LMB, 1-4, F, G) with icons, key labels, cooldown sweeps, mana costs, ready
// flashes and charge arcs, plus the vitals panel (health, mana, stamina, awakening) and the region-name banner. Drawn
// above everything else.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Abilities/MTAbilityComponent.h"

class APlayerController;
class AMTCharacterBase;

class MUSHOKURPG_API SMTHudOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMTHudOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PlayerController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.f, 1080.f); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Slots in hotbar order. */
	static const EMTAbilitySlot HotbarSlots[7];

private:
	AMTCharacterBase* GetCharacter() const;
	int32 PaintHotbar(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, AMTCharacterBase* Char) const;
	int32 PaintVitals(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, AMTCharacterBase* Char) const;
	/** "FITTOA REGION / Central Continent" banner when the player enters a new region (UMTRegionSubsystem). */
	int32 PaintRegionBanner(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;

	TWeakObjectPtr<APlayerController> PlayerController;
	double Now = 0.0;
	float LastCooldown[7] = {};
	double ReadyFlashAt[7] = { -100.0, -100.0, -100.0, -100.0, -100.0, -100.0, -100.0 };
	float HealthTrail = 1.f;
	float ManaTrail = 1.f;
	int32 AnnouncedRegion = INDEX_NONE;
	double BannerStart = -100.0;
	FString BannerTitle;
	FString BannerSubtitle;
};

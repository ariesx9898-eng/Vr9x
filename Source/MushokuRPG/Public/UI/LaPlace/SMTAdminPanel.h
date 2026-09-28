// The ADMIN popup (UMTAdminSubsystem): first a code prompt, then - once unlocked - the admin panel with player actions,
// cheat toggles, character switch, time of day and a teleport list. Painted by hand in the LA PLACE parchment style.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class UMTAdminSubsystem;

class MUSHOKURPG_API SMTAdminPanel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMTAdminPanel) {}
		SLATE_ARGUMENT(UMTAdminSubsystem*, Admin)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.f, 1080.f); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	struct FHit
	{
		FSlateRect Rect;
		FName Action;
		FName Param;
	};

	int32 PaintLock(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	int32 PaintPanel(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const;
	void Button(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, float X, float Y, float W, float H, const FString& Label,
		FName Action, FName Param = NAME_None, bool bPrimary = false, bool bActive = false, float FontSize = 17.f) const;
	void Heading(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, float X, float Y, float W, const FString& Label) const;
	FVector2D D(const FGeometry& G, float X, float Y) const;
	float DS(const FGeometry& G) const;
	float HoverAlpha(FName Action, FName Param) const;
	void Submit();
	void Run(FName Action, FName Param);

	UMTAdminSubsystem* Admin = nullptr;
	mutable TArray<FHit> Hits;
	FVector2D Mouse = FVector2D(-1.f, -1.f);
	FName Hovered;
	TMap<FName, float> Hover;
	double Now = 0.0;
	double OpenedAt = 0.0;
	FString Code;
	double WrongAt = -100.0;
	double UnlockedAt = -100.0;
	FString Toast;
	double ToastAt = -100.0;
	float Scroll = 0.f;
	mutable FSlateRect ListRect;
	mutable int32 ListRows = 0;
};

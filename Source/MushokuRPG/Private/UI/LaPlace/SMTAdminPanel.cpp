#include "UI/LaPlace/SMTAdminPanel.h"

#include "Core/MTDataRegistry.h"
#include "GameFramework/PlayerController.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Rendering/DrawElements.h"
#include "UI/LaPlace/MTAdminSubsystem.h"
#include "UI/LaPlace/MTUIStyle.h"

namespace MTAdminUI
{
	const FName ActUnlock(TEXT("Unlock"));
	const FName ActClose(TEXT("Close"));
	const FName ActTeleport(TEXT("Teleport"));
	constexpr float RowHeight = 46.f;
	constexpr int32 VisibleRows = 11;
}

void SMTAdminPanel::Construct(const FArguments& InArgs)
{
	Admin = InArgs._Admin;
	SetCanTick(true);
}

float SMTAdminPanel::DS(const FGeometry& G) const
{
	const FVector2D Size = G.GetLocalSize();
	return FMath::Min(Size.X / 1920.f, Size.Y / 1080.f);
}

FVector2D SMTAdminPanel::D(const FGeometry& G, float X, float Y) const
{
	const FVector2D Size = G.GetLocalSize();
	const float S = DS(G);
	return FVector2D((Size.X - 1920.f * S) * 0.5f + X * S, (Size.Y - 1080.f * S) * 0.5f + Y * S);
}

float SMTAdminPanel::HoverAlpha(FName Action, FName Param) const
{
	const float* A = Hover.Find(FName(*(Action.ToString() + TEXT("|") + Param.ToString())));
	return A ? *A : 0.f;
}

void SMTAdminPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = InCurrentTime;
	if (OpenedAt <= 0.0)
	{
		OpenedAt = InCurrentTime;
	}
	FName Under;
	for (int32 i = Hits.Num() - 1; i >= 0; --i)
	{
		if (Hits[i].Rect.ContainsPoint(Mouse))
		{
			Under = FName(*(Hits[i].Action.ToString() + TEXT("|") + Hits[i].Param.ToString()));
			break;
		}
	}
	if (Under != Hovered && !Under.IsNone())
	{
		MTUI::Sound(TEXT("ui_hover"), 0.2f);
	}
	Hovered = Under;
	if (!Under.IsNone())
	{
		Hover.FindOrAdd(Under);
	}
	for (TPair<FName, float>& Pair : Hover)
	{
		Pair.Value = FMath::FInterpTo(Pair.Value, Pair.Key == Under ? 1.f : 0.f, InDeltaTime, 14.f);
	}
}

void SMTAdminPanel::Button(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, float X, float Y, float W, float H, const FString& Label,
	FName Action, FName Param, bool bPrimary, bool bActive, float FontSize) const
{
	const float S = DS(G);
	const float Hov = HoverAlpha(Action, Param);
	const FVector2D P = D(G, X, Y - 2.f * Hov);
	const FVector2D Size = FVector2D(W, H) * S;
	FLinearColor Fill = bActive ? FLinearColor(0.32f, 0.2f, 0.05f, 0.96f) : (bPrimary ? FLinearColor(0.18f, 0.09f, 0.03f, 0.96f) : FLinearColor(0.06f, 0.045f, 0.03f, 0.93f));
	Fill = FMath::Lerp(Fill, FLinearColor(0.26f, 0.16f, 0.05f, 0.97f), Hov * 0.6f);
	if (Hov > 0.02f || bActive)
	{
		MTUI::Glow(Out, Layer, G, P + Size * 0.5f, Size.X * 0.6f, MTUI::Gold.CopyWithNewOpacity(0.18f * FMath::Max(Hov, bActive ? 0.8f : 0.f)));
	}
	MTUI::Box(Out, Layer + 1, G, P, Size, Fill);
	MTUI::GoldFrame(Out, Layer + 2, G, P, Size, 0.55f + 0.45f * FMath::Max(Hov, bActive ? 1.f : 0.f), false);
	MTUI::TextAligned(Out, Layer + 3, G, Label, MTUI::Heading(FontSize * S), P, Size, FVector2D(0.5f, 0.5f),
		bActive || bPrimary ? MTUI::GoldBright : FMath::Lerp(MTUI::TextLight, MTUI::GoldBright, Hov));
	FHit Hit;
	Hit.Rect = FSlateRect(P.X, P.Y, P.X + Size.X, P.Y + Size.Y);
	Hit.Action = Action;
	Hit.Param = Param;
	Hits.Add(Hit);
}

void SMTAdminPanel::Heading(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, float X, float Y, float W, const FString& Label) const
{
	const float S = DS(G);
	MTUI::Text(Out, Layer, G, Label, MTUI::Heading(19.f * S), D(G, X, Y), MTUI::Ink, false);
	MTUI::Line(Out, Layer, G, D(G, X, Y + 30.f), D(G, X + W, Y + 30.f), MTUI::GoldDim.CopyWithNewOpacity(0.8f), 1.5f * S);
}

int32 SMTAdminPanel::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	Hits.Reset();
	const float Open = FMath::Clamp(float(Now - OpenedAt) / 0.25f, 0.f, 1.f);
	MTUI::Box(Out, LayerId, G, FVector2D::ZeroVector, G.GetLocalSize(), FLinearColor(0.f, 0.f, 0.f, 0.62f * Open));
	return Admin && Admin->IsUnlocked() ? PaintPanel(G, Out, LayerId + 1) : PaintLock(G, Out, LayerId + 1);
}

int32 SMTAdminPanel::PaintLock(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const float SinceWrong = float(Now - WrongAt);
	const float Shake = SinceWrong < 0.45f ? FMath::Sin(SinceWrong * 60.f) * 12.f * (1.f - SinceWrong / 0.45f) : 0.f;
	const float X0 = 620.f + Shake;
	MTUI::ParchmentPanel(Out, Layer, G, D(G, X0, 330.f), FVector2D(680.f, 420.f) * S, 1.f);
	MTUI::GoldFrame(Out, Layer + 4, G, D(G, X0, 330.f), FVector2D(680.f, 420.f) * S, 1.f, true, 48.f * S);
	MTUI::TextAligned(Out, Layer + 5, G, TEXT("ADMIN"), MTUI::Title(52.f * S), D(G, X0, 352.f), FVector2D(680.f, 64.f) * S, FVector2D(0.5f, 0.5f), MTUI::Ink, false);
	MTUI::Divider(Out, Layer + 5, G, D(G, X0 + 340.f, 428.f), 360.f * S);
	MTUI::TextAligned(Out, Layer + 5, G, TEXT("Enter the admin code"), MTUI::BodyItalic(23.f * S), D(G, X0, 442.f), FVector2D(680.f, 34.f) * S,
		FVector2D(0.5f, 0.5f), MTUI::InkSoft, false);

	// Code field: one glowing mana gem per letter, and a blinking caret.
	const FVector2D FieldPos = D(G, X0 + 100.f, 494.f);
	const FVector2D FieldSize = FVector2D(480.f, 62.f) * S;
	MTUI::Box(Out, Layer + 5, G, FieldPos, FieldSize, FLinearColor(0.05f, 0.035f, 0.02f, 0.96f));
	MTUI::GoldFrame(Out, Layer + 6, G, FieldPos, FieldSize, 0.9f, false);
	const float Step = 30.f * S;
	const float StartX = FieldPos.X + FieldSize.X * 0.5f - (Code.Len() - 1) * Step * 0.5f;
	for (int32 i = 0; i < Code.Len(); ++i)
	{
		const FVector2D C(StartX + i * Step, FieldPos.Y + FieldSize.Y * 0.5f);
		MTUI::Glow(Out, Layer + 7, G, C, 14.f * S, MTUI::Mana.CopyWithNewOpacity(0.45f));
		MTUI::Glow(Out, Layer + 8, G, C, 5.f * S, FLinearColor(0.85f, 0.95f, 1.f, 1.f));
	}
	if (FMath::Fmod(Now, 1.0) < 0.55)
	{
		const float CaretX = Code.IsEmpty() ? FieldPos.X + FieldSize.X * 0.5f : StartX + (Code.Len() - 0.5f) * Step;
		MTUI::Line(Out, Layer + 8, G, FVector2D(CaretX, FieldPos.Y + 14.f * S), FVector2D(CaretX, FieldPos.Y + FieldSize.Y - 14.f * S), MTUI::GoldBright, 2.f * S);
	}
	if (SinceWrong < 2.2f)
	{
		MTUI::TextAligned(Out, Layer + 6, G, TEXT("Incorrect code"), MTUI::Heading(17.f * S), D(G, X0, 566.f), FVector2D(680.f, 30.f) * S,
			FVector2D(0.5f, 0.5f), MTUI::Danger.CopyWithNewOpacity(FMath::Clamp(2.2f - SinceWrong, 0.f, 1.f)), false);
	}
	Button(G, Out, Layer + 6, X0 + 100.f, 620.f, 225.f, 58.f, TEXT("UNLOCK"), MTAdminUI::ActUnlock, NAME_None, true, false, 21.f);
	Button(G, Out, Layer + 6, X0 + 355.f, 620.f, 225.f, 58.f, TEXT("CLOSE"), MTAdminUI::ActClose, NAME_None, false, false, 21.f);
	MTUI::TextAligned(Out, Layer + 5, G, TEXT("Enter to unlock   -   Esc to close"), MTUI::BodyItalic(17.f * S), D(G, X0, 700.f), FVector2D(680.f, 26.f) * S,
		FVector2D(0.5f, 0.5f), MTUI::InkSoft, false);
	return Layer + 12;
}

int32 SMTAdminPanel::PaintPanel(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	const float Reveal = FMath::Clamp(float(Now - UnlockedAt) / 0.35f, 0.f, 1.f);
	const float Y0 = 110.f + (1.f - Reveal) * 18.f;
	MTUI::ParchmentPanel(Out, Layer, G, D(G, 210.f, Y0), FVector2D(1500.f, 860.f) * S, 1.f);
	MTUI::GoldFrame(Out, Layer + 4, G, D(G, 210.f, Y0), FVector2D(1500.f, 860.f) * S, 1.f, true, 56.f * S);
	const int32 L = Layer + 5;
	const float dY = Y0 - 110.f;
	MTUI::TextAligned(Out, L, G, TEXT("ADMIN"), MTUI::Title(54.f * S), D(G, 210.f, 124.f + dY), FVector2D(1500.f, 66.f) * S, FVector2D(0.5f, 0.5f), MTUI::Ink, false);
	MTUI::Divider(Out, L, G, D(G, 960.f, 198.f + dY), 440.f * S);
	MTUI::TextAligned(Out, L, G,
		TEXT("Everything is unlocked: every character, element, race and ability, level 100, max mastery and ranks, spins, gold, items and every location."),
		MTUI::BodyItalic(19.f * S), D(G, 210.f, 208.f + dY), FVector2D(1500.f, 30.f) * S, FVector2D(0.5f, 0.5f), MTUI::InkSoft, false);

	// Player actions.
	Heading(G, Out, L, 260.f, 252.f + dY, 440.f, TEXT("PLAYER"));
	struct FAct { const TCHAR* Label; const TCHAR* Action; };
	const FAct Acts[] = {
		{ TEXT("FULL HEAL"), TEXT("Heal") }, { TEXT("FILL AWAKENING"), TEXT("Awaken") },
		{ TEXT("RESET COOLDOWNS"), TEXT("ResetCooldowns") }, { TEXT("+10 LEVELS"), TEXT("Levels") },
		{ TEXT("+99 SPINS"), TEXT("Spins") }, { TEXT("+10,000 GOLD"), TEXT("Gold") },
		{ TEXT("ALL ITEMS x99"), TEXT("Items") }, { TEXT("SMITE FOES"), TEXT("Smite") },
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Acts); ++i)
	{
		Button(G, Out, L, 260.f + (i % 2) * 230.f, 296.f + dY + (i / 2) * 64.f, 210.f, 52.f, Acts[i].Label, FName(Acts[i].Action));
	}
	Heading(G, Out, L, 260.f, 562.f + dY, 440.f, TEXT("CHEATS"));
	struct FToggle { const TCHAR* Label; const TCHAR* Key; };
	const FToggle Toggles[] = { { TEXT("GOD MODE"), TEXT("God") }, { TEXT("INFINITE MANA & STAMINA"), TEXT("Mana") }, { TEXT("NO COOLDOWNS"), TEXT("Cooldowns") } };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Toggles); ++i)
	{
		const bool bOn = Admin && Admin->IsToggleOn(Toggles[i].Key);
		Button(G, Out, L, 260.f, 604.f + dY + i * 64.f, 440.f, 52.f, FString::Printf(TEXT("%s:  %s"), Toggles[i].Label, bOn ? TEXT("ON") : TEXT("OFF")),
			TEXT("Toggle"), Toggles[i].Key, false, bOn);
	}

	APlayerController* PC = Admin ? Admin->GetController() : nullptr;
	const UMTDataRegistry* Registry = PC ? UMTDataRegistry::Get(PC) : nullptr;
	const UMTProgressionSubsystem* Progression = PC ? UMTProgressionSubsystem::Get(PC) : nullptr;

	// Character and time of day.
	Heading(G, Out, L, 750.f, 252.f + dY, 380.f, TEXT("CHARACTER"));
	int32 Row = 0;
	if (Registry)
	{
		for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
		{
			const bool bEquipped = Progression && Progression->GetEquippedCharacter() == Pair.Key;
			Button(G, Out, L, 750.f, 296.f + dY + Row * 64.f, 380.f, 54.f, Pair.Value.DisplayName.ToString().ToUpper(), TEXT("Character"), Pair.Key, false, bEquipped, 18.f);
			++Row;
		}
	}
	const float TimeY = 296.f + dY + FMath::Max(Row, 2) * 64.f + 24.f;
	Heading(G, Out, L, 750.f, TimeY, 380.f, TEXT("TIME OF DAY"));
	const struct { const TCHAR* Label; const TCHAR* Hours; } Times[] = { { TEXT("DAWN"), TEXT("6") }, { TEXT("NOON"), TEXT("12") }, { TEXT("DUSK"), TEXT("18.5") }, { TEXT("NIGHT"), TEXT("23") } };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Times); ++i)
	{
		Button(G, Out, L, 750.f + (i % 2) * 200.f, TimeY + 44.f + (i / 2) * 62.f, 180.f, 50.f, Times[i].Label, TEXT("Time"), Times[i].Hours);
	}
	if (Progression)
	{
		const FString Stats = FString::Printf(TEXT("Level %d   -   Gold %d\nSpins  %d / %d / %d   (character / element / race)"), Progression->GetLevel(), Progression->GetGold(),
			Progression->GetSpins(EMTRollCategory::Character), Progression->GetSpins(EMTRollCategory::Element), Progression->GetSpins(EMTRollCategory::Race));
		MTUI::Paragraph(Out, L, G, Stats, MTUI::Body(19.f * S), D(G, 750.f, TimeY + 180.f), 380.f * S, MTUI::Ink);
	}

	// Teleport list (spawn points first), scrolled by the mouse wheel.
	Heading(G, Out, L, 1180.f, 252.f + dY, 480.f, TEXT("TELEPORT"));
	TArray<const FMTLocationData*> Locations;
	if (Registry)
	{
		for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
		{
			Locations.Add(&Pair.Value);
		}
		Locations.Sort([](const FMTLocationData& A, const FMTLocationData& B)
		{
			return A.bSpawnPoint != B.bSpawnPoint ? A.bSpawnPoint : A.DisplayName.ToString() < B.DisplayName.ToString();
		});
	}
	ListRows = Locations.Num();
	const FVector2D ListPos = D(G, 1180.f, 296.f + dY);
	const FVector2D ListSize = FVector2D(480.f, MTAdminUI::RowHeight * MTAdminUI::VisibleRows) * S;
	ListRect = FSlateRect(ListPos.X, ListPos.Y, ListPos.X + ListSize.X, ListPos.Y + ListSize.Y);
	MTUI::Box(Out, L, G, ListPos, ListSize, FLinearColor(0.12f, 0.08f, 0.04f, 0.10f));
	const int32 First = FMath::Clamp(FMath::RoundToInt(Scroll), 0, FMath::Max(0, ListRows - MTAdminUI::VisibleRows));
	for (int32 i = 0; i < MTAdminUI::VisibleRows && First + i < Locations.Num(); ++i)
	{
		const FMTLocationData* Loc = Locations[First + i];
		const FVector2D RowPos = ListPos + FVector2D(0.f, i * MTAdminUI::RowHeight * S);
		const FVector2D RowSize(ListSize.X, (MTAdminUI::RowHeight - 4.f) * S);
		const float Hov = HoverAlpha(MTAdminUI::ActTeleport, Loc->LocationID);
		MTUI::Box(Out, L + 1, G, RowPos, RowSize, FLinearColor(0.06f, 0.045f, 0.03f, 0.08f + 0.8f * Hov));
		if (Loc->bSpawnPoint)
		{
			MTUI::Glow(Out, L + 2, G, RowPos + FVector2D(16.f * S, RowSize.Y * 0.5f), 7.f * S, MTUI::GoldBright);
		}
		MTUI::TextAligned(Out, L + 3, G, Loc->DisplayName.ToString(), MTUI::Heading(17.f * S), RowPos + FVector2D(32.f * S, 0.f), FVector2D(RowSize.X * 0.6f, RowSize.Y),
			FVector2D(0.f, 0.5f), FMath::Lerp(MTUI::Ink, MTUI::GoldBright, Hov), false);
		FString Where = Loc->Continent.ToString();
		if (!Loc->Region.IsNone())
		{
			Where += (Where.IsEmpty() ? TEXT("") : TEXT(" - ")) + Loc->Region.ToString();
		}
		MTUI::TextAligned(Out, L + 3, G, Where, MTUI::BodyItalic(15.f * S), RowPos, RowSize - FVector2D(12.f * S, 0.f), FVector2D(1.f, 0.5f),
			FMath::Lerp(MTUI::InkSoft, MTUI::TextLight, Hov), false);
		FHit Hit;
		Hit.Rect = FSlateRect(RowPos.X, RowPos.Y, RowPos.X + RowSize.X, RowPos.Y + RowSize.Y);
		Hit.Action = MTAdminUI::ActTeleport;
		Hit.Param = Loc->LocationID;
		Hits.Add(Hit);
	}
	if (ListRows > MTAdminUI::VisibleRows)
	{
		const float Frac = float(First) / float(ListRows - MTAdminUI::VisibleRows);
		const float BarH = ListSize.Y * MTAdminUI::VisibleRows / ListRows;
		MTUI::Box(Out, L + 2, G, ListPos + FVector2D(ListSize.X + 6.f * S, (ListSize.Y - BarH) * Frac), FVector2D(4.f * S, BarH), MTUI::Gold.CopyWithNewOpacity(0.8f));
	}

	// Feedback and footer.
	const float SinceToast = float(Now - ToastAt);
	if (SinceToast < 3.5f && !Toast.IsEmpty())
	{
		MTUI::TextAligned(Out, L + 4, G, Toast, MTUI::Heading(19.f * S), D(G, 210.f, 870.f + dY), FVector2D(1500.f, 32.f) * S, FVector2D(0.5f, 0.5f),
			MTUI::Ink.CopyWithNewOpacity(FMath::Clamp(3.5f - SinceToast, 0.f, 1.f)), false);
	}
	MTUI::Text(Out, L, G, TEXT("Esc to close   -   press 1 and 0 together to open this panel"), MTUI::BodyItalic(16.f * S), D(G, 262.f, 918.f + dY), MTUI::InkSoft, false);
	Button(G, Out, L, 1500.f, 906.f + dY, 160.f, 48.f, TEXT("CLOSE"), MTAdminUI::ActClose, NAME_None, false, false, 18.f);
	return L + 10;
}

void SMTAdminPanel::Submit()
{
	if (!Admin)
	{
		return;
	}
	if (Admin->TryUnlock(Code))
	{
		UnlockedAt = Now;
		Toast = TEXT("Admin unlocked - everything granted");
		ToastAt = Now;
		MTUI::Sound(TEXT("ui_spawn"), 0.7f);
	}
	else
	{
		WrongAt = Now;
		Code.Reset();
		MTUI::Sound(TEXT("ui_error"), 0.6f);
	}
}

void SMTAdminPanel::Run(FName Action, FName Param)
{
	if (!Admin)
	{
		return;
	}
	if (Action == MTAdminUI::ActClose)
	{
		Admin->Close();
		return;
	}
	if (Action == MTAdminUI::ActUnlock)
	{
		Submit();
		return;
	}
	MTUI::Sound(TEXT("ui_click"), 0.5f);
	const FString Result = Admin->DoAction(Action, Param);
	if (!Result.IsEmpty())
	{
		Toast = Result;
		ToastAt = Now;
	}
	if (Action == MTAdminUI::ActTeleport)
	{
		Admin->Close(); // removes this widget: nothing may touch members after this
	}
}

FReply SMTAdminPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Mouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	return FReply::Handled();
}

FReply SMTAdminPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Mouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	for (int32 i = Hits.Num() - 1; i >= 0; --i)
	{
		if (Hits[i].Rect.ContainsPoint(Mouse))
		{
			const FHit Hit = Hits[i];
			Run(Hit.Action, Hit.Param);
			return FReply::Handled();
		}
	}
	return FReply::Handled();
}

FReply SMTAdminPanel::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (ListRows > MTAdminUI::VisibleRows)
	{
		Scroll = FMath::Clamp(Scroll - MouseEvent.GetWheelDelta() * 2.f, 0.f, float(ListRows - MTAdminUI::VisibleRows));
	}
	return FReply::Handled();
}

FReply SMTAdminPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		if (Admin)
		{
			Admin->Close();
		}
		return FReply::Handled();
	}
	const bool bLocked = Admin && !Admin->IsUnlocked();
	if (bLocked && (Key == EKeys::Enter || Key == EKeys::Virtual_Accept))
	{
		Submit();
		return FReply::Handled();
	}
	if (bLocked && Key == EKeys::BackSpace)
	{
		Code.LeftChopInline(1);
		return FReply::Handled();
	}
	return FReply::Handled();
}

FReply SMTAdminPanel::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	const TCHAR Char = InCharacterEvent.GetCharacter();
	// The code is letters only: digits (the 1 + 0 that opened the panel) and control keys are ignored.
	if (Admin && !Admin->IsUnlocked() && FChar::IsAlpha(Char) && Code.Len() < 24)
	{
		Code.AppendChar(Char);
		MTUI::Sound(TEXT("ui_tab"), 0.25f);
	}
	return FReply::Handled();
}

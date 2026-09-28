#include "UI/LaPlace/SMTFrontEnd.h"
#include "UI/LaPlace/MTFrontEndSubsystem.h"
#include "UI/LaPlace/MTUIStyle.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTDataTypes.h"
#include "Core/MTTypes.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Save/MTSaveSubsystem.h"
#include "Save/MTSaveTypes.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"

namespace
{
	const FName ActPage(TEXT("Page"));
	const FName ActQuit(TEXT("Quit"));
	const FName ActBack(TEXT("Back"));
	const FName ActPin(TEXT("Pin"));
	const FName ActSpawn(TEXT("Spawn"));
	const FName ActPickCharacter(TEXT("PickCharacter"));
	const FName ActSlot(TEXT("Slot"));
	const FName ActPool(TEXT("Pool"));
	const FName ActSlider(TEXT("Slider"));
	const FName ActToggle(TEXT("Toggle"));
	const FName ActQuality(TEXT("Quality"));
	const FName ActResume(TEXT("Resume"));
	const FName ActTitle(TEXT("ToTitle"));

	FString Spaced(const FString& In)
	{
		FString Out;
		for (int32 i = 0; i < In.Len(); ++i)
		{
			Out.AppendChar(In[i]);
			if (i + 1 < In.Len())
			{
				Out.AppendChar(TEXT(' '));
			}
		}
		return Out;
	}

	const FMTAbilityData* FindAbility(FName Id)
	{
		const UMTDataRegistry* Registry = GEngine && GEngine->GameViewport ? UMTDataRegistry::Get(GEngine->GameViewport->GetWorld()) : nullptr;
		return Registry ? Registry->FindAbility(Id) : nullptr;
	}

	FString BehaviorName(EMTAbilityBehavior B)
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
		return TEXT("");
	}
}

// ---------------------------------------------------------------------------------------------
// Setup and layout
// ---------------------------------------------------------------------------------------------

void SMTFrontEnd::Construct(const FArguments& InArgs)
{
	PlayerController = InArgs._PlayerController;
	OnSpawn = InArgs._OnSpawn;
	OnResume = InArgs._OnResume;
	OnReturnToTitle = InArgs._OnReturnToTitle;
	OnQuit = InArgs._OnQuit;
	RefreshLocations();
	ShowPage(InArgs._InitialPage == EMTFrontPage::None ? EMTFrontPage::Title : InArgs._InitialPage);
	for (int32 i = 0; i < 40; ++i)
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

void SMTFrontEnd::RefreshLocations()
{
	SpawnLocations.Reset();
	const UMTDataRegistry* Registry = GEngine && GEngine->GameViewport ? UMTDataRegistry::Get(GEngine->GameViewport->GetWorld()) : nullptr;
	if (!Registry)
	{
		return;
	}
	for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
	{
		if (Pair.Value.bSpawnPoint)
		{
			SpawnLocations.Add(Pair.Key);
		}
	}
	if (SpawnLocations.Num() == 0)
	{
		// Older data without spawn flags: offer the fast-travel points.
		for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
		{
			if (Pair.Value.bFastTravel)
			{
				SpawnLocations.Add(Pair.Key);
			}
		}
	}
	if (!SpawnLocations.Contains(SelectedLocation))
	{
		SelectedLocation = SpawnLocations.Num() > 0 ? SpawnLocations[0] : NAME_None;
	}
}

void SMTFrontEnd::ShowPage(EMTFrontPage NewPage)
{
	if (NewPage == Page && PageSince > 0.0)
	{
		return;
	}
	if (NewPage == EMTFrontPage::Abilities || NewPage == EMTFrontPage::Settings)
	{
		ReturnPage = (Page == EMTFrontPage::Pause) ? EMTFrontPage::Pause : EMTFrontPage::Title;
	}
	else if (NewPage == EMTFrontPage::Map || NewPage == EMTFrontPage::Edit)
	{
		ReturnPage = EMTFrontPage::Title;
	}
	Page = NewPage;
	PageSince = Now;
	Hover.Reset();
	if (NewPage == EMTFrontPage::Map)
	{
		RefreshLocations();
	}
	if (NewPage == EMTFrontPage::Edit)
	{
		if (const UMTProgressionSubsystem* Progression = PlayerController.IsValid() ? UMTProgressionSubsystem::Get(PlayerController.Get()) : nullptr)
		{
			InspectCharacter = Progression->GetEquippedCharacter();
		}
	}
	SetVisibility(NewPage == EMTFrontPage::None ? EVisibility::HitTestInvisible : EVisibility::Visible);
}

void SMTFrontEnd::BeginSpawnTransition(FName LocationId)
{
	bSpawning = true;
	bSpawnFired = false;
	SpawnStart = Now;
	SpawnTarget = LocationId;
	MTUI::Sound(TEXT("ui_spawn"), 0.9f);
}

float SMTFrontEnd::DS(const FGeometry& G) const
{
	const FVector2D Size = G.GetLocalSize();
	return FMath::Min(Size.X / 1920.f, Size.Y / 1080.f);
}

FVector2D SMTFrontEnd::D(const FGeometry& G, float X, float Y) const
{
	const FVector2D Size = G.GetLocalSize();
	const float S = DS(G);
	return FVector2D((Size.X - 1920.f * S) * 0.5f + X * S, (Size.Y - 1080.f * S) * 0.5f + Y * S);
}

FName SMTFrontEnd::HoverKey(FName Action, FName Param, int32 Index)
{
	return FName(*FString::Printf(TEXT("%s|%s|%d"), *Action.ToString(), *Param.ToString(), Index));
}

void SMTFrontEnd::AddHit(const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, FName Action, FName Param, int32 Index) const
{
	FHit Hit;
	Hit.Rect = FSlateRect(Pos.X, Pos.Y, Pos.X + Size.X, Pos.Y + Size.Y);
	Hit.Action = Action;
	Hit.Param = Param;
	Hit.Index = Index;
	Hits.Add(Hit);
}

float SMTFrontEnd::HoverAlpha(FName Action, FName Param, int32 Index) const
{
	const float* A = Hover.Find(HoverKey(Action, Param, Index));
	return A ? *A : 0.f;
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

void SMTFrontEnd::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (PageSince <= 0.0)
	{
		PageSince = InCurrentTime;
	}
	Now = InCurrentTime;
	// The map page's location card shows a live view of the selected place.
	if (UMTFrontEndSubsystem* FrontEnd = UMTFrontEndSubsystem::Get(PlayerController.Get()))
	{
		FrontEnd->SetPreviewLocation(Page == EMTFrontPage::Map && !bSpawning ? SelectedLocation : NAME_None);
	}
	// Hover easing toward the hit under the mouse.
	FName Under;
	for (int32 i = Hits.Num() - 1; i >= 0; --i)
	{
		if (Hits[i].Rect.ContainsPoint(Mouse))
		{
			Under = HoverKey(Hits[i].Action, Hits[i].Param, Hits[i].Index);
			break;
		}
	}
	if (Under != HoveredKey && !Under.IsNone())
	{
		MTUI::Sound(TEXT("ui_hover"), 0.25f);
	}
	HoveredKey = Under;
	if (!Under.IsNone())
	{
		Hover.FindOrAdd(Under);
	}
	for (TPair<FName, float>& Pair : Hover)
	{
		const float Target = Pair.Key == Under ? 1.f : 0.f;
		Pair.Value = FMath::FInterpTo(Pair.Value, Target, InDeltaTime, 12.f);
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
	if (bSpawning && !bSpawnFired && Now - SpawnStart > 0.75)
	{
		bSpawnFired = true;
		Page = EMTFrontPage::None;
		SetVisibility(EVisibility::HitTestInvisible);
		OnSpawn.ExecuteIfBound(SpawnTarget);
	}
	if (bSpawning && bSpawnFired && Now - SpawnStart > 2.0)
	{
		bSpawning = false;
		OnResume.ExecuteIfBound(); // the owner removes this widget
	}
}

FReply SMTFrontEnd::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Mouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (!DraggingSetting.IsNone() && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		for (const FHit& Hit : Hits)
		{
			if (Hit.Action == ActSlider && Hit.Param == DraggingSetting)
			{
				const float Alpha = FMath::Clamp((Mouse.X - Hit.Rect.Left) / FMath::Max(1.f, Hit.Rect.Right - Hit.Rect.Left), 0.f, 1.f);
				ApplySettingsChange(Hit.Param, Alpha);
			}
		}
	}
	else
	{
		DraggingSetting = NAME_None;
	}
	return FReply::Handled();
}

FReply SMTFrontEnd::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bSpawning || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}
	Mouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	for (int32 i = Hits.Num() - 1; i >= 0; --i)
	{
		if (Hits[i].Rect.ContainsPoint(Mouse))
		{
			const FHit Hit = Hits[i];
			if (Hit.Action == ActSlider)
			{
				DraggingSetting = Hit.Param;
				const float Alpha = FMath::Clamp((Mouse.X - Hit.Rect.Left) / FMath::Max(1.f, Hit.Rect.Right - Hit.Rect.Left), 0.f, 1.f);
				ApplySettingsChange(Hit.Param, Alpha);
				return FReply::Handled();
			}
			MTUI::Sound(TEXT("ui_click"), 0.55f);
			HandleAction(Hit);
			return FReply::Handled();
		}
	}
	return FReply::Handled();
}

FReply SMTFrontEnd::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SMTFrontEnd::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_FaceButton_Right)
	{
		Back();
		return FReply::Handled();
	}
	if ((Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom) && Page == EMTFrontPage::Map && !SelectedLocation.IsNone())
	{
		BeginSpawnTransition(SelectedLocation);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SMTFrontEnd::Back()
{
	MTUI::Sound(TEXT("ui_close"), 0.5f);
	switch (Page)
	{
	case EMTFrontPage::Pause:
		OnResume.ExecuteIfBound();
		break;
	case EMTFrontPage::Title:
		break;
	default:
		ShowPage(ReturnPage);
		break;
	}
}

void SMTFrontEnd::HandleAction(const FHit& Hit)
{
	UMTProgressionSubsystem* Progression = PlayerController.IsValid() ? UMTProgressionSubsystem::Get(PlayerController.Get()) : nullptr;
	if (Hit.Action == ActPage)
	{
		MTUI::Sound(TEXT("ui_open"), 0.5f);
		ShowPage((EMTFrontPage)Hit.Index);
	}
	else if (Hit.Action == ActBack)
	{
		Back();
	}
	else if (Hit.Action == ActQuit)
	{
		OnQuit.ExecuteIfBound();
	}
	else if (Hit.Action == ActResume)
	{
		OnResume.ExecuteIfBound();
	}
	else if (Hit.Action == ActTitle)
	{
		OnReturnToTitle.ExecuteIfBound();
	}
	else if (Hit.Action == ActPin)
	{
		SelectedLocation = Hit.Param;
		MTUI::Sound(TEXT("map_select"), 0.6f);
	}
	else if (Hit.Action == ActSpawn && !SelectedLocation.IsNone())
	{
		BeginSpawnTransition(SelectedLocation);
	}
	else if (Hit.Action == ActPickCharacter && Progression)
	{
		InspectCharacter = Hit.Param;
		if (Progression->EquipCharacter(Hit.Param))
		{
			MTUI::Sound(TEXT("ui_confirm"), 0.6f);
		}
	}
	else if (Hit.Action == ActSlot)
	{
		SelectedSlot = FMath::Clamp(Hit.Index, 0, 3);
		if (Progression)
		{
			const TArray<FName> Loadout = Progression->GetLoadout(Progression->GetEquippedCharacter());
			InspectAbility = Loadout.IsValidIndex(SelectedSlot) ? Loadout[SelectedSlot] : NAME_None;
		}
	}
	else if (Hit.Action == ActPool && Progression)
	{
		InspectAbility = Hit.Param;
		Progression->SetLoadoutSlot(Progression->GetEquippedCharacter(), SelectedSlot, Hit.Param);
		MTUI::Sound(TEXT("ui_confirm"), 0.5f);
		SelectedSlot = (SelectedSlot + 1) % 4;
	}
	else if (Hit.Action == ActToggle && Progression)
	{
		FMTSettingsSave Settings = Progression->GetSettings();
		if (Hit.Param == TEXT("InvertY")) { Settings.bInvertY = !Settings.bInvertY; }
		if (Hit.Param == TEXT("ToggleSprint")) { Settings.bToggleSprint = !Settings.bToggleSprint; }
		Progression->SetSettings(Settings);
		if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(PlayerController.Get()))
		{
			Save->ApplySettings(false);
		}
	}
	else if (Hit.Action == ActQuality && Progression)
	{
		FMTSettingsSave Settings = Progression->GetSettings();
		Settings.GraphicsQuality = FMath::Clamp(Hit.Index, 0, 3);
		Progression->SetSettings(Settings);
		if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(PlayerController.Get()))
		{
			Save->ApplySettings(true);
		}
	}
}

void SMTFrontEnd::ApplySettingsChange(FName Setting, float Alpha)
{
	UMTProgressionSubsystem* Progression = PlayerController.IsValid() ? UMTProgressionSubsystem::Get(PlayerController.Get()) : nullptr;
	if (!Progression)
	{
		return;
	}
	FMTSettingsSave Settings = Progression->GetSettings();
	if (Setting == TEXT("Volume")) { Settings.MasterVolume = Alpha; }
	else if (Setting == TEXT("Mouse")) { Settings.MouseSensitivity = FMath::Lerp(0.2f, 3.f, Alpha); }
	else if (Setting == TEXT("FOV")) { Settings.FieldOfView = FMath::RoundToFloat(FMath::Lerp(70.f, 110.f, Alpha)); }
	else if (Setting == TEXT("Shake")) { Settings.CameraShakeScale = Alpha; }
	Progression->SetSettings(Settings);
	if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(PlayerController.Get()))
	{
		Save->ApplySettings(false);
	}
}

// ---------------------------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------------------------

int32 SMTFrontEnd::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	Hits.Reset();
	int32 Layer = LayerId;
	switch (Page)
	{
	case EMTFrontPage::Title: Layer = PaintTitle(G, Out, Layer); break;
	case EMTFrontPage::Map: Layer = PaintMap(G, Out, Layer); break;
	case EMTFrontPage::Edit: Layer = PaintEdit(G, Out, Layer); break;
	case EMTFrontPage::Abilities: Layer = PaintAbilities(G, Out, Layer); break;
	case EMTFrontPage::Settings: Layer = PaintSettings(G, Out, Layer); break;
	case EMTFrontPage::Pause: Layer = PaintPause(G, Out, Layer); break;
	default: break;
	}

	// Page transition: content rises out of a short fade.
	const FVector2D Screen = G.GetLocalSize();
	const float Intro = 1.f - FMath::Clamp(float(Now - PageSince) / 0.35f, 0.f, 1.f);
	if (Intro > 0.f && Page != EMTFrontPage::None)
	{
		MTUI::Box(Out, Layer + 1, G, FVector2D::ZeroVector, Screen, FLinearColor(0.f, 0.f, 0.f, 0.85f * Intro * Intro));
	}
	// Spawn: fade to black, hold, fade back in over the world.
	if (bSpawning)
	{
		const float T = float(Now - SpawnStart);
		const float Black = T < 0.75f ? T / 0.75f : FMath::Clamp(1.f - (T - 1.05f) / 0.9f, 0.f, 1.f);
		MTUI::Box(Out, Layer + 2, G, FVector2D::ZeroVector, Screen, FLinearColor(0.f, 0.f, 0.f, Black));
		if (T > 0.3f && T < 1.4f)
		{
			const float A = FMath::Clamp(FMath::Min((T - 0.3f) / 0.3f, (1.4f - T) / 0.3f), 0.f, 1.f);
			MTUI::TextAligned(Out, Layer + 3, G, TEXT("L A   P L A C E"), MTUI::Logo(40.f * DS(G)), FVector2D::ZeroVector, Screen, FVector2D(0.5f, 0.5f),
				MTUI::GoldBright.CopyWithNewOpacity(A));
		}
		Layer += 5;
	}
	return Layer + 4;
}

int32 SMTFrontEnd::PaintMotes(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	for (const FMote& M : Motes)
	{
		const float A = FMath::Sin(PI * FMath::Clamp(M.Life / M.MaxLife, 0.f, 1.f));
		const FLinearColor C = M.Hue < 0.7f ? MTUI::GoldBright : FLinearColor(0.45f, 0.8f, 1.f);
		MTUI::Glow(Out, Layer, G, D(G, M.Pos.X, M.Pos.Y), M.Size * 3.f * S, C.CopyWithNewOpacity(0.55f * A));
	}
	return Layer + 1;
}

int32 SMTFrontEnd::PaintBackdrop(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const TCHAR* Art, float Dim) const
{
	const FVector2D Screen = G.GetLocalSize();
	MTUI::Box(Out, Layer, G, FVector2D::ZeroVector, Screen, FLinearColor(0.01f, 0.01f, 0.015f, 1.f));
	if (const FSlateBrush* B = MTUI::UIBrush(TEXT("Art"), Art))
	{
		// Slow drift (Ken Burns): cover the screen, zoom and pan a little over time.
		const float Aspect = B->ImageSize.X / FMath::Max(1.f, (float)B->ImageSize.Y);
		FVector2D Size(Screen.X, Screen.X / Aspect);
		if (Size.Y < Screen.Y)
		{
			Size = FVector2D(Screen.Y * Aspect, Screen.Y);
		}
		const float Zoom = 1.06f + 0.03f * FMath::Sin(Now * 0.05);
		Size *= Zoom;
		const FVector2D Pan(FMath::Sin(Now * 0.037) * 0.015f * Screen.X, FMath::Cos(Now * 0.029) * 0.01f * Screen.Y);
		MTUI::Box(Out, Layer + 1, G, (Screen - Size) * 0.5f + Pan, Size, FLinearColor(Dim, Dim, Dim, 1.f), B);
	}
	return Layer + 2;
}

int32 SMTFrontEnd::Button(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FVector2D& Size,
	const FString& Label, FName Action, FName Param, bool bPrimary, bool bSelected, float FontSize) const
{
	const float S = DS(G);
	const float H = HoverAlpha(Action, Param);
	const FLinearColor Fill = bPrimary ? FLinearColor(0.32f + 0.12f * H, 0.19f + 0.08f * H, 0.05f, 0.95f) : FLinearColor(0.04f + 0.05f * H, 0.035f + 0.04f * H, 0.03f, 0.85f);
	if (H > 0.01f || bPrimary)
	{
		MTUI::Glow(Out, Layer, G, Pos + Size * 0.5f, FMath::Max(Size.X, Size.Y) * (0.55f + 0.05f * H), MTUI::Gold.CopyWithNewOpacity((bPrimary ? 0.18f : 0.f) + 0.22f * H));
	}
	MTUI::Box(Out, Layer + 1, G, Pos, Size, Fill);
	MTUI::Box(Out, Layer + 1, G, Pos, FVector2D(Size.X, Size.Y * 0.45f), FLinearColor(1.f, 0.9f, 0.7f, 0.04f + 0.04f * H));
	MTUI::GoldFrame(Out, Layer + 2, G, Pos, Size, bSelected ? 1.f : 0.7f + 0.3f * H, false);
	const FLinearColor TextColor = bSelected ? MTUI::GoldBright : FLinearColor::LerpUsingHSV(MTUI::TextLight, MTUI::GoldBright, H);
	MTUI::TextAligned(Out, Layer + 3, G, Label, MTUI::Heading(FontSize * S), Pos, Size, FVector2D(0.5f, 0.5f), TextColor);
	AddHit(G, Pos, Size, Action, Param);
	return Layer + 5;
}

int32 SMTFrontEnd::TextEntry(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Pos, const FString& Label, FName Action, float FontSize) const
{
	const float S = DS(G);
	const int32 Index = (int32)Action.GetNumber();
	const float H = HoverAlpha(Action, NAME_None);
	const FSlateFontInfo Font = MTUI::Heading(FontSize * S);
	const FVector2D TextSize = MTUI::Measure(Label, Font);
	const FVector2D At = Pos + FVector2D(18.f * S * H, 0.f);
	if (H > 0.01f)
	{
		MTUI::Glow(Out, Layer, G, At + FVector2D(TextSize.X * 0.5f, TextSize.Y * 0.55f), TextSize.X * 0.8f, MTUI::Gold.CopyWithNewOpacity(0.18f * H));
		// Diamond marker.
		const FVector2D Mark = Pos + FVector2D(-10.f * S, TextSize.Y * 0.5f);
		MTUI::Glow(Out, Layer + 1, G, Mark, 10.f * S * H, MTUI::GoldBright.CopyWithNewOpacity(H));
		MTUI::Divider(Out, Layer + 1, G, At + FVector2D(TextSize.X * 0.5f, TextSize.Y + 4.f * S), TextSize.X * 1.2f * H, H);
	}
	MTUI::Text(Out, Layer + 2, G, Label, Font, At, FLinearColor::LerpUsingHSV(MTUI::TextLight, MTUI::GoldBright, H));
	AddHit(G, Pos - FVector2D(20.f * S, 6.f * S), TextSize + FVector2D(60.f * S, 12.f * S), Action, NAME_None);
	(void)Index;
	return Layer + 4;
}

// ---------------------------------------------------------------------------------------------
// Title
// ---------------------------------------------------------------------------------------------

int32 SMTFrontEnd::PaintTitle(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const FVector2D Screen = G.GetLocalSize();
	const float S = DS(G);
	Layer = PaintBackdrop(G, Out, Layer, TEXT("T_Title_Fittoa_Dawn"), 1.f);

	// Darken the left third for the logo and menu, and the bottom edge.
	TArray<FSlateGradientStop> Stops;
	Stops.Add(FSlateGradientStop(FVector2D(0.f, 0.f), FLinearColor(0.f, 0.f, 0.f, 0.78f)));
	Stops.Add(FSlateGradientStop(FVector2D(Screen.X * 0.45f, 0.f), FLinearColor(0.f, 0.f, 0.f, 0.25f)));
	Stops.Add(FSlateGradientStop(FVector2D(Screen.X * 0.7f, 0.f), FLinearColor(0.f, 0.f, 0.f, 0.f)));
	FSlateDrawElement::MakeGradient(Out, Layer, G.ToPaintGeometry(), Stops, Orient_Vertical);
	TArray<FSlateGradientStop> Bottom;
	Bottom.Add(FSlateGradientStop(FVector2D(0.f, Screen.Y * 0.72f), FLinearColor(0.f, 0.f, 0.f, 0.f)));
	Bottom.Add(FSlateGradientStop(FVector2D(0.f, Screen.Y), FLinearColor(0.f, 0.f, 0.f, 0.7f)));
	FSlateDrawElement::MakeGradient(Out, Layer, G.ToPaintGeometry(), Bottom, Orient_Horizontal);
	Layer += 1;
	Layer = PaintMotes(G, Out, Layer);

	// Logo: a soft gold glow behind the letters, a dark outline, then bright gold.
	const FSlateFontInfo LogoFont = MTUI::Logo(132.f * S);
	const FString Logo = TEXT("LA PLACE");
	const FVector2D LogoPos = D(G, 128.f, 150.f);
	const FVector2D LogoSize = MTUI::Measure(Logo, LogoFont);
	const float Breath = 0.5f + 0.5f * FMath::Sin(Now * 1.3);
	MTUI::Glow(Out, Layer, G, LogoPos + LogoSize * FVector2D(0.5f, 0.55f), LogoSize.X * 0.62f, MTUI::Gold.CopyWithNewOpacity(0.16f + 0.06f * Breath));
	for (const FVector2D& Off : { FVector2D(3.f, 3.f), FVector2D(-2.f, 2.f), FVector2D(2.f, -2.f) })
	{
		MTUI::Text(Out, Layer + 1, G, Logo, LogoFont, LogoPos + Off * S, FLinearColor(0.08f, 0.04f, 0.01f, 0.9f), false);
	}
	MTUI::Text(Out, Layer + 3, G, Logo, LogoFont, LogoPos, FLinearColor::LerpUsingHSV(MTUI::Gold, MTUI::GoldBright, 0.55f + 0.2f * Breath), false);
	const FString Sub = Spaced(TEXT("A MUSHOKU TENSEI ADVENTURE"));
	MTUI::Text(Out, Layer + 3, G, Sub, MTUI::Heading(20.f * S), LogoPos + FVector2D(10.f * S, LogoSize.Y + 4.f * S), MTUI::TextLight.CopyWithNewOpacity(0.9f));
	MTUI::Divider(Out, Layer + 3, G, LogoPos + FVector2D(LogoSize.X * 0.5f, LogoSize.Y + 58.f * S), LogoSize.X * 0.95f, 0.95f);

	// Menu.
	struct FEntry { const TCHAR* Label; EMTFrontPage Page; bool bQuit; };
	const FEntry Entries[] = {
		{ TEXT("PLAY"), EMTFrontPage::Map, false },
		{ TEXT("EDIT"), EMTFrontPage::Edit, false },
		{ TEXT("ABILITIES"), EMTFrontPage::Abilities, false },
		{ TEXT("SETTINGS"), EMTFrontPage::Settings, false },
		{ TEXT("QUIT"), EMTFrontPage::None, true },
	};
	int32 Row = 0;
	for (const FEntry& E : Entries)
	{
		const FVector2D At = D(G, 170.f, 520.f + Row * 82.f);
		const FName Action = E.bQuit ? ActQuit : ActPage;
		const float H = HoverAlpha(Action, NAME_None, E.bQuit ? INDEX_NONE : (int32)E.Page);
		const FSlateFontInfo Font = MTUI::Heading((Row == 0 ? 46.f : 38.f) * S);
		const FVector2D TextSize = MTUI::Measure(E.Label, Font);
		const FVector2D Pos = At + FVector2D(22.f * S * H, 0.f);
		if (H > 0.01f)
		{
			MTUI::Glow(Out, Layer + 3, G, Pos + TextSize * FVector2D(0.5f, 0.55f), TextSize.X * 0.9f, MTUI::Gold.CopyWithNewOpacity(0.2f * H));
			MTUI::Glow(Out, Layer + 4, G, At + FVector2D(-14.f * S, TextSize.Y * 0.52f), 12.f * S, MTUI::GoldBright.CopyWithNewOpacity(H));
		}
		MTUI::Text(Out, Layer + 5, G, E.Label, Font, Pos, FLinearColor::LerpUsingHSV(MTUI::TextLight, MTUI::GoldBright, H));
		AddHit(G, At - FVector2D(30.f * S, 8.f * S), TextSize + FVector2D(90.f * S, 16.f * S), Action, NAME_None, E.bQuit ? INDEX_NONE : (int32)E.Page);
		++Row;
	}

	// Who you are playing, bottom-right.
	FString Who = TEXT("Rudeus Greyrat");
	FString Epithet;
	if (const UMTProgressionSubsystem* Progression = PlayerController.IsValid() ? UMTProgressionSubsystem::Get(PlayerController.Get()) : nullptr)
	{
		if (const UMTDataRegistry* Registry = UMTDataRegistry::Get(PlayerController.Get()))
		{
			if (const FMTCharacterData* C = Registry->FindCharacter(Progression->GetEquippedCharacter()))
			{
				Who = C->DisplayName.ToString();
				Epithet = C->Title.ToString();
			}
		}
	}
	const FVector2D Card = D(G, 1460.f, 930.f);
	MTUI::DarkPanel(Out, Layer + 3, G, Card, FVector2D(420.f, 96.f) * S, 0.9f, true);
	MTUI::Text(Out, Layer + 6, G, TEXT("PLAYING AS"), MTUI::Heading(13.f * S), Card + FVector2D(22.f, 14.f) * S, MTUI::GoldDim.CopyWithNewOpacity(1.f));
	MTUI::Text(Out, Layer + 6, G, Who, MTUI::Heading(24.f * S), Card + FVector2D(22.f, 34.f) * S, MTUI::GoldBright);
	MTUI::Text(Out, Layer + 6, G, Epithet, MTUI::BodyItalic(18.f * S), Card + FVector2D(22.f, 64.f) * S, MTUI::TextLight);
	MTUI::Text(Out, Layer + 6, G, TEXT("Rudeus  -  Orsted   |   an original fan game"), MTUI::Body(16.f * S), D(G, 60.f, 1040.f), MTUI::TextLight.CopyWithNewOpacity(0.55f));
	return Layer + 8;
}

// ---------------------------------------------------------------------------------------------
// World map (PLAY)
// ---------------------------------------------------------------------------------------------

int32 SMTFrontEnd::PaintMap(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	Layer = PaintBackdrop(G, Out, Layer, TEXT("T_Title_World_Dusk"), 0.32f);
	Layer = PaintMotes(G, Out, Layer);
	MTUI::TextAligned(Out, Layer, G, TEXT("CHOOSE WHERE YOUR STORY BEGINS"), MTUI::Title(40.f * S), D(G, 0.f, 26.f), FVector2D(1920.f, 60.f) * S, FVector2D(0.5f, 0.f), MTUI::GoldBright);
	MTUI::Divider(Out, Layer, G, D(G, 960.f, 100.f), 520.f * S);

	// The map: 4:3, eases in when the page opens.
	const float Open = FMath::Clamp(float(Now - PageSince) / 0.8f, 0.f, 1.f);
	const float Ease = 1.f - FMath::Pow(1.f - Open, 3.f);
	const FVector2D MapPos = D(G, 60.f, 128.f);
	const FVector2D MapSize = FVector2D(1180.f, 885.f) * S;
	MTUI::Box(Out, Layer + 1, G, MapPos + FVector2D(10.f, 12.f) * S, MapSize, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	if (const FSlateBrush* Map = MTUI::UIBrush(TEXT("Art"), TEXT("T_WorldMap")))
	{
		const float Zoom = FMath::Lerp(1.07f, 1.f, Ease);
		FSlateBrush Cropped = *Map;
		const float Inset = (1.f - 1.f / Zoom) * 0.5f;
		Cropped.SetUVRegion(FBox2f(FVector2f(Inset, Inset), FVector2f(1.f - Inset, 1.f - Inset)));
		FSlateDrawElement::MakeBox(Out, Layer + 2, G.ToPaintGeometry(FVector2f(MapSize), FSlateLayoutTransform(FVector2f(MapPos))), &Cropped, ESlateDrawEffect::None, FLinearColor::White);
	}
	else
	{
		MTUI::ParchmentPanel(Out, Layer + 2, G, MapPos, MapSize, 1.f);
		MTUI::TextAligned(Out, Layer + 8, G, TEXT("The cartographers are still drawing this world..."), MTUI::BodyItalic(26.f * S), MapPos, MapSize, FVector2D(0.5f, 0.5f), MTUI::Ink, false);
	}
	MTUI::GoldFrame(Out, Layer + 6, G, MapPos, MapSize, 1.f, true, 60.f * S);

	// Pins.
	const UMTDataRegistry* Registry = PlayerController.IsValid() ? UMTDataRegistry::Get(PlayerController.Get()) : nullptr;
	int32 PinLayer = Layer + 9;
	for (const FName& Id : SpawnLocations)
	{
		const FMTLocationData* Loc = Registry ? Registry->FindLocation(Id) : nullptr;
		if (!Loc)
		{
			continue;
		}
		const FVector2D At = MapPos + MapSize * Loc->MapUV;
		const float H = HoverAlpha(ActPin, Id);
		const bool bSel = Id == SelectedLocation;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 3.0);
		const float R = (9.f + 5.f * H + (bSel ? 3.f : 0.f)) * S;
		MTUI::Glow(Out, PinLayer, G, At, R * (bSel ? 4.f + 1.2f * Pulse : 3.f), (bSel ? MTUI::GoldBright : MTUI::Gold).CopyWithNewOpacity(bSel ? 0.55f : 0.35f + 0.3f * H));
		MTUI::Glow(Out, PinLayer + 1, G, At, R, FLinearColor(1.f, 0.95f, 0.8f, 1.f));
		if (bSel)
		{
			MTUI::Arc(Out, PinLayer + 1, G, At, R * 2.6f + 4.f * Pulse * S, 1.f, MTUI::GoldBright.CopyWithNewOpacity(0.8f), 2.f);
		}
		if (bSel || H > 0.05f)
		{
			const FString Name = Loc->DisplayName.ToString();
			const FSlateFontInfo Font = MTUI::Heading(18.f * S);
			const FVector2D TS = MTUI::Measure(Name, Font);
			const FVector2D LabelPos = At + FVector2D(-TS.X * 0.5f, -R - TS.Y - 10.f * S);
			MTUI::Box(Out, PinLayer + 2, G, LabelPos - FVector2D(8.f, 3.f) * S, TS + FVector2D(16.f, 6.f) * S, FLinearColor(0.03f, 0.02f, 0.01f, 0.8f));
			MTUI::GoldFrame(Out, PinLayer + 3, G, LabelPos - FVector2D(8.f, 3.f) * S, TS + FVector2D(16.f, 6.f) * S, 0.8f, false);
			MTUI::Text(Out, PinLayer + 4, G, Name, Font, LabelPos, MTUI::GoldBright);
		}
		AddHit(G, At - FVector2D(22.f * S), FVector2D(44.f * S), ActPin, Id);
	}
	Layer = PinLayer + 6;

	// Location card.
	const FVector2D CardPos = D(G, 1280.f, 128.f);
	const FVector2D CardSize = FVector2D(580.f, 885.f) * S;
	MTUI::ParchmentPanel(Out, Layer, G, CardPos, CardSize, 1.f);
	Layer += 6;
	const FMTLocationData* Sel = Registry ? Registry->FindLocation(SelectedLocation) : nullptr;
	if (!Sel)
	{
		MTUI::TextAligned(Out, Layer, G, TEXT("Select a place on the map."), MTUI::BodyItalic(26.f * S), CardPos, CardSize, FVector2D(0.5f, 0.5f), MTUI::Ink, false);
		Button(G, Out, Layer + 2, D(G, 60.f, 1030.f) - FVector2D(0.f, 10.f * S), FVector2D(180.f, 44.f) * S, TEXT("BACK"), ActBack, NAME_None, false, false, 18.f);
		return Layer + 8;
	}
	const FVector2D ArtPos = CardPos + FVector2D(36.f, 40.f) * S;
	const FVector2D ArtSize = FVector2D(508.f, 286.f) * S;
	if (const FSlateBrush* Art = MTUI::UIBrush(TEXT("Art"), FString::Printf(TEXT("T_Location_%s"), *SelectedLocation.ToString())))
	{
		MTUI::Box(Out, Layer, G, ArtPos, ArtSize, FLinearColor::White, Art);
	}
	else
	{
		MTUI::Box(Out, Layer, G, ArtPos, ArtSize, FLinearColor(0.1f, 0.08f, 0.06f, 1.f));
	}
	// Live view of the place (scene capture along its preview camera), cross-faded over the painting.
	const UMTFrontEndSubsystem* FrontEnd = UMTFrontEndSubsystem::Get(PlayerController.Get());
	if (const FSlateBrush* Live = FrontEnd ? FrontEnd->GetPreviewBrush() : nullptr)
	{
		const float Fade = FMath::Clamp((FrontEnd->GetPreviewAge() - 0.35f) / 1.2f, 0.f, 1.f);
		MTUI::Box(Out, Layer, G, ArtPos, ArtSize, FLinearColor(1.f, 1.f, 1.f, Fade), Live);
	}
	MTUI::GoldFrame(Out, Layer + 1, G, ArtPos, ArtSize, 1.f, false);
	float Y = ArtPos.Y + ArtSize.Y + 22.f * S;
	const float X = CardPos.X + 40.f * S;
	const float W = CardSize.X - 80.f * S;
	MTUI::Text(Out, Layer + 2, G, Sel->DisplayName.ToString(), MTUI::Title(34.f * S), FVector2D(X, Y), MTUI::Ink, false);
	Y += 48.f * S;
	FString Where = Sel->Continent.ToString();
	if (!Sel->Region.IsNone())
	{
		Where += FString(Where.IsEmpty() ? TEXT("") : TEXT("   -   ")) + Sel->Region.ToString();
	}
	MTUI::Text(Out, Layer + 2, G, Where.ToUpper(), MTUI::Heading(15.f * S), FVector2D(X, Y), MTUI::InkSoft, false);
	Y += 28.f * S;
	if (!Sel->Biome.IsEmpty())
	{
		MTUI::Text(Out, Layer + 2, G, Sel->Biome.ToString(), MTUI::BodyItalic(21.f * S), FVector2D(X, Y), MTUI::InkSoft, false);
		Y += 30.f * S;
	}
	// Danger: five diamonds.
	MTUI::Text(Out, Layer + 2, G, TEXT("DANGER"), MTUI::Heading(14.f * S), FVector2D(X, Y + 2.f * S), MTUI::InkSoft, false);
	for (int32 i = 0; i < 5; ++i)
	{
		const bool bOn = i < FMath::Clamp(Sel->Difficulty, 1, 5);
		const FVector2D C(X + 96.f * S + i * 26.f * S, Y + 11.f * S);
		MTUI::Glow(Out, Layer + 2, G, C, (bOn ? 11.f : 7.f) * S, bOn ? FLinearColor(0.75f, 0.12f, 0.05f, 1.f) : FLinearColor(0.2f, 0.15f, 0.1f, 0.5f));
	}
	Y += 38.f * S;
	MTUI::Divider(Out, Layer + 2, G, FVector2D(CardPos.X + CardSize.X * 0.5f, Y), W * 0.8f, 0.9f);
	Y += 16.f * S;
	MTUI::Paragraph(Out, Layer + 2, G, Sel->Description.ToString(), MTUI::Body(21.f * S), FVector2D(X, Y), W, MTUI::Ink);

	Button(G, Out, Layer + 4, CardPos + FVector2D(40.f, 885.f - 110.f) * S, FVector2D(500.f, 74.f) * S, TEXT("SPAWN"), ActSpawn, NAME_None, true, false, 30.f);
	Button(G, Out, Layer + 4, D(G, 60.f, 1026.f), FVector2D(180.f, 44.f) * S, TEXT("BACK"), ActBack, NAME_None, false, false, 18.f);
	return Layer + 10;
}

// ---------------------------------------------------------------------------------------------
// Edit (character)
// ---------------------------------------------------------------------------------------------

int32 SMTFrontEnd::PaintEdit(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	Layer = PaintBackdrop(G, Out, Layer, TEXT("T_Title_World_Dusk"), 0.28f);
	Layer = PaintMotes(G, Out, Layer);
	MTUI::TextAligned(Out, Layer, G, TEXT("CHOOSE YOUR LINEAGE"), MTUI::Title(40.f * S), D(G, 0.f, 26.f), FVector2D(1920.f, 60.f) * S, FVector2D(0.5f, 0.f), MTUI::GoldBright);
	MTUI::Divider(Out, Layer, G, D(G, 960.f, 100.f), 460.f * S);

	const UMTProgressionSubsystem* Progression = PlayerController.IsValid() ? UMTProgressionSubsystem::Get(PlayerController.Get()) : nullptr;
	const UMTDataRegistry* Registry = PlayerController.IsValid() ? UMTDataRegistry::Get(PlayerController.Get()) : nullptr;
	if (!Registry || !Progression)
	{
		return Layer + 2;
	}
	TArray<const FMTCharacterData*> Characters;
	for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
	{
		Characters.Add(&Pair.Value);
	}
	float MaxHealth = 1.f, MaxMana = 1.f, MaxPoise = 1.f, MaxSpeed = 1.f;
	for (const FMTCharacterData* C : Characters)
	{
		MaxHealth = FMath::Max(MaxHealth, C->MaxHealth);
		MaxMana = FMath::Max(MaxMana, C->MaxMana);
		MaxPoise = FMath::Max(MaxPoise, C->MaxPoise);
		MaxSpeed = FMath::Max(MaxSpeed, C->SprintSpeed);
	}
	const float CardW = 600.f;
	const float Gap = 60.f;
	const float StartX = (1920.f - Characters.Num() * CardW - (Characters.Num() - 1) * Gap) * 0.5f;
	int32 Index = 0;
	for (const FMTCharacterData* C : Characters)
	{
		const bool bEquipped = Progression->GetEquippedCharacter() == C->CharacterID;
		const float H = HoverAlpha(ActPickCharacter, C->CharacterID);
		const FVector2D P = D(G, StartX + Index * (CardW + Gap), 140.f - 8.f * H);
		const FVector2D Size = FVector2D(CardW, 850.f) * S;
		if (bEquipped)
		{
			MTUI::Glow(Out, Layer, G, P + Size * 0.5f, Size.Y * 0.62f, MTUI::Gold.CopyWithNewOpacity(0.22f));
		}
		MTUI::ParchmentPanel(Out, Layer + 1, G, P, Size, 1.f);
		int32 L = Layer + 7;
		// Portrait (rendered from the game model), else the awakening art.
		const FVector2D PortraitPos = P + FVector2D(40.f, 40.f) * S;
		const FVector2D PortraitSize = FVector2D(CardW - 80.f, 330.f) * S;
		const FSlateBrush* Portrait = MTUI::UIBrush(TEXT("Art"), FString::Printf(TEXT("T_Portrait_%s"), *C->CharacterID.ToString()));
		if (!Portrait)
		{
			Portrait = MTUI::AbilityIcon(C->AwakeningAbility);
		}
		MTUI::Box(Out, L, G, PortraitPos, PortraitSize, FLinearColor(0.03f, 0.03f, 0.04f, 1.f));
		if (Portrait)
		{
			const float Aspect = Portrait->ImageSize.X / FMath::Max(1.f, (float)Portrait->ImageSize.Y);
			FVector2D ImgSize(PortraitSize.Y * Aspect, PortraitSize.Y);
			if (ImgSize.X > PortraitSize.X)
			{
				ImgSize = FVector2D(PortraitSize.X, PortraitSize.X / Aspect);
			}
			MTUI::Box(Out, L + 1, G, PortraitPos + (PortraitSize - ImgSize) * 0.5f, ImgSize, FLinearColor::White, Portrait);
		}
		MTUI::GoldFrame(Out, L + 2, G, PortraitPos, PortraitSize, 1.f, false);
		float Y = PortraitPos.Y + PortraitSize.Y + 20.f * S;
		const float X = P.X + 44.f * S;
		const float W = Size.X - 88.f * S;
		MTUI::Text(Out, L + 3, G, C->DisplayName.ToString(), MTUI::Title(32.f * S), FVector2D(X, Y), MTUI::Ink, false);
		Y += 46.f * S;
		MTUI::Text(Out, L + 3, G, C->Title.ToString(), MTUI::BodyItalic(24.f * S), FVector2D(X, Y), MTUI::InkSoft, false);
		Y += 34.f * S;
		{
			// First two sentences of the lore description.
			FString Blurb = C->Description.ToString();
			int32 Cut = INDEX_NONE;
			int32 Found = 0;
			for (int32 i = 0; i < Blurb.Len(); ++i)
			{
				if (Blurb[i] == TEXT('.') && (i + 1 == Blurb.Len() || Blurb[i + 1] == TEXT(' ')) && ++Found == 2)
				{
					Cut = i + 1;
					break;
				}
			}
			if (Cut != INDEX_NONE)
			{
				Blurb = Blurb.Left(Cut);
			}
			Y += MTUI::Paragraph(Out, L + 3, G, Blurb, MTUI::Body(17.f * S), FVector2D(X, Y), W, MTUI::Ink, 1.05f) + 10.f * S;
		}
		// Stats.
		struct FStat { const TCHAR* Name; float Value; float Max; FLinearColor Color; };
		const FStat Stats[] = {
			{ TEXT("VITALITY"), C->MaxHealth, MaxHealth, MTUI::Health },
			{ TEXT("MANA"), C->MaxMana, MaxMana, MTUI::Mana },
			{ TEXT("POISE"), C->MaxPoise, MaxPoise, FLinearColor(0.7f, 0.55f, 0.3f, 1.f) },
			{ TEXT("SPEED"), C->SprintSpeed, MaxSpeed, MTUI::Stamina },
		};
		for (const FStat& Stat : Stats)
		{
			MTUI::Text(Out, L + 3, G, Stat.Name, MTUI::Heading(14.f * S), FVector2D(X, Y), MTUI::InkSoft, false);
			MTUI::Bar(Out, L + 3, G, FVector2D(X + 130.f * S, Y + 4.f * S), FVector2D(W - 130.f * S, 12.f * S), Stat.Value / Stat.Max, Stat.Value / Stat.Max, Stat.Color);
			Y += 26.f * S;
		}
		Y += 8.f * S;
		// Signature techniques.
		TArray<FName> Signature = C->Abilities;
		Signature.Add(C->SpecialAbility);
		Signature.Add(C->AwakeningAbility);
		float IX = X;
		for (const FName& Id : Signature)
		{
			if (const FSlateBrush* Icon = MTUI::AbilityIcon(Id))
			{
				MTUI::Box(Out, L + 3, G, FVector2D(IX, Y), FVector2D(60.f * S), FLinearColor::White, Icon);
				MTUI::GoldFrame(Out, L + 4, G, FVector2D(IX, Y), FVector2D(60.f * S), 0.8f, false);
				IX += 70.f * S;
			}
		}
		Button(G, Out, L + 6, P + FVector2D(60.f, 850.f - 100.f) * S, FVector2D(CardW - 120.f, 66.f) * S, bEquipped ? TEXT("SELECTED") : TEXT("SELECT"),
			ActPickCharacter, C->CharacterID, !bEquipped, bEquipped, 26.f);
		AddHit(G, P, FVector2D(Size.X, Size.Y - 110.f * S), ActPickCharacter, C->CharacterID);
		++Index;
	}
	Button(G, Out, Layer + 20, D(G, 60.f, 1026.f), FVector2D(180.f, 44.f) * S, TEXT("BACK"), ActBack, NAME_None, false, false, 18.f);
	return Layer + 26;
}

// ---------------------------------------------------------------------------------------------
// Abilities (loadout)
// ---------------------------------------------------------------------------------------------

int32 SMTFrontEnd::PaintAbilities(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	Layer = PaintBackdrop(G, Out, Layer, TEXT("T_Title_World_Dusk"), 0.25f);
	Layer = PaintMotes(G, Out, Layer);
	MTUI::TextAligned(Out, Layer, G, TEXT("PREPARE YOUR SPELLS"), MTUI::Title(40.f * S), D(G, 0.f, 26.f), FVector2D(1920.f, 60.f) * S, FVector2D(0.5f, 0.f), MTUI::GoldBright);
	MTUI::Divider(Out, Layer, G, D(G, 960.f, 100.f), 460.f * S);

	UMTProgressionSubsystem* Progression = PlayerController.IsValid() ? UMTProgressionSubsystem::Get(PlayerController.Get()) : nullptr;
	const UMTDataRegistry* Registry = PlayerController.IsValid() ? UMTDataRegistry::Get(PlayerController.Get()) : nullptr;
	if (!Progression || !Registry)
	{
		return Layer + 2;
	}
	const FName CharId = Progression->GetEquippedCharacter();
	const FMTCharacterData* Char = Registry->FindCharacter(CharId);
	const TArray<FName> Loadout = Progression->GetLoadout(CharId);

	// Hotbar editor: LMB and F/G are fixed per character; 1-4 are the player's choice.
	const FVector2D BarPos = D(G, 60.f, 128.f);
	const FVector2D BarSize = FVector2D(1180.f, 210.f) * S;
	MTUI::DarkPanel(Out, Layer + 1, G, BarPos, BarSize, 0.95f, true);
	MTUI::Text(Out, Layer + 3, G, FString::Printf(TEXT("%s  -  HOTBAR"), Char ? *Char->DisplayName.ToString().ToUpper() : TEXT("")), MTUI::Heading(18.f * S),
		BarPos + FVector2D(30.f, -34.f) * S, MTUI::GoldBright);
	MTUI::Text(Out, Layer + 3, G, TEXT("Click a key, then pick a spell below."), MTUI::BodyItalic(18.f * S), BarPos + FVector2D(700.f, -34.f) * S, MTUI::TextLight.CopyWithNewOpacity(0.8f));
	struct FKeySlot { const TCHAR* Key; FName Id; int32 Editable; };
	const FKeySlot Keys[] = {
		{ TEXT("LMB"), Char ? Char->BasicAbility : NAME_None, -1 },
		{ TEXT("1"), Loadout.IsValidIndex(0) ? Loadout[0] : NAME_None, 0 },
		{ TEXT("2"), Loadout.IsValidIndex(1) ? Loadout[1] : NAME_None, 1 },
		{ TEXT("3"), Loadout.IsValidIndex(2) ? Loadout[2] : NAME_None, 2 },
		{ TEXT("4"), Loadout.IsValidIndex(3) ? Loadout[3] : NAME_None, 3 },
		{ TEXT("F"), Char ? Char->SpecialAbility : NAME_None, -1 },
		{ TEXT("G"), Char ? Char->AwakeningAbility : NAME_None, -1 },
	};
	for (int32 i = 0; i < 7; ++i)
	{
		const FKeySlot& K = Keys[i];
		const float X = 40.f + i * 160.f + (i >= 5 ? 40.f : 0.f);
		const FVector2D P = BarPos + FVector2D(X, 58.f) * S;
		const FVector2D Size(104.f * S, 104.f * S);
		const bool bEditable = K.Editable >= 0;
		const bool bSel = bEditable && K.Editable == SelectedSlot;
		const float H = bEditable ? HoverAlpha(ActSlot, NAME_None, K.Editable) : 0.f;
		if (bSel)
		{
			MTUI::Glow(Out, Layer + 3, G, P + Size * 0.5f, 90.f * S * (1.f + 0.06f * FMath::Sin(Now * 5.0)), MTUI::GoldBright.CopyWithNewOpacity(0.45f));
		}
		MTUI::Box(Out, Layer + 4, G, P, Size, FLinearColor(0.f, 0.f, 0.f, 0.75f));
		if (const FSlateBrush* Icon = MTUI::AbilityIcon(K.Id))
		{
			MTUI::Box(Out, Layer + 5, G, P + FVector2D(4.f * S), Size - FVector2D(8.f * S), bEditable ? FLinearColor::White : FLinearColor(0.75f, 0.75f, 0.75f, 1.f), Icon);
		}
		MTUI::GoldFrame(Out, Layer + 6, G, P, Size, bSel ? 1.f : 0.55f + 0.45f * H, false);
		MTUI::TextAligned(Out, Layer + 7, G, K.Key, MTUI::Heading(18.f * S), P - FVector2D(0.f, 30.f * S), FVector2D(Size.X, 26.f * S), FVector2D(0.5f, 0.5f),
			bEditable ? MTUI::GoldBright : MTUI::TextLight.CopyWithNewOpacity(0.6f));
		if (const FMTAbilityData* A = Registry->FindAbility(K.Id))
		{
			FString Name = A->DisplayName.ToString();
			Name.RemoveFromStart(TEXT("Dragon God Style: "));
			Name.RemoveFromStart(TEXT("Awakening: "));
			Name = Name.Replace(TEXT(" of Foresight"), TEXT(""));
			const FSlateFontInfo NameFont = MTUI::Body(16.f * S);
			while (Name.Len() > 4 && MTUI::Measure(Name, NameFont).X > Size.X + 44.f * S)
			{
				Name = Name.LeftChop(2) + TEXT(".");
			}
			MTUI::TextAligned(Out, Layer + 7, G, Name, NameFont, P + FVector2D(-28.f * S, Size.Y + 4.f * S), FVector2D(Size.X + 56.f * S, 20.f * S), FVector2D(0.5f, 0.f), MTUI::TextLight);
		}
		if (bEditable)
		{
			AddHit(G, P, Size, ActSlot, NAME_None, K.Editable);
		}
	}

	// Pool: the character's techniques and every element spell, grouped.
	const FVector2D PoolPos = D(G, 60.f, 370.f);
	const FVector2D PoolSize = FVector2D(1180.f, 640.f) * S;
	MTUI::ParchmentPanel(Out, Layer + 1, G, PoolPos, PoolSize, 0.97f);
	TArray<TPair<FString, TArray<FName>>> Groups;
	if (Char)
	{
		Groups.Add(TPair<FString, TArray<FName>>(FString::Printf(TEXT("%s's Techniques"), *Char->DisplayName.ToString().Replace(TEXT(" Greyrat"), TEXT(""))), Char->Abilities));
	}
	for (const TPair<EMTElement, FMTElementData>& Pair : Registry->GetElements())
	{
		Groups.Add(TPair<FString, TArray<FName>>(Pair.Value.DisplayName.ToString() + TEXT(" Magic"), Pair.Value.Abilities));
	}
	float GY = PoolPos.Y + 30.f * S;
	float GX = PoolPos.X + 40.f * S;
	int32 Column = 0;
	for (const TPair<FString, TArray<FName>>& Group : Groups)
	{
		// Two groups per row: left and right halves.
		const float ColX = GX + Column * 560.f * S;
		MTUI::Text(Out, Layer + 8, G, Group.Key.ToUpper(), MTUI::Heading(16.f * S), FVector2D(ColX, GY), MTUI::InkSoft, false);
		for (int32 i = 0; i < Group.Value.Num(); ++i)
		{
			const FName Id = Group.Value[i];
			const FVector2D P(ColX + i * 150.f * S, GY + 30.f * S);
			const FVector2D Size(96.f * S, 96.f * S);
			const float H = HoverAlpha(ActPool, Id);
			const bool bOn = Loadout.Contains(Id);
			if (H > 0.01f)
			{
				MTUI::Glow(Out, Layer + 8, G, P + Size * 0.5f, 80.f * S, MTUI::Gold.CopyWithNewOpacity(0.3f * H));
			}
			MTUI::Box(Out, Layer + 9, G, P, Size, FLinearColor(0.f, 0.f, 0.f, 0.8f));
			if (const FSlateBrush* Icon = MTUI::AbilityIcon(Id))
			{
				MTUI::Box(Out, Layer + 10, G, P + FVector2D(3.f * S), Size - FVector2D(6.f * S), FLinearColor::White, Icon);
			}
			MTUI::GoldFrame(Out, Layer + 11, G, P, Size, bOn ? 1.f : 0.5f + 0.5f * H, false);
			if (bOn)
			{
				const int32 KeyIndex = Loadout.IndexOfByKey(Id);
				const FVector2D Tag = P + FVector2D(Size.X - 26.f * S, -8.f * S);
				MTUI::Box(Out, Layer + 12, G, Tag, FVector2D(30.f * S, 24.f * S), FLinearColor(0.3f, 0.18f, 0.04f, 0.95f));
				MTUI::TextAligned(Out, Layer + 13, G, FString::FromInt(KeyIndex + 1), MTUI::Heading(15.f * S), Tag, FVector2D(30.f * S, 24.f * S), FVector2D(0.5f, 0.5f), MTUI::GoldBright);
			}
			if (const FMTAbilityData* A = Registry->FindAbility(Id))
			{
				FString Name = A->DisplayName.ToString();
				Name.RemoveFromStart(TEXT("Dragon God Style: "));
				MTUI::TextAligned(Out, Layer + 12, G, Name, MTUI::Body(17.f * S), P + FVector2D(-22.f * S, Size.Y + 2.f * S), FVector2D(Size.X + 44.f * S, 20.f * S), FVector2D(0.5f, 0.f), MTUI::Ink, false);
			}
			AddHit(G, P, Size, ActPool, Id);
		}
		if (++Column >= 2)
		{
			Column = 0;
			GY += 200.f * S;
		}
	}

	// Detail card for the hovered (or last picked) spell.
	FName Detail = InspectAbility;
	for (const TPair<FName, float>& Pair : Hover)
	{
		if (Pair.Value > 0.5f && Pair.Key.ToString().StartsWith(TEXT("Pool|")))
		{
			TArray<FString> Parts;
			Pair.Key.ToString().ParseIntoArray(Parts, TEXT("|"));
			if (Parts.Num() >= 2)
			{
				Detail = FName(*Parts[1]);
			}
		}
	}
	if (Detail.IsNone() && Loadout.IsValidIndex(SelectedSlot))
	{
		Detail = Loadout[SelectedSlot];
	}
	const FVector2D CardPos = D(G, 1280.f, 128.f);
	const FVector2D CardSize = FVector2D(580.f, 882.f) * S;
	MTUI::ParchmentPanel(Out, Layer + 1, G, CardPos, CardSize, 1.f);
	if (const FMTAbilityData* A = Registry->FindAbility(Detail))
	{
		const float X = CardPos.X + 44.f * S;
		const float W = CardSize.X - 88.f * S;
		float Y = CardPos.Y + 44.f * S;
		if (const FSlateBrush* Icon = MTUI::AbilityIcon(Detail))
		{
			const FVector2D IconSize(200.f * S, 200.f * S);
			const FVector2D IconPos(CardPos.X + (CardSize.X - IconSize.X) * 0.5f, Y);
			MTUI::Glow(Out, Layer + 8, G, IconPos + IconSize * 0.5f, 170.f * S, MTUtil::ElementColor(A->Element).CopyWithNewOpacity(0.25f));
			MTUI::Box(Out, Layer + 9, G, IconPos, IconSize, FLinearColor::White, Icon);
			MTUI::GoldFrame(Out, Layer + 10, G, IconPos, IconSize, 1.f, true, 36.f * S);
			Y += IconSize.Y + 24.f * S;
		}
		FString Name = A->DisplayName.ToString();
		Name.RemoveFromStart(TEXT("Dragon God Style: "));
		MTUI::TextAligned(Out, Layer + 10, G, Name, MTUI::Title(30.f * S), FVector2D(CardPos.X, Y), FVector2D(CardSize.X, 40.f * S), FVector2D(0.5f, 0.f), MTUI::Ink, false);
		Y += 46.f * S;
		const FString Kind = FString::Printf(TEXT("%s   -   %s"), *MTUtil::ElementToString(A->Element).ToUpper(), *BehaviorName(A->Behavior).ToUpper());
		MTUI::TextAligned(Out, Layer + 10, G, Kind, MTUI::Heading(14.f * S), FVector2D(CardPos.X, Y), FVector2D(CardSize.X, 20.f * S), FVector2D(0.5f, 0.f), MTUI::InkSoft, false);
		Y += 34.f * S;
		struct FRow { const TCHAR* Label; FString Value; };
		const FRow Rows[] = {
			{ TEXT("DAMAGE"), A->Damage > 0.f ? FString::Printf(TEXT("%d"), FMath::RoundToInt(A->Damage)) : FString(TEXT("-")) },
			{ TEXT("COOLDOWN"), FString::Printf(TEXT("%.1f s"), A->Cooldown) },
			{ TEXT("MANA"), A->ManaCost > 0.f ? FString::Printf(TEXT("%d"), FMath::RoundToInt(A->ManaCost)) : FString(TEXT("-")) },
			{ TEXT("RANGE"), A->Range > 0.f ? FString::Printf(TEXT("%d m"), FMath::RoundToInt(A->Range / 100.f)) : FString(TEXT("self")) },
		};
		for (int32 i = 0; i < 4; ++i)
		{
			const float RX = X + (i % 2) * W * 0.5f;
			const float RY = Y + (i / 2) * 34.f * S;
			MTUI::Text(Out, Layer + 10, G, Rows[i].Label, MTUI::Heading(13.f * S), FVector2D(RX, RY + 4.f * S), MTUI::InkSoft, false);
			MTUI::Text(Out, Layer + 10, G, Rows[i].Value, MTUI::Heading(19.f * S), FVector2D(RX + 118.f * S, RY), MTUI::Ink, false);
		}
		Y += 80.f * S;
		MTUI::Divider(Out, Layer + 10, G, FVector2D(CardPos.X + CardSize.X * 0.5f, Y), W * 0.8f, 0.9f);
		Y += 18.f * S;
		MTUI::Paragraph(Out, Layer + 10, G, A->Description.ToString(), MTUI::Body(21.f * S), FVector2D(X, Y), W, MTUI::Ink);
		const int32 KeyIndex = Loadout.IndexOfByKey(Detail);
		const FString Status = KeyIndex != INDEX_NONE ? FString::Printf(TEXT("Equipped on key %d"), KeyIndex + 1)
			: FString::Printf(TEXT("Click to put it on key %d"), SelectedSlot + 1);
		MTUI::TextAligned(Out, Layer + 10, G, Status, MTUI::BodyItalic(20.f * S), FVector2D(CardPos.X, CardPos.Y + CardSize.Y - 64.f * S), FVector2D(CardSize.X, 30.f * S),
			FVector2D(0.5f, 0.f), KeyIndex != INDEX_NONE ? FLinearColor(0.25f, 0.4f, 0.1f, 1.f) : MTUI::InkSoft, false);
	}
	Button(G, Out, Layer + 20, D(G, 60.f, 1026.f), FVector2D(180.f, 44.f) * S, TEXT("BACK"), ActBack, NAME_None, false, false, 18.f);
	return Layer + 26;
}

// ---------------------------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------------------------

int32 SMTFrontEnd::PaintSettings(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	if (ReturnPage == EMTFrontPage::Pause)
	{
		MTUI::Box(Out, Layer, G, FVector2D::ZeroVector, G.GetLocalSize(), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	}
	else
	{
		Layer = PaintBackdrop(G, Out, Layer, TEXT("T_Title_Fittoa_Dawn"), 0.3f);
	}
	Layer = PaintMotes(G, Out, Layer);
	const UMTProgressionSubsystem* Progression = PlayerController.IsValid() ? UMTProgressionSubsystem::Get(PlayerController.Get()) : nullptr;
	const FMTSettingsSave Settings = Progression ? Progression->GetSettings() : FMTSettingsSave();
	const FVector2D P = D(G, 460.f, 110.f);
	const FVector2D Size = FVector2D(1000.f, 860.f) * S;
	MTUI::ParchmentPanel(Out, Layer, G, P, Size, 1.f);
	Layer += 6;
	MTUI::TextAligned(Out, Layer, G, TEXT("SETTINGS"), MTUI::Title(40.f * S), P + FVector2D(0.f, 36.f * S), FVector2D(Size.X, 56.f * S), FVector2D(0.5f, 0.f), MTUI::Ink, false);
	MTUI::Divider(Out, Layer, G, P + FVector2D(Size.X * 0.5f, 104.f * S), 420.f * S, 0.9f);

	struct FSlider { FName Id; const TCHAR* Label; float Alpha; FString Value; };
	const FSlider Sliders[] = {
		{ TEXT("Volume"), TEXT("MASTER VOLUME"), Settings.MasterVolume, FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings.MasterVolume * 100.f)) },
		{ TEXT("Mouse"), TEXT("MOUSE SENSITIVITY"), (Settings.MouseSensitivity - 0.2f) / 2.8f, FString::Printf(TEXT("%.2f"), Settings.MouseSensitivity) },
		{ TEXT("FOV"), TEXT("FIELD OF VIEW"), (Settings.FieldOfView - 70.f) / 40.f, FString::Printf(TEXT("%d"), FMath::RoundToInt(Settings.FieldOfView)) },
		{ TEXT("Shake"), TEXT("CAMERA SHAKE"), Settings.CameraShakeScale, FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings.CameraShakeScale * 100.f)) },
	};
	float Y = P.Y + 140.f * S;
	const float LX = P.X + 80.f * S;
	const float CX = P.X + 460.f * S;
	const float CW = 380.f * S;
	for (const FSlider& Sl : Sliders)
	{
		const float H = HoverAlpha(ActSlider, Sl.Id);
		MTUI::Text(Out, Layer, G, Sl.Label, MTUI::Heading(19.f * S), FVector2D(LX, Y), MTUI::Ink, false);
		const FVector2D TrackPos(CX, Y + 10.f * S);
		const FVector2D TrackSize(CW, 10.f * S);
		MTUI::Bar(Out, Layer, G, TrackPos, TrackSize, FMath::Clamp(Sl.Alpha, 0.f, 1.f), 0.f, MTUI::Gold);
		const FVector2D Knob(CX + CW * FMath::Clamp(Sl.Alpha, 0.f, 1.f), Y + 15.f * S);
		MTUI::Glow(Out, Layer + 5, G, Knob, (22.f + 6.f * H) * S, MTUI::GoldBright.CopyWithNewOpacity(0.6f));
		const FVector2D Stud(12.f * S, 26.f * S);
		MTUI::Box(Out, Layer + 6, G, Knob - Stud * 0.5f, Stud, FLinearColor(0.35f, 0.21f, 0.05f, 1.f));
		MTUI::GoldFrame(Out, Layer + 7, G, Knob - Stud * 0.5f, Stud, 1.f, false);
		MTUI::Text(Out, Layer, G, Sl.Value, MTUI::Heading(18.f * S), FVector2D(CX + CW + 30.f * S, Y), MTUI::InkSoft, false);
		AddHit(G, FVector2D(CX - 10.f * S, Y - 6.f * S), FVector2D(CW + 20.f * S, 40.f * S), ActSlider, Sl.Id);
		Y += 74.f * S;
	}
	struct FToggle { FName Id; const TCHAR* Label; bool bOn; };
	const FToggle Toggles[] = {
		{ TEXT("InvertY"), TEXT("INVERT CAMERA Y"), Settings.bInvertY },
		{ TEXT("ToggleSprint"), TEXT("TOGGLE SPRINT"), Settings.bToggleSprint },
	};
	for (const FToggle& T : Toggles)
	{
		MTUI::Text(Out, Layer, G, T.Label, MTUI::Heading(19.f * S), FVector2D(LX, Y), MTUI::Ink, false);
		Button(G, Out, Layer, FVector2D(CX, Y - 8.f * S), FVector2D(160.f, 44.f) * S, T.bOn ? TEXT("ON") : TEXT("OFF"), ActToggle, T.Id, T.bOn, T.bOn, 18.f);
		Y += 74.f * S;
	}
	MTUI::Text(Out, Layer, G, TEXT("GRAPHICS"), MTUI::Heading(19.f * S), FVector2D(LX, Y), MTUI::Ink, false);
	const TCHAR* Qualities[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bOn = Settings.GraphicsQuality == i;
		const FVector2D BP(CX + i * 130.f * S, Y - 8.f * S);
		const float H = HoverAlpha(ActQuality, NAME_None, i);
		MTUI::Box(Out, Layer, G, BP, FVector2D(118.f, 44.f) * S, bOn ? FLinearColor(0.32f, 0.19f, 0.05f, 0.95f) : FLinearColor(0.05f + 0.05f * H, 0.04f, 0.03f, 0.85f));
		MTUI::GoldFrame(Out, Layer + 1, G, BP, FVector2D(118.f, 44.f) * S, bOn ? 1.f : 0.6f + 0.4f * H, false);
		MTUI::TextAligned(Out, Layer + 2, G, Qualities[i], MTUI::Heading(16.f * S), BP, FVector2D(118.f, 44.f) * S, FVector2D(0.5f, 0.5f), bOn ? MTUI::GoldBright : MTUI::TextLight);
		AddHit(G, BP, FVector2D(118.f, 44.f) * S, ActQuality, NAME_None, i);
	}
	Button(G, Out, Layer + 4, P + FVector2D(Size.X * 0.5f - 130.f * S, Size.Y - 100.f * S), FVector2D(260.f, 58.f) * S, TEXT("BACK"), ActBack, NAME_None, false, false, 22.f);
	return Layer + 10;
}

// ---------------------------------------------------------------------------------------------
// Pause
// ---------------------------------------------------------------------------------------------

int32 SMTFrontEnd::PaintPause(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float S = DS(G);
	MTUI::Box(Out, Layer, G, FVector2D::ZeroVector, G.GetLocalSize(), FLinearColor(0.f, 0.f, 0.01f, 0.62f));
	Layer = PaintMotes(G, Out, Layer + 1);
	const FVector2D Size = FVector2D(560.f, 640.f) * S;
	const FVector2D P = D(G, 680.f, 220.f);
	MTUI::ParchmentPanel(Out, Layer, G, P, Size, 1.f);
	Layer += 6;
	MTUI::TextAligned(Out, Layer, G, TEXT("PAUSED"), MTUI::Title(46.f * S), P + FVector2D(0.f, 40.f * S), FVector2D(Size.X, 60.f * S), FVector2D(0.5f, 0.f), MTUI::Ink, false);
	MTUI::Divider(Out, Layer, G, P + FVector2D(Size.X * 0.5f, 116.f * S), 320.f * S, 0.9f);
	struct FEntry { const TCHAR* Label; FName Action; int32 Index; };
	const FEntry Entries[] = {
		{ TEXT("RESUME"), ActResume, INDEX_NONE },
		{ TEXT("ABILITIES"), ActPage, (int32)EMTFrontPage::Abilities },
		{ TEXT("SETTINGS"), ActPage, (int32)EMTFrontPage::Settings },
		{ TEXT("RETURN TO TITLE"), ActTitle, INDEX_NONE },
		{ TEXT("QUIT GAME"), ActQuit, INDEX_NONE },
	};
	for (int32 i = 0; i < 5; ++i)
	{
		const FVector2D BP = P + FVector2D(80.f, 150.f + i * 88.f) * S;
		const FVector2D BS = FVector2D(400.f, 64.f) * S;
		const float H = HoverAlpha(Entries[i].Action, NAME_None, Entries[i].Index);
		MTUI::Box(Out, Layer, G, BP, BS, FLinearColor(0.05f + 0.25f * H, 0.035f + 0.15f * H, 0.02f + 0.03f * H, 0.9f));
		MTUI::GoldFrame(Out, Layer + 1, G, BP, BS, 0.6f + 0.4f * H, false);
		MTUI::TextAligned(Out, Layer + 2, G, Entries[i].Label, MTUI::Heading(22.f * S), BP, BS, FVector2D(0.5f, 0.5f), FLinearColor::LerpUsingHSV(MTUI::TextLight, MTUI::GoldBright, H));
		AddHit(G, BP, BS, Entries[i].Action, NAME_None, Entries[i].Index);
	}
	return Layer + 6;
}

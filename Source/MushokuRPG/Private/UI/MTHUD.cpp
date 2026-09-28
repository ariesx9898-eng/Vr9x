#include "UI/MTHUD.h"
#include "Engine/GameViewportClient.h"
#include "UI/LaPlace/SMTHudOverlay.h"
#include "Abilities/MTAbility.h"
#include "Abilities/MTAbilityComponent.h"
#include "Character/MTAttributeComponent.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTPlayerCharacter.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameEvents.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Quests/MTInteractableComponent.h"
#include "Quests/MTQuestSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "MTHUD"

// ============================================================================ Static helpers

void AMTHUD::UpdateTrail(float Current, float& Trail, float DeltaSeconds)
{
	if (Current >= Trail)
	{
		Trail = Current;
	}
	else
	{
		Trail = FMath::Max(Current, FMath::FInterpTo(Trail, Current, DeltaSeconds, 2.2f) - DeltaSeconds * 0.05f);
	}
}

FString AMTHUD::FormatCooldown(float Seconds)
{
	return Seconds >= 10.f ? FString::Printf(TEXT("%d"), FMath::CeilToInt(Seconds)) : FString::Printf(TEXT("%.1f"), Seconds);
}

// ============================================================================ Lifecycle

AMTHUD::AMTHUD()
{
}

void AMTHUD::BeginPlay()
{
	Super::BeginPlay();

	TagAwakened = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Awakened")), false);
	TagForesight = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Foresight")), false);

	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->OnNotification.AddDynamic(this, &AMTHUD::HandleNotification);
	}

	// The hotbar and vitals are Slate (SMTHudOverlay), layered above the canvas so nothing can cover them.
	if (GEngine && GEngine->GameViewport && PlayerOwner)
	{
		SlateHud = SNew(SMTHudOverlay).PlayerController(PlayerOwner);
		SlateHudContainer = SlateHud;
		GEngine->GameViewport->AddViewportWidgetContent(SlateHudContainer.ToSharedRef(), 20);
	}
}

void AMTHUD::SetSlateHudVisible(bool bVisible)
{
	if (SlateHud.IsValid())
	{
		SlateHud->SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
}

void AMTHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SlateHudContainer.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(SlateHudContainer.ToSharedRef());
	}
	SlateHudContainer.Reset();
	SlateHud.Reset();
	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->OnNotification.RemoveDynamic(this, &AMTHUD::HandleNotification);
	}
	Super::EndPlay(EndPlayReason);
}

void AMTHUD::HandleNotification(FText Message, FLinearColor Color)
{
	FToast Toast;
	Toast.Message = Message;
	Toast.Color = Color;
	Toasts.Insert(Toast, 0);
	if (Toasts.Num() > 6)
	{
		Toasts.SetNum(6);
	}
}

// ============================================================================ Public API

void AMTHUD::ToggleMenu(EMTMenuPage Page)
{
	if (Page == EMTMenuPage::None || (OpenPage == Page && !bDialogOpen))
	{
		CloseAll();
		return;
	}
	bDialogOpen = false;
	OpenPageInternal(Page);
}

void AMTHUD::CloseAll()
{
	FinishRollAnimation();
	OpenPage = EMTMenuPage::None;
	bDialogOpen = false;
	DialogNpcId = NAME_None;
	DialogQuestId = NAME_None;
	FocusedButton = NAME_None;
}

void AMTHUD::MenuNavigate(FIntPoint Direction)
{
	if (!IsMenuOpen() || Buttons.Num() == 0 || (Direction.X == 0 && Direction.Y == 0))
	{
		return;
	}
	const FHudButton* Current = Buttons.FindByPredicate([this](const FHudButton& B) { return B.Id == FocusedButton; });
	if (!Current)
	{
		for (const FHudButton& B : Buttons)
		{
			if (B.bEnabled)
			{
				FocusedButton = B.Id;
				break;
			}
		}
		return;
	}

	const FVector2D From = Current->Pos + Current->Size * 0.5;
	const FVector2D Dir = FVector2D(static_cast<double>(Direction.X), static_cast<double>(Direction.Y)).GetSafeNormal();
	double BestScore = TNumericLimits<double>::Max();
	FName Best = NAME_None;
	for (const FHudButton& B : Buttons)
	{
		if (!B.bEnabled || B.Id == FocusedButton)
		{
			continue;
		}
		const FVector2D Delta = (B.Pos + B.Size * 0.5) - From;
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
			Best = B.Id;
		}
	}
	if (!Best.IsNone())
	{
		FocusedButton = Best;
	}
}

void AMTHUD::MenuConfirm()
{
	if (bRollAnimating)
	{
		FinishRollAnimation();
		return;
	}
	const FHudButton* Focused = Buttons.FindByPredicate([this](const FHudButton& B) { return B.Id == FocusedButton; });
	if (Focused && Focused->bEnabled)
	{
		HandleButton(FocusedButton);
	}
}

void AMTHUD::MenuBack()
{
	if (bRollAnimating)
	{
		FinishRollAnimation();
		return;
	}
	if (bDialogOpen)
	{
		bDialogOpen = false;
		DialogNpcId = NAME_None;
		FocusedButton = NAME_None;
		return;
	}
	CloseAll();
}

void AMTHUD::MenuNextTab(int32 Dir)
{
	if (bDialogOpen || OpenPage == EMTMenuPage::None)
	{
		return;
	}
	const int32 First = static_cast<int32>(EMTMenuPage::Character);
	const int32 Count = static_cast<int32>(EMTMenuPage::Roll) - First + 1;
	const int32 Current = static_cast<int32>(OpenPage) - First;
	const int32 Next = ((Current + (Dir >= 0 ? 1 : -1)) % Count + Count) % Count;
	OpenPageInternal(static_cast<EMTMenuPage>(First + Next));
}

void AMTHUD::OpenNPCDialog(FName NpcId, const FText& NpcName)
{
	FinishRollAnimation();
	OpenPage = EMTMenuPage::None;
	bDialogOpen = true;
	DialogNpcId = NpcId;
	DialogNpcName = NpcName.IsEmpty() ? FText::FromName(NpcId) : NpcName;
	DialogQuestId = NAME_None;
	FocusedButton = NAME_None;
}

void AMTHUD::AddDamageNumber(const FVector& WorldLocation, float Amount, EMTElement Element, bool bHeavy)
{
	FDamageNumber Number;
	Number.World = WorldLocation;
	Number.Amount = Amount;
	Number.Color = Element == EMTElement::None ? Parchment() : MTUtil::ElementColor(Element);
	Number.bHeavy = bHeavy;
	Number.Life = bHeavy ? 1.35f : 1.05f;
	Number.Drift = FMath::FRandRange(-1.f, 1.f);
	DamageNumbers.Add(Number);
	if (DamageNumbers.Num() > 60)
	{
		DamageNumbers.RemoveAt(0, DamageNumbers.Num() - 60);
	}
}

void AMTHUD::ShowCenterMessage(const FText& Text, float Duration)
{
	CenterText = Text;
	CenterAge = 0.f;
	CenterDuration = FMath::Max(0.3f, Duration);
}

void AMTHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);

	if (bRollAnimating)
	{
		// Any click skips the reveal; clicking the roll button again must not start another roll.
		FinishRollAnimation();
		if (BoxName.ToString().StartsWith(TEXT("Roll")))
		{
			return;
		}
	}
	FocusedButton = BoxName;
	HandleButton(BoxName);
}

// ============================================================================ Frame

void AMTHUD::TickTransient(float DeltaSeconds)
{
	for (int32 i = DamageNumbers.Num() - 1; i >= 0; --i)
	{
		DamageNumbers[i].Age += DeltaSeconds;
		if (DamageNumbers[i].Age >= DamageNumbers[i].Life)
		{
			DamageNumbers.RemoveAt(i);
		}
	}
	for (int32 i = Toasts.Num() - 1; i >= 0; --i)
	{
		Toasts[i].Age += DeltaSeconds;
		if (Toasts[i].Age >= Toasts[i].Life)
		{
			Toasts.RemoveAt(i);
		}
	}
	CenterAge += DeltaSeconds;
	RollRevealAge += DeltaSeconds;
	if (bRollAnimating)
	{
		RollAnimTime += DeltaSeconds;
		if (RollAnimTime >= RollAnimDuration)
		{
			FinishRollAnimation();
		}
	}
}

void AMTHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	// Real time so menus animate while the game is paused.
	const double Now = FPlatformTime::Seconds();
	FrameDelta = LastDrawSeconds > 0.0 ? FMath::Clamp(static_cast<float>(Now - LastDrawSeconds), 0.f, 0.1f) : 0.f;
	LastDrawSeconds = Now;
	HudTime += FrameDelta;
	UIScale = FMath::Clamp(Canvas->ClipY / 1080.f, 0.6f, 2.5f);
	if (!FMath::IsNearlyEqual(FitCacheScale, UIScale) || FitCache.Num() > 512)
	{
		FitCache.Reset();
		FitCacheScale = UIScale;
	}

	Buttons.Reset();
	HitBoxPriority = 0;
	bMouseValid = false;
	if (PlayerOwner)
	{
		float MX = 0.f;
		float MY = 0.f;
		if (PlayerOwner->GetMousePosition(MX, MY))
		{
			MousePos = FVector2D(MX, MY);
			bMouseValid = true;
		}
	}

	TickTransient(FrameDelta);

	AMTCharacterBase* Char = Cast<AMTCharacterBase>(GetOwningPawn());
	UpdateWorldScan(Char);

	if (Char && !bGameplayHudHidden)
	{
		DrawStateOverlays(Char);
		SetSlateHudVisible(!IsMenuOpen());
		if (!IsMenuOpen())
		{
			DrawLockOn(Char);
			DrawDamageNumbers();
			if (!SlateHud.IsValid())
			{
				DrawVitals(Char);
				DrawHotbar(Char);
			}
			DrawTopCenter(Char);
			DrawMinimap(Char);
			DrawInteractionPrompt(Char);
		}
	}

	if (OpenPage != EMTMenuPage::None)
	{
		DrawMenu();
	}
	if (bDialogOpen)
	{
		DrawNPCDialog();
	}
	DrawCenterMessage();
	DrawToasts();

	// Keep keyboard focus valid for the buttons that exist this frame.
	if (IsMenuOpen() && !FocusedButton.IsNone() && !Buttons.ContainsByPredicate([this](const FHudButton& B) { return B.Id == FocusedButton; }))
	{
		FocusedButton = NAME_None;
	}
}

// ============================================================================ World scan

void AMTHUD::UpdateWorldScan(AMTCharacterBase* Char)
{
	NearbyHostiles.Reset();
	NearestBoss.Reset();
	UWorld* World = GetWorld();
	if (!Char || !World)
	{
		NpcMarkers.Reset();
		return;
	}

	// Quest-giver markers ("!" offer, "?" turn in), refreshed twice a second.
	NpcMarkerRefresh -= FrameDelta;
	if (NpcMarkerRefresh <= 0.f)
	{
		NpcMarkerRefresh = 0.5f;
		NpcMarkers.Reset();
		for (TObjectIterator<UMTQuestGiverComponent> It; It; ++It)
		{
			UMTQuestGiverComponent* Giver = *It;
			if (!IsValid(Giver) || Giver->IsTemplate() || Giver->GetWorld() != World || !Giver->GetOwner())
			{
				continue;
			}
			bool bTurnIn = false;
			if (Giver->HasQuestIndicator(bTurnIn))
			{
				FNpcMarker Marker;
				Marker.Actor = Giver->GetOwner();
				Marker.bTurnIn = bTurnIn;
				NpcMarkers.Add(Marker);
			}
		}
	}

	const FVector PlayerLoc = Char->GetActorLocation();
	const double RangeSq = FMath::Square(static_cast<double>(MinimapRange));
	double BestBossSq = FMath::Square(static_cast<double>(BossBarRange));

	for (TActorIterator<AMTCharacterBase> It(World); It; ++It)
	{
		AMTCharacterBase* Other = *It;
		if (!Other || Other == Char || !Other->IsAlive())
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(PlayerLoc, Other->GetActorLocation());
		if (Char->IsHostileTo(Other))
		{
			if (DistSq <= RangeSq)
			{
				NearbyHostiles.Add(Other);
			}
			const UMTAttributeComponent* Attr = Other->GetAttributes();
			if (Attr && Attr->bCrowdControlImmune && DistSq < BestBossSq)
			{
				BestBossSq = DistSq;
				NearestBoss = Other;
			}
		}
	}
}

// ============================================================================ Vitals

void AMTHUD::DrawVitals(AMTCharacterBase* Char)
{
	UMTAttributeComponent* Attr = Char->GetAttributes();
	if (!Attr)
	{
		return;
	}
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);

	const float PanelW = Sc(370.f);
	const float PanelH = Sc(150.f);
	const float X = Sc(24.f);
	const float Y = Canvas->ClipY - Sc(24.f) - PanelH;
	DrawPanelBox(X, Y, PanelW, PanelH, 0.72f, false);

	const FMTCharacterData* Data = Char->GetCharacterData();
	const FString Name = (Data && !Data->DisplayName.IsEmpty()) ? Data->DisplayName.ToString() : Char->GetCharacterId().ToString();
	DrawStr(Name, X + Sc(12.f), Y + Sc(8.f), Gold(), MediumFont(), UIScale * 0.8f);
	if (Prog)
	{
		DrawStr(FString::Printf(TEXT("Lv %d"), Prog->GetLevel()), X + PanelW - Sc(12.f), Y + Sc(8.f), Parchment(), MediumFont(), UIScale * 0.8f, 1.f);

		// Build line: race + elements.
		FString Build = Prog->GetRaceDisplayName(Char->GetRace()).ToString();
		for (int32 SlotIndex = 0; SlotIndex < UMTProgressionSubsystem::MaxElementSlots; ++SlotIndex)
		{
			const EMTElement E = Char->GetElementInSlot(SlotIndex);
			if (E != EMTElement::None)
			{
				Build += TEXT("  |  ") + MTUtil::ElementToString(E);
			}
		}
		DrawStr(Build, X + Sc(12.f), Y + Sc(32.f), Dim(), SmallFont(), UIScale);
	}

	const float HP = SafeFraction(Attr->GetHealth(), Attr->GetMaxHealth());
	const float MP = SafeFraction(Attr->GetMana(), Attr->GetMaxMana());
	const float SP = SafeFraction(Attr->GetStamina(), Attr->GetMaxStamina());
	const float PP = SafeFraction(Attr->GetPoise(), Attr->GetMaxPoise());
	UpdateTrail(HP, HealthTrail, FrameDelta);
	UpdateTrail(MP, ManaTrail, FrameDelta);
	UpdateTrail(SP, StaminaTrail, FrameDelta);

	const float BarX = X + Sc(12.f);
	const float BarW = PanelW - Sc(24.f);
	float BY = Y + Sc(52.f);

	DrawMeter(BarX, BY, BarW, Sc(16.f), HP, HealthTrail, Health(), FLinearColor(1.f, 0.85f, 0.6f, 0.85f));
	DrawStr(FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Attr->GetHealth()), FMath::RoundToInt(Attr->GetMaxHealth())),
		BarX + BarW * 0.5f, BY + Sc(8.f), Parchment(), SmallFont(), UIScale * 0.9f, 0.5f, 0.5f);
	BY += Sc(21.f);

	DrawMeter(BarX, BY, BarW, Sc(12.f), MP, ManaTrail, Mana(), FLinearColor(0.7f, 0.85f, 1.f, 0.7f));
	DrawStr(FString::Printf(TEXT("%d / %d"), FMath::FloorToInt(Attr->GetMana()), FMath::RoundToInt(Attr->GetMaxMana())),
		BarX + BarW * 0.5f, BY + Sc(6.f), Parchment(), SmallFont(), UIScale * 0.8f, 0.5f, 0.5f);
	BY += Sc(17.f);

	DrawMeter(BarX, BY, BarW, Sc(8.f), SP, StaminaTrail, Stamina(), FLinearColor(1.f, 1.f, 0.8f, 0.6f));
	BY += Sc(13.f);

	// Poise (thin, only meaningful when damaged).
	DrawMeter(BarX, BY, BarW * 0.5f, Sc(4.f), PP, PP, Poise(PP < 0.35f ? 1.f : 0.75f), Poise());
	DrawStr(TEXT("POISE"), BarX + BarW * 0.5f + Sc(6.f), BY + Sc(2.f), Dim(), SmallFont(), UIScale * 0.7f, 0.f, 0.5f);

	// Awakening meter (0..100).
	const float Meter = FMath::Clamp(Attr->GetAwakeningMeter() / 100.f, 0.f, 1.f);
	const float AwX = BarX + BarW * 0.62f;
	const float AwW = BarW * 0.38f;
	const bool bFull = Meter >= 0.999f;
	const float Pulse = bFull ? 0.65f + 0.35f * FMath::Sin(HudTime * 5.f) : 1.f;
	DrawMeter(AwX, BY - Sc(2.f), AwW, Sc(8.f), Meter, Meter, Awakening(Pulse), Awakening());
	DrawStr(bFull ? TEXT("AWAKEN READY") : FString::Printf(TEXT("AWAKEN %d%%"), FMath::FloorToInt(Meter * 100.f)),
		AwX + AwW, BY - Sc(4.f), bFull ? Awakening() : Dim(), SmallFont(), UIScale * 0.7f, 1.f, 1.f);

	// Status effects as small chips above the panel.
	float ChipX = X;
	const float ChipY = Y - Sc(24.f);
	int32 Shown = 0;
	for (const FMTStatusEffect& Effect : Attr->GetStatusEffects())
	{
		if (Shown >= 6 || Effect.Id.IsNone())
		{
			break;
		}
		const FString Label = Effect.Remaining > 0.f
			? FString::Printf(TEXT("%s %ds"), *Effect.Id.ToString(), FMath::CeilToInt(Effect.Remaining))
			: Effect.Id.ToString();
		const float LabelW = MeasureStr(Label, SmallFont(), UIScale * 0.8f).X + Sc(12.f);
		FillRect(ChipX, ChipY, LabelW, Sc(18.f), PanelColor(0.75f));
		StrokeRect(ChipX, ChipY, LabelW, Sc(18.f), Gold(0.5f));
		DrawStr(Label, ChipX + Sc(6.f), ChipY + Sc(9.f), Parchment(), SmallFont(), UIScale * 0.8f, 0.f, 0.5f);
		ChipX += LabelW + Sc(4.f);
		++Shown;
	}
}

// ============================================================================ Hotbar

void AMTHUD::DrawHotbar(AMTCharacterBase* Char)
{
	UMTAbilityComponent* Abilities = Char->GetAbilities();
	if (!Abilities)
	{
		return;
	}
	const UMTAttributeComponent* Attr = Char->GetAttributes();
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	const int32 UnlockedSlots = Prog ? Prog->GetUnlockedElementSlots() : 2;
	const UMTAbility* Active = Abilities->GetActiveAbility();

	const int32 Count = static_cast<int32>(EMTAbilitySlot::MAX);
	const float Size = Sc(54.f);
	const float Gap = Sc(5.f);
	const float GroupGap = Sc(16.f);
	auto StartsGroup = [](int32 Index)
	{
		return Index == static_cast<int32>(EMTAbilitySlot::ElementA1)
			|| Index == static_cast<int32>(EMTAbilitySlot::ElementB1)
			|| Index == static_cast<int32>(EMTAbilitySlot::RaceActive);
	};

	const float TotalW = Count * Size + (Count - 1) * Gap + 3.f * GroupGap;
	float X = (Canvas->ClipX - TotalW) * 0.5f;
	// Keep clear of the vitals panel on narrow screens.
	X = FMath::Max(X, Sc(24.f) + Sc(370.f) + Sc(16.f));
	const float Y = Canvas->ClipY - Sc(28.f) - Size;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (StartsGroup(Index))
		{
			X += GroupGap;
		}
		const EMTAbilitySlot Slot = static_cast<EMTAbilitySlot>(Index);
		const bool bSlotB = Slot == EMTAbilitySlot::ElementB1 || Slot == EMTAbilitySlot::ElementB2 || Slot == EMTAbilitySlot::ElementB3;
		const bool bLocked = bSlotB && UnlockedSlots < 2;

		const FName Id = Abilities->GetSlotAbilityId(Slot);
		const FMTAbilityData* Data = Id.IsNone() ? nullptr : Abilities->GetSlotData(Slot);

		FillRect(X, Y, Size, Size, PanelColor(0.8f));
		FLinearColor Border = Gold(0.35f);

		if (Data)
		{
			const FLinearColor ElemColor = Data->Element == EMTElement::None ? Gold() : MTUtil::ElementColor(Data->Element);
			Border = WithAlpha(ElemColor, 0.85f);
			FillRect(X, Y + Size - Sc(4.f), Size, Sc(4.f), WithAlpha(ElemColor, 0.8f));

			const FString Short = FitStr(Data->DisplayName.IsEmpty() ? Id.ToString() : Data->DisplayName.ToString(), Size - Sc(6.f), SmallFont(), UIScale * 0.78f);
			DrawStr(Short, X + Size * 0.5f, Y + Size * 0.55f, Parchment(), SmallFont(), UIScale * 0.78f, 0.5f, 0.5f);

			const UMTAbility* Instance = Abilities->GetSlotAbility(Slot);
			const float Cost = Instance ? Instance->GetEffectiveManaCost() : Data->ManaCost;
			const bool bNoMana = Attr && Cost > 0.f && Attr->GetMana() + KINDA_SMALL_NUMBER < Cost;
			const bool bNoMeter = Attr && Data->AwakeningMeterCost > 0.f && Attr->GetAwakeningMeter() + KINDA_SMALL_NUMBER < Data->AwakeningMeterCost;
			if (bNoMana || bNoMeter)
			{
				FillRect(X, Y, Size, Size, FLinearColor(0.15f, 0.15f, 0.18f, 0.6f));
			}

			// Vertical cooldown sweep (fraction = remaining / duration) + seconds.
			const float Remaining = Abilities->GetCooldownRemaining(Id);
			if (Remaining > 0.f)
			{
				const float Frac = FMath::Clamp(Abilities->GetCooldownFraction(Id), 0.f, 1.f);
				FillRect(X, Y, Size, Size * Frac, FLinearColor(0.f, 0.f, 0.f, 0.62f));
				DrawStr(FormatCooldown(Remaining), X + Size * 0.5f, Y + Size * 0.5f, Parchment(), MediumFont(), UIScale * 0.7f, 0.5f, 0.5f);
			}

			if (Cost > 0.f)
			{
				DrawStr(FString::FromInt(FMath::RoundToInt(Cost)), X + Size - Sc(3.f), Y + Size - Sc(5.f),
					bNoMana ? Bad() : FLinearColor(0.55f, 0.75f, 1.f), SmallFont(), UIScale * 0.7f, 1.f, 1.f);
			}

			// Active ability highlight + charge bar.
			const bool bIsActive = Active && (Active->GetAbilityId() == Id || Active->GetAbilityId() == Abilities->ResolveOverride(Id));
			if (bIsActive)
			{
				StrokeRect(X - Sc(2.f), Y - Sc(2.f), Size + Sc(4.f), Size + Sc(4.f), Gold(), Sc(2.f));
				if (Active->GetPhase() == EMTAbilityPhase::Anticipation && Active->GetData().bChargeable)
				{
					const float Charge = FMath::Clamp(Active->GetChargeAlpha(), 0.f, 1.f);
					const bool bMax = Charge >= 0.999f;
					DrawMeter(X - Sc(8.f), Y - Sc(16.f), Size + Sc(16.f), Sc(7.f), Charge, Charge,
						bMax ? Gold(0.75f + 0.25f * FMath::Sin(HudTime * 14.f)) : ElemColor, Gold());
					DrawStr(bMax ? TEXT("MAX") : TEXT("CHARGE"), X + Size * 0.5f, Y - Sc(18.f), bMax ? Gold() : Parchment(0.8f), SmallFont(), UIScale * 0.65f, 0.5f, 1.f);
				}
			}
		}
		else if (bLocked)
		{
			FillRect(X, Y, Size, Size, FLinearColor(0.f, 0.f, 0.f, 0.45f));
			DrawStr(FString::Printf(TEXT("Lv %d"), UMTProgressionSubsystem::SecondElementSlotLevel), X + Size * 0.5f, Y + Size * 0.55f,
				Dim(), SmallFont(), UIScale * 0.8f, 0.5f, 0.5f);
		}

		StrokeRect(X, Y, Size, Size, Border, FMath::Max(1.f, Sc(1.2f)));
		DrawStr(UMTAbilityComponent::SlotToKeyLabel(Slot), X + Sc(3.f), Y + Sc(2.f), Gold(0.95f), SmallFont(), UIScale * 0.72f);

		X += Size + Gap;
	}
}

// ============================================================================ Top centre: boss bar + quest tracker

void AMTHUD::DrawTopCenter(AMTCharacterBase* Char)
{
	AMTCharacterBase* Boss = nullptr;
	if (AMTCharacterBase* Locked = Cast<AMTCharacterBase>(Char->GetLockTarget()))
	{
		const UMTAttributeComponent* Attr = Locked->GetAttributes();
		if (Attr && Attr->bCrowdControlImmune && Locked->IsAlive())
		{
			Boss = Locked;
		}
	}
	if (!Boss)
	{
		Boss = NearestBoss.Get();
	}

	float TopY = Sc(18.f);
	if (Boss)
	{
		TopY = DrawBossBar(Boss, TopY) + Sc(12.f);
	}
	else
	{
		BossTarget.Reset();
	}
	DrawQuestTracker(TopY);
}

float AMTHUD::DrawBossBar(AMTCharacterBase* Boss, float TopY)
{
	const UMTAttributeComponent* Attr = Boss->GetAttributes();
	if (!Attr)
	{
		return TopY;
	}
	if (BossTarget.Get() != Boss)
	{
		BossTarget = Boss;
		BossTrail = SafeFraction(Attr->GetHealth(), Attr->GetMaxHealth());
	}

	// Name: enemy row display name -> lineage display name -> GameplayId.
	FString Name = Boss->GameplayId.ToString();
	if (const UMTDataRegistry* Registry = UMTDataRegistry::Get(this))
	{
		if (const FMTEnemyData* Enemy = Registry->FindEnemy(Boss->GameplayId))
		{
			Name = Enemy->DisplayName.IsEmpty() ? Name : Enemy->DisplayName.ToString();
		}
		else if (const FMTCharacterData* Lineage = Boss->GetCharacterData())
		{
			Name = Lineage->DisplayName.IsEmpty() ? Name : Lineage->DisplayName.ToString();
		}
	}
	if (Name.IsEmpty() || Name == TEXT("None"))
	{
		Name = Boss->GetName();
	}

	const float W = FMath::Min(Sc(620.f), Canvas->ClipX * 0.6f);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float HP = SafeFraction(Attr->GetHealth(), Attr->GetMaxHealth());
	UpdateTrail(HP, BossTrail, FrameDelta);

	DrawStr(Name.ToUpper(), Canvas->ClipX * 0.5f, TopY, Gold(), MediumFont(), UIScale * 0.8f, 0.5f, 0.f);
	const float BarY = TopY + LineHeight(MediumFont(), UIScale * 0.8f) + Sc(2.f);
	DrawMeter(X, BarY, W, Sc(14.f), HP, BossTrail, FLinearColor(0.72f, 0.12f, 0.16f), FLinearColor(1.f, 0.8f, 0.55f, 0.85f));
	StrokeRect(X - Sc(2.f), BarY - Sc(2.f), W + Sc(4.f), Sc(18.f), Gold(0.8f));
	const float PP = SafeFraction(Attr->GetPoise(), Attr->GetMaxPoise());
	DrawMeter(X, BarY + Sc(19.f), W, Sc(4.f), PP, PP, Poise(0.8f), Poise());
	return BarY + Sc(24.f);
}

void AMTHUD::DrawQuestTracker(float TopY)
{
	UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this);
	if (!Quests)
	{
		return;
	}
	FName QuestId = Quests->GetTrackedQuest();
	if (QuestId.IsNone())
	{
		const TArray<FName> Active = Quests->GetActiveQuestIds();
		if (Active.Num() == 0)
		{
			return;
		}
		QuestId = Active[0];
	}

	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTQuestData* Quest = Registry ? Registry->FindQuest(QuestId) : nullptr;
	const FMTQuestSaveState* State = Quests->GetQuestState(QuestId);

	struct FLine { FString Text; bool bDone; };
	TArray<FLine> Lines;
	if (Quest)
	{
		for (int32 i = 0; i < Quest->Objectives.Num(); ++i)
		{
			if (Quest->bSequentialObjectives && State && i > State->Stage)
			{
				break; // future stages stay hidden
			}
			const bool bDone = Quests->IsObjectiveComplete(QuestId, i);
			Lines.Add({ Quests->GetObjectiveText(QuestId, i).ToString(), bDone });
		}
	}

	const float W = Sc(460.f);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float TitleH = LineHeight(MediumFont(), UIScale * 0.75f);
	const float LineH = LineHeight(SmallFont(), UIScale);
	const float H = Sc(14.f) + TitleH + Lines.Num() * (LineH + Sc(3.f)) + Sc(6.f);
	DrawPanelBox(X, TopY, W, H, 0.6f, false);

	DrawStr(GetQuestTitle(QuestId), X + W * 0.5f, TopY + Sc(6.f), Gold(), MediumFont(), UIScale * 0.75f, 0.5f, 0.f);
	float LY = TopY + Sc(10.f) + TitleH;
	for (const FLine& Line : Lines)
	{
		const float Box = Sc(9.f);
		StrokeRect(X + Sc(14.f), LY + (LineH - Box) * 0.5f, Box, Box, Gold(0.8f));
		if (Line.bDone)
		{
			FillRect(X + Sc(16.f), LY + (LineH - Box) * 0.5f + Sc(2.f), Box - Sc(4.f), Box - Sc(4.f), Good());
		}
		DrawStr(FitStr(Line.Text, W - Sc(40.f), SmallFont(), UIScale), X + Sc(30.f), LY, Line.bDone ? Dim() : Parchment(), SmallFont(), UIScale);
		LY += LineH + Sc(3.f);
	}
}

// ============================================================================ Minimap (north-up radar)

void AMTHUD::DrawMinimap(AMTCharacterBase* Char)
{
	const float R = Sc(95.f);
	const FVector2D C(Canvas->ClipX - Sc(28.f) - R, Sc(28.f) + R);
	const FVector PlayerLoc = Char->GetActorLocation();
	const float Scale = R / FMath::Max(1000.f, MinimapRange);

	// World +X is north (screen up), +Y is east (screen right).
	auto ToMap = [&](const FVector& World, bool bClampToEdge, bool& bOutInside) -> FVector2D
	{
		const FVector Delta = World - PlayerLoc;
		FVector2D P(Delta.Y * Scale, -Delta.X * Scale);
		const double Len = P.Size();
		bOutInside = Len <= R - Sc(4.f);
		if (!bOutInside && bClampToEdge && Len > KINDA_SMALL_NUMBER)
		{
			P *= (R - Sc(7.f)) / Len;
		}
		return C + P;
	};

	FillDisc(C, R, PanelColor(0.72f));
	StrokeCircle(C, R * 0.333f, Gold(0.12f), 1.f, 40);
	StrokeCircle(C, R * 0.666f, Gold(0.12f), 1.f, 48);
	FillRect(static_cast<float>(C.X) - R, static_cast<float>(C.Y), 2.f * R, 1.f, Gold(0.08f));
	FillRect(static_cast<float>(C.X), static_cast<float>(C.Y) - R, 1.f, 2.f * R, Gold(0.08f));
	StrokeCircle(C, R, Gold(0.9f), FMath::Max(1.5f, Sc(2.f)), 64);
	DrawStr(TEXT("N"), static_cast<float>(C.X), static_cast<float>(C.Y) - R - Sc(2.f), Gold(), SmallFont(), UIScale, 0.5f, 1.f);

	// Discovered locations (fast travel points drawn as hollow diamonds).
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const UMTProgressionSubsystem* Prog = UMTProgressionSubsystem::Get(this);
	if (Registry && Prog)
	{
		for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
		{
			if (!Prog->IsLocationDiscovered(Pair.Key))
			{
				continue;
			}
			bool bInside = false;
			const FVector2D P = ToMap(Pair.Value.WorldLocation, false, bInside);
			if (bInside)
			{
				DrawDiamondShape(P, Sc(4.f), Pair.Value.bFastTravel ? FLinearColor(0.45f, 0.85f, 1.f) : Parchment(0.7f), !Pair.Value.bFastTravel);
			}
		}
	}

	// Enemies.
	for (const TWeakObjectPtr<AMTCharacterBase>& Weak : NearbyHostiles)
	{
		const AMTCharacterBase* Enemy = Weak.Get();
		if (!Enemy)
		{
			continue;
		}
		bool bInside = false;
		const FVector2D P = ToMap(Enemy->GetActorLocation(), false, bInside);
		if (!bInside)
		{
			continue;
		}
		const UMTAttributeComponent* Attr = Enemy->GetAttributes();
		const bool bBoss = Attr && Attr->bCrowdControlImmune;
		const float Dot = bBoss ? Sc(4.5f) : Sc(3.f);
		FillRect(static_cast<float>(P.X) - Dot, static_cast<float>(P.Y) - Dot, Dot * 2.f, Dot * 2.f, bBoss ? FLinearColor(1.f, 0.35f, 0.1f) : FLinearColor(0.95f, 0.2f, 0.2f));
	}

	// Quest givers.
	for (const FNpcMarker& Marker : NpcMarkers)
	{
		const AActor* Npc = Marker.Actor.Get();
		if (!Npc)
		{
			continue;
		}
		bool bInside = false;
		const FVector2D P = ToMap(Npc->GetActorLocation(), true, bInside);
		DrawStr(Marker.bTurnIn ? TEXT("?") : TEXT("!"), static_cast<float>(P.X), static_cast<float>(P.Y), Marker.bTurnIn ? Good() : Gold(), MediumFont(), UIScale * 0.7f, 0.5f, 0.5f);
	}

	// Quest objective markers (tracked quest bright, other active quests dim), clamped to the edge.
	if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
	{
		const FName Tracked = Quests->GetTrackedQuest();
		for (const FName& QuestId : Quests->GetActiveQuestIds())
		{
			FVector MarkerLoc;
			if (!Quests->GetObjectiveMarker(QuestId, MarkerLoc))
			{
				continue;
			}
			bool bInside = false;
			const FVector2D P = ToMap(MarkerLoc, true, bInside);
			const bool bTracked = QuestId == Tracked;
			DrawDiamondShape(P, bTracked ? Sc(6.f) : Sc(4.5f), bTracked ? Gold() : Gold(0.5f), true);
		}
	}

	// Player arrow (pawn facing).
	const float Yaw = FMath::DegreesToRadians(Char->GetActorRotation().Yaw);
	const FVector2D Fwd(FMath::Sin(Yaw), -FMath::Cos(Yaw));
	const FVector2D Right(-Fwd.Y, Fwd.X);
	const FVector2D Tip = C + Fwd * Sc(9.f);
	const FVector2D L = C - Fwd * Sc(6.f) - Right * Sc(5.f);
	const FVector2D Rr = C - Fwd * Sc(6.f) + Right * Sc(5.f);
	const FVector2D Notch = C - Fwd * Sc(2.5f);
	const float T = FMath::Max(1.5f, Sc(2.f));
	DrawLine(Tip.X, Tip.Y, L.X, L.Y, Parchment(), T);
	DrawLine(Tip.X, Tip.Y, Rr.X, Rr.Y, Parchment(), T);
	DrawLine(L.X, L.Y, Notch.X, Notch.Y, Parchment(), T);
	DrawLine(Rr.X, Rr.Y, Notch.X, Notch.Y, Parchment(), T);
}

// ============================================================================ Lock-on

void AMTHUD::DrawLockOn(AMTCharacterBase* Char)
{
	AActor* Target = Char->GetLockTarget();
	if (!Target)
	{
		return;
	}
	AMTCharacterBase* TargetChar = Cast<AMTCharacterBase>(Target);
	const FVector Aim = Target->GetActorLocation() + FVector(0.f, 0.f, TargetChar ? 20.f : 0.f);
	FVector2D S;
	if (!ProjectToScreen(Aim, S))
	{
		return;
	}

	const UMTAttributeComponent* Attr = TargetChar ? TargetChar->GetAttributes() : nullptr;
	const bool bBoss = Attr && Attr->bCrowdControlImmune;
	const FLinearColor Color = bBoss ? FLinearColor(1.f, 0.45f, 0.25f, 0.95f) : Gold(0.95f);
	const float R = Sc(24.f) + Sc(2.f) * FMath::Sin(HudTime * 4.f);
	const float Base = HudTime * 1.2f;
	const float T = FMath::Max(1.5f, Sc(2.f));

	FVector2D Corners[4];
	for (int32 k = 0; k < 4; ++k)
	{
		const float A = Base + k * HALF_PI + PI * 0.25f;
		Corners[k] = S + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
	}
	for (int32 k = 0; k < 4; ++k)
	{
		const FVector2D& P = Corners[k];
		const FVector2D ToNext = (Corners[(k + 1) % 4] - P) * 0.28;
		const FVector2D ToPrev = (Corners[(k + 3) % 4] - P) * 0.28;
		DrawLine(P.X, P.Y, P.X + ToNext.X, P.Y + ToNext.Y, Color, T);
		DrawLine(P.X, P.Y, P.X + ToPrev.X, P.Y + ToPrev.Y, Color, T);
	}
	FillRect(static_cast<float>(S.X) - Sc(1.5f), static_cast<float>(S.Y) - Sc(1.5f), Sc(3.f), Sc(3.f), Color);

	// Small health bar for regular targets (bosses use the top bar).
	if (Attr && !bBoss)
	{
		const float W = Sc(64.f);
		DrawMeter(static_cast<float>(S.X) - W * 0.5f, static_cast<float>(S.Y) + R + Sc(6.f), W, Sc(5.f),
			SafeFraction(Attr->GetHealth(), Attr->GetMaxHealth()), SafeFraction(Attr->GetHealth(), Attr->GetMaxHealth()), Health(), Health());
	}
}

// ============================================================================ Damage numbers

void AMTHUD::DrawDamageNumbers()
{
	for (const FDamageNumber& Number : DamageNumbers)
	{
		const float T = FMath::Clamp(Number.Age / Number.Life, 0.f, 1.f);
		FVector2D S;
		if (!ProjectToScreen(Number.World + FVector(0.f, 0.f, 40.f + 90.f * T), S))
		{
			continue;
		}
		S.X += Number.Drift * T * Sc(30.f);
		const float Pop = 1.f + 0.5f * FMath::Max(0.f, 1.f - Number.Age * 8.f);
		const float Scale = UIScale * (Number.bHeavy ? 1.1f : 0.8f) * Pop;
		const FLinearColor Color = WithAlpha(Number.Color, 1.f - T * T);
		const FString Label = FString::FromInt(FMath::Max(1, FMath::RoundToInt(Number.Amount)));
		DrawStr(Number.bHeavy ? Label + TEXT("!") : Label, static_cast<float>(S.X), static_cast<float>(S.Y), Color,
			Number.bHeavy ? LargeFont() : MediumFont(), Scale, 0.5f, 0.5f, true);
	}
}

// ============================================================================ Interaction prompt

void AMTHUD::DrawInteractionPrompt(AMTCharacterBase* Char)
{
	const AMTPlayerCharacter* Player = Cast<AMTPlayerCharacter>(Char);
	const UMTInteractableComponent* Focus = Player ? Player->GetFocusedInteractable() : nullptr;
	if (!Focus)
	{
		return;
	}
	const FString Prompt = Focus->GetPromptText().ToString();
	const FString Name = Focus->GetDisplayName().ToString();

	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.64f;
	const float KeySize = Sc(28.f);
	const float PromptW = MeasureStr(Prompt, MediumFont(), UIScale * 0.7f).X;
	const float TotalW = KeySize + Sc(10.f) + PromptW;
	const float X = CX - TotalW * 0.5f;

	if (!Name.IsEmpty())
	{
		DrawStr(Name, CX, Y - Sc(6.f), Gold(), SmallFont(), UIScale, 0.5f, 1.f);
	}
	FillRect(X - Sc(8.f), Y - Sc(4.f), TotalW + Sc(16.f), KeySize + Sc(8.f), PanelColor(0.7f));
	FillRect(X, Y, KeySize, KeySize, Parchment(0.9f));
	DrawStr(TEXT("E"), X + KeySize * 0.5f, Y + KeySize * 0.5f, PanelColor(1.f), MediumFont(), UIScale * 0.75f, 0.5f, 0.5f, false);
	DrawStr(Prompt, X + KeySize + Sc(10.f), Y + KeySize * 0.5f, Parchment(), MediumFont(), UIScale * 0.7f, 0.f, 0.5f);
}

// ============================================================================ Toasts / centre message

void AMTHUD::DrawToasts()
{
	const float W = Sc(360.f);
	const float X = Canvas->ClipX - Sc(28.f) - W;
	float Y = Sc(28.f) + Sc(190.f) + Sc(36.f); // below the minimap
	const float H = Sc(30.f);
	for (const FToast& Toast : Toasts)
	{
		const float FadeIn = FMath::Clamp(Toast.Age / 0.2f, 0.f, 1.f);
		const float FadeOut = FMath::Clamp((Toast.Life - Toast.Age) / 0.6f, 0.f, 1.f);
		const float A = FMath::Min(FadeIn, FadeOut);
		const float Slide = (1.f - FadeIn) * Sc(30.f);
		FillRect(X + Slide, Y, W, H, PanelColor(0.8f * A));
		FillRect(X + Slide, Y, Sc(4.f), H, WithAlpha(Toast.Color, A));
		StrokeRect(X + Slide, Y, W, H, Gold(0.35f * A));
		DrawStr(FitStr(Toast.Message.ToString(), W - Sc(18.f), SmallFont(), UIScale), X + Slide + Sc(12.f), Y + H * 0.5f,
			WithAlpha(Parchment(), A), SmallFont(), UIScale, 0.f, 0.5f);
		Y += H + Sc(6.f);
	}
}

void AMTHUD::DrawCenterMessage()
{
	if (CenterText.IsEmpty() || CenterAge >= CenterDuration)
	{
		return;
	}
	const float A = FMath::Min(FMath::Clamp(CenterAge / 0.15f, 0.f, 1.f), FMath::Clamp((CenterDuration - CenterAge) / 0.5f, 0.f, 1.f));
	const FString Str = CenterText.ToString();
	const FVector2D Size = MeasureStr(Str, LargeFont(), UIScale * 0.8f);
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.3f;
	FillRect(CX - Size.X * 0.5f - Sc(24.f), CY - Size.Y * 0.5f - Sc(8.f), Size.X + Sc(48.f), Size.Y + Sc(16.f), PanelColor(0.7f * A));
	FillRect(CX - Size.X * 0.5f - Sc(24.f), CY + Size.Y * 0.5f + Sc(8.f), Size.X + Sc(48.f), Sc(1.5f), Gold(0.8f * A));
	DrawStr(Str, CX, CY, WithAlpha(Parchment(), A), LargeFont(), UIScale * 0.8f, 0.5f, 0.5f);
}

// ============================================================================ Awakening / Demon Eye overlays

void AMTHUD::DrawEdgeGlow(const FLinearColor& Color, float MaxAlpha, float Thickness, int32 Bands)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	for (int32 i = 0; i < Bands; ++i)
	{
		const float A = MaxAlpha * (1.f - static_cast<float>(i) / Bands);
		const float O = i * Thickness;
		const FLinearColor C = WithAlpha(Color, A);
		FillRect(O, O, W - 2.f * O, Thickness, C);
		FillRect(O, H - O - Thickness, W - 2.f * O, Thickness, C);
		FillRect(O, O + Thickness, Thickness, H - 2.f * O - 2.f * Thickness, C);
		FillRect(W - O - Thickness, O + Thickness, Thickness, H - 2.f * O - 2.f * Thickness, C);
	}
}

void AMTHUD::DrawStateOverlays(AMTCharacterBase* Char)
{
	// Awakening: subtle pulsing border in the lineage aura colour.
	if (TagAwakened.IsValid() && Char->HasStateTag(TagAwakened))
	{
		const FMTCharacterData* Data = Char->GetCharacterData();
		const FLinearColor Aura = Data ? Data->AuraColor : Awakening();
		const float Pulse = 0.75f + 0.25f * FMath::Sin(HudTime * 2.5f);
		DrawEdgeGlow(Aura, 0.14f * Pulse, Sc(5.f), 6);
	}

	// Demon Eye: faint violet vignette confined to the edges + small eye icon. Never covers the view.
	if (TagForesight.IsValid() && Char->HasStateTag(TagForesight))
	{
		const FLinearColor Violet(0.55f, 0.35f, 0.95f, 1.f);
		DrawEdgeGlow(Violet, 0.07f, Sc(8.f), 5);

		const FVector2D C(Sc(24.f) + Sc(28.f), Canvas->ClipY - Sc(24.f) - Sc(150.f) - Sc(60.f));
		const float EyeW = Sc(22.f);
		const float EyeH = Sc(9.f);
		const float Pulse = 0.7f + 0.3f * FMath::Sin(HudTime * 3.f);
		const FLinearColor EyeColor = WithAlpha(FLinearColor(0.8f, 0.7f, 1.f), 0.9f * Pulse);
		const int32 Segments = 12;
		for (int32 i = 0; i < Segments; ++i)
		{
			const float T0 = static_cast<float>(i) / Segments;
			const float T1 = static_cast<float>(i + 1) / Segments;
			const float X0 = C.X - EyeW + 2.f * EyeW * T0;
			const float X1 = C.X - EyeW + 2.f * EyeW * T1;
			const float Y0 = EyeH * FMath::Sin(PI * T0);
			const float Y1 = EyeH * FMath::Sin(PI * T1);
			DrawLine(X0, C.Y - Y0, X1, C.Y - Y1, EyeColor, 1.5f);
			DrawLine(X0, C.Y + Y0, X1, C.Y + Y1, EyeColor, 1.5f);
		}
		StrokeCircle(C, Sc(5.f), EyeColor, 1.5f, 16);
		FillDisc(C, Sc(2.5f), EyeColor, 6);
		DrawStr(TEXT("FORESIGHT"), static_cast<float>(C.X) + EyeW + Sc(8.f), static_cast<float>(C.Y), EyeColor, SmallFont(), UIScale * 0.8f, 0.f, 0.5f);
	}
}

// ============================================================================ Drawing helpers

UFont* AMTHUD::SmallFont() const { return GEngine ? GEngine->GetSmallFont() : nullptr; }
UFont* AMTHUD::MediumFont() const { return GEngine ? GEngine->GetMediumFont() : nullptr; }
UFont* AMTHUD::LargeFont() const { return GEngine ? GEngine->GetLargeFont() : nullptr; }

void AMTHUD::FillRect(float X, float Y, float W, float H, const FLinearColor& Color)
{
	if (W > 0.f && H > 0.f && Color.A > 0.f)
	{
		DrawRect(Color, X, Y, W, H);
	}
}

void AMTHUD::StrokeRect(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	const float T = FMath::Max(1.f, Thickness);
	FillRect(X, Y, W, T, Color);
	FillRect(X, Y + H - T, W, T, Color);
	FillRect(X, Y + T, T, H - 2.f * T, Color);
	FillRect(X + W - T, Y + T, T, H - 2.f * T, Color);
}

void AMTHUD::DrawPanelBox(float X, float Y, float W, float H, float Alpha, bool bOrnate)
{
	FillRect(X, Y, W, H, PanelColor(Alpha));
	StrokeRect(X, Y, W, H, Gold(0.85f), FMath::Max(1.f, Sc(1.5f)));
	if (bOrnate)
	{
		const float Inset = Sc(5.f);
		StrokeRect(X + Inset, Y + Inset, W - 2.f * Inset, H - 2.f * Inset, Gold(0.22f), 1.f);
		const float D = Sc(5.f);
		DrawDiamondShape(FVector2D(X, Y), D, Gold(), true);
		DrawDiamondShape(FVector2D(X + W, Y), D, Gold(), true);
		DrawDiamondShape(FVector2D(X, Y + H), D, Gold(), true);
		DrawDiamondShape(FVector2D(X + W, Y + H), D, Gold(), true);
	}
}

void AMTHUD::DrawMeter(float X, float Y, float W, float H, float Fraction, float Trail, const FLinearColor& Fill, const FLinearColor& TrailColor)
{
	const float F = FMath::Clamp(Fraction, 0.f, 1.f);
	const float Tr = FMath::Clamp(Trail, 0.f, 1.f);
	FillRect(X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.55f));
	if (Tr > F)
	{
		FillRect(X + W * F, Y, W * (Tr - F), H, TrailColor);
	}
	FillRect(X, Y, W * F, H, Fill);
	FillRect(X, Y, W * F, FMath::Max(1.f, H * 0.3f), FLinearColor(1.f, 1.f, 1.f, 0.18f));
	StrokeRect(X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f);
}

FVector2D AMTHUD::MeasureStr(const FString& Str, UFont* Font, float Scale) const
{
	float W = 0.f;
	float H = 0.f;
	if (Canvas && !Str.IsEmpty())
	{
		GetTextSize(Str, W, H, Font, Scale);
	}
	return FVector2D(W, H);
}

float AMTHUD::LineHeight(UFont* Font, float Scale) const
{
	return static_cast<float>(MeasureStr(TEXT("Ag"), Font, Scale).Y);
}

void AMTHUD::DrawStr(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, float HAlign, float VAlign, bool bShadow)
{
	if (Str.IsEmpty() || Color.A <= 0.f || !Canvas)
	{
		return;
	}
	float W = 0.f;
	float H = 0.f;
	if (HAlign != 0.f || VAlign != 0.f)
	{
		GetTextSize(Str, W, H, Font, Scale);
	}
	const float DX = X - W * HAlign;
	const float DY = Y - H * VAlign;
	if (bShadow)
	{
		DrawText(Str, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.7f), DX + 1.f, DY + 1.f, Font, Scale);
	}
	DrawText(Str, Color, DX, DY, Font, Scale);
}

float AMTHUD::DrawWrapped(const FString& Str, float X, float Y, float MaxW, const FLinearColor& Color, UFont* Font, float Scale, int32 MaxLines)
{
	const float LineH = LineHeight(Font, Scale);
	TArray<FString> Paragraphs;
	Str.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	float CY = Y;
	int32 Lines = 0;
	for (const FString& Paragraph : Paragraphs)
	{
		TArray<FString> Words;
		Paragraph.ParseIntoArrayWS(Words);
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (!Line.IsEmpty() && MeasureStr(Candidate, Font, Scale).X > MaxW)
			{
				if (Lines >= MaxLines)
				{
					return CY - Y;
				}
				DrawStr(Line, X, CY, Color, Font, Scale);
				CY += LineH;
				++Lines;
				Line = Word;
			}
			else
			{
				Line = Candidate;
			}
		}
		if (Lines >= MaxLines)
		{
			break;
		}
		DrawStr(Line, X, CY, Color, Font, Scale);
		CY += LineH;
		++Lines;
	}
	return CY - Y;
}

FString AMTHUD::FitStr(const FString& Str, float MaxW, UFont* Font, float Scale)
{
	const FString Key = FString::Printf(TEXT("%s|%d|%d|%p"), *Str, FMath::RoundToInt(MaxW), FMath::RoundToInt(Scale * 100.f), Font);
	if (const FString* Cached = FitCache.Find(Key))
	{
		return *Cached;
	}
	FString Result = Str;
	if (MeasureStr(Str, Font, Scale).X > MaxW)
	{
		// Prefer the first word for multi-word ability names, then truncate.
		FString First, Rest;
		if (Str.Split(TEXT(" "), &First, &Rest) && MeasureStr(First, Font, Scale).X <= MaxW)
		{
			Result = First;
		}
		else
		{
			Result = Str;
			while (Result.Len() > 1 && MeasureStr(Result + TEXT(".."), Font, Scale).X > MaxW)
			{
				Result.LeftChopInline(1);
			}
			Result += TEXT("..");
		}
	}
	FitCache.Add(Key, Result);
	return Result;
}

bool AMTHUD::DrawButtonBox(FName Id, const FString& Label, float X, float Y, float W, float H, bool bEnabled, bool bActive, const FLinearColor& Accent)
{
	FHudButton& Button = Buttons.AddDefaulted_GetRef();
	Button.Id = Id;
	Button.Pos = FVector2D(X, Y);
	Button.Size = FVector2D(W, H);
	Button.bEnabled = bEnabled;

	const bool bHover = bMouseValid && MousePos.X >= X && MousePos.X <= X + W && MousePos.Y >= Y && MousePos.Y <= Y + H;
	const bool bFocus = FocusedButton == Id;
	if (bEnabled && !GetHitBoxWithName(Id))
	{
		AddHitBox(FVector2D(X, Y), FVector2D(W, H), Id, true, HitBoxPriority);
	}

	FLinearColor Bg = bActive ? FLinearColor(0.32f, 0.25f, 0.12f, 0.92f) : FLinearColor(0.08f, 0.09f, 0.15f, 0.92f);
	if (bEnabled && (bHover || bFocus))
	{
		Bg = bActive ? FLinearColor(0.42f, 0.33f, 0.16f, 0.95f) : FLinearColor(0.14f, 0.15f, 0.24f, 0.95f);
	}
	if (!bEnabled)
	{
		Bg.A *= 0.55f;
	}
	FillRect(X, Y, W, H, Bg);
	float TextX = X + W * 0.5f;
	if (Accent.A > 0.f)
	{
		FillRect(X, Y, Sc(4.f), H, Accent);
		TextX += Sc(2.f);
	}
	StrokeRect(X, Y, W, H, (bEnabled && (bHover || bFocus)) ? Gold() : Gold(bActive ? 0.8f : 0.35f), bFocus ? FMath::Max(1.f, Sc(2.f)) : 1.f);
	DrawStr(FitStr(Label, W - Sc(12.f), SmallFont(), UIScale), TextX, Y + H * 0.5f, bEnabled ? Parchment() : Dim(0.8f), SmallFont(), UIScale, 0.5f, 0.5f);
	return bHover;
}

void AMTHUD::StrokeCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, int32 Segments)
{
	const int32 N = FMath::Max(6, Segments);
	FVector2D Prev = Center + FVector2D(Radius, 0.f);
	for (int32 i = 1; i <= N; ++i)
	{
		const float A = 2.f * PI * i / N;
		const FVector2D P = Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius;
		DrawLine(Prev.X, Prev.Y, P.X, P.Y, Color, Thickness);
		Prev = P;
	}
}

void AMTHUD::FillDisc(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Strips)
{
	const int32 N = FMath::Max(2, Strips);
	const float StripH = 2.f * Radius / N;
	for (int32 i = 0; i < N; ++i)
	{
		const float YC = -Radius + (i + 0.5f) * StripH;
		const float Half = FMath::Sqrt(FMath::Max(0.f, Radius * Radius - YC * YC));
		FillRect(static_cast<float>(Center.X) - Half, static_cast<float>(Center.Y) - Radius + i * StripH, 2.f * Half, StripH, Color);
	}
}

void AMTHUD::DrawDiamondShape(const FVector2D& Center, float Radius, const FLinearColor& Color, bool bFilled)
{
	const float CX = static_cast<float>(Center.X);
	const float CY = static_cast<float>(Center.Y);
	if (bFilled)
	{
		// Stacked strips form a filled diamond.
		const int32 N = FMath::Max(2, FMath::RoundToInt(Radius));
		const float StripH = 2.f * Radius / N;
		for (int32 i = 0; i < N; ++i)
		{
			const float YC = -Radius + (i + 0.5f) * StripH;
			const float Half = Radius - FMath::Abs(YC);
			FillRect(CX - Half, CY - Radius + i * StripH, 2.f * Half, StripH, Color);
		}
	}
	else
	{
		DrawLine(CX, CY - Radius, CX + Radius, CY, Color, 1.5f);
		DrawLine(CX + Radius, CY, CX, CY + Radius, Color, 1.5f);
		DrawLine(CX, CY + Radius, CX - Radius, CY, Color, 1.5f);
		DrawLine(CX - Radius, CY, CX, CY - Radius, Color, 1.5f);
	}
}

bool AMTHUD::ProjectToScreen(const FVector& World, FVector2D& OutScreen)
{
	if (!Canvas || !PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return false;
	}
	const FVector CamLoc = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector CamDir = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	if (FVector::DotProduct(World - CamLoc, CamDir) <= 10.f)
	{
		return false; // behind the camera
	}
	const FVector Projected = Project(World);
	OutScreen = FVector2D(Projected.X, Projected.Y);
	return OutScreen.X > -200.0 && OutScreen.X < Canvas->ClipX + 200.0 && OutScreen.Y > -200.0 && OutScreen.Y < Canvas->ClipY + 200.0;
}

#undef LOCTEXT_NAMESPACE

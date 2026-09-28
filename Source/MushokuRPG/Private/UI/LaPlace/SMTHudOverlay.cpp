#include "UI/LaPlace/SMTHudOverlay.h"
#include "UI/LaPlace/MTUIStyle.h"
#include "Abilities/MTAbility.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Core/MTDataTypes.h"
#include "Core/MTTypes.h"
#include "Progression/MTProgressionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "World/MTRegionSubsystem.h"

const EMTAbilitySlot SMTHudOverlay::HotbarSlots[7] = {
	EMTAbilitySlot::Basic, EMTAbilitySlot::Loadout1, EMTAbilitySlot::Loadout2, EMTAbilitySlot::Loadout3, EMTAbilitySlot::Loadout4,
	EMTAbilitySlot::Special, EMTAbilitySlot::Awakening,
};

void SMTHudOverlay::Construct(const FArguments& InArgs)
{
	PlayerController = InArgs._PlayerController;
	SetVisibility(EVisibility::HitTestInvisible);
}

AMTCharacterBase* SMTHudOverlay::GetCharacter() const
{
	const APlayerController* PC = PlayerController.Get();
	return PC ? Cast<AMTCharacterBase>(PC->GetPawn()) : nullptr;
}

void SMTHudOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = InCurrentTime;
	// Announce each newly entered region (the overlay only ticks while the gameplay HUD is shown, so the region the
	// player spawns into is announced as the menu fades away).
	if (const UMTRegionSubsystem* Regions = UMTRegionSubsystem::Get(PlayerController.Get()))
	{
		const int32 Region = Regions->IsActive() ? Regions->GetCurrentRegion() : INDEX_NONE;
		if (Region != INDEX_NONE && Region != AnnouncedRegion)
		{
			AnnouncedRegion = Region;
			const FMTRegionInfo* Info = Regions->GetRegionInfo(Region);
			if (Info && Info->bBanner && !Info->Name.IsEmpty())
			{
				BannerTitle = Info->Name.ToString().ToUpper();
				BannerSubtitle = Info->Continent.ToString();
				BannerStart = InCurrentTime + 0.4;
				MTUI::Sound(TEXT("map_select"), 0.45f);
			}
		}
	}
	AMTCharacterBase* Char = GetCharacter();
	if (!Char || !Char->GetAbilities() || !Char->GetAttributes())
	{
		return;
	}
	UMTAbilityComponent* Abilities = Char->GetAbilities();
	for (int32 i = 0; i < 7; ++i)
	{
		const FName Id = Abilities->GetSlotAbilityId(HotbarSlots[i]);
		const float Remaining = Id.IsNone() ? 0.f : Abilities->GetCooldownRemaining(Id);
		if (LastCooldown[i] > 0.05f && Remaining <= 0.f)
		{
			ReadyFlashAt[i] = InCurrentTime; // just came off cooldown: brief highlight
			MTUI::Sound(TEXT("cooldown_ready"), 0.35f);
		}
		LastCooldown[i] = Remaining;
	}
	// Trailing bars ease down after damage / spending.
	const UMTAttributeComponent* Attr = Char->GetAttributes();
	const float Health = Attr->GetMaxHealth() > 0.f ? Attr->GetHealth() / Attr->GetMaxHealth() : 0.f;
	const float Mana = Attr->GetMaxMana() > 0.f ? Attr->GetMana() / Attr->GetMaxMana() : 0.f;
	HealthTrail = Health >= HealthTrail ? Health : FMath::FInterpTo(HealthTrail, Health, InDeltaTime, 2.2f);
	ManaTrail = Mana >= ManaTrail ? Mana : FMath::FInterpTo(ManaTrail, Mana, InDeltaTime, 2.2f);
}

int32 SMTHudOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	AMTCharacterBase* Char = GetCharacter();
	if (!Char || !Char->GetAbilities() || !Char->GetAttributes())
	{
		return LayerId;
	}
	int32 Layer = PaintVitals(G, Out, LayerId, Char);
	Layer = PaintHotbar(G, Out, Layer + 1, Char);
	Layer = PaintRegionBanner(G, Out, Layer + 1);
	return Layer;
}

int32 SMTHudOverlay::PaintRegionBanner(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	const float T = (float)(Now - BannerStart);
	if (T < 0.f || T > 5.6f || BannerTitle.IsEmpty())
	{
		return Layer;
	}
	const float In = FMath::SmoothStep(0.f, 1.1f, T);
	const float A = In * (1.f - FMath::SmoothStep(4.2f, 5.6f, T));
	const float S = MTUI::Scale(G);
	const FVector2D Screen = G.GetLocalSize();
	const float Y = 118.f * S - (1.f - In) * 14.f * S;
	const FVector2D Center(Screen.X * 0.5f, Y + 34.f * S);
	MTUI::Glow(Out, Layer, G, Center, 380.f * S, FLinearColor(0.f, 0.f, 0.f, 0.42f * A));
	MTUI::TextAligned(Out, Layer + 2, G, BannerTitle, MTUI::Title(40.f * S), FVector2D(0.f, Y), FVector2D(Screen.X, 50.f * S), FVector2D(0.5f, 0.5f),
		MTUI::GoldBright.CopyWithNewOpacity(A));
	MTUI::Divider(Out, Layer + 2, G, FVector2D(Screen.X * 0.5f, Y + 62.f * S), 460.f * S, A);
	if (!BannerSubtitle.IsEmpty())
	{
		MTUI::TextAligned(Out, Layer + 2, G, BannerSubtitle, MTUI::BodyItalic(24.f * S), FVector2D(0.f, Y + 72.f * S), FVector2D(Screen.X, 30.f * S),
			FVector2D(0.5f, 0.5f), MTUI::TextLight.CopyWithNewOpacity(0.92f * A));
	}
	return Layer + 4;
}

int32 SMTHudOverlay::PaintHotbar(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, AMTCharacterBase* Char) const
{
	const float S = MTUI::Scale(G);
	const FVector2D Screen = G.GetLocalSize();
	const float Slot = 66.f * S;
	const float Gap = 10.f * S;
	const float GroupGap = 30.f * S;
	const float Total = Slot * 7.f + Gap * 5.f + GroupGap;
	const float X0 = (Screen.X - Total) * 0.5f;
	const float Y0 = Screen.Y - Slot - 46.f * S;

	// Backing plate and ornament.
	MTUI::DarkPanel(Out, Layer, G, FVector2D(X0 - 18.f * S, Y0 - 16.f * S), FVector2D(Total + 36.f * S, Slot + 46.f * S), 0.85f, true);
	MTUI::Divider(Out, Layer + 2, G, FVector2D(Screen.X * 0.5f, Y0 - 16.f * S), 300.f * S, 0.95f);

	UMTAbilityComponent* Abilities = Char->GetAbilities();
	const UMTAttributeComponent* Attr = Char->GetAttributes();
	const UMTAbility* Active = Abilities->GetActiveAbility();
	const int32 Base = Layer + 4;

	for (int32 i = 0; i < 7; ++i)
	{
		const EMTAbilitySlot SlotId = HotbarSlots[i];
		const float X = X0 + i * (Slot + Gap) + (i >= 5 ? GroupGap - Gap : 0.f);
		const FVector2D P(X, Y0);
		const FVector2D Size(Slot, Slot);
		const FMTAbilityData* Data = Abilities->GetSlotData(SlotId);
		const FName Id = Abilities->GetSlotAbilityId(SlotId);

		MTUI::Box(Out, Base, G, P, Size, FLinearColor(0.f, 0.f, 0.f, 0.7f));
		if (!Data)
		{
			MTUI::TextAligned(Out, Base + 1, G, TEXT("-"), MTUI::Heading(16.f * S), P, Size, FVector2D(0.5f, 0.5f), MTUI::GoldDim);
			MTUI::GoldFrame(Out, Base + 3, G, P, Size, 0.5f, false);
			continue;
		}

		const UMTAbility* Instance = Abilities->GetSlotAbility(SlotId);
		const float ManaCost = Instance ? Instance->GetEffectiveManaCost() : Data->ManaCost;
		const bool bAffordable = Attr->GetMana() + 0.5f >= ManaCost && (Data->AwakeningMeterCost <= 0.f || Attr->GetAwakeningMeter() + 0.5f >= Data->AwakeningMeterCost);
		const float Remaining = Abilities->GetCooldownRemaining(Id);
		const float Fraction = Abilities->GetCooldownFraction(Id);
		const bool bActive = Active && Instance == Active;

		// Icon (painted art) or an element-coloured tile with initials.
		const FVector2D IconPos = P + FVector2D(3.f * S);
		const FVector2D IconSize = Size - FVector2D(6.f * S);
		if (const FSlateBrush* Icon = MTUI::AbilityIcon(Id))
		{
			const FLinearColor Tint = bAffordable ? FLinearColor::White : FLinearColor(0.5f, 0.3f, 0.3f, 1.f);
			MTUI::Box(Out, Base + 1, G, IconPos, IconSize, Tint, Icon);
		}
		else
		{
			MTUI::Box(Out, Base + 1, G, IconPos, IconSize, MTUtil::ElementColor(Data->Element) * 0.45f);
			MTUI::TextAligned(Out, Base + 2, G, Data->DisplayName.ToString().Left(2), MTUI::Heading(20.f * S), IconPos, IconSize, FVector2D(0.5f, 0.5f), MTUI::TextLight);
		}

		// Cooldown sweep and seconds left.
		if (Remaining > 0.f)
		{
			MTUI::RadialWipe(Out, Base + 3, G, IconPos, IconSize.X, Fraction, FLinearColor(0.f, 0.f, 0.02f, 0.72f));
			const FString Secs = Remaining >= 10.f ? FString::Printf(TEXT("%d"), FMath::CeilToInt(Remaining)) : FString::Printf(TEXT("%.1f"), Remaining);
			MTUI::TextAligned(Out, Base + 5, G, Secs, MTUI::Heading(20.f * S), IconPos, IconSize, FVector2D(0.5f, 0.5f), MTUI::TextLight);
		}

		// Frame: gold, brighter while casting; a glow pulse just after coming off cooldown.
		const float Flash = FMath::Clamp(1.f - float(Now - ReadyFlashAt[i]) / 0.6f, 0.f, 1.f);
		if (Flash > 0.f)
		{
			MTUI::Glow(Out, Base, G, P + Size * 0.5f, Slot * (0.85f + 0.3f * (1.f - Flash)), MTUI::GoldBright.CopyWithNewOpacity(0.8f * Flash));
		}
		if (bActive)
		{
			MTUI::Glow(Out, Base, G, P + Size * 0.5f, Slot * 0.95f, MTUI::GoldBright.CopyWithNewOpacity(0.45f + 0.2f * FMath::Sin(Now * 10.0)));
		}
		MTUI::GoldFrame(Out, Base + 6, G, P, Size, bActive || Flash > 0.f ? 1.f : 0.75f, false);
		if (bActive && Data->bChargeable && Instance)
		{
			MTUI::Arc(Out, Base + 7, G, P + Size * 0.5f, Slot * 0.64f, Instance->GetChargeAlpha(), MTUI::GoldBright, 3.f * S);
		}

		// Key tag (top-left).
		const FString Key = UMTAbilityComponent::SlotToKeyLabel(SlotId);
		const FSlateFontInfo KeyFont = MTUI::Heading(12.f * S);
		const FVector2D KeySize = MTUI::Measure(Key, KeyFont) + FVector2D(9.f * S, 2.f * S);
		const FVector2D KeyPos = P + FVector2D(-5.f * S, -7.f * S);
		MTUI::Box(Out, Base + 7, G, KeyPos, KeySize, FLinearColor(0.05f, 0.035f, 0.02f, 0.95f));
		MTUI::GoldFrame(Out, Base + 8, G, KeyPos, KeySize, 0.9f, false);
		MTUI::TextAligned(Out, Base + 9, G, Key, KeyFont, KeyPos, KeySize, FVector2D(0.5f, 0.5f), MTUI::GoldBright, false);

		// Mana cost gem (bottom-right).
		if (ManaCost > 0.5f)
		{
			const FVector2D Gem = P + FVector2D(Slot - 4.f * S, Slot - 4.f * S);
			MTUI::Glow(Out, Base + 7, G, Gem, 15.f * S, (bAffordable ? MTUI::Mana : MTUI::Danger).CopyWithNewOpacity(0.95f));
			MTUI::TextAligned(Out, Base + 9, G, FString::Printf(TEXT("%d"), FMath::RoundToInt(ManaCost)), MTUI::Heading(10.f * S),
				Gem - FVector2D(14.f * S, 8.f * S), FVector2D(28.f * S, 16.f * S), FVector2D(0.5f, 0.5f), FLinearColor::White);
		}

		// Name under the slot.
		FString Name = Data->DisplayName.ToString();
		Name.RemoveFromStart(TEXT("Dragon God Style: "));
		const FSlateFontInfo NameFont = MTUI::Body(14.f * S);
		while (Name.Len() > 3 && MTUI::Measure(Name, NameFont).X > Slot + Gap)
		{
			Name = Name.LeftChop(2) + TEXT(".");
		}
		MTUI::TextAligned(Out, Base + 7, G, Name, NameFont, FVector2D(P.X - Gap * 0.5f, P.Y + Slot + 3.f * S), FVector2D(Slot + Gap, 18.f * S),
			FVector2D(0.5f, 0.f), MTUI::TextLight);
	}
	return Base + 12;
}

int32 SMTHudOverlay::PaintVitals(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, AMTCharacterBase* Char) const
{
	const float S = MTUI::Scale(G);
	const FVector2D Screen = G.GetLocalSize();
	const FVector2D Size(390.f * S, 118.f * S);
	const FVector2D P(34.f * S, Screen.Y - Size.Y - 34.f * S);
	const UMTAttributeComponent* Attr = Char->GetAttributes();
	MTUI::DarkPanel(Out, Layer, G, P, Size, 0.9f, true);

	FString Name = Char->GetCharacterId().ToString();
	if (const FMTCharacterData* Data = Char->GetCharacterData())
	{
		Name = Data->DisplayName.ToString();
	}
	int32 Level = 1;
	if (const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(Char))
	{
		Level = Progression->GetLevel();
	}
	MTUI::Text(Out, Layer + 3, G, Name, MTUI::Heading(19.f * S), P + FVector2D(18.f * S, 10.f * S), MTUI::GoldBright);
	MTUI::TextAligned(Out, Layer + 3, G, FString::Printf(TEXT("Lv %d"), Level), MTUI::Heading(15.f * S), P + FVector2D(0.f, 12.f * S),
		FVector2D(Size.X - 18.f * S, 20.f * S), FVector2D(1.f, 0.f), MTUI::TextLight);

	const float BarX = P.X + 18.f * S;
	const float BarW = Size.X - 36.f * S;
	const float Health = Attr->GetMaxHealth() > 0.f ? Attr->GetHealth() / Attr->GetMaxHealth() : 0.f;
	const float Mana = Attr->GetMaxMana() > 0.f ? Attr->GetMana() / Attr->GetMaxMana() : 0.f;
	const float Stamina = Attr->GetMaxStamina() > 0.f ? Attr->GetStamina() / Attr->GetMaxStamina() : 0.f;
	const float Awakening = Attr->GetAwakeningMeter() / 100.f;

	MTUI::Bar(Out, Layer + 3, G, FVector2D(BarX, P.Y + 44.f * S), FVector2D(BarW, 15.f * S), Health, HealthTrail, MTUI::Health);
	MTUI::TextAligned(Out, Layer + 8, G, FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(Attr->GetHealth()), FMath::RoundToInt(Attr->GetMaxHealth())),
		MTUI::Heading(10.f * S), FVector2D(BarX, P.Y + 44.f * S), FVector2D(BarW, 15.f * S), FVector2D(0.5f, 0.5f), FLinearColor::White);
	MTUI::Bar(Out, Layer + 3, G, FVector2D(BarX, P.Y + 64.f * S), FVector2D(BarW, 12.f * S), Mana, ManaTrail, MTUI::Mana);
	MTUI::TextAligned(Out, Layer + 8, G, FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(Attr->GetMana()), FMath::RoundToInt(Attr->GetMaxMana())),
		MTUI::Heading(9.f * S), FVector2D(BarX, P.Y + 64.f * S), FVector2D(BarW, 12.f * S), FVector2D(0.5f, 0.5f), FLinearColor::White);
	MTUI::Bar(Out, Layer + 3, G, FVector2D(BarX, P.Y + 81.f * S), FVector2D(BarW * 0.6f, 7.f * S), Stamina, Stamina, MTUI::Stamina);

	// Awakening meter: glows and reads "AWAKENING READY" when full (G).
	const FVector2D AwP(BarX + BarW * 0.64f, P.Y + 81.f * S);
	const FVector2D AwS(BarW * 0.36f, 7.f * S);
	MTUI::Bar(Out, Layer + 3, G, AwP, AwS, Awakening, Awakening, MTUI::Awakening);
	if (Awakening >= 0.999f)
	{
		const float Pulse = 0.55f + 0.45f * FMath::Sin(Now * 4.0);
		MTUI::Glow(Out, Layer + 2, G, AwP + AwS * 0.5f, AwS.X * 0.7f, MTUI::GoldBright.CopyWithNewOpacity(0.35f * Pulse));
		MTUI::TextAligned(Out, Layer + 8, G, TEXT("AWAKENING READY  [G]"), MTUI::Heading(10.f * S), FVector2D(AwP.X, AwP.Y + 9.f * S), FVector2D(AwS.X, 14.f * S),
			FVector2D(0.5f, 0.f), MTUI::GoldBright.CopyWithNewOpacity(0.7f + 0.3f * Pulse));
	}
	else
	{
		MTUI::TextAligned(Out, Layer + 8, G, TEXT("AWAKENING"), MTUI::Heading(9.f * S), FVector2D(AwP.X, AwP.Y + 9.f * S), FVector2D(AwS.X, 14.f * S),
			FVector2D(0.5f, 0.f), MTUI::TextLight.CopyWithNewOpacity(0.6f));
	}
	return Layer + 10;
}

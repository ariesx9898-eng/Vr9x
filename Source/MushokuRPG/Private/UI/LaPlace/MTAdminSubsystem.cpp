#include "UI/LaPlace/MTAdminSubsystem.h"

#include "Abilities/MTAbilityComponent.h"
#include "Character/MTAttributeComponent.h"
#include "Character/MTCharacterBase.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Save/MTSaveSubsystem.h"
#include "UI/LaPlace/MTFrontEndSubsystem.h"
#include "UI/LaPlace/MTUIStyle.h"
#include "UI/LaPlace/SMTAdminPanel.h"
#include "World/MTTimeOfDaySubsystem.h"

namespace MTAdmin
{
	const TCHAR* Code = TEXT("aishayams");

	AMTCharacterBase* PlayerCharacter(APlayerController* PC)
	{
		return PC ? Cast<AMTCharacterBase>(PC->GetPawn()) : nullptr;
	}
}

UMTAdminSubsystem* UMTAdminSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMTAdminSubsystem>() : nullptr;
}

void UMTAdminSubsystem::Deinitialize()
{
	Close();
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

void UMTAdminSubsystem::Toggle(APlayerController* PC)
{
	if (IsOpen())
	{
		Close();
		return;
	}
	if (!PC || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	Controller = PC;
	Widget = SNew(SMTAdminPanel).Admin(this);
	GEngine->GameViewport->AddViewportWidgetContent(Widget.ToSharedRef(), 200);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Widget);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
	// Nothing hits you while you type.
	if (!UGameplayStatics::IsGamePaused(PC))
	{
		UGameplayStatics::SetGamePaused(PC, true);
		bPausedByPanel = true;
	}
	MTUI::Sound(TEXT("ui_open"), 0.6f);
}

void UMTAdminSubsystem::Close()
{
	if (Widget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
	}
	const bool bWasOpen = Widget.IsValid();
	Widget.Reset();
	APlayerController* PC = Controller.Get();
	if (!PC || !bWasOpen)
	{
		return;
	}
	if (bPausedByPanel)
	{
		UGameplayStatics::SetGamePaused(PC, false);
		bPausedByPanel = false;
	}
	const UMTFrontEndSubsystem* FrontEnd = UMTFrontEndSubsystem::Get(PC);
	if (FrontEnd && FrontEnd->IsOpen())
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	MTUI::Sound(TEXT("ui_close"), 0.5f);
}

bool UMTAdminSubsystem::TryUnlock(const FString& InCode)
{
	if (!InCode.TrimStartAndEnd().Equals(MTAdmin::Code, ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (!bUnlocked)
	{
		bUnlocked = true;
		GrantEverything();
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMTAdminSubsystem::TickCheats));
	}
	return true;
}

void UMTAdminSubsystem::GrantEverything()
{
	APlayerController* PC = Controller.Get();
	UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(PC);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(PC);
	if (!Progression || !Registry)
	{
		return;
	}
	for (const TPair<FName, FMTCharacterData>& Pair : Registry->GetCharacters())
	{
		Progression->GrantCharacter(Pair.Key);
	}
	const EMTElement Elements[] = { EMTElement::Fire, EMTElement::Water, EMTElement::Earth, EMTElement::Wind, EMTElement::Arcane };
	for (EMTElement Element : Elements)
	{
		Progression->GrantElement(Element);
		Progression->AddMagicRankXP(Element, 1.0e7f);
	}
	for (uint8 Race = 0; Race <= (uint8)EMTRace::SeaRace; ++Race)
	{
		Progression->GrantRace((EMTRace)Race);
	}
	for (int32 Guard = 0; Guard < 400 && Progression->GetLevel() < UMTProgressionSubsystem::MaxLevel; ++Guard)
	{
		Progression->AddXP(FMath::Max(1, Progression->GetXPToNextLevel()));
	}
	for (const TPair<FName, FMTAbilityData>& Pair : Registry->GetAbilities())
	{
		EMTMasteryTrack Track;
		FName Key;
		if (Progression->GetAbilityMasteryTrack(Pair.Value, Track, Key))
		{
			Progression->AddMasteryXP(Track, Key, 1.0e7f);
		}
	}
	Progression->AddAdventurerPoints(1000000);
	for (EMTRollCategory Category : { EMTRollCategory::Character, EMTRollCategory::Element, EMTRollCategory::Race })
	{
		Progression->AddSpins(Category, 99);
	}
	Progression->AddGold(99999);
	for (const TPair<FName, FMTItemData>& Pair : Registry->GetItems())
	{
		Progression->AddItem(Pair.Key, 99);
	}
	for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
	{
		Progression->DiscoverLocation(Pair.Key);
	}
	if (AMTCharacterBase* Char = MTAdmin::PlayerCharacter(PC))
	{
		Progression->ApplyBuildTo(Char);
		if (UMTAttributeComponent* Attributes = Char->GetAttributes())
		{
			Attributes->RefillAll();
			Attributes->AddAwakeningMeter(1000.f);
		}
	}
	if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(PC))
	{
		Save->SaveGame(TEXT("Slot0"));
	}
}

bool UMTAdminSubsystem::TickCheats(float DeltaTime)
{
	if (AMTCharacterBase* Char = MTAdmin::PlayerCharacter(Controller.Get()))
	{
		if (UMTAttributeComponent* Attributes = Char->GetAttributes())
		{
			Attributes->bGodMode = bGodMode;
			Attributes->bInfiniteMana = bInfiniteMana;
		}
		if (UMTAbilityComponent* Abilities = Char->GetAbilities())
		{
			Abilities->bNoCooldowns = bNoCooldowns;
			if (bNoCooldowns)
			{
				Abilities->ResetAllCooldowns();
			}
		}
	}
	return true;
}

bool UMTAdminSubsystem::IsToggleOn(FName Toggle) const
{
	if (Toggle == TEXT("God")) return bGodMode;
	if (Toggle == TEXT("Mana")) return bInfiniteMana;
	if (Toggle == TEXT("Cooldowns")) return bNoCooldowns;
	return false;
}

FString UMTAdminSubsystem::DoAction(FName Action, FName Param)
{
	APlayerController* PC = Controller.Get();
	if (!bUnlocked || !PC)
	{
		return FString();
	}
	AMTCharacterBase* Char = MTAdmin::PlayerCharacter(PC);
	UMTAttributeComponent* Attributes = Char ? Char->GetAttributes() : nullptr;
	UMTAbilityComponent* Abilities = Char ? Char->GetAbilities() : nullptr;
	UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(PC);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(PC);
	const FString P = Param.ToString();

	if (Action == TEXT("Heal") && Attributes)
	{
		Attributes->RefillAll();
		return TEXT("Health, mana and stamina restored");
	}
	if (Action == TEXT("Awaken") && Attributes)
	{
		Attributes->AddAwakeningMeter(1000.f);
		return TEXT("Awakening meter full - press G to awaken");
	}
	if (Action == TEXT("ResetCooldowns") && Abilities)
	{
		Abilities->ResetAllCooldowns();
		return TEXT("Every cooldown reset");
	}
	if (Action == TEXT("Toggle"))
	{
		bool* Flag = P == TEXT("God") ? &bGodMode : P == TEXT("Mana") ? &bInfiniteMana : P == TEXT("Cooldowns") ? &bNoCooldowns : nullptr;
		if (!Flag)
		{
			return FString();
		}
		*Flag = !*Flag;
		TickCheats(0.f);
		const TCHAR* Name = P == TEXT("God") ? TEXT("God mode") : P == TEXT("Mana") ? TEXT("Infinite mana and stamina") : TEXT("No cooldowns");
		return FString::Printf(TEXT("%s %s"), Name, *Flag ? TEXT("ON") : TEXT("OFF"));
	}
	if (Action == TEXT("Levels") && Progression)
	{
		for (int32 i = 0; i < 10 && Progression->GetLevel() < UMTProgressionSubsystem::MaxLevel; ++i)
		{
			Progression->AddXP(FMath::Max(1, Progression->GetXPToNextLevel()));
		}
		return FString::Printf(TEXT("Level %d"), Progression->GetLevel());
	}
	if (Action == TEXT("Spins") && Progression)
	{
		for (EMTRollCategory Category : { EMTRollCategory::Character, EMTRollCategory::Element, EMTRollCategory::Race })
		{
			Progression->AddSpins(Category, 99);
		}
		return TEXT("+99 character, element and race spins");
	}
	if (Action == TEXT("Gold") && Progression)
	{
		Progression->AddGold(10000);
		return FString::Printf(TEXT("+10,000 gold (%d)"), Progression->GetGold());
	}
	if (Action == TEXT("Items") && Progression && Registry)
	{
		for (const TPair<FName, FMTItemData>& Pair : Registry->GetItems())
		{
			Progression->AddItem(Pair.Key, 99);
		}
		return FString::Printf(TEXT("+99 of all %d items"), Registry->GetItems().Num());
	}
	if (Action == TEXT("Smite") && Char)
	{
		int32 Count = 0;
		for (TActorIterator<AMTCharacterBase> It(PC->GetWorld()); It; ++It)
		{
			AMTCharacterBase* Other = *It;
			UMTAttributeComponent* OtherAttributes = Other != Char ? Other->GetAttributes() : nullptr;
			if (!OtherAttributes || !OtherAttributes->IsAlive() || FVector::Dist(Other->GetActorLocation(), Char->GetActorLocation()) > 6000.f)
			{
				continue;
			}
			FMTDamageSpec Spec;
			Spec.Damage = 1.0e6f;
			Spec.Instigator = Char;
			Spec.HitLocation = Other->GetActorLocation();
			Spec.HitDirection = (Other->GetActorLocation() - Char->GetActorLocation()).GetSafeNormal();
			OtherAttributes->ApplyDamage(Spec);
			++Count;
		}
		return FString::Printf(TEXT("Smote %d foe%s within 60 m"), Count, Count == 1 ? TEXT("") : TEXT("s"));
	}
	if (Action == TEXT("Character") && Progression && Registry)
	{
		const FMTCharacterData* Data = Registry->GetCharacters().Find(Param);
		if (Data && Progression->EquipCharacter(Param))
		{
			return FString::Printf(TEXT("Now playing as %s"), *Data->DisplayName.ToString());
		}
		return TEXT("Could not switch character");
	}
	if (Action == TEXT("Time"))
	{
		if (UMTTimeOfDaySubsystem* Clock = UMTTimeOfDaySubsystem::Get(PC))
		{
			const float Hours = FCString::Atof(*P);
			Clock->SetTimeOfDayHours(Hours);
			return FString::Printf(TEXT("Time set to %02d:%02d"), FMath::FloorToInt(Hours), FMath::RoundToInt(FMath::Frac(Hours) * 60.f) % 60);
		}
		return FString();
	}
	if (Action == TEXT("Teleport") && Registry)
	{
		const FMTLocationData* Loc = Registry->FindLocation(Param);
		UMTFrontEndSubsystem* FrontEnd = UMTFrontEndSubsystem::Get(PC);
		if (Loc && FrontEnd)
		{
			FrontEnd->SetController(PC);
			return FrontEnd->TeleportTo(Param) ? FString::Printf(TEXT("Teleported to %s"), *Loc->DisplayName.ToString())
				: FString::Printf(TEXT("%s is not part of this map"), *Loc->DisplayName.ToString());
		}
		return TEXT("Unknown location");
	}
	return FString();
}

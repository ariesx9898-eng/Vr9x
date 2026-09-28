#include "Save/MTSaveSubsystem.h"
#include "Save/MTSaveGame.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Core/MTGameEvents.h"
#include "Character/MTCharacterBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Scalability.h"
#include "Templates/UnrealTemplate.h"

#define LOCTEXT_NAMESPACE "MTSave"

namespace MTSavePrivate
{
	static const FLinearColor ColorSave(0.65f, 0.85f, 0.65f);
	static const FLinearColor ColorError(1.f, 0.45f, 0.4f);
}

// ============================================================================ Lifecycle

void UMTSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMTProgressionSubsystem::StaticClass());
	Collection.InitializeDependency(UMTGameEvents::StaticClass());
	Super::Initialize(Collection);

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMTSaveSubsystem::HandleTick), 0.f);
}

void UMTSaveSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle = FTSTicker::FDelegateHandle();
	OnGatherSaveData.Clear();
	OnApplySaveData.Clear();
	Super::Deinitialize();
}

UMTSaveSubsystem* UMTSaveSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	if (const UGameInstance* GI = Cast<UGameInstance>(WorldContext))
	{
		return GI->GetSubsystem<UMTSaveSubsystem>();
	}
	if (const UGameInstanceSubsystem* Sub = Cast<UGameInstanceSubsystem>(WorldContext))
	{
		const UGameInstance* GI = Sub->GetGameInstance();
		return GI ? GI->GetSubsystem<UMTSaveSubsystem>() : nullptr;
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMTSaveSubsystem>() : nullptr;
}

UWorld* UMTSaveSubsystem::GetGameWorld() const
{
	const UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	return (World && World->IsGameWorld()) ? World : nullptr;
}

APawn* UMTSaveSubsystem::GetPlayerPawn() const
{
	UWorld* World = GetGameWorld();
	return World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
}

UMTProgressionSubsystem* UMTSaveSubsystem::GetProgression() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMTProgressionSubsystem>() : nullptr;
}

void UMTSaveSubsystem::Notify(const FText& Message, const FLinearColor& Color) const
{
	const UGameInstance* GI = GetGameInstance();
	if (UMTGameEvents* Events = GI ? GI->GetSubsystem<UMTGameEvents>() : nullptr)
	{
		Events->Notify(Message, Color);
	}
	UE_LOG(LogMushoku, Log, TEXT("Save: %s"), *Message.ToString());
}

// ============================================================================ Tick (autosave + pawn application)

bool UMTSaveSubsystem::HandleTick(float DeltaTime)
{
	APawn* Pawn = GetPlayerPawn();
	if (!Pawn)
	{
		return true; // menus / loading / editor preview: nothing to do
	}

	if (!bAutoLoadDone)
	{
		bAutoLoadDone = true;
		if (bAutoLoadOnStart && HasSave(CurrentSlot))
		{
			LoadGame(CurrentSlot);
		}
	}

	// New pawn (first spawn, respawn, possession change): apply the equipped build.
	if (LastAppliedPawn.Get() != Pawn)
	{
		ApplyToPawn(Pawn);
	}

	if (AutosaveInterval > 0.f)
	{
		AutosaveAccumulator += DeltaTime;
		if (AutosaveAccumulator >= AutosaveInterval)
		{
			AutosaveAccumulator = 0.f;
			if (SaveGame(CurrentSlot))
			{
				Notify(LOCTEXT("Autosaved", "Autosaved"), MTSavePrivate::ColorSave);
			}
		}
	}
	return true;
}

void UMTSaveSubsystem::ApplyToPawn(APawn* Pawn)
{
	if (!IsValid(Pawn))
	{
		return;
	}
	LastAppliedPawn = Pawn;

	if (UMTProgressionSubsystem* Progression = GetProgression())
	{
		if (AMTCharacterBase* Character = Cast<AMTCharacterBase>(Pawn))
		{
			Progression->ApplyBuildTo(Character);
		}
	}

	if (bPendingTransform)
	{
		bPendingTransform = false;
		UWorld* World = Pawn->GetWorld();
		const FName CurrentMap = World ? FName(*UGameplayStatics::GetCurrentLevelName(World, true)) : NAME_None;
		if (PendingMapName.IsNone() || PendingMapName == CurrentMap)
		{
			Pawn->TeleportTo(PendingLocation, FRotator(0.f, PendingRotation.Yaw, 0.f), false, true);
			if (AController* Controller = Pawn->GetController())
			{
				Controller->SetControlRotation(PendingRotation);
			}
		}
		else
		{
			// Cross-map restore is not automatic (no level travel from the save system); the pawn keeps its spawn.
			UE_LOG(LogMushoku, Warning, TEXT("Save: saved map '%s' differs from current '%s'; player transform not restored."),
				*PendingMapName.ToString(), *CurrentMap.ToString());
		}
	}

	ApplySettings(false);
}

// ============================================================================ Save / load

bool UMTSaveSubsystem::SaveGame(const FString& Slot)
{
	if (bSaving || Slot.IsEmpty())
	{
		return false;
	}
	TGuardValue<bool> Guard(bSaving, true);

	UMTProgressionSubsystem* Progression = GetProgression();
	UMTSaveGame* SaveObject = Cast<UMTSaveGame>(UGameplayStatics::CreateSaveGameObject(UMTSaveGame::StaticClass()));
	if (!Progression || !SaveObject)
	{
		return false;
	}

	FMTSaveData& Data = SaveObject->Data;
	Data = FMTSaveData();
	Data.SaveVersion = MT_SAVE_VERSION;
	Data.SavedAtUtc = FDateTime::UtcNow().ToIso8601();
	Progression->WriteToSave(Data);
	OnGatherSaveData.Broadcast(Data);

	if (UWorld* World = GetGameWorld())
	{
		Data.MapName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Data.PlayerLocation = Pawn->GetActorLocation();
			const AController* Controller = Pawn->GetController();
			Data.PlayerRotation = Controller ? Controller->GetControlRotation() : Pawn->GetActorRotation();
			Data.bHasPlayerTransform = true;
		}
	}

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveObject, Slot, 0);
	if (bSuccess)
	{
		CurrentSlot = Slot;
		AutosaveAccumulator = 0.f;
	}
	else
	{
		Notify(LOCTEXT("SaveFailed", "Save failed"), MTSavePrivate::ColorError);
	}
	OnSaved.Broadcast(Slot, bSuccess);
	return bSuccess;
}

bool UMTSaveSubsystem::LoadGame(const FString& Slot)
{
	if (Slot.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		return false;
	}
	UMTProgressionSubsystem* Progression = GetProgression();
	UMTSaveGame* SaveObject = Cast<UMTSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Progression || !SaveObject)
	{
		Notify(LOCTEXT("LoadFailed", "Could not read the save file"), MTSavePrivate::ColorError);
		OnLoaded.Broadcast(Slot, false);
		return false;
	}

	FMTSaveData Data = SaveObject->Data;
	if (MigrateSave(Data))
	{
		UE_LOG(LogMushoku, Log, TEXT("Save: migrated slot '%s' to version %d"), *Slot, MT_SAVE_VERSION);
	}

	Progression->ReadFromSave(Data);
	OnApplySaveData.Broadcast(Data);

	CurrentSlot = Slot;
	bAutoLoadDone = true;
	AutosaveAccumulator = 0.f;

	bPendingTransform = Data.bHasPlayerTransform;
	PendingMapName = Data.MapName;
	PendingLocation = Data.PlayerLocation;
	PendingRotation = Data.PlayerRotation;

	if (APawn* Pawn = GetPlayerPawn())
	{
		ApplyToPawn(Pawn);
	}
	else
	{
		LastAppliedPawn.Reset(); // applied by HandleTick when the pawn spawns
	}
	ApplySettings(false);

	OnLoaded.Broadcast(Slot, true);
	return true;
}

bool UMTSaveSubsystem::HasSave(const FString& Slot) const
{
	return !Slot.IsEmpty() && UGameplayStatics::DoesSaveGameExist(Slot, 0);
}

void UMTSaveSubsystem::DeleteSave(const FString& Slot)
{
	if (HasSave(Slot))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
	}
}

// ============================================================================ Migration

bool UMTSaveSubsystem::MigrateSave(FMTSaveData& Data)
{
	bool bChanged = false;

	if (Data.SaveVersion > MT_SAVE_VERSION)
	{
		UE_LOG(LogMushoku, Warning, TEXT("Save: version %d is newer than this build (%d); loading best-effort."), Data.SaveVersion, MT_SAVE_VERSION);
	}

	while (Data.SaveVersion < MT_SAVE_VERSION)
	{
		switch (Data.SaveVersion)
		{
		case 0:
			// v0 -> v1: element slots introduced. Old saves had no EquippedElements / slot count.
			if (Data.EquippedElements.Num() == 0)
			{
				Data.EquippedElements.Add(Data.OwnedElements.Num() > 0 ? Data.OwnedElements[0] : EMTElement::Earth);
			}
			Data.UnlockedElementSlots = FMath::Max(1, Data.UnlockedElementSlots);
			if (Data.OwnedElements.Num() == 0)
			{
				Data.OwnedElements.Add(Data.EquippedElements[0]);
			}
			Data.SaveVersion = 1;
			break;

		default:
			// Unknown / corrupt version number: jump to current and rely on the sanitisation below.
			Data.SaveVersion = MT_SAVE_VERSION;
			break;
		}
		bChanged = true;
	}

	// Always-on sanitisation (cheap, protects against hand-edited or partially written saves).
	if (Data.EquippedElements.Num() == 0)
	{
		Data.EquippedElements.Add(EMTElement::Earth);
		bChanged = true;
	}
	if (Data.UnlockedElementSlots < 1)
	{
		Data.UnlockedElementSlots = 1;
		bChanged = true;
	}
	if (Data.OwnedCharacters.Num() == 0)
	{
		Data.OwnedCharacters.Add(Data.EquippedCharacter.IsNone() ? FName(TEXT("Rudeus")) : Data.EquippedCharacter);
		bChanged = true;
	}
	if (Data.OwnedRaces.Num() == 0)
	{
		Data.OwnedRaces.Add(Data.EquippedRace);
		bChanged = true;
	}
	if (Data.Level < 1)
	{
		Data.Level = 1;
		bChanged = true;
	}
	return bChanged;
}

// ============================================================================ Settings

void UMTSaveSubsystem::ApplySettings(bool bIncludeGraphics) const
{
	const UMTProgressionSubsystem* Progression = GetProgression();
	if (!Progression)
	{
		return;
	}
	const FMTSettingsSave& Settings = Progression->GetSettings();

	if (UWorld* World = GetGameWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->SetFOV(Settings.FieldOfView);
			}
		}
	}

	if (bIncludeGraphics)
	{
		Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
		Levels.SetFromSingleQualityLevel(FMath::Clamp(Settings.GraphicsQuality, 0, 3));
		Scalability::SetQualityLevels(Levels);
	}
}

#undef LOCTEXT_NAMESPACE

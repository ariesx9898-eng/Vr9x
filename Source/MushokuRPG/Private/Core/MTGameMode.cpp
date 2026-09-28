#include "Core/MTGameMode.h"
#include "Character/MTPlayerCharacter.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Save/MTSaveSubsystem.h"
#include "UI/MTHUD.h"
#include "UI/LaPlace/MTFrontEndSubsystem.h"
#include "Kismet/GameplayStatics.h"

AMTGameMode::AMTGameMode()
{
	DefaultPawnClass = AMTPlayerCharacter::StaticClass();
	PlayerControllerClass = AMTPlayerController::StaticClass();
	HUDClass = AMTHUD::StaticClass();
}

void AMTGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (UGameplayStatics::HasOption(Options, TEXT("Menu")))
	{
		bFrontEndOnStart = UGameplayStatics::GetIntOption(Options, TEXT("Menu"), 1) != 0;
	}
}

void AMTGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	if (bFrontEndOnStart && NewPlayer && NewPlayer->IsLocalController())
	{
		if (UMTFrontEndSubsystem* FrontEnd = UMTFrontEndSubsystem::Get(this))
		{
			FrontEnd->OpenTitle(NewPlayer);
			if (UGameplayStatics::HasOption(OptionsString, TEXT("UITour")))
			{
				FrontEnd->StartTour(NewPlayer);
			}
		}
	}
}

AMTPlayerController::AMTPlayerController()
{
	bShowMouseCursor = false;
}

void AMTPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	const AMTHUD* MTHUD = Cast<AMTHUD>(GetHUD());
	const bool bWantMenu = MTHUD && MTHUD->IsMenuOpen();
	if (bWantMenu == bMenuMode)
	{
		return;
	}
	bMenuMode = bWantMenu;
	bShowMouseCursor = bMenuMode;
	bEnableClickEvents = bMenuMode;
	if (bMenuMode)
	{
		// Game and UI: keyboard shortcuts still reach Enhanced Input while the cursor is free.
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		SetIgnoreLookInput(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		ResetIgnoreLookInput();
	}
}

void AMTPlayerController::MTGiveSpins(int32 Count)
{
	if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Progression->AddSpins(EMTRollCategory::Character, Count);
		Progression->AddSpins(EMTRollCategory::Element, Count);
		Progression->AddSpins(EMTRollCategory::Race, Count);
	}
}

void AMTPlayerController::MTSetCharacter(FName CharacterId)
{
	if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Progression->EquipCharacter(CharacterId);
	}
}

void AMTPlayerController::MTSaveNow()
{
	if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(this))
	{
		Save->SaveGame(TEXT("Slot0"));
	}
}

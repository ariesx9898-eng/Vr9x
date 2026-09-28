#include "UI/LaPlace/MTFrontEndSubsystem.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "UI/MTHUD.h"
#include "Character/MTCharacterBase.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTTypes.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Save/MTSaveSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformMisc.h"
#include "CollisionQueryParams.h"

UMTFrontEndSubsystem* UMTFrontEndSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMTFrontEndSubsystem>() : nullptr;
}

void UMTFrontEndSubsystem::Deinitialize()
{
	Close();
	Super::Deinitialize();
}

void UMTFrontEndSubsystem::OpenTitle(APlayerController* PC)
{
	Show(PC, EMTFrontPage::Title, true);
	PlayTitleMusic(true);
}

void UMTFrontEndSubsystem::PlayTitleMusic(bool bPlay)
{
	if (!bPlay)
	{
		if (Music)
		{
			Music->FadeOut(1.5f, 0.f);
			Music = nullptr;
		}
		return;
	}
	if (Music && Music->IsPlaying())
	{
		return;
	}
	APlayerController* PC = Controller.Get();
	USoundBase* Theme = LoadObject<USoundBase>(nullptr, TEXT("/Game/LaPlace/Audio/Music/music_title_theme.music_title_theme"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (PC && Theme)
	{
		Music = UGameplayStatics::SpawnSound2D(PC, Theme, 0.55f, 1.f, 0.f, nullptr, true, false);
		if (Music)
		{
			Music->bIsUISound = true; // keeps playing while the game is paused behind the menu
			Music->FadeIn(2.f, 0.55f);
		}
	}
}

void UMTFrontEndSubsystem::StartTour(APlayerController* PC)
{
	Controller = PC;
	TourClock = 0.f;
	TourStep = 0;
	TourHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMTFrontEndSubsystem::TickTour));
}

bool UMTFrontEndSubsystem::TickTour(float DeltaTime)
{
	TourClock += DeltaTime;
	APlayerController* PC = Controller.Get();
	if (!PC)
	{
		return false;
	}
	auto Shot = [](const TCHAR* Name)
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("UITour");
		FScreenshotRequest::RequestScreenshot(Dir / Name + FString(TEXT(".png")), true, false);
	};
	// Each step: wait, then capture; the page changes right after a capture.
	struct FStep { float At; EMTFrontPage Page; const TCHAR* Name; };
	static const FStep Steps[] = {
		{ 6.0f, EMTFrontPage::Title, TEXT("01_Title") },
		{ 9.0f, EMTFrontPage::Map, TEXT("02_Map") },
		{ 12.0f, EMTFrontPage::Edit, TEXT("03_Edit") },
		{ 15.0f, EMTFrontPage::Abilities, TEXT("04_Abilities") },
		{ 18.0f, EMTFrontPage::Settings, TEXT("05_Settings") },
	};
	constexpr int32 NumPages = UE_ARRAY_COUNT(Steps);
	if (TourStep < NumPages)
	{
		if (Widget.IsValid() && Widget->GetPage() != Steps[TourStep].Page)
		{
			Widget->ShowPage(Steps[TourStep].Page);
		}
		if (TourClock >= Steps[TourStep].At)
		{
			Shot(Steps[TourStep].Name);
			++TourStep;
		}
		return true;
	}
	if (TourStep == NumPages)
	{
		// Spawn at the first offered location and look at the HUD.
		if (Widget.IsValid())
		{
			Widget->ShowPage(EMTFrontPage::Map);
		}
		const UMTDataRegistry* Registry = UMTDataRegistry::Get(PC);
		FName First;
		if (Registry)
		{
			for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
			{
				if (Pair.Value.bSpawnPoint || Pair.Value.bFastTravel)
				{
					First = Pair.Key;
					break;
				}
			}
		}
		HandleSpawn(First);
		Close();
		++TourStep;
		TourClock = 0.f;
		return true;
	}
	if (TourStep == NumPages + 1 && TourClock > 4.f)
	{
		Shot(TEXT("06_Gameplay_HUD"));
		++TourStep;
		TourClock = 0.f;
		return true;
	}
	if (TourStep == NumPages + 2 && TourClock > 1.f)
	{
		OpenPause(PC);
		++TourStep;
		TourClock = 0.f;
		return true;
	}
	if (TourStep == NumPages + 3 && TourClock > 2.f)
	{
		Shot(TEXT("07_Pause"));
		++TourStep;
		TourClock = 0.f;
		return true;
	}
	if (TourStep == NumPages + 4 && TourClock > 1.5f)
	{
		FPlatformMisc::RequestExit(false, TEXT("UI tour finished"));
		return false;
	}
	return true;
}

void UMTFrontEndSubsystem::OpenPause(APlayerController* PC)
{
	Show(PC, EMTFrontPage::Pause, false);
}

void UMTFrontEndSubsystem::OpenPage(APlayerController* PC, EMTFrontPage Page)
{
	Show(PC, Page, Page == EMTFrontPage::Title || Page == EMTFrontPage::Map || Page == EMTFrontPage::Edit);
}

void UMTFrontEndSubsystem::Show(APlayerController* PC, EMTFrontPage Page, bool bHidePawn)
{
	if (!PC || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	Controller = PC;
	if (!Widget.IsValid())
	{
		Widget = SNew(SMTFrontEnd)
			.PlayerController(PC)
			.InitialPage(Page)
			.OnSpawn(FMTOnSpawnRequested::CreateUObject(this, &UMTFrontEndSubsystem::HandleSpawn))
			.OnResume(FSimpleDelegate::CreateUObject(this, &UMTFrontEndSubsystem::HandleResume))
			.OnReturnToTitle(FSimpleDelegate::CreateUObject(this, &UMTFrontEndSubsystem::HandleReturnToTitle))
			.OnQuit(FSimpleDelegate::CreateUObject(this, &UMTFrontEndSubsystem::HandleQuit));
		GEngine->GameViewport->AddViewportWidgetContent(Widget.ToSharedRef(), 50);
	}
	else
	{
		Widget->ShowPage(Page);
	}
	if (bHidePawn)
	{
		SetPawnParked(true);
	}
	if (AMTHUD* HUD = Cast<AMTHUD>(PC->GetHUD()))
	{
		HUD->SetGameplayHudHidden(true);
	}
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Widget);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	UGameplayStatics::SetGamePaused(PC, true);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
	}
}

void UMTFrontEndSubsystem::SetPawnParked(bool bParked)
{
	APlayerController* PC = Controller.Get();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	bPawnParked = bParked;
	if (!Pawn)
	{
		return;
	}
	Pawn->SetActorHiddenInGame(bParked);
	Pawn->SetActorEnableCollision(!bParked);
	if (bParked)
	{
		Pawn->DisableInput(PC);
	}
	else
	{
		Pawn->EnableInput(PC);
	}
}

void UMTFrontEndSubsystem::Close()
{
	APlayerController* PC = Controller.Get();
	if (Widget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
	}
	Widget.Reset();
	if (!PC)
	{
		return;
	}
	if (bPawnParked)
	{
		SetPawnParked(false);
	}
	if (AMTHUD* HUD = Cast<AMTHUD>(PC->GetHUD()))
	{
		HUD->SetGameplayHudHidden(false);
	}
	PC->SetInputMode(FInputModeGameOnly());
	PC->SetShowMouseCursor(false);
	UGameplayStatics::SetGamePaused(PC, false);
}

bool UMTFrontEndSubsystem::SpawnAt(FName LocationId)
{
	APlayerController* PC = Controller.Get();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(PC);
	const FMTLocationData* Loc = Registry ? Registry->FindLocation(LocationId) : nullptr;
	if (!Pawn || !Loc)
	{
		UE_LOG(LogMushoku, Warning, TEXT("[FrontEnd] cannot spawn at %s"), *LocationId.ToString());
		return false;
	}
	// The character chosen in EDIT and the loadout from ABILITIES take effect now.
	if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(PC))
	{
		Progression->ApplyBuildTo(Cast<AMTCharacterBase>(Pawn));
		Progression->DiscoverLocation(LocationId);
	}
	// Ground under the location (static geometry only), searching well above and below the data height.
	FHitResult Hit;
	FCollisionObjectQueryParams Ground(ECC_WorldStatic);
	const FVector From = Loc->WorldLocation + FVector(0.f, 0.f, 20000.f);
	const FVector To = Loc->WorldLocation - FVector(0.f, 0.f, 60000.f);
	const float HalfHeight = Pawn->GetSimpleCollisionHalfHeight();
	if (Pawn->GetWorld()->LineTraceSingleByObjectType(Hit, From, To, Ground))
	{
		const FRotator Facing(0.f, Loc->SpawnYaw, 0.f);
		Pawn->SetActorLocationAndRotation(Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 5.f), Facing, false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(-12.f, Loc->SpawnYaw, 0.f));
	}
	else
	{
		UE_LOG(LogMushoku, Warning, TEXT("[FrontEnd] no ground under %s at %s: staying at the player start"), *LocationId.ToString(), *Loc->WorldLocation.ToCompactString());
	}
	UE_LOG(LogMushoku, Log, TEXT("[FrontEnd] spawned at %s (%s)"), *LocationId.ToString(), *Pawn->GetActorLocation().ToCompactString());
	if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(PC))
	{
		Save->SaveGame(TEXT("Slot0"));
	}
	return true;
}

void UMTFrontEndSubsystem::HandleSpawn(FName LocationId)
{
	PlayTitleMusic(false);
	SpawnAt(LocationId);
	// Hand control back now; the widget keeps fading in over the world, then HandleResume removes it.
	APlayerController* PC = Controller.Get();
	if (bPawnParked)
	{
		SetPawnParked(false);
	}
	if (PC)
	{
		if (AMTHUD* HUD = Cast<AMTHUD>(PC->GetHUD()))
		{
			HUD->SetGameplayHudHidden(false);
		}
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		UGameplayStatics::SetGamePaused(PC, false);
	}
}

void UMTFrontEndSubsystem::HandleResume()
{
	Close();
}

void UMTFrontEndSubsystem::HandleReturnToTitle()
{
	if (APlayerController* PC = Controller.Get())
	{
		if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(PC))
		{
			Save->SaveGame(TEXT("Slot0"));
		}
		Show(PC, EMTFrontPage::Title, true);
		PlayTitleMusic(true);
	}
}

void UMTFrontEndSubsystem::HandleQuit()
{
	if (APlayerController* PC = Controller.Get())
	{
		if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(PC))
		{
			Save->SaveGame(TEXT("Slot0"));
		}
	}
	FPlatformMisc::RequestExit(false, TEXT("Quit from the LA PLACE menu"));
}

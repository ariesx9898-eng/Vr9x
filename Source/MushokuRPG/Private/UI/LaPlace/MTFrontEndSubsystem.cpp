#include "UI/LaPlace/MTFrontEndSubsystem.h"
#include "UI/LaPlace/MTAdminSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
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
#include "InputKeyEventArgs.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformMisc.h"
#include "CollisionQueryParams.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "LandscapeProxy.h"
#include "WorldPartition/WorldPartitionStreamingSourceComponent.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace
{
	/** XY extent of this map's terrain (every landscape proxy; LA PLACE keeps its terrain always loaded). Invalid when
	 *  the map has no landscape. Cached briefly: RefreshLocations and the preview ask for it several times at once. */
	FBox MTFrontEnd_TerrainBounds(const UWorld* World)
	{
		static TWeakObjectPtr<const UWorld> CachedWorld;
		static FBox Cached(ForceInit);
		static double CachedAt = -1.0;
		if (!World)
		{
			return FBox(ForceInit);
		}
		const double Now = FPlatformTime::Seconds();
		if (CachedWorld.Get() != World || Now - CachedAt > 2.0)
		{
			FBox Bounds(ForceInit);
			for (TActorIterator<ALandscapeProxy> It(const_cast<UWorld*>(World)); It; ++It)
			{
				Bounds += It->GetComponentsBoundingBox(true);
			}
			CachedWorld = World;
			Cached = Bounds;
			CachedAt = Now;
		}
		return Cached;
	}
}

UMTFrontEndSubsystem* UMTFrontEndSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMTFrontEndSubsystem>() : nullptr;
}

void UMTFrontEndSubsystem::Deinitialize()
{
	SetPreviewLocation(NAME_None);
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
	// Admin popup through the real shortcut (1 and 0 held together, seen by AMTPlayerController::PlayerTick): the code
	// prompt, a wrong code, then the unlocked panel.
	if (TourStep == NumPages + 4 && TourClock > 1.5f)
	{
		HandleResume();
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::One, IE_Pressed, 1.f));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Zero, IE_Pressed, 1.f));
		++TourStep;
		TourClock = 0.f;
		return true;
	}
	if (TourStep == NumPages + 5 && TourClock > 1.5f)
	{
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::One, IE_Released, 0.f));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Zero, IE_Released, 0.f));
		const UMTAdminSubsystem* Admin = UMTAdminSubsystem::Get(PC);
		UE_LOG(LogMushoku, Log, TEXT("[UITour] admin: 1+0 shortcut opened the popup=%d"), Admin && Admin->IsOpen() ? 1 : 0);
		Shot(TEXT("08_Admin_Code"));
		++TourStep;
		TourClock = 0.f;
		return true;
	}
	if (TourStep == NumPages + 6 && TourClock > 1.f)
	{
		if (UMTAdminSubsystem* Admin = UMTAdminSubsystem::Get(PC))
		{
			UE_LOG(LogMushoku, Log, TEXT("[UITour] admin: wrong code accepted=%d"), Admin->TryUnlock(TEXT("letmein")) ? 1 : 0);
			UE_LOG(LogMushoku, Log, TEXT("[UITour] admin: right code accepted=%d"), Admin->TryUnlock(TEXT("AishaYams")) ? 1 : 0);
			UE_LOG(LogMushoku, Log, TEXT("[UITour] admin: %s | %s | %s"), *Admin->DoAction(TEXT("Toggle"), TEXT("God")),
				*Admin->DoAction(TEXT("Awaken"), NAME_None), *Admin->DoAction(TEXT("Levels"), NAME_None));
		}
		++TourStep;
		TourClock = 0.f;
		return true;
	}
	if (TourStep == NumPages + 7 && TourClock > 1.5f)
	{
		Shot(TEXT("09_Admin_Panel"));
		++TourStep;
		TourClock = 0.f;
		return true;
	}
	if (TourStep == NumPages + 8 && TourClock > 1.5f)
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

void UMTFrontEndSubsystem::SetPreviewLocation(FName LocationId)
{
	if (LocationId == PreviewId)
	{
		return;
	}
	PreviewId = LocationId;
	APlayerController* PC = Controller.Get();
	const UMTDataRegistry* Registry = PC ? UMTDataRegistry::Get(PC) : nullptr;
	const FMTLocationData* Loc = Registry && !LocationId.IsNone() ? Registry->FindLocation(LocationId) : nullptr;
	USceneCaptureComponent2D* Capture = PreviewCapture ? PreviewCapture->GetCaptureComponent2D() : nullptr;
	// A place this map does not contain (the Fittoa test map lists Buena from the LA PLACE world): its preview camera
	// would look at empty sky, so the card keeps the painted art.
	const bool bInThisMap = Loc && PC && IsInThisMap(PC->GetWorld(), Loc->WorldLocation);
	if (!Loc || !bInThisMap || Loc->PreviewCamera.Location.IsNearlyZero() || !PC->GetWorld())
	{
		// No preview: stop rendering the capture (it costs a second scene render every frame).
		if (Capture)
		{
			Capture->bCaptureEveryFrame = false;
		}
		PreviewBrush.Reset();
		FTSTicker::GetCoreTicker().RemoveTicker(PreviewTicker);
		PreviewTicker.Reset();
		return;
	}
	if (!PreviewTarget)
	{
		PreviewTarget = NewObject<UTextureRenderTarget2D>(this);
		PreviewTarget->ClearColor = FLinearColor::Black;
		PreviewTarget->InitCustomFormat(1024, 576, PF_B8G8R8A8, false);
		PreviewTarget->UpdateResourceImmediate(true);
	}
	if (!PreviewCapture)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		PreviewCapture = PC->GetWorld()->SpawnActor<ASceneCapture2D>(Params);
		Capture = PreviewCapture ? PreviewCapture->GetCaptureComponent2D() : nullptr;
		if (Capture)
		{
			Capture->TextureTarget = PreviewTarget;
			Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Capture->FOVAngle = 62.f;
			Capture->bCaptureOnMovement = false;
			Capture->bAlwaysPersistRenderingState = true;
		}
	}
	if (!Capture)
	{
		return;
	}
	PreviewStart = Loc->PreviewCamera.Location;
	PreviewRotation = Loc->PreviewCamera.Rotation;
	PreviewCapture->SetActorLocationAndRotation(PreviewStart, PreviewRotation);
	Capture->bCaptureEveryFrame = true;
	PreviewSince = FPlatformTime::Seconds();
	PreviewReadyAt = -1.f;
	PreviewSettled = 0.f;
	// World Partition maps load only around streaming sources (normally the player, parked elsewhere while the menus
	// are open): the preview camera becomes one, so the town itself streams in, and is loaded when the player spawns.
	if (!PreviewStreaming && PC->GetWorld()->GetWorldPartition())
	{
		PreviewStreaming = NewObject<UWorldPartitionStreamingSourceComponent>(PreviewCapture);
		PreviewStreaming->RegisterComponent();
	}
	PreviewBrush = MakeShared<FSlateBrush>();
	PreviewBrush->SetResourceObject(PreviewTarget);
	PreviewBrush->ImageSize = FVector2D(1024.f, 576.f);
	if (!PreviewTicker.IsValid())
	{
		PreviewTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMTFrontEndSubsystem::TickPreview));
	}
}

bool UMTFrontEndSubsystem::TickPreview(float DeltaTime)
{
	if (!PreviewCapture || PreviewId.IsNone())
	{
		PreviewTicker.Reset();
		return false;
	}
	// A slow cinematic drift: glide forward and turn a little, like a crane shot settling on the town.
	const float T = GetPreviewAge();
	const FVector Forward = PreviewRotation.Vector();
	const FVector Location = PreviewStart + Forward * (T * 220.f) + FVector(0.f, 0.f, -T * 25.f);
	const FRotator Rotation(PreviewRotation.Pitch + FMath::Min(T, 12.f) * 0.15f, PreviewRotation.Yaw + T * 1.2f, 0.f);
	PreviewCapture->SetActorLocationAndRotation(Location, Rotation);
	// The live view fades in once the place has streamed in around the camera (or after 8 s, with whatever is there).
	if (PreviewReadyAt < 0.f)
	{
		UWorld* World = PreviewCapture->GetWorld();
		UWorldPartitionSubsystem* Partition = World ? World->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
		const bool bLoaded = !PreviewStreaming || !Partition || Partition->IsAllStreamingCompleted();
		PreviewSettled = (T > 0.3f && bLoaded) ? PreviewSettled + DeltaTime : 0.f;
		if (PreviewSettled > 0.25f || T > 8.f)
		{
			PreviewReadyAt = T;
		}
	}
	return true;
}

void UMTFrontEndSubsystem::StopPreviewStreaming()
{
	if (IsValid(PreviewStreaming))
	{
		PreviewStreaming->DestroyComponent();
	}
	PreviewStreaming = nullptr;
}

const FSlateBrush* UMTFrontEndSubsystem::GetPreviewBrush() const
{
	return PreviewBrush.IsValid() && PreviewReadyAt >= 0.f ? PreviewBrush.Get() : nullptr;
}

float UMTFrontEndSubsystem::GetPreviewFade() const
{
	return PreviewReadyAt < 0.f ? 0.f : FMath::Clamp((GetPreviewAge() - PreviewReadyAt) / 1.f, 0.f, 1.f);
}

float UMTFrontEndSubsystem::GetPreviewAge() const
{
	return PreviewSince > 0.0 ? float(FPlatformTime::Seconds() - PreviewSince) : 0.f;
}

void UMTFrontEndSubsystem::Close()
{
	SetPreviewLocation(NAME_None);
	StopPreviewStreaming();
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
	// The place in this map: its own spot, or on the Fittoa test map (none of the world's towns) the map's own start.
	FVector Spot = Loc->WorldLocation;
	float Yaw = Loc->SpawnYaw;
	FVector StandAt;
	const bool bPlaced = ResolvePlace(Pawn->GetWorld(), *Loc, Spot, Yaw)
		&& FindSpawnGround(Pawn->GetWorld(), Spot, Pawn->GetSimpleCollisionHalfHeight(), StandAt);
	if (bPlaced)
	{
		const FRotator Facing(0.f, Yaw, 0.f);
		Pawn->SetActorLocationAndRotation(StandAt, Facing, false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(-12.f, Yaw, 0.f));
	}
	else
	{
		UE_LOG(LogMushoku, Warning, TEXT("[FrontEnd] no ground under %s at %s: the player stays where it is"), *LocationId.ToString(), *Spot.ToCompactString());
	}
	UE_LOG(LogMushoku, Log, TEXT("[FrontEnd] spawned at %s (%s)"), *LocationId.ToString(), *Pawn->GetActorLocation().ToCompactString());
	if (UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(PC))
	{
		Save->SaveGame(TEXT("Slot0"));
	}
	return bPlaced;
}

bool UMTFrontEndSubsystem::FindSpawnGround(const UWorld* World, const FVector& Location, float HalfHeight, FVector& OutStandAt)
{
	FHitResult Hit;
	const FVector From = Location + FVector(0.f, 0.f, 20000.f);
	const FVector To = Location - FVector(0.f, 0.f, 60000.f);
	if (!World || !World->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		return false;
	}
	OutStandAt = Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 5.f);
	return true;
}

bool UMTFrontEndSubsystem::IsInThisMap(const UWorld* World, const FVector& Location)
{
	const FBox Bounds = MTFrontEnd_TerrainBounds(World);
	if (!Bounds.IsValid)
	{
		return true; // no landscape to judge by
	}
	return Location.X >= Bounds.Min.X && Location.X <= Bounds.Max.X && Location.Y >= Bounds.Min.Y && Location.Y <= Bounds.Max.Y;
}

bool UMTFrontEndSubsystem::HasWorldPlaces(const UWorld* World)
{
	const UMTDataRegistry* Registry = World ? UMTDataRegistry::Get(World) : nullptr;
	if (!Registry)
	{
		return false;
	}
	for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
	{
		if ((Pair.Value.bSpawnPoint || Pair.Value.bFastTravel) && IsInThisMap(World, Pair.Value.WorldLocation))
		{
			return true;
		}
	}
	return false;
}

bool UMTFrontEndSubsystem::ResolvePlace(const UWorld* World, const FMTLocationData& Loc, FVector& OutSpot, float& OutYaw)
{
	if (IsInThisMap(World, Loc.WorldLocation))
	{
		OutSpot = Loc.WorldLocation;
		OutYaw = Loc.SpawnYaw;
		return true;
	}
	if (!World || HasWorldPlaces(World))
	{
		return false; // this map is part of the world, just not this place
	}
	// A map with none of the world's places (the 2 km Fittoa test map around Buena, in its own coordinates): its player
	// start stands in for the place.
	TActorIterator<APlayerStart> Start(const_cast<UWorld*>(World));
	if (!Start)
	{
		return false;
	}
	OutSpot = Start->GetActorLocation();
	OutYaw = static_cast<float>(Start->GetActorRotation().Yaw);
	return true;
}

bool UMTFrontEndSubsystem::TeleportTo(FName LocationId)
{
	// Only places this map has: on the Fittoa test map the world's towns do not exist, and a teleport there must not
	// quietly land somewhere else.
	const APlayerController* PC = Controller.Get();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(PC);
	const FMTLocationData* Loc = Registry ? Registry->FindLocation(LocationId) : nullptr;
	FVector StandAt;
	const bool bExists = Pawn && Loc && IsInThisMap(Pawn->GetWorld(), Loc->WorldLocation)
		&& FindSpawnGround(Pawn->GetWorld(), Loc->WorldLocation, Pawn->GetSimpleCollisionHalfHeight(), StandAt);
	if (!bExists)
	{
		return false;
	}
	if (IsOpen())
	{
		HandleSpawn(LocationId);
		Close();
		return true;
	}
	return SpawnAt(LocationId);
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

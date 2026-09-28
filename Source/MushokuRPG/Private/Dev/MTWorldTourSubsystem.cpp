#include "Dev/MTWorldTourSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "ContentStreaming.h"
#include "Core/MTDataRegistry.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "Misc/Paths.h"
#include "MushokuRPG.h"
#include "UI/MTHUD.h"
#include "UI/LaPlace/MTFrontEndSubsystem.h"
#include "UnrealClient.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

void UMTWorldTourSubsystem::Start(APlayerController* PC, const FString& Options)
{
	Controller = PC;
	BuildShots(Options);
	Index = 0;
	Clock = 0.f;
	ReadyClock = 0.f;
	bCaptured = false;
	UE_LOG(LogMushoku, Log, TEXT("[WorldTour] %d shots"), Shots.Num());
	Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMTWorldTourSubsystem::Tick));
}

void UMTWorldTourSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(Handle);
	Super::Deinitialize();
}

void UMTWorldTourSubsystem::BuildShots(const FString& Options)
{
	Shots.Reset();
	APlayerController* PC = Controller.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	FBox Bounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		Bounds += It->GetComponentsBoundingBox(true);
	}
	if (!Bounds.IsValid)
	{
		Bounds = FBox(FVector(-50000.f, -50000.f, -1000.f), FVector(50000.f, 50000.f, 10000.f));
	}
	const FString Only = UGameplayStatics::ParseOption(Options, TEXT("TourOnly"));
	TArray<FString> OnlyList;
	Only.ParseIntoArray(OnlyList, TEXT(","));

	if (const UMTDataRegistry* Registry = UMTDataRegistry::Get(PC))
	{
		TArray<const FMTLocationData*> Spawns;
		for (const TPair<FName, FMTLocationData>& Pair : Registry->GetLocations())
		{
			const FMTLocationData& Loc = Pair.Value;
			const bool bInside = Loc.WorldLocation.X > Bounds.Min.X && Loc.WorldLocation.X < Bounds.Max.X
				&& Loc.WorldLocation.Y > Bounds.Min.Y && Loc.WorldLocation.Y < Bounds.Max.Y;
			if (Loc.bSpawnPoint && bInside && (OnlyList.IsEmpty() || OnlyList.Contains(Pair.Key.ToString())))
			{
				Spawns.Add(&Loc);
			}
		}
		for (const FMTLocationData* Loc : Spawns)
		{
			if (!Loc->PreviewCamera.Location.IsNearlyZero())
			{
				FShot View;
				View.Name = Loc->LocationID.ToString() + TEXT("_View");
				View.Location = Loc->PreviewCamera.Location;
				View.Rotation = Loc->PreviewCamera.Rotation;
				View.Wait = 6.f;
				Shots.Add(View);
			}
			FShot Foot;
			Foot.Name = Loc->LocationID.ToString() + TEXT("_Ground");
			Foot.SpawnAt = Loc->LocationID;
			Foot.Wait = 5.f;
			Shots.Add(Foot);
		}
	}
	if (UGameplayStatics::GetIntOption(Options, TEXT("TourAerial"), 1) != 0)
	{
		// Four oblique views from beyond each side, one high overview.
		const FVector Center = Bounds.GetCenter();
		const FVector Extent = Bounds.GetExtent();
		const float Altitude = Bounds.Max.Z + FMath::Max(Extent.X, Extent.Y) * 0.35f;
		const struct { const TCHAR* Name; FVector Dir; } Sides[] = {
			{ TEXT("Aerial_South"), FVector(0.f, 1.f, 0.f) }, { TEXT("Aerial_West"), FVector(-1.f, 0.f, 0.f) },
			{ TEXT("Aerial_North"), FVector(0.f, -1.f, 0.f) }, { TEXT("Aerial_East"), FVector(1.f, 0.f, 0.f) },
		};
		for (const auto& Side : Sides)
		{
			FShot Shot;
			Shot.Name = Side.Name;
			Shot.Location = Center + Side.Dir * FVector(Extent.X * 0.55f, Extent.Y * 0.55f, 0.f) + FVector(0.f, 0.f, Altitude - Center.Z);
			const FVector Look = FVector(Center.X, Center.Y, Bounds.Min.Z + Extent.Z * 0.5f) - Shot.Location;
			Shot.Rotation = Look.Rotation();
			Shot.Wait = 6.f;
			Shots.Add(Shot);
		}
		FShot Top;
		Top.Name = TEXT("Aerial_Overview");
		Top.Location = FVector(Center.X, Center.Y + Extent.Y * 0.9f, Bounds.Max.Z + FMath::Max(Extent.X, Extent.Y) * 1.1f);
		Top.Rotation = (FVector(Center.X, Center.Y - Extent.Y * 0.05f, Bounds.Min.Z) - Top.Location).Rotation();
		Top.Wait = 6.f;
		Shots.Add(Top);
	}
}

bool UMTWorldTourSubsystem::IsWorldReady() const
{
	bool bCompiling = false;
#if WITH_EDITOR
	bCompiling = GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
#endif
	const APlayerController* PC = Controller.Get();
	UWorldPartitionSubsystem* Partition = PC && PC->GetWorld() ? PC->GetWorld()->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
	const bool bStreaming = Partition && !Partition->IsAllStreamingCompleted();
	return !bCompiling && !bStreaming;
}

bool UMTWorldTourSubsystem::Tick(float DeltaTime)
{
	APlayerController* PC = Controller.Get();
	if (!PC || !PC->GetWorld())
	{
		return false;
	}
	if (!Shots.IsValidIndex(Index))
	{
		if (Clock > 2.f)
		{
			FPlatformMisc::RequestExit(false, TEXT("World tour finished"));
			return false;
		}
		Clock += DeltaTime;
		return true;
	}
	const FShot& Shot = Shots[Index];
	AMTHUD* HUD = Cast<AMTHUD>(PC->GetHUD());
	if (Clock == 0.f)
	{
		// Frame the shot.
		if (!Shot.SpawnAt.IsNone())
		{
			if (UMTFrontEndSubsystem* FrontEnd = UMTFrontEndSubsystem::Get(PC))
			{
				FrontEnd->SetController(PC);
				FrontEnd->SpawnAt(Shot.SpawnAt);
			}
			PC->SetViewTarget(PC->GetPawn());
			if (HUD)
			{
				HUD->SetGameplayHudHidden(false);
			}
		}
		else
		{
			ACameraActor* Cam = Camera.Get();
			if (!Cam)
			{
				Cam = PC->GetWorld()->SpawnActor<ACameraActor>();
				Cam->GetCameraComponent()->SetFieldOfView(75.f);
				Cam->GetCameraComponent()->bConstrainAspectRatio = false;
				Camera = Cam;
			}
			Cam->SetActorLocationAndRotation(Shot.Location, Shot.Rotation);
			PC->SetViewTarget(Cam);
			if (HUD)
			{
				HUD->SetGameplayHudHidden(true);
			}
		}
		IStreamingManager::Get().StreamAllResources(1.f);
	}
	Clock += DeltaTime;
	ReadyClock = IsWorldReady() ? ReadyClock + DeltaTime : 0.f;
	if (!bCaptured && Clock >= Shot.Wait && (ReadyClock > 1.5f || Clock > 90.f))
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("WorldTour");
		const FString File = Dir / FString::Printf(TEXT("%02d_%s.png"), Index + 1, *Shot.Name);
		FScreenshotRequest::RequestScreenshot(File, !Shot.SpawnAt.IsNone(), false);
		UE_LOG(LogMushoku, Log, TEXT("[WorldTour] %s (ready after %.1f s)"), *File, Clock);
		bCaptured = true;
	}
	if (bCaptured && Clock >= Shot.Wait + 1.f && ReadyClock > 0.f)
	{
		++Index;
		Clock = 0.f;
		ReadyClock = 0.f;
		bCaptured = false;
	}
	return true;
}

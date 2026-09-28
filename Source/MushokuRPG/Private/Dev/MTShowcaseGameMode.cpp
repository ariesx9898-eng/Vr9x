#include "Dev/MTShowcaseGameMode.h"
#include "AI/MTEnemyCharacter.h"
#include "Character/MTPlayerCharacter.h"
#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbilityComponent.h"
#include "Core/MTDataRegistry.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "VFX/MTVFXLibrary.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
	const TCHAR* DefaultShowcase[] = {
		TEXT("Rudeus_Basic"), TEXT("Rudeus_StoneCannon"), TEXT("Rudeus_Quagmire"), TEXT("Rudeus_ElementalBarrage"), TEXT("Rudeus_DemonEye"),
		TEXT("Rudeus_Awakening_QuagmireMagician"),
		TEXT("Fire_Fireball"), TEXT("Fire_FlameWave"), TEXT("Fire_Inferno"),
		TEXT("Water_WaterBullet"), TEXT("Water_WaterDragon"), TEXT("Water_Flood"),
		TEXT("Wind_WindBlade"), TEXT("Wind_Tornado"), TEXT("Wind_WindBurst"),
		TEXT("Earth_StoneCannon"), TEXT("Earth_EarthWall"), TEXT("Earth_EarthSpikes"),
		TEXT("Orsted_Basic"), TEXT("Orsted_DisturbMagic"), TEXT("Orsted_DragonStep"), TEXT("Orsted_DragonCrush"),
		TEXT("Orsted_SaintDragonAura"), TEXT("Orsted_Awakening_DragonGod"),
	};
	constexpr float SecondsPerAbility = 3.8f;

	FVector Ground(UWorld* World, const FVector& P)
	{
		FHitResult Hit;
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		if (World && World->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 3000.f), P - FVector(0.f, 0.f, 20000.f), Objects))
		{
			return Hit.ImpactPoint;
		}
		return P;
	}
}

AMTShowcaseGameMode::AMTShowcaseGameMode()
{
	bFrontEndOnStart = false;
	PrimaryActorTick.bCanEverTick = true;
	ShotTimes = { 0.2f, 0.45f, 0.8f, 1.2f, 1.7f, 2.4f, 3.3f };
}

void AMTShowcaseGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const FString List = UGameplayStatics::ParseOption(Options, TEXT("Abilities"));
	if (!List.IsEmpty())
	{
		TArray<FString> Parts;
		List.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			Abilities.Add(FName(*Part.TrimStartAndEnd()));
		}
	}
	else
	{
		for (const TCHAR* Id : DefaultShowcase)
		{
			Abilities.Add(FName(Id));
		}
	}
}

void AMTShowcaseGameMode::BeginPlay()
{
	Super::BeginPlay();
	Clock = 0.f;
}

void AMTShowcaseGameMode::SetupStage()
{
	UWorld* World = GetWorld();
	AMTPlayerCharacter* Player = Cast<AMTPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!World || !Player)
	{
		return;
	}
	StageOrigin = Ground(World, Player->GetActorLocation());
	Player->SetActorLocationAndRotation(StageOrigin + FVector(0.f, 0.f, Player->GetSimpleCollisionHalfHeight() + 5.f), FRotator::ZeroRotator,
		false, nullptr, ETeleportType::TeleportPhysics);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FTransform DummyAt(FRotator(0.f, 180.f, 0.f), StageOrigin + FVector(1200.f, 0.f, 100.f));
	if (AMTEnemyCharacter* Target = World->SpawnActorDeferred<AMTEnemyCharacter>(AMTEnemyCharacter::StaticClass(), DummyAt, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
	{
		Target->EnemyId = TEXT("Arena_Rudeus");
		Target->AutoPossessAI = EAutoPossessAI::Disabled;
		Target->FinishSpawning(DummyAt);
		Dummy = Target;
	}

	ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), StageOrigin + FVector(450.f, -1350.f, 420.f), FRotator::ZeroRotator, Params);
	if (Cam)
	{
		const FVector Look = StageOrigin + FVector(550.f, 0.f, 140.f);
		Cam->SetActorRotation((Look - Cam->GetActorLocation()).Rotation());
		Cam->GetCameraComponent()->SetFieldOfView(70.f);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetViewTarget(Cam);
		}
		Camera = Cam;
	}
	bStaged = true;
	UE_LOG(LogMushoku, Display, TEXT("[Showcase] Stage at %s, %d abilities"), *StageOrigin.ToCompactString(), Abilities.Num());
}

void AMTShowcaseGameMode::StartAbility(int32 Index)
{
	Current = Index;
	AbilityClock = 0.f;
	NextShot = 0;
	bReleased = false;
	AMTPlayerCharacter* Player = Cast<AMTPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Player || !Registry || !Abilities.IsValidIndex(Index))
	{
		return;
	}
	const FName Id = Abilities[Index];
	const FMTAbilityData* Row = Registry->FindAbility(Id);
	if (!Row)
	{
		UE_LOG(LogMushoku, Warning, TEXT("[Showcase] unknown ability %s"), *Id.ToString());
		return;
	}
	const FName Lineage = Row->CharacterRequirement == TEXT("Orsted") ? FName(TEXT("Orsted")) : FName(TEXT("Rudeus"));
	if (Player->GetCharacterId() != Lineage)
	{
		Player->ApplyCharacterLineage(Lineage);
	}
	Player->SetActorLocationAndRotation(StageOrigin + FVector(0.f, 0.f, Player->GetSimpleCollisionHalfHeight() + 5.f), FRotator::ZeroRotator,
		false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* Controller = Player->GetController())
	{
		Controller->SetControlRotation(FRotator::ZeroRotator);
	}
	float Distance = 1200.f;
	if (Row->Behavior == EMTAbilityBehavior::Melee)
	{
		Distance = 250.f;
	}
	else if (Row->ZoneKind == EMTZoneKind::Burst && Row->Behavior == EMTAbilityBehavior::Zone)
	{
		Distance = 450.f;
	}
	if (AMTEnemyCharacter* Target = Dummy.Get())
	{
		Target->SetActorLocationAndRotation(StageOrigin + FVector(Distance, 0.f, Target->GetSimpleCollisionHalfHeight() + 5.f), FRotator(0.f, 180.f, 0.f),
			false, nullptr, ETeleportType::TeleportPhysics);
		if (UMTAttributeComponent* Attr = Target->GetAttributes())
		{
			Attr->InitializeAttributes(100000.f, 1000.f, 100.f, 100000.f, 0.f);
		}
		Player->SetLockTarget(Target);
	}
	if (UMTAttributeComponent* Attr = Player->GetAttributes())
	{
		Attr->RestoreMana(100000.f);
		Attr->RestoreStamina(1000.f);
		Attr->AddAwakeningMeter(100.f);
	}
	const bool bOk = Player->GetAbilities()->ActivateAbilityById(Id);
	UE_LOG(LogMushoku, Display, TEXT("[Showcase] %02d %s (%s): %s"), Index, *Id.ToString(), *Lineage.ToString(), bOk ? TEXT("activated") : TEXT("FAILED to activate"));
}

void AMTShowcaseGameMode::Capture(const FString& Label)
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Showcase");
	FScreenshotRequest::RequestScreenshot(Dir / Label + TEXT(".png"), false, false);
}

void AMTShowcaseGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	if (!bStaged)
	{
		// Let streaming and the pawn settle, and compile every effect material, before building the stage.
		static bool bWarmed = false;
		if (Clock > 2.5f && !bWarmed)
		{
			bWarmed = true;
			for (const TCHAR* Path : { MTVFX::Paths::MatGlow, MTVFX::Paths::MatSprite, MTVFX::Paths::MatSmoke, MTVFX::Paths::MatWater, MTVFX::Paths::MatAir,
				MTVFX::Paths::MatRock, MTVFX::Paths::MatGhost, MTVFX::Paths::DecalScorch, MTVFX::Paths::DecalCracks, MTVFX::Paths::DecalWet, MTVFX::Paths::DecalCircle })
			{
				MTVFX::LoadMaterial(Path);
			}
		}
		bool bCompiling = false;
#if WITH_EDITOR
		bCompiling = GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
#endif
		if (Clock > 3.f && (!bCompiling || Clock > 240.f))
		{
			SetupStage();
			StartAbility(0);
		}
		return;
	}
	if (!Abilities.IsValidIndex(Current))
	{
		if (Clock > 1.f && Current >= Abilities.Num())
		{
			FPlatformMisc::RequestExit(false, TEXT("Showcase finished"));
		}
		return;
	}
	AbilityClock += DeltaSeconds;
	const FName Id = Abilities[Current];
	if (!bReleased && AbilityClock > 1.0f)
	{
		bReleased = true;
		if (AMTPlayerCharacter* Player = Cast<AMTPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		{
			Player->GetAbilities()->ReleaseAbilityById(Id);
		}
	}
	if (ShotTimes.IsValidIndex(NextShot) && AbilityClock >= ShotTimes[NextShot])
	{
		Capture(FString::Printf(TEXT("%02d_%s_%04d"), Current, *Id.ToString(), FMath::RoundToInt(ShotTimes[NextShot] * 1000.f)));
		++NextShot;
	}
	if (AbilityClock >= SecondsPerAbility)
	{
		if (Current + 1 < Abilities.Num())
		{
			StartAbility(Current + 1);
		}
		else
		{
			Current = Abilities.Num();
			Clock = 0.f;
		}
	}
}

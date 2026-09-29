#include "Dev/MTShowcaseGameMode.h"
#include "AI/MTEnemyCharacter.h"
#include "Character/MTPlayerCharacter.h"
#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbility.h"
#include "Abilities/MTAbilityComponent.h"
#include "Combat/MTZoneActor.h"
#include "Combat/MTProjectile.h"
#include "Combat/MTWaterSerpent.h"
#include "Core/MTDataRegistry.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
		// Orsted casts the same element magic: shorter, cleaner, just as hard.
		TEXT("Orsted:Fire_Fireball"), TEXT("Orsted:Fire_Inferno"), TEXT("Orsted:Water_WaterDragon"), TEXT("Orsted:Earth_EarthSpikes"),
		TEXT("Orsted:Wind_WindBlade"), TEXT("Orsted:Wind_Tornado"),
		// The two signature combos.
		TEXT("Rudeus_StoneCannon+Rudeus_Quagmire+Rudeus_ElementalBarrage"),
		TEXT("Orsted_DisturbMagic+Orsted_DragonStep+Orsted_DragonCrush"),
	};
	const FName ShowcaseDisturb(TEXT("Orsted_DisturbMagic"));

	FVector ShowcaseGround(UWorld* World, const FVector& P)
	{
		FHitResult Hit;
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		if (World && World->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 3000.f), P - FVector(0.f, 0.f, 20000.f), Objects))
		{
			return Hit.ImpactPoint;
		}
		return P;
	}

	/** Seconds an entry needs to play out: wind-up, its effect, and a beat of aftermath. */
	float ShowcaseSeconds(const FMTAbilityData& Row)
	{
		float Life = Row.CastTime + Row.RecoveryTime + 1.6f;
		if (Row.bChargeable)
		{
			Life += 1.0f;
		}
		if (Row.Behavior == EMTAbilityBehavior::Zone || Row.Behavior == EMTAbilityBehavior::Structure)
		{
			Life += FMath::Min(Row.Duration, 5.f);
		}
		else if (Row.Behavior == EMTAbilityBehavior::Barrage)
		{
			Life += Row.GetParam(TEXT("BarrageTime"), 3.f) + Row.GetParam(TEXT("CollapseTime"), 0.35f) + 0.6f;
		}
		else if (Row.Behavior == EMTAbilityBehavior::Serpent)
		{
			Life += Row.GetParam(TEXT("CircleTime"), 0.5f) + 1.6f;
		}
		return FMath::Clamp(Life, 3.8f, 9.f);
	}
}

AMTShowcaseGameMode::AMTShowcaseGameMode()
{
	bFrontEndOnStart = false;
	PrimaryActorTick.bCanEverTick = true;
	// Fractions of each entry's length.
	ShotTimes = { 0.05f, 0.12f, 0.22f, 0.33f, 0.46f, 0.62f, 0.85f };
}

void AMTShowcaseGameMode::AddEntry(const FString& Token)
{
	FString Body = Token.TrimStartAndEnd();
	if (Body.IsEmpty())
	{
		return;
	}
	FShowcaseEntry Entry;
	FString Prefix;
	FString Rest;
	if (Body.Split(TEXT(":"), &Prefix, &Rest))
	{
		Entry.Lineage = FName(*Prefix.TrimStartAndEnd());
		Body = Rest;
	}
	TArray<FString> Parts;
	Body.ParseIntoArray(Parts, TEXT("+"), true);
	for (const FString& Part : Parts)
	{
		Entry.Chain.Add(FName(*Part.TrimStartAndEnd()));
	}
	if (Entry.Chain.Num() == 0)
	{
		return;
	}
	Entry.Label = Body.Replace(TEXT("+"), TEXT("-"));
	if (!Entry.Lineage.IsNone())
	{
		Entry.Label = Entry.Lineage.ToString() + TEXT("-") + Entry.Label;
	}
	Entries.Add(Entry);
}

void AMTShowcaseGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const FString List = UGameplayStatics::ParseOption(Options, TEXT("Abilities"));
	if (!List.IsEmpty())
	{
		TArray<FString> Tokens;
		List.ParseIntoArray(Tokens, TEXT(","), true);
		for (const FString& Token : Tokens)
		{
			AddEntry(Token);
		}
	}
	else
	{
		for (const TCHAR* Token : DefaultShowcase)
		{
			AddEntry(Token);
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
	StageOrigin = ShowcaseGround(World, Player->GetActorLocation());
	Player->SetActorLocationAndRotation(StageOrigin + FVector(0.f, 0.f, Player->GetSimpleCollisionHalfHeight() + 5.f), FRotator::ZeroRotator,
		false, nullptr, ETeleportType::TeleportPhysics);

	// The main dummy (a caster, so Disturb Magic has something to disturb) and a crowd around it: area spells should
	// visibly catch several enemies.
	auto SpawnDummy = [World](FName EnemyId, const FVector& Where) -> AMTEnemyCharacter*
	{
		const FTransform At(FRotator(0.f, 180.f, 0.f), Where);
		AMTEnemyCharacter* Target = World->SpawnActorDeferred<AMTEnemyCharacter>(AMTEnemyCharacter::StaticClass(), At, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Target)
		{
			Target->EnemyId = EnemyId;
			Target->AutoPossessAI = EAutoPossessAI::Disabled;
			Target->FinishSpawning(At);
		}
		return Target;
	};
	Dummy = SpawnDummy(TEXT("Arena_Rudeus"), StageOrigin + FVector(1200.f, 0.f, 100.f));
	for (int32 i = 0; i < 4; ++i)
	{
		Crowd.Add(SpawnDummy(TEXT("Enemy_Bandit"), StageOrigin + FVector(1200.f, (i < 2 ? -1.f : 1.f) * (i % 2 == 0 ? 220.f : 440.f), 100.f)));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), StageOrigin + FVector(450.f, -1550.f, 520.f), FRotator::ZeroRotator, Params);
	if (Cam)
	{
		const FVector Look = StageOrigin + FVector(600.f, 0.f, 140.f);
		Cam->SetActorRotation((Look - Cam->GetActorLocation()).Rotation());
		Cam->GetCameraComponent()->SetFieldOfView(75.f);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetViewTarget(Cam);
		}
		Camera = Cam;
	}
	bStaged = true;
	UE_LOG(LogMushoku, Display, TEXT("[Showcase] Stage at %s, %d entries"), *StageOrigin.ToCompactString(), Entries.Num());
}

void AMTShowcaseGameMode::StartEntry(int32 Index)
{
	Current = Index;
	EntryClock = 0.f;
	StepClock = 0.f;
	ChainIndex = 0;
	Executed = 0;
	NextShot = 0;
	bReleased = false;
	AMTPlayerCharacter* Player = Cast<AMTPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Player || !Registry || !Entries.IsValidIndex(Index))
	{
		return;
	}
	FShowcaseEntry& Entry = Entries[Index];
	const FMTAbilityData* First = Registry->FindAbility(Entry.Chain[0]);
	if (!First)
	{
		UE_LOG(LogMushoku, Warning, TEXT("[Showcase] unknown ability %s"), *Entry.Chain[0].ToString());
		return;
	}
	FName Lineage = Entry.Lineage;
	if (Lineage.IsNone())
	{
		Lineage = First->CharacterRequirement == TEXT("Orsted") ? FName(TEXT("Orsted")) : FName(TEXT("Rudeus"));
	}
	if (Player->GetCharacterId() != Lineage)
	{
		Player->ApplyCharacterLineage(Lineage);
	}
	Entry.Seconds = 0.f;
	for (const FName Id : Entry.Chain)
	{
		if (const FMTAbilityData* Row = Registry->FindAbility(Id))
		{
			Entry.Seconds += ShowcaseSeconds(*Row);
		}
	}
	Entry.Seconds = FMath::Clamp(Entry.Seconds, 3.8f, 18.f);
	Player->SetActorLocationAndRotation(StageOrigin + FVector(0.f, 0.f, Player->GetSimpleCollisionHalfHeight() + 5.f), FRotator::ZeroRotator,
		false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* Controller = Player->GetController())
	{
		Controller->SetControlRotation(FRotator::ZeroRotator);
	}
	float Distance = 1200.f;
	if (First->Behavior == EMTAbilityBehavior::Melee || First->Behavior == EMTAbilityBehavior::Strike)
	{
		Distance = 320.f;
	}
	else if (First->Behavior == EMTAbilityBehavior::Zone && (First->ZoneKind == EMTZoneKind::Burst || First->ZoneKind == EMTZoneKind::Arc))
	{
		Distance = First->ZoneKind == EMTZoneKind::Arc ? 800.f : 450.f;
	}
	auto Place = [this](AMTEnemyCharacter* Target, const FVector& Offset)
	{
		if (!Target)
		{
			return;
		}
		Target->SetActorLocationAndRotation(StageOrigin + Offset + FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight() + 5.f), FRotator(0.f, 180.f, 0.f),
			false, nullptr, ETeleportType::TeleportPhysics);
		if (UMTAttributeComponent* Attr = Target->GetAttributes())
		{
			Attr->InitializeAttributes(100000.f, 1000.f, 100.f, 100000.f, 0.f);
		}
		if (UMTAbilityComponent* TargetAbilities = Target->GetAbilities())
		{
			TargetAbilities->CancelAll();
		}
	};
	Place(Dummy.Get(), FVector(Distance, 0.f, 0.f));
	for (int32 i = 0; i < Crowd.Num(); ++i)
	{
		const float Side = (i < 2 ? -1.f : 1.f) * (i % 2 == 0 ? 230.f : 460.f);
		Place(Crowd[i].Get(), FVector(Distance + (i % 2 == 0 ? 120.f : -60.f), Side, 0.f));
	}
	if (AMTEnemyCharacter* Target = Dummy.Get())
	{
		Player->SetLockTarget(Target);
		DummyHealthAtStart = Target->GetAttributes() ? Target->GetAttributes()->GetHealth() : 0.f;
		// Something for Disturb Magic to disturb: the dummy starts forming a Stone Cannon.
		if (Entry.Chain.Contains(ShowcaseDisturb) && Target->GetAbilities())
		{
			Target->GetAbilities()->ActivateAbilityById(TEXT("Rudeus_StoneCannon"));
		}
	}
	if (UMTAttributeComponent* Attr = Player->GetAttributes())
	{
		Attr->RestoreMana(100000.f);
		Attr->RestoreStamina(1000.f);
		Attr->AddAwakeningMeter(100.f);
	}
	const bool bOk = Player->GetAbilities()->ActivateAbilityById(Entry.Chain[0]);
	Executed += bOk ? 1 : 0;
	UE_LOG(LogMushoku, Display, TEXT("[Showcase] %02d %s (%s): %s"), Index, *Entry.Label, *Lineage.ToString(), bOk ? TEXT("activated") : TEXT("FAILED to activate"));
}

void AMTShowcaseGameMode::AdvanceChain()
{
	AMTPlayerCharacter* Player = Cast<AMTPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Player || !Entries.IsValidIndex(Current))
	{
		return;
	}
	const FShowcaseEntry& Entry = Entries[Current];
	if (!Entry.Chain.IsValidIndex(ChainIndex + 1))
	{
		return;
	}
	// The next link starts as soon as the previous one lets go (combo inputs are buffered in play; here we wait).
	UMTAbilityComponent* Abilities = Player->GetAbilities();
	if (StepClock < 0.35f || Abilities->IsCasting())
	{
		return;
	}
	++ChainIndex;
	StepClock = 0.f;
	bReleased = false;
	Player->GetAttributes()->RestoreMana(100000.f);
	Player->GetAttributes()->RestoreStamina(1000.f);
	const bool bOk = Abilities->ActivateAbilityById(Entry.Chain[ChainIndex]);
	Executed += bOk ? 1 : 0;
	UE_LOG(LogMushoku, Display, TEXT("[Showcase]    + %s: %s"), *Entry.Chain[ChainIndex].ToString(), bOk ? TEXT("activated") : TEXT("FAILED to activate"));
}

void AMTShowcaseGameMode::FinishEntry()
{
	if (!Entries.IsValidIndex(Current))
	{
		return;
	}
	const FShowcaseEntry& Entry = Entries[Current];
	const AMTEnemyCharacter* Target = Dummy.Get();
	const float Dealt = (Target && Target->GetAttributes()) ? DummyHealthAtStart - Target->GetAttributes()->GetHealth() : 0.f;
	int32 CrowdHit = 0;
	for (const TWeakObjectPtr<AMTEnemyCharacter>& Weak : Crowd)
	{
		const AMTEnemyCharacter* Member = Weak.Get();
		CrowdHit += (Member && Member->GetAttributes() && Member->GetAttributes()->GetHealth() < 100000.f) ? 1 : 0;
	}
	UE_LOG(LogMushoku, Display, TEXT("[Showcase] RESULT %02d %s: %s (%d/%d cast), main dummy took %.0f, %d of %d crowd dummies hit"),
		Current, *Entry.Label, Executed == Entry.Chain.Num() ? TEXT("PASS") : TEXT("FAIL"), Executed, Entry.Chain.Num(), Dealt, CrowdHit, Crowd.Num());
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
			StartEntry(0);
		}
		return;
	}
	if (!Entries.IsValidIndex(Current))
	{
		if (Clock > 1.f && Current >= Entries.Num())
		{
			FPlatformMisc::RequestExit(false, TEXT("Showcase finished"));
		}
		return;
	}
	EntryClock += DeltaSeconds;
	StepClock += DeltaSeconds;
	const FShowcaseEntry& Entry = Entries[Current];
	const FName Id = Entry.Chain[ChainIndex];
	// Charged spells are held for a second (full-looking charge), everything else is a tap.
	if (!bReleased && StepClock > 1.0f)
	{
		bReleased = true;
		if (AMTPlayerCharacter* Player = Cast<AMTPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		{
			Player->GetAbilities()->ReleaseAbilityById(Id);
		}
	}
	if (bReleased || StepClock > 0.35f)
	{
		AdvanceChain();
	}
	if (ShotTimes.IsValidIndex(NextShot) && EntryClock >= ShotTimes[NextShot] * Entry.Seconds)
	{
		Capture(FString::Printf(TEXT("%02d_%s_%04d"), Current, *Entry.Label, FMath::RoundToInt(ShotTimes[NextShot] * Entry.Seconds * 1000.f)));
		++NextShot;
	}
	if (EntryClock >= Entry.Seconds)
	{
		FinishEntry();
		if (Current + 1 < Entries.Num())
		{
			StartEntry(Current + 1);
		}
		else
		{
			Current = Entries.Num();
			Clock = 0.f;
		}
	}
}

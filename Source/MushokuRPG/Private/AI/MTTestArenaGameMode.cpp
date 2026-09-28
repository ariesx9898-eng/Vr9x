#include "AI/MTTestArenaGameMode.h"

#include "AI/MTEnemyCharacter.h"
#include "AI/MTEnemySpawner.h"
#include "AI/MTEnemyAIController.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Core/MTGameEvents.h"
#include "Progression/MTProgressionSubsystem.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MTArena"

AMTTestArenaGameMode::AMTTestArenaGameMode()
{
	bFrontEndOnStart = false;
	OpponentClass = AMTEnemyCharacter::StaticClass();
}

void AMTTestArenaGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const FString PlayAs = UGameplayStatics::ParseOption(Options, TEXT("PlayAs"));
	if (!PlayAs.IsEmpty())
	{
		PlayerLineageOverride = PlayAs.Contains(OrstedLineageId.ToString()) ? OrstedLineageId : RudeusLineageId;
	}
}

void AMTTestArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	ArenaStartTime = GetWorld()->GetTimeSeconds();

	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->OnAbilityUsed.AddUniqueDynamic(this, &AMTTestArenaGameMode::HandleAbilityUsed);
	}
	// After BeginPlay the player pawn exists and the save subsystem has applied the build.
	GetWorldTimerManager().SetTimer(SpawnTimer, this, &AMTTestArenaGameMode::SpawnOpponent, FMath::Max(0.1f, SpawnDelay), false);
	GetWorldTimerManager().SetTimer(ReportTimer, this, &AMTTestArenaGameMode::PrintCombatReport, FMath::Max(1.f, ReportInterval), true);
	GetWorldTimerManager().SetTimer(PollTimer, this, &AMTTestArenaGameMode::PollCombatants, 0.1f, true);
}

void AMTTestArenaGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->OnAbilityUsed.RemoveDynamic(this, &AMTTestArenaGameMode::HandleAbilityUsed);
	}
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

AMTEnemyCharacter* AMTTestArenaGameMode::GetOpponent() const
{
	return Opponent.Get();
}

AMTCharacterBase* AMTTestArenaGameMode::GetPlayerCharacter() const
{
	return Cast<AMTCharacterBase>(UGameplayStatics::GetPlayerPawn(this, 0));
}

FName AMTTestArenaGameMode::GetPlayerLineage() const
{
	if (!PlayerLineageOverride.IsNone())
	{
		return PlayerLineageOverride;
	}
	if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		const FName Equipped = Progression->GetEquippedCharacter();
		if (!Equipped.IsNone())
		{
			return Equipped;
		}
	}
	if (const AMTCharacterBase* Player = GetPlayerCharacter())
	{
		if (!Player->GetCharacterId().IsNone())
		{
			return Player->GetCharacterId();
		}
	}
	return RudeusLineageId;
}

FTransform AMTTestArenaGameMode::GetArenaStartTransform() const
{
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const AActor* Spot = PC->StartSpot.Get())
		{
			return Spot->GetActorTransform();
		}
	}
	if (TActorIterator<APlayerStart> It(GetWorld()); It)
	{
		return It->GetActorTransform();
	}
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return Pawn->GetActorTransform();
	}
	return FTransform::Identity;
}

void AMTTestArenaGameMode::SpawnOpponent()
{
	DestroyOpponent();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	AMTCharacterBase* Player = GetPlayerCharacter();
	BindPlayer(Player);
	// The launcher's character choice: the save starts everyone as Rudeus.
	if (Player && !PlayerLineageOverride.IsNone() && Player->GetCharacterId() != PlayerLineageOverride)
	{
		Player->ApplyCharacterLineage(PlayerLineageOverride);
	}

	const bool bPlayerIsOrsted = GetPlayerLineage().ToString().Contains(OrstedLineageId.ToString());
	const FName OpponentId = bPlayerIsOrsted ? RudeusOpponentId : OrstedOpponentId;

	// In front of the player where they stand now: the start spot can be far away after a respawn, or above the
	// ground while the landscape is still a stand-in.
	const FTransform Start = Player ? Player->GetActorTransform() : GetArenaStartTransform();
	FVector Forward = Start.GetRotation().GetForwardVector();
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Desired = Start.GetLocation() + Forward * OpponentDistance;
	const TSubclassOf<AMTEnemyCharacter> Class = OpponentClass ? OpponentClass : TSubclassOf<AMTEnemyCharacter>(AMTEnemyCharacter::StaticClass());

	// Exactly 1500 cm ahead when that spot is clear, otherwise the nearest valid point around it.
	AMTEnemyCharacter* Spawned = AMTEnemySpawner::SpawnEnemyNear(World, Class, OpponentId, Desired, 40.f, 3, this);
	if (!Spawned)
	{
		Spawned = AMTEnemySpawner::SpawnEnemyNear(World, Class, OpponentId, Desired, 500.f, 16, this);
	}
	if (!Spawned)
	{
		UE_LOG(LogMushoku, Warning, TEXT("[Arena] Could not spawn opponent %s near %s."), *OpponentId.ToString(), *Desired.ToCompactString());
		return;
	}

	FVector ToStart = Start.GetLocation() - Spawned->GetActorLocation();
	ToStart.Z = 0.f;
	if (!ToStart.IsNearlyZero())
	{
		Spawned->SetActorRotation(FRotator(0.f, ToStart.Rotation().Yaw, 0.f));
	}
	Opponent = Spawned;
	if (UMTAttributeComponent* Attr = Spawned->GetAttributes())
	{
		Attr->OnDamaged.AddUniqueDynamic(this, &AMTTestArenaGameMode::HandleOpponentDamaged);
		Attr->OnDeath.AddUniqueDynamic(this, &AMTTestArenaGameMode::HandleOpponentDeath);
	}
	PerfectDefenseHandle = Spawned->OnPerfectDefense.AddUObject(this, &AMTTestArenaGameMode::HandleOpponentPerfectDefense);
	bOpponentWasDodging = false;

	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->Notify(FText::Format(LOCTEXT("OpponentEnters", "Arena: {0} enters the arena"), FText::FromName(OpponentId)), FLinearColor(0.9f, 0.8f, 0.5f));
	}
	UE_LOG(LogMushoku, Log, TEXT("[Arena] Spawned %s (%s) vs player lineage %s."), *OpponentId.ToString(), *Spawned->GetName(), *GetPlayerLineage().ToString());
}

void AMTTestArenaGameMode::DestroyOpponent()
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	if (AMTEnemyCharacter* Existing = Opponent.Get())
	{
		if (UMTAttributeComponent* Attr = Existing->GetAttributes())
		{
			Attr->OnDamaged.RemoveDynamic(this, &AMTTestArenaGameMode::HandleOpponentDamaged);
			Attr->OnDeath.RemoveDynamic(this, &AMTTestArenaGameMode::HandleOpponentDeath);
		}
		Existing->OnPerfectDefense.Remove(PerfectDefenseHandle);
		Existing->Destroy();
	}
	Opponent = nullptr;
	PerfectDefenseHandle.Reset();
}

void AMTTestArenaGameMode::BindPlayer(AMTCharacterBase* Player)
{
	if (BoundPlayer.Get() == Player)
	{
		return;
	}
	if (AMTCharacterBase* Old = BoundPlayer.Get())
	{
		if (UMTAttributeComponent* Attr = Old->GetAttributes())
		{
			Attr->OnDamaged.RemoveDynamic(this, &AMTTestArenaGameMode::HandlePlayerDamaged);
		}
	}
	BoundPlayer = Player;
	bPlayerWasDodging = false;
	bPlayerWasAlive = Player && Player->IsAlive();
	if (Player)
	{
		if (UMTAttributeComponent* Attr = Player->GetAttributes())
		{
			Attr->OnDamaged.AddUniqueDynamic(this, &AMTTestArenaGameMode::HandlePlayerDamaged);
		}
	}
}

void AMTTestArenaGameMode::ResetArena()
{
	DestroyOpponent();

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	AMTCharacterBase* Player = GetPlayerCharacter();
	const FTransform Start = GetArenaStartTransform();
	if (PC && (!Player || !Player->IsAlive()))
	{
		if (APawn* OldPawn = PC->GetPawn())
		{
			PC->UnPossess();
			OldPawn->Destroy();
		}
		RestartPlayer(PC);
		Player = GetPlayerCharacter();
		if (Player)
		{
			Player->ApplyCharacterLineage(GetPlayerLineage());
		}
	}
	else if (Player)
	{
		Player->SetActorLocationAndRotation(Start.GetLocation(), Start.GetRotation().Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
		if (PC)
		{
			PC->SetControlRotation(Start.GetRotation().Rotator());
		}
		Player->ApplyCharacterLineage(GetPlayerLineage()); // re-initialises attributes (full heal)
	}

	PlayerStats = FMTArenaStats();
	OpponentStats = FMTArenaStats();
	OpponentsDefeated = 0;
	PlayerDeaths = 0;
	ArenaStartTime = GetWorld()->GetTimeSeconds();
	BindPlayer(Player);

	GetWorldTimerManager().SetTimer(SpawnTimer, this, &AMTTestArenaGameMode::SpawnOpponent, FMath::Max(0.1f, SpawnDelay), false);
	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->Notify(LOCTEXT("ArenaReset", "Arena reset"), FLinearColor(0.8f, 0.8f, 0.8f));
	}
	UE_LOG(LogMushoku, Log, TEXT("[Arena] Reset (player lineage %s)."), *GetPlayerLineage().ToString());
}

void AMTTestArenaGameMode::SwapCharacters()
{
	const bool bPlayerIsOrsted = GetPlayerLineage().ToString().Contains(OrstedLineageId.ToString());
	PlayerLineageOverride = bPlayerIsOrsted ? RudeusLineageId : OrstedLineageId;
	UE_LOG(LogMushoku, Log, TEXT("[Arena] Swapping: player becomes %s."), *PlayerLineageOverride.ToString());
	ResetArena(); // re-applies the player lineage and spawns the opposite opponent
}

// ---------------------------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------------------------

void AMTTestArenaGameMode::HandleOpponentDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result)
{
	if (Result.bDodged)
	{
		++OpponentStats.HitsEvaded;
		return;
	}
	if (Result.bBlocked)
	{
		++OpponentStats.HitsBlocked;
	}
	if (Result.DamageDealt > 0.f)
	{
		++PlayerStats.HitsLanded;
		PlayerStats.DamageDealt += Result.DamageDealt;
	}
}

void AMTTestArenaGameMode::HandlePlayerDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result)
{
	if (Result.bDodged)
	{
		++PlayerStats.HitsEvaded;
		return;
	}
	if (Result.bBlocked)
	{
		++PlayerStats.HitsBlocked;
	}
	if (Result.DamageDealt > 0.f)
	{
		++OpponentStats.HitsLanded;
		OpponentStats.DamageDealt += Result.DamageDealt;
	}
}

void AMTTestArenaGameMode::HandleAbilityUsed(FName AbilityId, AActor* User)
{
	if (!AbilityId.ToString().Contains(TEXT("DisturbMagic")))
	{
		return;
	}
	if (User && User == BoundPlayer.Get())
	{
		++PlayerStats.DisturbCasts;
	}
	else if (User && User == Opponent.Get())
	{
		++OpponentStats.DisturbCasts;
	}
}

void AMTTestArenaGameMode::HandleOpponentPerfectDefense(AMTEnemyCharacter* Enemy, FName Kind)
{
	if (Kind == TEXT("DisturbMagic"))
	{
		++OpponentStats.SpellsDisrupted;
	}
}

void AMTTestArenaGameMode::HandleOpponentDeath(AActor* Killer)
{
	++OpponentsDefeated;
	UE_LOG(LogMushoku, Log, TEXT("[Arena] Opponent defeated by %s."), *GetNameSafe(Killer));
	PrintCombatReport();
	if (bAutoRespawnOpponent)
	{
		GetWorldTimerManager().SetTimer(SpawnTimer, this, &AMTTestArenaGameMode::SpawnOpponent, FMath::Max(0.5f, OpponentRespawnDelay), false);
	}
}

void AMTTestArenaGameMode::PollCombatants()
{
	// Rising edges of IsDodging() count dodges for both sides (player input and AI alike).
	AMTCharacterBase* Player = GetPlayerCharacter();
	if (Player != BoundPlayer.Get())
	{
		BindPlayer(Player);
	}
	if (Player)
	{
		const bool bDodging = Player->IsDodging();
		if (bDodging && !bPlayerWasDodging)
		{
			++PlayerStats.Dodges;
		}
		bPlayerWasDodging = bDodging;

		const bool bAlive = Player->IsAlive();
		if (bPlayerWasAlive && !bAlive)
		{
			++PlayerDeaths;
			UE_LOG(LogMushoku, Log, TEXT("[Arena] Player defeated - call ResetArena() to go again."));
			PrintCombatReport();
		}
		bPlayerWasAlive = bAlive;
	}
	if (const AMTEnemyCharacter* Enemy = Opponent.Get())
	{
		const bool bDodging = Enemy->IsDodging();
		if (bDodging && !bOpponentWasDodging)
		{
			++OpponentStats.Dodges;
		}
		bOpponentWasDodging = bDodging;
	}
}

void AMTTestArenaGameMode::PrintCombatReport()
{
	const float Elapsed = GetWorld() ? GetWorld()->GetTimeSeconds() - ArenaStartTime : 0.f;
	const AMTEnemyCharacter* Enemy = Opponent.Get();
	const AMTEnemyAIController* AI = Enemy ? Cast<AMTEnemyAIController>(Enemy->GetController()) : nullptr;
	const FString OpponentName = Enemy ? Enemy->EnemyId.ToString() : FString(TEXT("(none)"));

	auto Line = [](const TCHAR* Who, const FMTArenaStats& S)
	{
		return FString::Printf(TEXT("%-8s hits %3d (%6.0f dmg) | disturb casts %2d, spells disrupted %2d | dodges %2d, evaded %2d, blocked %2d"),
			Who, S.HitsLanded, S.DamageDealt, S.DisturbCasts, S.SpellsDisrupted, S.Dodges, S.HitsEvaded, S.HitsBlocked);
	};

	UE_LOG(LogMushoku, Log, TEXT("[Arena] ===== Combat report (%.0fs) player=%s opponent=%s | opponents defeated %d, player deaths %d ====="),
		Elapsed, *GetPlayerLineage().ToString(), *OpponentName, OpponentsDefeated, PlayerDeaths);
	UE_LOG(LogMushoku, Log, TEXT("[Arena] %s"), *Line(TEXT("Player"), PlayerStats));
	UE_LOG(LogMushoku, Log, TEXT("[Arena] %s"), *Line(TEXT("Opponent"), OpponentStats));
	if (AI)
	{
		UE_LOG(LogMushoku, Log, TEXT("[Arena] AI: action=%s attacks %d, dodges %d, blocks %d, disrupt attempts %d, health %.0f%%"),
			*UEnum::GetValueAsString(AI->GetCurrentAction()), AI->GetAttackCount(), AI->GetDodgeCount(), AI->GetBlockCount(),
			AI->GetDisruptAttemptCount(), Enemy->GetHealthFraction() * 100.f);
	}
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor(230, 210, 140), FString::Printf(TEXT("Arena %.0fs | Player: %d hits, %d disrupted | Opponent: %d hits, %d disrupted"),
			Elapsed, PlayerStats.HitsLanded, PlayerStats.SpellsDisrupted, OpponentStats.HitsLanded, OpponentStats.SpellsDisrupted));
	}
#endif
}

#undef LOCTEXT_NAMESPACE

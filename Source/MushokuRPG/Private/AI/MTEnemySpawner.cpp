#include "AI/MTEnemySpawner.h"

#include "AI/MTEnemyCharacter.h"
#include "AI/MTEnemyAIController.h"
#include "Quests/MTQuestSubsystem.h"
#include "Core/MTDataRegistry.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "TimerManager.h"

AMTEnemySpawner::AMTEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	EnemyClass = AMTEnemyCharacter::StaticClass();
}

void AMTEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bRegisterAsQuestActor && !EnemyId.IsNone())
	{
		if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
		{
			Quests->RegisterQuestActor(EnemyId, this);
		}
	}
	if (bSpawnOnBeginPlay)
	{
		SpawnAll();
	}
	// Low-rate bookkeeping (deaths, respawns); staggered across spawners.
	GetWorldTimerManager().SetTimer(SpawnerTimer, this, &AMTEnemySpawner::TickSpawner, 1.f, true, FMath::FRandRange(0.5f, 1.5f));
}

void AMTEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (bRegisterAsQuestActor && !EnemyId.IsNone())
	{
		if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
		{
			Quests->UnregisterQuestActor(EnemyId, this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

int32 AMTEnemySpawner::GetAliveCount() const
{
	int32 Alive = 0;
	for (const TObjectPtr<AMTEnemyCharacter>& Enemy : Spawned)
	{
		if (IsValid(Enemy) && Enemy->IsAlive())
		{
			++Alive;
		}
	}
	return Alive;
}

void AMTEnemySpawner::SpawnAll()
{
	PendingRespawnTimes.Reset();
	const int32 Missing = Count - GetAliveCount();
	for (int32 Index = 0; Index < Missing; ++Index)
	{
		if (!TrySpawnOne())
		{
			// Retry shortly (navmesh may still be building on level load).
			PendingRespawnTimes.Add(GetWorld()->GetTimeSeconds() + 2.f);
		}
	}
}

void AMTEnemySpawner::DespawnAll()
{
	for (const TObjectPtr<AMTEnemyCharacter>& Enemy : Spawned)
	{
		if (IsValid(Enemy))
		{
			Enemy->Destroy();
		}
	}
	Spawned.Reset();
	PendingRespawnTimes.Reset();
}

bool AMTEnemySpawner::IsPlayerNearby() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	return Player && FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) < FMath::Square(MinPlayerDistanceForRespawn);
}

void AMTEnemySpawner::TickSpawner()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();

	// Dead or destroyed enemies free their slot and queue a respawn.
	for (int32 Index = Spawned.Num() - 1; Index >= 0; --Index)
	{
		const AMTEnemyCharacter* Enemy = Spawned[Index];
		if (!IsValid(Enemy) || !Enemy->IsAlive())
		{
			Spawned.RemoveAtSwap(Index);
			if (RespawnDelay > 0.f)
			{
				PendingRespawnTimes.Add(Now + RespawnDelay);
			}
		}
	}

	if (PendingRespawnTimes.Num() == 0 || IsPlayerNearby())
	{
		return;
	}
	for (int32 Index = PendingRespawnTimes.Num() - 1; Index >= 0; --Index)
	{
		if (PendingRespawnTimes[Index] > Now)
		{
			continue;
		}
		if (Spawned.Num() >= Count || TrySpawnOne())
		{
			PendingRespawnTimes.RemoveAtSwap(Index);
		}
		else
		{
			PendingRespawnTimes[Index] = Now + 3.f; // blocked / no navmesh yet: try again later
		}
	}
}

bool AMTEnemySpawner::TrySpawnOne()
{
	if (EnemyId.IsNone() || Spawned.Num() >= Count)
	{
		return false;
	}
	AMTEnemyCharacter* Enemy = SpawnEnemyNear(GetWorld(), EnemyClass, EnemyId, GetActorLocation(), SpawnRadius, MaxSpawnAttempts, this);
	if (!Enemy)
	{
		return false;
	}
	Spawned.Add(Enemy);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Static helpers (also used by the world event director and the test arena)
// ---------------------------------------------------------------------------------------------

bool AMTEnemySpawner::FindValidSpawnLocation(UWorld* World, const FVector& Center, float Radius, float CapsuleRadius, float CapsuleHalfHeight, FVector& OutLocation, int32 MaxAttempts)
{
	if (!World)
	{
		return false;
	}
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTSpawnTest), false);

	// A navigation system with no NavMesh built around here (a map before its navigation is generated) cannot project
	// anything: fall back to finding the floor with traces instead of refusing to spawn.
	if (NavSystem)
	{
		FNavLocation Probe;
		const FVector ProbeExtent(FMath::Max(1000.f, Radius), FMath::Max(1000.f, Radius), 5000.f);
		if (!NavSystem->ProjectPointToNavigation(Center, Probe, ProbeExtent))
		{
			NavSystem = nullptr;
		}
	}

	for (int32 Attempt = 0; Attempt < FMath::Max(1, MaxAttempts); ++Attempt)
	{
		FVector Candidate = Center;
		if (Radius > 0.f && (Attempt > 0 || Radius > CapsuleRadius * 2.f))
		{
			const FVector2D Offset = FMath::RandPointInCircle(Radius);
			Candidate += FVector(Offset.X, Offset.Y, 0.f);
		}

		FVector Ground = Candidate;
		if (NavSystem)
		{
			FNavLocation NavLocation;
			const FVector Extent(FMath::Max(100.f, Radius * 0.25f), FMath::Max(100.f, Radius * 0.25f), 500.f);
			if (!NavSystem->ProjectPointToNavigation(Candidate, NavLocation, Extent))
			{
				continue;
			}
			Ground = NavLocation.Location;
		}
		else
		{
			// No navigation: find the floor with a trace, near the point first, then through the whole column (the
			// point can sit far above or below the ground, e.g. data heights before the landscape is final).
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Candidate + FVector(0.f, 0.f, 500.f), Candidate - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params)
				&& !World->LineTraceSingleByChannel(Hit, Candidate + FVector(0.f, 0.f, 50000.f), Candidate - FVector(0.f, 0.f, 200000.f), ECC_Visibility, Params))
			{
				continue;
			}
			Ground = Hit.ImpactPoint;
		}

		// Capsule centre slightly above the floor; reject anything that intersects geometry or pawns.
		const FVector CapsuleCenter = Ground + FVector(0.f, 0.f, CapsuleHalfHeight + 20.f);
		if (World->OverlapBlockingTestByChannel(CapsuleCenter, FQuat::Identity, ECC_Pawn, Capsule, Params))
		{
			continue;
		}
		OutLocation = CapsuleCenter;
		return true;
	}
	return false;
}

AMTEnemyCharacter* AMTEnemySpawner::SpawnEnemy(UWorld* World, TSubclassOf<AMTEnemyCharacter> Class, FName InEnemyId, const FVector& Location, const FRotator& Rotation, AActor* Owner)
{
	if (!World || !Class)
	{
		return nullptr;
	}
	const FTransform SpawnTransform(Rotation, Location);
	AMTEnemyCharacter* Enemy = World->SpawnActorDeferred<AMTEnemyCharacter>(Class, SpawnTransform, Owner, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Enemy)
	{
		return nullptr;
	}
	Enemy->EnemyId = InEnemyId;
	if (!Enemy->AIControllerClass)
	{
		Enemy->AIControllerClass = AMTEnemyAIController::StaticClass();
	}
	Enemy->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	Enemy->FinishSpawning(SpawnTransform);

	if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed())
	{
		return nullptr;
	}
	if (!Enemy->GetController())
	{
		Enemy->SpawnDefaultController();
	}
	return Enemy;
}

AMTEnemyCharacter* AMTEnemySpawner::SpawnEnemyNear(UWorld* World, TSubclassOf<AMTEnemyCharacter> Class, FName InEnemyId, const FVector& Center, float Radius, int32 MaxAttempts, AActor* Owner)
{
	if (!World || !Class || InEnemyId.IsNone())
	{
		return nullptr;
	}
	// Capsule of the class, scaled by the enemy row (bigger monsters need more room).
	float CapsuleRadius = 42.f;
	float CapsuleHalfHeight = 92.f;
	if (const AMTEnemyCharacter* Defaults = Class->GetDefaultObject<AMTEnemyCharacter>())
	{
		if (const UCapsuleComponent* Capsule = Defaults->GetCapsuleComponent())
		{
			CapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
			CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		}
	}
	if (const UMTDataRegistry* Registry = UMTDataRegistry::Get(World))
	{
		if (const FMTEnemyData* Row = Registry->FindEnemy(InEnemyId))
		{
			const float Scale = Row->Scale > 0.f ? Row->Scale : 1.f;
			CapsuleRadius *= Scale;
			CapsuleHalfHeight *= Scale;
		}
	}

	FVector Location;
	if (!FindValidSpawnLocation(World, Center, Radius, CapsuleRadius, CapsuleHalfHeight, Location, MaxAttempts))
	{
		UE_LOG(LogMushoku, Verbose, TEXT("No valid spawn location for %s near %s"), *InEnemyId.ToString(), *Center.ToCompactString());
		return nullptr;
	}
	const FRotator Rotation(0.f, FMath::FRandRange(-180.f, 180.f), 0.f);
	return SpawnEnemy(World, Class, InEnemyId, Location, Rotation, Owner);
}

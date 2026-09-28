// Keeps Count enemies of EnemyId alive around the spawner. Spawn points are validated
// (navmesh projection + capsule overlap test) so enemies never appear inside geometry.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTEnemySpawner.generated.h"

class AMTEnemyCharacter;

UCLASS(Blueprintable)
class MUSHOKURPG_API AMTEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AMTEnemySpawner();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Spawner") FName EnemyId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Spawner", meta = (ClampMin = "0")) int32 Count = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Spawner", meta = (ClampMin = "0")) float SpawnRadius = 600.f;
	/** Seconds before a dead enemy is replaced (<= 0: never respawn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Spawner") float RespawnDelay = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Spawner") bool bSpawnOnBeginPlay = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Spawner") TSubclassOf<AMTEnemyCharacter> EnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Spawner", meta = (ClampMin = "1")) int32 MaxSpawnAttempts = 12;
	/** Respawns wait until the player is at least this far away (no pop-in in front of them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Spawner") float MinPlayerDistanceForRespawn = 1500.f;
	/** Registers the spawner under EnemyId so Kill objectives get a map marker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Spawner") bool bRegisterAsQuestActor = true;

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Spawner") void SpawnAll();
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Spawner") void DespawnAll();
	UFUNCTION(BlueprintPure, Category = "Mushoku|Spawner") int32 GetAliveCount() const;

	/**
	 * Finds a spawn location near Center: random point in Radius -> navmesh projection ->
	 * capsule overlap test (ECC_Pawn). OutLocation is the capsule centre.
	 */
	static bool FindValidSpawnLocation(UWorld* World, const FVector& Center, float Radius, float CapsuleRadius, float CapsuleHalfHeight, FVector& OutLocation, int32 MaxAttempts = 12);

	/** Deferred-spawns an enemy with EnemyId set before BeginPlay and makes sure it is AI-possessed. */
	static AMTEnemyCharacter* SpawnEnemy(UWorld* World, TSubclassOf<AMTEnemyCharacter> Class, FName InEnemyId, const FVector& Location, const FRotator& Rotation, AActor* Owner = nullptr);

	/** Validated spawn near Center using the class capsule (scaled by the enemy row). */
	static AMTEnemyCharacter* SpawnEnemyNear(UWorld* World, TSubclassOf<AMTEnemyCharacter> Class, FName InEnemyId, const FVector& Center, float Radius, int32 MaxAttempts = 12, AActor* Owner = nullptr);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void TickSpawner();
	bool TrySpawnOne();
	bool IsPlayerNearby() const;

	UPROPERTY(Transient) TArray<TObjectPtr<AMTEnemyCharacter>> Spawned;
	TArray<float> PendingRespawnTimes;
	FTimerHandle SpawnerTimer;
};

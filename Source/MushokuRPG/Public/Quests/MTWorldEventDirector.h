// Periodically starts a WorldEvent quest for its region: spawns the quest's enemies at
// validated navmesh points near a random tagged ATargetPoint anchor, announces it,
// auto-accepts it for the player and cleans everything up on completion or timeout.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTWorldEventDirector.generated.h"

class AMTEnemyCharacter;
class ATargetPoint;

UCLASS(Blueprintable)
class MUSHOKURPG_API AMTWorldEventDirector : public AActor
{
	GENERATED_BODY()

public:
	AMTWorldEventDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") bool bEnabled = true;
	/** Only WorldEvent quests with this Region are picked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") FName Region = TEXT("Fittoa");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents", meta = (ClampMin = "0.1")) float IntervalMinutes = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") float FirstEventDelaySeconds = 90.f;
	/** Unfinished events fail and despawn after this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") float EventTimeoutSeconds = 300.f;
	/** ATargetPoint actors with this tag are candidate event locations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") FName AnchorTag = TEXT("WorldEventAnchor");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") float SpawnRadius = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") int32 MaxEnemiesPerEvent = 8;
	/** Anchors closer than this to the player are avoided (no spawning in their face). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") float MinAnchorDistanceFromPlayer = 2000.f;
	/** Anchors farther than this are avoided when a closer one exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") float MaxAnchorDistanceFromPlayer = 15000.f;
	/** Used when an objective target is not a valid enemy row (tag-based Kill objectives). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") FName FallbackEnemyId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") TSubclassOf<AMTEnemyCharacter> EnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|WorldEvents") TSubclassOf<AMTEnemyCharacter> BossClass;

	/** Starts an event now (debug / scripted). Returns false when nothing could start. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|WorldEvents") bool TryStartEvent();
	UFUNCTION(BlueprintCallable, Category = "Mushoku|WorldEvents") void EndEvent(bool bTimedOut);
	UFUNCTION(BlueprintPure, Category = "Mushoku|WorldEvents") bool IsEventActive() const { return !ActiveQuestId.IsNone(); }
	UFUNCTION(BlueprintPure, Category = "Mushoku|WorldEvents") FName GetActiveQuestId() const { return ActiveQuestId; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|WorldEvents") void ReceiveEventStarted(FName QuestId, AActor* Anchor);
	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|WorldEvents") void ReceiveEventEnded(FName QuestId, bool bTimedOut);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleIntervalElapsed();
	void MonitorEvent();
	void CacheAnchors();
	AActor* PickAnchor() const;
	int32 SpawnEventEnemies(FName QuestId, AActor* Anchor);

	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> Anchors;
	UPROPERTY(Transient) TArray<TObjectPtr<AMTEnemyCharacter>> EventEnemies;
	TWeakObjectPtr<AActor> ActiveAnchor;
	TArray<FName> RegisteredMarkerIds;
	FName ActiveQuestId;
	float EventStartTime = 0.f;
	FTimerHandle IntervalTimer;
	FTimerHandle MonitorTimer;
};

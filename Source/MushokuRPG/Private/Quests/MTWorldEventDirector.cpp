#include "Quests/MTWorldEventDirector.h"

#include "Quests/MTQuestSubsystem.h"
#include "AI/MTEnemySpawner.h"
#include "AI/MTEnemyCharacter.h"
#include "AI/MTBossCharacter.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameEvents.h"
#include "Components/SceneComponent.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MTWorldEvents"

AMTWorldEventDirector::AMTWorldEventDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	EnemyClass = AMTEnemyCharacter::StaticClass();
	BossClass = AMTBossCharacter::StaticClass();
}

void AMTWorldEventDirector::BeginPlay()
{
	Super::BeginPlay();
	CacheAnchors();
	const float Interval = FMath::Max(6.f, IntervalMinutes * 60.f);
	GetWorldTimerManager().SetTimer(IntervalTimer, this, &AMTWorldEventDirector::HandleIntervalElapsed, Interval, true,
		FMath::Max(1.f, FirstEventDelaySeconds));
}

void AMTWorldEventDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// World events are transient: fail them when their level goes away (not on PIE/app exit).
	const bool bFail = EndPlayReason == EEndPlayReason::LevelTransition || EndPlayReason == EEndPlayReason::RemovedFromWorld;
	if (IsEventActive())
	{
		EndEvent(bFail);
	}
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void AMTWorldEventDirector::CacheAnchors()
{
	Anchors.Reset();
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClassWithTag(this, ATargetPoint::StaticClass(), AnchorTag, Found);
	for (AActor* Actor : Found)
	{
		Anchors.Add(Actor);
	}
	UE_LOG(LogMushoku, Log, TEXT("WorldEventDirector (%s): %d anchors tagged '%s'."), *Region.ToString(), Anchors.Num(), *AnchorTag.ToString());
}

void AMTWorldEventDirector::HandleIntervalElapsed()
{
	if (bEnabled && !IsEventActive())
	{
		TryStartEvent();
	}
}

AActor* AMTWorldEventDirector::PickAnchor() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	TArray<AActor*> Preferred;
	TArray<AActor*> Fallback;
	for (const TObjectPtr<AActor>& Anchor : Anchors)
	{
		if (!IsValid(Anchor))
		{
			continue;
		}
		Fallback.Add(Anchor);
		if (Player)
		{
			const float Dist = FVector::Dist2D(Anchor->GetActorLocation(), Player->GetActorLocation());
			if (Dist >= MinAnchorDistanceFromPlayer && Dist <= MaxAnchorDistanceFromPlayer)
			{
				Preferred.Add(Anchor);
			}
		}
	}
	const TArray<AActor*>& Pool = Preferred.Num() > 0 ? Preferred : Fallback;
	return Pool.Num() > 0 ? Pool[FMath::RandRange(0, Pool.Num() - 1)] : nullptr;
}

bool AMTWorldEventDirector::TryStartEvent()
{
	if (IsEventActive())
	{
		return false;
	}
	UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Quests || !Registry)
	{
		return false;
	}
	if (Anchors.Num() == 0)
	{
		CacheAnchors();
	}

	TArray<FName> Candidates;
	for (const TPair<FName, FMTQuestData>& Pair : Registry->GetQuests())
	{
		if (Pair.Value.Type == EMTQuestType::WorldEvent && Pair.Value.Region == Region && Quests->CanAcceptQuest(Pair.Key))
		{
			Candidates.Add(Pair.Key);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	AActor* Anchor = PickAnchor();
	if (!Anchor)
	{
		UE_LOG(LogMushoku, Warning, TEXT("WorldEventDirector: no ATargetPoint tagged '%s' in the level."), *AnchorTag.ToString());
		return false;
	}

	const FName QuestId = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	const FMTQuestData* Quest = Registry->FindQuest(QuestId);
	bool bNeedsEnemies = false;
	for (const FMTQuestObjective& Objective : Quest->Objectives)
	{
		bNeedsEnemies |= Objective.Type == EMTObjectiveType::Kill || Objective.Type == EMTObjectiveType::DefeatBoss;
	}

	ActiveAnchor = Anchor;
	const int32 SpawnedCount = SpawnEventEnemies(QuestId, Anchor);
	if (bNeedsEnemies && SpawnedCount == 0)
	{
		UE_LOG(LogMushoku, Warning, TEXT("WorldEventDirector: could not spawn enemies for %s; skipping."), *QuestId.ToString());
		ActiveQuestId = QuestId; // lets EndEvent clean markers
		EndEvent(false);
		return false;
	}
	if (!Quests->AcceptQuest(QuestId))
	{
		ActiveQuestId = QuestId;
		EndEvent(false);
		return false;
	}

	ActiveQuestId = QuestId;
	EventStartTime = GetWorld()->GetTimeSeconds();
	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->Notify(FText::Format(LOCTEXT("EventStarted", "World Event: {0}"), Quest->Title), FLinearColor(1.f, 0.6f, 0.25f));
	}
	UE_LOG(LogMushoku, Log, TEXT("World event started: %s at %s (%d enemies)."), *QuestId.ToString(), *Anchor->GetName(), SpawnedCount);

	GetWorldTimerManager().SetTimer(MonitorTimer, this, &AMTWorldEventDirector::MonitorEvent, 2.f, true);
	ReceiveEventStarted(QuestId, Anchor);
	return true;
}

int32 AMTWorldEventDirector::SpawnEventEnemies(FName QuestId, AActor* Anchor)
{
	UWorld* World = GetWorld();
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this);
	const FMTQuestData* Quest = Registry ? Registry->FindQuest(QuestId) : nullptr;
	if (!World || !Quest || !Anchor)
	{
		return 0;
	}

	int32 Total = 0;
	for (const FMTQuestObjective& Objective : Quest->Objectives)
	{
		if (Objective.Type != EMTObjectiveType::Kill && Objective.Type != EMTObjectiveType::DefeatBoss)
		{
			continue;
		}
		FName SpawnId = Objective.TargetId;
		const FMTEnemyData* Row = Registry->FindEnemy(SpawnId);
		if (!Row && !FallbackEnemyId.IsNone())
		{
			SpawnId = FallbackEnemyId;
			Row = Registry->FindEnemy(SpawnId);
		}
		if (!Row)
		{
			continue;
		}
		const bool bBoss = Objective.Type == EMTObjectiveType::DefeatBoss || Row->bIsBoss;
		TSubclassOf<AMTEnemyCharacter> Class = bBoss ? BossClass : EnemyClass;
		if (!Class)
		{
			Class = bBoss ? AMTBossCharacter::StaticClass() : AMTEnemyCharacter::StaticClass();
		}
		const int32 Wanted = bBoss ? 1 : FMath::Max(1, Objective.Count);
		for (int32 Index = 0; Index < Wanted && Total < MaxEnemiesPerEvent; ++Index)
		{
			if (AMTEnemyCharacter* Enemy = AMTEnemySpawner::SpawnEnemyNear(World, Class, SpawnId, Anchor->GetActorLocation(), SpawnRadius, 15, this))
			{
				EventEnemies.Add(Enemy);
				++Total;
			}
		}
		// Point the objective marker at the event site.
		if (Quests && !Objective.TargetId.IsNone())
		{
			Quests->RegisterQuestActor(Objective.TargetId, Anchor);
			RegisteredMarkerIds.AddUnique(Objective.TargetId);
		}
	}
	return Total;
}

void AMTWorldEventDirector::MonitorEvent()
{
	if (!IsEventActive())
	{
		GetWorldTimerManager().ClearTimer(MonitorTimer);
		return;
	}
	UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this);
	if (!Quests || !Quests->IsQuestActive(ActiveQuestId))
	{
		EndEvent(false); // completed, failed or abandoned
		return;
	}
	if (GetWorld()->GetTimeSeconds() - EventStartTime >= EventTimeoutSeconds)
	{
		EndEvent(true);
	}
}

void AMTWorldEventDirector::EndEvent(bool bTimedOut)
{
	if (ActiveQuestId.IsNone())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(MonitorTimer);
	const FName EndedQuest = ActiveQuestId;
	ActiveQuestId = NAME_None;

	UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this);
	bool bSucceeded = false;
	if (Quests)
	{
		if (bTimedOut && Quests->IsQuestActive(EndedQuest))
		{
			Quests->FailQuest(EndedQuest, LOCTEXT("EventTimeout", "the moment has passed"));
		}
		const FMTQuestSaveState* State = Quests->GetQuestState(EndedQuest);
		bSucceeded = State && !State->bActive && !State->bFailed && State->bCompleted;
	}

	// Failed / abandoned events withdraw their survivors; after a success they stay as normal foes.
	if (!bSucceeded)
	{
		for (const TObjectPtr<AMTEnemyCharacter>& Enemy : EventEnemies)
		{
			if (IsValid(Enemy) && Enemy->IsAlive())
			{
				Enemy->Destroy();
			}
		}
	}
	EventEnemies.Reset();

	if (Quests)
	{
		for (const FName& MarkerId : RegisteredMarkerIds)
		{
			Quests->UnregisterQuestActor(MarkerId, ActiveAnchor.Get());
		}
	}
	RegisteredMarkerIds.Reset();
	ActiveAnchor = nullptr;

	UE_LOG(LogMushoku, Log, TEXT("World event ended: %s (%s)."), *EndedQuest.ToString(), bTimedOut ? TEXT("timeout") : (bSucceeded ? TEXT("success") : TEXT("ended")));
	ReceiveEventEnded(EndedQuest, bTimedOut);
}

#undef LOCTEXT_NAMESPACE

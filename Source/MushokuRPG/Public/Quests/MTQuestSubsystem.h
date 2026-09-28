// Quest runtime: acceptance gating, objective tracking (event-bus driven), timers for
// Defend/Escort objectives, rewards, repeatable cooldowns, save/load and HUD markers.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Templates/Function.h"
#include "GameplayTagContainer.h"
#include "Core/MTDataTypes.h"
#include "Save/MTSaveTypes.h"
#include "MTQuestSubsystem.generated.h"

class UMTGameEvents;
class UMTSaveSubsystem;
class UMTDataRegistry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnQuestSignature, FName, QuestId);

UCLASS()
class MUSHOKURPG_API UMTQuestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UMTQuestSubsystem* Get(const UObject* WorldContext);

	// ---- Acceptance / lifecycle ----
	/** Level / rank / prerequisite / repeat-cooldown gating. OutReason is filled when false. */
	bool CanAcceptQuest(FName QuestId, FText* OutReason = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Quests")
	bool AcceptQuest(FName QuestId);

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Quests")
	void AbandonQuest(FName QuestId);

	/** Completes an active quest whose objectives are all done and grants rewards. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Quests")
	bool TurnInQuest(FName QuestId);

	/** Fails an active quest (world-event timeout, protected target lost...). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Quests")
	void FailQuest(FName QuestId, FText Reason);

	// ---- Queries (HUD) ----
	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	TArray<FName> GetActiveQuestIds() const;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	FName GetTrackedQuest() const { return TrackedQuest; }

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Quests")
	void SetTrackedQuest(FName QuestId);

	const FMTQuestSaveState* GetQuestState(FName QuestId) const { return QuestStates.Find(QuestId); }

	/** Objective line including progress, e.g. "Defeat wolves (3/5)". */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	FText GetObjectiveText(FName QuestId, int32 ObjectiveIndex) const;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	TArray<FName> GetAvailableQuestsForNPC(FName NpcId) const;

	/** Active quests whose objectives are complete and that are turned in to this NPC. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	TArray<FName> GetTurnInQuestsForNPC(FName NpcId) const;

	/** True when an active quest's current TalkTo/Deliver objective points at this NPC. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	bool HasPendingObjectiveForNPC(FName NpcId) const;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	bool IsQuestCompleted(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	bool IsQuestActive(FName QuestId) const;

	/** True when every objective of an active quest is done (ready to turn in). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	bool AreObjectivesComplete(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	TArray<FName> GetCompletedQuestIds() const;

	/** Per-objective completion using the real requirement (Defend = seconds, TalkTo/Reach/... = 1). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	bool IsObjectiveComplete(FName QuestId, int32 ObjectiveIndex) const;

	/** Index of the objective the HUD should highlight (current stage / first incomplete). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	int32 GetCurrentObjectiveIndex(FName QuestId) const;

	/** World position for the tracked-objective marker. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	bool GetObjectiveMarker(FName QuestId, FVector& OutWorldLocation) const;

	/** Display name for an NPC id (quest giver components, character rows, id fallback). */
	FText GetNPCDisplayName(FName NpcId) const;

	// ---- Quest actor registry (markers, escort/defend targets) ----
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Quests")
	void RegisterQuestActor(FName Id, AActor* Actor);

	/** Removes Actor from Id (or every actor registered under Id when Actor is null). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Quests")
	void UnregisterQuestActor(FName Id, AActor* Actor = nullptr);

	/** Registered actor for Id closest to Near (any valid one when Near is zero). */
	AActor* FindQuestActor(FName Id, const FVector& Near = FVector::ZeroVector) const;

	/** Location triggers report non-player characters entering them (escort arrival). */
	void NotifyActorReachedLocation(FName ActorGameplayId, FName LocationId);

	UPROPERTY(BlueprintAssignable) FMTOnQuestSignature OnQuestAccepted;
	UPROPERTY(BlueprintAssignable) FMTOnQuestSignature OnQuestUpdated;
	UPROPERTY(BlueprintAssignable) FMTOnQuestSignature OnQuestCompleted;
	UPROPERTY(BlueprintAssignable) FMTOnQuestSignature OnQuestFailed;

	/** Escortee must be this close to the destination (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Quests") float EscortArrivalRadius = 450.f;
	/** Defend timers pause when the player is farther than this from the defended target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Quests") float DefendLeashDistance = 6000.f;

private:
	// Event bus handlers.
	UFUNCTION() void HandleEnemyKilled(FName EnemyId, FGameplayTagContainer EnemyTags, AActor* Killer);
	UFUNCTION() void HandleItemCollected(FName ItemId, int32 Count);
	UFUNCTION() void HandleNPCInteracted(FName NpcId);
	UFUNCTION() void HandleObjectInteracted(FName ObjectId);
	UFUNCTION() void HandleLocationReached(FName LocationId);
	UFUNCTION() void HandleBossDefeated(FName BossId);
	UFUNCTION() void HandlePuzzleSolved(FName PuzzleId);
	UFUNCTION() void HandleProtectedTargetLost(FName TargetId);
	UFUNCTION() void HandleAbilityUsed(FName AbilityId, AActor* User);

	void HandleGatherSaveData(FMTSaveData& Data);
	void HandleApplySaveData(const FMTSaveData& Data);

	bool TickQuests(float DeltaTime);

	const FMTQuestData* FindQuestData(FName QuestId) const;
	static int32 GetRequiredProgress(const FMTQuestObjective& Objective);
	static bool IsObjectiveDone(const FMTQuestObjective& Objective, const FMTQuestSaveState& State, int32 Index);
	static bool IsObjectiveCurrent(const FMTQuestData& Quest, const FMTQuestSaveState& State, int32 Index);
	static bool AllObjectivesDone(const FMTQuestData& Quest, const FMTQuestSaveState& State);
	static void EnsureProgressSize(const FMTQuestData& Quest, FMTQuestSaveState& State);

	/** Adds Amount (or completes when bSetComplete) to every matching current objective. */
	void ApplyProgress(EMTObjectiveType Type, TFunctionRef<bool(const FMTQuestObjective&)> Matches, int32 Amount, bool bSetComplete);
	/** Stage advance, completion checks, notifications and broadcasts after progress. */
	void OnProgressChanged(FName QuestId, const TArray<int32>& CompletedObjectives);
	void CompleteQuestInternal(FName QuestId);
	void GrantRewards(const FMTQuestData& Quest);
	void RetrackIfNeeded(FName ChangedQuest);

	FText BuildDefaultObjectiveDescription(const FMTQuestObjective& Objective) const;
	FText GetIdDisplayName(FName Id, EMTObjectiveType Type) const;
	FVector GetPlayerLocation() const;
	bool ResolveIdLocation(FName Id, FVector& OutLocation) const;
	bool IsPlayerCredit(const AActor* Actor) const;
	void Notify(const FText& Message, const FLinearColor& Color) const;

	TMap<FName, FMTQuestSaveState> QuestStates;
	/** Fractional Defend timers (seconds) per quest/objective; Progress holds whole seconds. */
	TMap<FName, TArray<float>> DefendElapsed;
	/** Quests that already showed the "return to X" toast. */
	TSet<FName> ReadyNotified;
	TMap<FName, TArray<TWeakObjectPtr<AActor>>> QuestActors;
	FName TrackedQuest;

	const UMTDataRegistry* GetRegistry() const;
	void TryBindSaveSubsystem(UMTSaveSubsystem* Save);

	TWeakObjectPtr<UMTGameEvents> CachedEvents;
	TWeakObjectPtr<UMTDataRegistry> CachedRegistry;
	TWeakObjectPtr<UMTSaveSubsystem> CachedSave;
	FDelegateHandle GatherHandle;
	FDelegateHandle ApplyHandle;
	FTSTicker::FDelegateHandle TickHandle;
};

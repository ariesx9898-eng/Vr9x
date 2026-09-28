#include "Quests/MTQuestSubsystem.h"

#include "Core/MTDataRegistry.h"
#include "Core/MTGameEvents.h"
#include "Save/MTSaveSubsystem.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Quests/MTInteractableComponent.h"
#include "Character/MTCharacterBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/DateTime.h"

#define LOCTEXT_NAMESPACE "MTQuests"

namespace MTQuestPrivate
{
	static const FLinearColor ColorAccepted(0.95f, 0.85f, 0.45f);
	static const FLinearColor ColorProgress(0.78f, 0.86f, 0.95f);
	static const FLinearColor ColorComplete(1.f, 0.78f, 0.25f);
	static const FLinearColor ColorFailed(0.92f, 0.32f, 0.26f);
	static const FLinearColor ColorInfo(0.85f, 0.85f, 0.85f);

	static FText FormatDuration(int64 Seconds)
	{
		Seconds = FMath::Max<int64>(0, Seconds);
		const int32 H = static_cast<int32>(Seconds / 3600);
		const int32 M = static_cast<int32>((Seconds % 3600) / 60);
		const int32 S = static_cast<int32>(Seconds % 60);
		if (H > 0)
		{
			return FText::FromString(FString::Printf(TEXT("%dh %02dm"), H, M));
		}
		if (M > 0)
		{
			return FText::FromString(FString::Printf(TEXT("%dm %02ds"), M, S));
		}
		return FText::FromString(FString::Printf(TEXT("%ds"), S));
	}

	static FText FormatClock(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
	}
}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

UMTQuestSubsystem* UMTQuestSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	if (const UGameInstance* DirectGI = Cast<UGameInstance>(WorldContext))
	{
		return DirectGI->GetSubsystem<UMTQuestSubsystem>();
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMTQuestSubsystem>() : nullptr;
}

void UMTQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Only request dependencies that really are game-instance subsystems (InitializeDependency
	// ensures on a type mismatch).
	if (UMTDataRegistry::StaticClass()->IsChildOf(UGameInstanceSubsystem::StaticClass()))
	{
		CachedRegistry = Cast<UMTDataRegistry>(Collection.InitializeDependency(UMTDataRegistry::StaticClass()));
	}

	UMTGameEvents* Events = nullptr;
	if (UMTGameEvents::StaticClass()->IsChildOf(UGameInstanceSubsystem::StaticClass()))
	{
		Events = Cast<UMTGameEvents>(Collection.InitializeDependency(UMTGameEvents::StaticClass()));
	}
	if (Events)
	{
		CachedEvents = Events;
		Events->OnEnemyKilled.AddDynamic(this, &UMTQuestSubsystem::HandleEnemyKilled);
		Events->OnItemCollected.AddDynamic(this, &UMTQuestSubsystem::HandleItemCollected);
		Events->OnNPCInteracted.AddDynamic(this, &UMTQuestSubsystem::HandleNPCInteracted);
		Events->OnObjectInteracted.AddDynamic(this, &UMTQuestSubsystem::HandleObjectInteracted);
		Events->OnLocationReached.AddDynamic(this, &UMTQuestSubsystem::HandleLocationReached);
		Events->OnBossDefeated.AddDynamic(this, &UMTQuestSubsystem::HandleBossDefeated);
		Events->OnPuzzleSolved.AddDynamic(this, &UMTQuestSubsystem::HandlePuzzleSolved);
		Events->OnProtectedTargetLost.AddDynamic(this, &UMTQuestSubsystem::HandleProtectedTargetLost);
		Events->OnAbilityUsed.AddDynamic(this, &UMTQuestSubsystem::HandleAbilityUsed);
	}
	else
	{
		UE_LOG(LogMushoku, Warning, TEXT("MTQuestSubsystem: UMTGameEvents unavailable - objectives will not progress."));
	}

	if (UMTSaveSubsystem::StaticClass()->IsChildOf(UGameInstanceSubsystem::StaticClass()))
	{
		TryBindSaveSubsystem(Cast<UMTSaveSubsystem>(Collection.InitializeDependency(UMTSaveSubsystem::StaticClass())));
	}

	// Game-instance subsystems are not tickable; a low-rate core ticker drives Defend/Escort.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMTQuestSubsystem::TickQuests), 0.25f);
}

void UMTQuestSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();

	if (UMTSaveSubsystem* Save = CachedSave.Get())
	{
		Save->OnGatherSaveData.Remove(GatherHandle);
		Save->OnApplySaveData.Remove(ApplyHandle);
	}
	if (UMTGameEvents* Events = CachedEvents.Get())
	{
		Events->OnEnemyKilled.RemoveAll(this);
		Events->OnItemCollected.RemoveAll(this);
		Events->OnNPCInteracted.RemoveAll(this);
		Events->OnObjectInteracted.RemoveAll(this);
		Events->OnLocationReached.RemoveAll(this);
		Events->OnBossDefeated.RemoveAll(this);
		Events->OnPuzzleSolved.RemoveAll(this);
		Events->OnProtectedTargetLost.RemoveAll(this);
		Events->OnAbilityUsed.RemoveAll(this);
	}
	QuestActors.Reset();
	Super::Deinitialize();
}

void UMTQuestSubsystem::TryBindSaveSubsystem(UMTSaveSubsystem* Save)
{
	if (!Save || CachedSave.Get() == Save)
	{
		return;
	}
	CachedSave = Save;
	GatherHandle = Save->OnGatherSaveData.AddUObject(this, &UMTQuestSubsystem::HandleGatherSaveData);
	ApplyHandle = Save->OnApplySaveData.AddUObject(this, &UMTQuestSubsystem::HandleApplySaveData);
}

const UMTDataRegistry* UMTQuestSubsystem::GetRegistry() const
{
	if (const UMTDataRegistry* Registry = CachedRegistry.Get())
	{
		return Registry;
	}
	return UMTDataRegistry::Get(this);
}

const FMTQuestData* UMTQuestSubsystem::FindQuestData(FName QuestId) const
{
	const UMTDataRegistry* Registry = GetRegistry();
	return (Registry && !QuestId.IsNone()) ? Registry->FindQuest(QuestId) : nullptr;
}

// ---------------------------------------------------------------------------------------------
// Objective helpers
// ---------------------------------------------------------------------------------------------

int32 UMTQuestSubsystem::GetRequiredProgress(const FMTQuestObjective& Objective)
{
	switch (Objective.Type)
	{
	case EMTObjectiveType::Defend:
		return FMath::Max(1, FMath::CeilToInt(Objective.Duration));
	case EMTObjectiveType::TalkTo:
	case EMTObjectiveType::Reach:
	case EMTObjectiveType::Escort:
	case EMTObjectiveType::Puzzle:
	case EMTObjectiveType::DefeatBoss:
		return 1;
	default:
		return FMath::Max(1, Objective.Count);
	}
}

bool UMTQuestSubsystem::IsObjectiveDone(const FMTQuestObjective& Objective, const FMTQuestSaveState& State, int32 Index)
{
	return State.Progress.IsValidIndex(Index) && State.Progress[Index] >= GetRequiredProgress(Objective);
}

bool UMTQuestSubsystem::IsObjectiveCurrent(const FMTQuestData& Quest, const FMTQuestSaveState& State, int32 Index)
{
	return !Quest.bSequentialObjectives || Index == State.Stage;
}

bool UMTQuestSubsystem::AllObjectivesDone(const FMTQuestData& Quest, const FMTQuestSaveState& State)
{
	for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
	{
		if (!IsObjectiveDone(Quest.Objectives[Index], State, Index))
		{
			return false;
		}
	}
	return true;
}

void UMTQuestSubsystem::EnsureProgressSize(const FMTQuestData& Quest, FMTQuestSaveState& State)
{
	if (State.Progress.Num() != Quest.Objectives.Num())
	{
		State.Progress.SetNumZeroed(Quest.Objectives.Num());
	}
	State.Stage = FMath::Clamp(State.Stage, 0, Quest.Objectives.Num());
}

bool UMTQuestSubsystem::IsPlayerCredit(const AActor* Actor) const
{
	if (!Actor)
	{
		return true;
	}
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		// AI-controlled pawns (allies, infighting monsters) do not give credit.
		return Pawn->IsPlayerControlled() || Pawn->GetController() == nullptr;
	}
	if (const APawn* InstigatorPawn = Actor->GetInstigator())
	{
		return InstigatorPawn->IsPlayerControlled();
	}
	if (const APawn* OwnerPawn = Cast<APawn>(Actor->GetOwner()))
	{
		return OwnerPawn->IsPlayerControlled();
	}
	return true;
}

void UMTQuestSubsystem::Notify(const FText& Message, const FLinearColor& Color) const
{
	if (Message.IsEmpty())
	{
		return;
	}
	UMTGameEvents* Events = CachedEvents.Get();
	if (!Events)
	{
		Events = UMTGameEvents::Get(this);
	}
	if (Events)
	{
		Events->Notify(Message, Color);
	}
	UE_LOG(LogMushoku, Verbose, TEXT("[Quest] %s"), *Message.ToString());
}

FVector UMTQuestSubsystem::GetPlayerLocation() const
{
	const UGameInstance* GI = GetGameInstance();
	const APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
}

// ---------------------------------------------------------------------------------------------
// Acceptance / lifecycle
// ---------------------------------------------------------------------------------------------

bool UMTQuestSubsystem::CanAcceptQuest(FName QuestId, FText* OutReason) const
{
	auto Fail = [OutReason](const FText& Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return false;
	};

	const FMTQuestData* Quest = FindQuestData(QuestId);
	if (!Quest)
	{
		return Fail(LOCTEXT("UnknownQuest", "Unknown quest."));
	}

	if (const FMTQuestSaveState* State = QuestStates.Find(QuestId))
	{
		if (State->bActive)
		{
			return Fail(LOCTEXT("AlreadyActive", "You are already on this quest."));
		}
		if (State->bCompleted || State->TimesCompleted > 0)
		{
			if (!Quest->bRepeatable)
			{
				return Fail(LOCTEXT("AlreadyDone", "You have already completed this quest."));
			}
			const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
			const int64 Remaining = State->LastCompletedUnix + static_cast<int64>(FMath::CeilToInt(Quest->RepeatCooldown)) - Now;
			if (Remaining > 0)
			{
				return Fail(FText::Format(LOCTEXT("RepeatCooldown", "Available again in {0}."), MTQuestPrivate::FormatDuration(Remaining)));
			}
		}
	}

	if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		if (static_cast<int32>(Progression->GetLevel()) < Quest->RequiredLevel)
		{
			return Fail(FText::Format(LOCTEXT("NeedLevel", "Requires level {0}."), FText::AsNumber(Quest->RequiredLevel)));
		}
		if (static_cast<int32>(Progression->GetAdventurerRank()) < static_cast<int32>(Quest->RequiredRank))
		{
			return Fail(FText::Format(LOCTEXT("NeedRank", "Requires adventurer rank {0}."),
				FText::FromString(MTUtil::AdventurerRankToString(Quest->RequiredRank))));
		}
	}

	for (const FName& Prerequisite : Quest->PrerequisiteQuests)
	{
		if (!Prerequisite.IsNone() && !IsQuestCompleted(Prerequisite))
		{
			const FMTQuestData* PrereqData = FindQuestData(Prerequisite);
			return Fail(FText::Format(LOCTEXT("NeedPrereq", "Requires: {0}."),
				PrereqData ? PrereqData->Title : FText::FromName(Prerequisite)));
		}
	}
	return true;
}

bool UMTQuestSubsystem::AcceptQuest(FName QuestId)
{
	FText Reason;
	if (!CanAcceptQuest(QuestId, &Reason))
	{
		Notify(Reason, MTQuestPrivate::ColorFailed);
		return false;
	}
	const FMTQuestData* Quest = FindQuestData(QuestId);
	check(Quest);

	FMTQuestSaveState& State = QuestStates.FindOrAdd(QuestId);
	State.QuestId = QuestId;
	State.Stage = 0;
	State.Progress.Init(0, Quest->Objectives.Num());
	State.bActive = true;
	State.bCompleted = false;
	State.bFailed = false;

	DefendElapsed.FindOrAdd(QuestId).Init(0.f, Quest->Objectives.Num());
	ReadyNotified.Remove(QuestId);

	if (TrackedQuest.IsNone() || !IsQuestActive(TrackedQuest))
	{
		TrackedQuest = QuestId;
	}

	const FText Title = Quest->Title;
	Notify(FText::Format(LOCTEXT("QuestAccepted", "New quest: {0}"), Title), MTQuestPrivate::ColorAccepted);
	UE_LOG(LogMushoku, Log, TEXT("Quest accepted: %s"), *QuestId.ToString());
	OnQuestAccepted.Broadcast(QuestId);

	// Degenerate quests (no objectives / zero counts) resolve immediately.
	const FMTQuestSaveState* Fresh = QuestStates.Find(QuestId);
	const FMTQuestData* FreshQuest = FindQuestData(QuestId);
	if (Fresh && FreshQuest && Fresh->bActive && AllObjectivesDone(*FreshQuest, *Fresh))
	{
		OnProgressChanged(QuestId, TArray<int32>());
	}
	return true;
}

void UMTQuestSubsystem::AbandonQuest(FName QuestId)
{
	FMTQuestSaveState* State = QuestStates.Find(QuestId);
	if (!State || !State->bActive)
	{
		return;
	}
	const FMTQuestData* Quest = FindQuestData(QuestId);
	const FText Title = Quest ? Quest->Title : FText::FromName(QuestId);

	if (State->TimesCompleted > 0)
	{
		// A repeat run was abandoned: keep the completion history.
		State->bActive = false;
		State->bCompleted = true;
		State->Stage = 0;
		for (int32& Value : State->Progress)
		{
			Value = 0;
		}
	}
	else
	{
		QuestStates.Remove(QuestId);
	}
	DefendElapsed.Remove(QuestId);
	ReadyNotified.Remove(QuestId);

	Notify(FText::Format(LOCTEXT("QuestAbandoned", "Quest abandoned: {0}"), Title), MTQuestPrivate::ColorInfo);
	RetrackIfNeeded(QuestId);
	OnQuestUpdated.Broadcast(QuestId);
}

bool UMTQuestSubsystem::TurnInQuest(FName QuestId)
{
	const FMTQuestSaveState* State = QuestStates.Find(QuestId);
	const FMTQuestData* Quest = FindQuestData(QuestId);
	if (!State || !Quest || !State->bActive)
	{
		return false;
	}
	if (!AllObjectivesDone(*Quest, *State))
	{
		Notify(LOCTEXT("NotReady", "The objectives are not complete yet."), MTQuestPrivate::ColorInfo);
		return false;
	}
	CompleteQuestInternal(QuestId);
	return true;
}

void UMTQuestSubsystem::FailQuest(FName QuestId, FText Reason)
{
	FMTQuestSaveState* State = QuestStates.Find(QuestId);
	if (!State || !State->bActive)
	{
		return;
	}
	State->bActive = false;
	State->bFailed = true;
	State->bCompleted = State->TimesCompleted > 0;
	DefendElapsed.Remove(QuestId);
	ReadyNotified.Remove(QuestId);

	const FMTQuestData* Quest = FindQuestData(QuestId);
	const FText Title = Quest ? Quest->Title : FText::FromName(QuestId);
	const FText Message = Reason.IsEmpty()
		? FText::Format(LOCTEXT("QuestFailed", "Quest failed: {0}"), Title)
		: FText::Format(LOCTEXT("QuestFailedReason", "Quest failed: {0} ({1})"), Title, Reason);
	Notify(Message, MTQuestPrivate::ColorFailed);
	UE_LOG(LogMushoku, Log, TEXT("Quest failed: %s"), *QuestId.ToString());

	RetrackIfNeeded(QuestId);
	OnQuestFailed.Broadcast(QuestId);
}

void UMTQuestSubsystem::CompleteQuestInternal(FName QuestId)
{
	FMTQuestSaveState* State = QuestStates.Find(QuestId);
	const FMTQuestData* Quest = FindQuestData(QuestId);
	if (!State || !Quest)
	{
		return;
	}
	State->bActive = false;
	State->bCompleted = true;
	State->bFailed = false;
	State->TimesCompleted++;
	State->LastCompletedUnix = FDateTime::UtcNow().ToUnixTimestamp();
	DefendElapsed.Remove(QuestId);
	ReadyNotified.Remove(QuestId);

	const FMTQuestData QuestCopy = *Quest; // registry pointer must not be held across callbacks
	Notify(FText::Format(LOCTEXT("QuestComplete", "Quest complete: {0}"), QuestCopy.Title), MTQuestPrivate::ColorComplete);
	GrantRewards(QuestCopy);

	if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Progression->MarkQuestCompleted(QuestId);
	}
	UE_LOG(LogMushoku, Log, TEXT("Quest completed: %s"), *QuestId.ToString());

	RetrackIfNeeded(QuestId);
	OnQuestCompleted.Broadcast(QuestId);
}

void UMTQuestSubsystem::GrantRewards(const FMTQuestData& Quest)
{
	const FMTQuestReward& Reward = Quest.Reward;
	UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this);
	if (!Progression)
	{
		UE_LOG(LogMushoku, Warning, TEXT("Quest %s: progression subsystem missing, rewards skipped."), *Quest.QuestID.ToString());
		return;
	}

	TArray<FText> Parts;
	if (Reward.XP > 0)
	{
		Progression->AddXP(Reward.XP);
		Parts.Add(FText::Format(LOCTEXT("RewardXP", "+{0} XP"), FText::AsNumber(Reward.XP)));
	}
	if (Reward.Gold > 0)
	{
		Progression->AddGold(Reward.Gold);
		Parts.Add(FText::Format(LOCTEXT("RewardGold", "+{0} Gold"), FText::AsNumber(Reward.Gold)));
	}
	if (Reward.AdventurerPoints > 0)
	{
		Progression->AddAdventurerPoints(Reward.AdventurerPoints);
		Parts.Add(FText::Format(LOCTEXT("RewardAP", "+{0} Adventurer Points"), FText::AsNumber(Reward.AdventurerPoints)));
	}
	if (Reward.CharacterSpins > 0)
	{
		Progression->AddSpins(EMTRollCategory::Character, Reward.CharacterSpins);
		Parts.Add(FText::Format(LOCTEXT("RewardCharSpin", "+{0} Character Spin(s)"), FText::AsNumber(Reward.CharacterSpins)));
	}
	if (Reward.ElementSpins > 0)
	{
		Progression->AddSpins(EMTRollCategory::Element, Reward.ElementSpins);
		Parts.Add(FText::Format(LOCTEXT("RewardElemSpin", "+{0} Element Spin(s)"), FText::AsNumber(Reward.ElementSpins)));
	}
	if (Reward.RaceSpins > 0)
	{
		Progression->AddSpins(EMTRollCategory::Race, Reward.RaceSpins);
		Parts.Add(FText::Format(LOCTEXT("RewardRaceSpin", "+{0} Race Spin(s)"), FText::AsNumber(Reward.RaceSpins)));
	}
	const UMTDataRegistry* Registry = GetRegistry();
	for (const TPair<FName, int32>& Item : Reward.Items)
	{
		if (Item.Key.IsNone() || Item.Value <= 0)
		{
			continue;
		}
		Progression->AddItem(Item.Key, Item.Value);
		const FMTItemData* ItemData = Registry ? Registry->FindItem(Item.Key) : nullptr;
		const FText ItemName = (ItemData && !ItemData->DisplayName.IsEmpty()) ? ItemData->DisplayName : FText::FromName(Item.Key);
		Parts.Add(FText::Format(LOCTEXT("RewardItem", "{0} x{1}"), ItemName, FText::AsNumber(Item.Value)));
	}
	if (Reward.MasteryXP > 0.f)
	{
		const FName Equipped = Progression->GetEquippedCharacter();
		if (!Equipped.IsNone())
		{
			Progression->AddMasteryXP(EMTMasteryTrack::Character, Equipped, Reward.MasteryXP);
			Parts.Add(FText::Format(LOCTEXT("RewardMastery", "+{0} {1} Mastery"), FText::AsNumber(FMath::RoundToInt(Reward.MasteryXP)), FText::FromName(Equipped)));
		}
	}
	if (Parts.Num() > 0)
	{
		Notify(FText::Join(FText::FromString(TEXT(", ")), Parts), MTQuestPrivate::ColorComplete);
	}
}

void UMTQuestSubsystem::RetrackIfNeeded(FName /*ChangedQuest*/)
{
	if (!TrackedQuest.IsNone() && IsQuestActive(TrackedQuest))
	{
		return;
	}
	TrackedQuest = NAME_None;
	for (const TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		if (Pair.Value.bActive)
		{
			TrackedQuest = Pair.Key;
			break;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Progress
// ---------------------------------------------------------------------------------------------

void UMTQuestSubsystem::ApplyProgress(EMTObjectiveType Type, TFunctionRef<bool(const FMTQuestObjective&)> Matches, int32 Amount, bool bSetComplete)
{
	TArray<TPair<FName, TArray<int32>>> Pending;
	for (TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		FMTQuestSaveState& State = Pair.Value;
		if (!State.bActive)
		{
			continue;
		}
		const FMTQuestData* Quest = FindQuestData(Pair.Key);
		if (!Quest)
		{
			continue;
		}
		EnsureProgressSize(*Quest, State);

		bool bChanged = false;
		TArray<int32> Completed;
		for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
		{
			const FMTQuestObjective& Objective = Quest->Objectives[Index];
			if (Objective.Type != Type || !IsObjectiveCurrent(*Quest, State, Index) || IsObjectiveDone(Objective, State, Index))
			{
				continue;
			}
			if (!Matches(Objective))
			{
				continue;
			}
			const int32 Required = GetRequiredProgress(Objective);
			const int32 NewValue = bSetComplete ? Required : FMath::Clamp(State.Progress[Index] + FMath::Max(Amount, 0), 0, Required);
			if (NewValue == State.Progress[Index])
			{
				continue;
			}
			State.Progress[Index] = NewValue;
			bChanged = true;
			if (NewValue >= Required)
			{
				Completed.Add(Index);
			}
		}
		if (bChanged)
		{
			Pending.Emplace(Pair.Key, MoveTemp(Completed));
		}
	}

	// Broadcast outside the map iteration (listeners may accept quests).
	for (const TPair<FName, TArray<int32>>& Entry : Pending)
	{
		OnProgressChanged(Entry.Key, Entry.Value);
	}
}

void UMTQuestSubsystem::OnProgressChanged(FName QuestId, const TArray<int32>& CompletedObjectives)
{
	FMTQuestSaveState* State = QuestStates.Find(QuestId);
	const FMTQuestData* Quest = FindQuestData(QuestId);
	if (!State || !Quest || !State->bActive)
	{
		return;
	}

	for (const int32 Index : CompletedObjectives)
	{
		Notify(FText::Format(LOCTEXT("ObjectiveComplete", "Objective complete: {0}"), GetObjectiveText(QuestId, Index)), MTQuestPrivate::ColorProgress);
	}

	if (Quest->bSequentialObjectives)
	{
		const int32 Num = Quest->Objectives.Num();
		const int32 OldStage = State->Stage;
		while (State->Stage < Num && IsObjectiveDone(Quest->Objectives[State->Stage], *State, State->Stage))
		{
			++State->Stage;
		}
		if (State->Stage != OldStage && State->Stage < Num)
		{
			if (TArray<float>* Timers = DefendElapsed.Find(QuestId))
			{
				if (Timers->IsValidIndex(State->Stage))
				{
					(*Timers)[State->Stage] = 0.f;
				}
			}
			Notify(FText::Format(LOCTEXT("NewObjective", "New objective: {0}"), GetObjectiveText(QuestId, State->Stage)), MTQuestPrivate::ColorAccepted);
		}
	}

	const bool bAllDone = AllObjectivesDone(*Quest, *State);
	const FName TurnInNPC = Quest->TurnInNPC;
	const FText Title = Quest->Title;

	OnQuestUpdated.Broadcast(QuestId);

	// Listeners may have changed the map; re-validate.
	State = QuestStates.Find(QuestId);
	if (!State || !State->bActive || !bAllDone)
	{
		return;
	}
	if (TurnInNPC.IsNone())
	{
		CompleteQuestInternal(QuestId);
	}
	else if (!ReadyNotified.Contains(QuestId))
	{
		ReadyNotified.Add(QuestId);
		Notify(FText::Format(LOCTEXT("ReturnTo", "{0}: return to {1}"), Title, GetNPCDisplayName(TurnInNPC)), MTQuestPrivate::ColorAccepted);
	}
}

void UMTQuestSubsystem::HandleEnemyKilled(FName EnemyId, FGameplayTagContainer EnemyTags, AActor* Killer)
{
	if (!IsPlayerCredit(Killer))
	{
		return;
	}
	ApplyProgress(EMTObjectiveType::Kill, [&EnemyId, &EnemyTags](const FMTQuestObjective& Objective)
	{
		if (Objective.TargetId.IsNone() || Objective.TargetId == EnemyId)
		{
			return true;
		}
		for (const FGameplayTag& Tag : EnemyTags)
		{
			if (Tag.GetTagName() == Objective.TargetId)
			{
				return true;
			}
		}
		// Hierarchical match: "Enemy.Beast" counts "Enemy.Beast.Wolf".
		const FGameplayTag Wanted = FGameplayTag::RequestGameplayTag(Objective.TargetId, /*ErrorIfNotFound*/ false);
		return Wanted.IsValid() && EnemyTags.HasTag(Wanted);
	}, 1, false);
}

void UMTQuestSubsystem::HandleItemCollected(FName ItemId, int32 Count)
{
	ApplyProgress(EMTObjectiveType::Gather, [&ItemId](const FMTQuestObjective& Objective)
	{
		return Objective.TargetId == ItemId;
	}, FMath::Max(1, Count), false);
}

void UMTQuestSubsystem::HandleNPCInteracted(FName NpcId)
{
	if (NpcId.IsNone())
	{
		return;
	}

	ApplyProgress(EMTObjectiveType::TalkTo, [&NpcId](const FMTQuestObjective& Objective)
	{
		return Objective.TargetId == NpcId;
	}, 1, true);

	// Deliver: needs the items in the bag; consumes them when handing over.
	UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this);
	const UMTDataRegistry* Registry = GetRegistry();
	TArray<TPair<FName, TArray<int32>>> Pending;
	TArray<FText> Messages;
	for (TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		FMTQuestSaveState& State = Pair.Value;
		if (!State.bActive)
		{
			continue;
		}
		const FMTQuestData* Quest = FindQuestData(Pair.Key);
		if (!Quest)
		{
			continue;
		}
		EnsureProgressSize(*Quest, State);

		TArray<int32> Completed;
		for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
		{
			const FMTQuestObjective& Objective = Quest->Objectives[Index];
			if (Objective.Type != EMTObjectiveType::Deliver || !IsObjectiveCurrent(*Quest, State, Index) || IsObjectiveDone(Objective, State, Index))
			{
				continue;
			}
			const FName DeliverTo = !Objective.SecondaryId.IsNone() ? Objective.SecondaryId
				: (!Quest->TurnInNPC.IsNone() ? Quest->TurnInNPC : Quest->GiverNPC);
			if (DeliverTo != NpcId)
			{
				continue;
			}
			const int32 Required = GetRequiredProgress(Objective);
			const int32 Have = Progression ? Progression->GetItemCount(Objective.TargetId) : 0;
			if (Progression && Have >= Required && Progression->RemoveItem(Objective.TargetId, Required))
			{
				State.Progress[Index] = Required;
				Completed.Add(Index);
			}
			else
			{
				const FMTItemData* Item = Registry ? Registry->FindItem(Objective.TargetId) : nullptr;
				const FText ItemName = (Item && !Item->DisplayName.IsEmpty()) ? Item->DisplayName : FText::FromName(Objective.TargetId);
				Messages.Add(FText::Format(LOCTEXT("DeliverMissing", "Bring {0}x {1} (you have {2})."),
					FText::AsNumber(Required), ItemName, FText::AsNumber(Have)));
			}
		}
		if (Completed.Num() > 0)
		{
			Pending.Emplace(Pair.Key, MoveTemp(Completed));
		}
	}
	for (const FText& Message : Messages)
	{
		Notify(Message, MTQuestPrivate::ColorInfo);
	}
	for (const TPair<FName, TArray<int32>>& Entry : Pending)
	{
		OnProgressChanged(Entry.Key, Entry.Value);
	}
}

void UMTQuestSubsystem::HandleObjectInteracted(FName ObjectId)
{
	if (ObjectId.IsNone())
	{
		return;
	}
	ApplyProgress(EMTObjectiveType::Interact, [&ObjectId](const FMTQuestObjective& Objective)
	{
		return Objective.TargetId == ObjectId;
	}, 1, false);
}

void UMTQuestSubsystem::HandleLocationReached(FName LocationId)
{
	ApplyProgress(EMTObjectiveType::Reach, [&LocationId](const FMTQuestObjective& Objective)
	{
		return Objective.TargetId == LocationId;
	}, 1, true);
}

void UMTQuestSubsystem::HandleBossDefeated(FName BossId)
{
	ApplyProgress(EMTObjectiveType::DefeatBoss, [&BossId](const FMTQuestObjective& Objective)
	{
		return Objective.TargetId.IsNone() || Objective.TargetId == BossId;
	}, 1, true);
}

void UMTQuestSubsystem::HandlePuzzleSolved(FName PuzzleId)
{
	ApplyProgress(EMTObjectiveType::Puzzle, [&PuzzleId](const FMTQuestObjective& Objective)
	{
		return Objective.TargetId == PuzzleId;
	}, 1, true);
}

/** "Rudeus_StoneCannon", "Earth_StoneCannon" and "Rudeus_StoneCannon_Awakened" share the family "StoneCannon". */
static FString MTAbilityFamily(FName AbilityId)
{
	FString Id = AbilityId.ToString();
	Id.RemoveFromEnd(TEXT("_Awakened"));
	int32 Underscore = INDEX_NONE;
	return Id.FindChar(TEXT('_'), Underscore) ? Id.Mid(Underscore + 1) : Id;
}

void UMTQuestSubsystem::HandleAbilityUsed(FName AbilityId, AActor* User)
{
	if (const APawn* Pawn = Cast<APawn>(User))
	{
		if (!Pawn->IsPlayerControlled())
		{
			return;
		}
	}
	const UMTDataRegistry* Registry = GetRegistry();
	const FMTAbilityData* Ability = Registry ? Registry->FindAbility(AbilityId) : nullptr;
	const EMTElement Element = Ability ? Ability->Element : EMTElement::None;
	const FName ElementName = (Element != EMTElement::None) ? FName(*MTUtil::ElementToString(Element)) : NAME_None;

	const FString Family = MTAbilityFamily(AbilityId);
	ApplyProgress(EMTObjectiveType::UseAbility, [&AbilityId, &ElementName, &Family](const FMTQuestObjective& Objective)
	{
		// TargetId = ability id, an element name ("Fire") for "cast any fire spell" tasks, or any
		// ability of the same family, so the element/awakened versions of a spell also count.
		return Objective.TargetId.IsNone() || Objective.TargetId == AbilityId || (!ElementName.IsNone() && Objective.TargetId == ElementName)
			|| (!Family.IsEmpty() && MTAbilityFamily(Objective.TargetId) == Family);
	}, 1, false);
}

void UMTQuestSubsystem::HandleProtectedTargetLost(FName TargetId)
{
	if (TargetId.IsNone())
	{
		return;
	}
	TArray<FName> ToFail;
	for (const TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		const FMTQuestSaveState& State = Pair.Value;
		const FMTQuestData* Quest = State.bActive ? FindQuestData(Pair.Key) : nullptr;
		if (!Quest)
		{
			continue;
		}
		for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
		{
			const FMTQuestObjective& Objective = Quest->Objectives[Index];
			if ((Objective.Type == EMTObjectiveType::Defend || Objective.Type == EMTObjectiveType::Escort)
				&& Objective.TargetId == TargetId
				&& IsObjectiveCurrent(*Quest, State, Index)
				&& !IsObjectiveDone(Objective, State, Index))
			{
				ToFail.Add(Pair.Key);
				break;
			}
		}
	}
	for (const FName& QuestId : ToFail)
	{
		FailQuest(QuestId, FText::Format(LOCTEXT("TargetLost", "{0} was lost"), GetNPCDisplayName(TargetId)));
	}
}

void UMTQuestSubsystem::NotifyActorReachedLocation(FName ActorGameplayId, FName LocationId)
{
	if (ActorGameplayId.IsNone() || LocationId.IsNone())
	{
		return;
	}
	ApplyProgress(EMTObjectiveType::Escort, [&ActorGameplayId, &LocationId](const FMTQuestObjective& Objective)
	{
		return Objective.TargetId == ActorGameplayId && Objective.SecondaryId == LocationId;
	}, 1, true);
}

bool UMTQuestSubsystem::TickQuests(float DeltaTime)
{
	const UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;

	// Late binding when the save subsystem is not a game-instance subsystem / was created later.
	if (!CachedSave.IsValid() && World)
	{
		TryBindSaveSubsystem(UMTSaveSubsystem::Get(World));
	}

	if (!World || World->IsPaused() || !World->HasBegunPlay() || QuestStates.Num() == 0)
	{
		return true;
	}
	DeltaTime = FMath::Min(DeltaTime, 1.f); // ignore hitches / loading stalls

	const FVector PlayerLocation = GetPlayerLocation();
	TArray<TPair<FName, TArray<int32>>> Progressed;
	TArray<TPair<FName, FText>> Failures;

	for (TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		FMTQuestSaveState& State = Pair.Value;
		if (!State.bActive)
		{
			continue;
		}
		const FMTQuestData* Quest = FindQuestData(Pair.Key);
		if (!Quest)
		{
			continue;
		}
		EnsureProgressSize(*Quest, State);

		bool bChanged = false;
		bool bFailed = false;
		FText FailReason;
		TArray<int32> Completed;
		for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
		{
			const FMTQuestObjective& Objective = Quest->Objectives[Index];
			if ((Objective.Type != EMTObjectiveType::Defend && Objective.Type != EMTObjectiveType::Escort)
				|| !IsObjectiveCurrent(*Quest, State, Index) || IsObjectiveDone(Objective, State, Index))
			{
				continue;
			}

			AActor* Target = FindQuestActor(Objective.TargetId, PlayerLocation);
			if (const AMTCharacterBase* TargetCharacter = Cast<AMTCharacterBase>(Target))
			{
				if (!TargetCharacter->IsAlive())
				{
					bFailed = true;
					FailReason = FText::Format(LOCTEXT("TargetDied", "{0} was lost"), GetNPCDisplayName(Objective.TargetId));
					break;
				}
			}

			if (Objective.Type == EMTObjectiveType::Defend)
			{
				// Timer pauses while the player is far from the defended target.
				if (Target && !PlayerLocation.IsZero() && FVector::Dist(PlayerLocation, Target->GetActorLocation()) > DefendLeashDistance)
				{
					continue;
				}
				TArray<float>& Timers = DefendElapsed.FindOrAdd(Pair.Key);
				if (Timers.Num() != Quest->Objectives.Num())
				{
					Timers.SetNumZeroed(Quest->Objectives.Num());
				}
				Timers[Index] = FMath::Max(Timers[Index], static_cast<float>(State.Progress[Index])) + DeltaTime;
				const int32 Required = GetRequiredProgress(Objective);
				const int32 Whole = FMath::Min(Required, FMath::FloorToInt(Timers[Index]));
				if (Whole != State.Progress[Index])
				{
					State.Progress[Index] = Whole;
					bChanged = true;
					if (Whole >= Required)
					{
						Completed.Add(Index);
					}
				}
			}
			else if (Target) // Escort: poll the escortee against the destination.
			{
				FVector Destination;
				if (ResolveIdLocation(Objective.SecondaryId, Destination)
					&& FVector::Dist2D(Target->GetActorLocation(), Destination) <= EscortArrivalRadius)
				{
					State.Progress[Index] = 1;
					bChanged = true;
					Completed.Add(Index);
				}
			}
		}

		if (bFailed)
		{
			Failures.Emplace(Pair.Key, FailReason);
		}
		else if (bChanged)
		{
			Progressed.Emplace(Pair.Key, MoveTemp(Completed));
		}
	}

	for (const TPair<FName, FText>& Failure : Failures)
	{
		FailQuest(Failure.Key, Failure.Value);
	}
	for (const TPair<FName, TArray<int32>>& Entry : Progressed)
	{
		OnProgressChanged(Entry.Key, Entry.Value);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------

TArray<FName> UMTQuestSubsystem::GetActiveQuestIds() const
{
	TArray<FName> Result;
	for (const TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		if (Pair.Value.bActive)
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

void UMTQuestSubsystem::SetTrackedQuest(FName QuestId)
{
	if (QuestId.IsNone())
	{
		TrackedQuest = NAME_None;
		OnQuestUpdated.Broadcast(NAME_None);
		return;
	}
	if (IsQuestActive(QuestId) && TrackedQuest != QuestId)
	{
		TrackedQuest = QuestId;
		OnQuestUpdated.Broadcast(QuestId);
	}
}

bool UMTQuestSubsystem::IsQuestCompleted(FName QuestId) const
{
	const FMTQuestSaveState* State = QuestStates.Find(QuestId);
	return State && (State->bCompleted || State->TimesCompleted > 0);
}

bool UMTQuestSubsystem::IsQuestActive(FName QuestId) const
{
	const FMTQuestSaveState* State = QuestStates.Find(QuestId);
	return State && State->bActive;
}

bool UMTQuestSubsystem::AreObjectivesComplete(FName QuestId) const
{
	const FMTQuestSaveState* State = QuestStates.Find(QuestId);
	const FMTQuestData* Quest = FindQuestData(QuestId);
	return State && Quest && State->bActive && AllObjectivesDone(*Quest, *State);
}

bool UMTQuestSubsystem::IsObjectiveComplete(FName QuestId, int32 ObjectiveIndex) const
{
	const FMTQuestSaveState* State = QuestStates.Find(QuestId);
	const FMTQuestData* Quest = FindQuestData(QuestId);
	if (!State || !Quest || !Quest->Objectives.IsValidIndex(ObjectiveIndex))
	{
		return false;
	}
	return IsObjectiveDone(Quest->Objectives[ObjectiveIndex], *State, ObjectiveIndex);
}

TArray<FName> UMTQuestSubsystem::GetCompletedQuestIds() const
{
	TArray<FName> Result;
	for (const TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		if (Pair.Value.bCompleted || Pair.Value.TimesCompleted > 0)
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

TArray<FName> UMTQuestSubsystem::GetAvailableQuestsForNPC(FName NpcId) const
{
	TArray<FName> Result;
	const UMTDataRegistry* Registry = GetRegistry();
	if (!Registry || NpcId.IsNone())
	{
		return Result;
	}
	for (const TPair<FName, FMTQuestData>& Pair : Registry->GetQuests())
	{
		if (Pair.Value.GiverNPC == NpcId && Pair.Value.Type != EMTQuestType::WorldEvent && CanAcceptQuest(Pair.Key))
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

TArray<FName> UMTQuestSubsystem::GetTurnInQuestsForNPC(FName NpcId) const
{
	TArray<FName> Result;
	if (NpcId.IsNone())
	{
		return Result;
	}
	for (const TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		if (!Pair.Value.bActive)
		{
			continue;
		}
		const FMTQuestData* Quest = FindQuestData(Pair.Key);
		if (Quest && Quest->TurnInNPC == NpcId && AllObjectivesDone(*Quest, Pair.Value))
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

bool UMTQuestSubsystem::HasPendingObjectiveForNPC(FName NpcId) const
{
	if (NpcId.IsNone())
	{
		return false;
	}
	for (const TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		const FMTQuestData* Quest = Pair.Value.bActive ? FindQuestData(Pair.Key) : nullptr;
		if (!Quest)
		{
			continue;
		}
		for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
		{
			const FMTQuestObjective& Objective = Quest->Objectives[Index];
			if (!IsObjectiveCurrent(*Quest, Pair.Value, Index) || IsObjectiveDone(Objective, Pair.Value, Index))
			{
				continue;
			}
			if ((Objective.Type == EMTObjectiveType::TalkTo && Objective.TargetId == NpcId)
				|| (Objective.Type == EMTObjectiveType::Deliver && Objective.SecondaryId == NpcId))
			{
				return true;
			}
		}
	}
	return false;
}

int32 UMTQuestSubsystem::GetCurrentObjectiveIndex(FName QuestId) const
{
	const FMTQuestSaveState* State = QuestStates.Find(QuestId);
	const FMTQuestData* Quest = FindQuestData(QuestId);
	if (!Quest)
	{
		return INDEX_NONE;
	}
	if (!State)
	{
		return Quest->Objectives.Num() > 0 ? 0 : INDEX_NONE;
	}
	if (Quest->bSequentialObjectives)
	{
		return Quest->Objectives.IsValidIndex(State->Stage) ? State->Stage : INDEX_NONE;
	}
	for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
	{
		if (!IsObjectiveDone(Quest->Objectives[Index], *State, Index))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FText UMTQuestSubsystem::GetObjectiveText(FName QuestId, int32 ObjectiveIndex) const
{
	const FMTQuestData* Quest = FindQuestData(QuestId);
	if (!Quest || !Quest->Objectives.IsValidIndex(ObjectiveIndex))
	{
		return FText::GetEmpty();
	}
	const FMTQuestObjective& Objective = Quest->Objectives[ObjectiveIndex];
	const FMTQuestSaveState* State = QuestStates.Find(QuestId);
	const int32 Current = (State && State->Progress.IsValidIndex(ObjectiveIndex)) ? State->Progress[ObjectiveIndex] : 0;
	const int32 Required = GetRequiredProgress(Objective);
	const bool bDone = Current >= Required;
	const FText Base = Objective.Description.IsEmpty() ? BuildDefaultObjectiveDescription(Objective) : Objective.Description;

	switch (Objective.Type)
	{
	case EMTObjectiveType::Kill:
	case EMTObjectiveType::Gather:
	case EMTObjectiveType::Interact:
	case EMTObjectiveType::UseAbility:
		if (Required > 1)
		{
			return FText::Format(LOCTEXT("ObjCount", "{0} ({1}/{2})"), Base, FText::AsNumber(FMath::Min(Current, Required)), FText::AsNumber(Required));
		}
		break;

	case EMTObjectiveType::Deliver:
		if (!bDone)
		{
			UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this);
			const int32 InBag = Progression ? Progression->GetItemCount(Objective.TargetId) : 0;
			return FText::Format(LOCTEXT("ObjDeliver", "{0} ({1}/{2})"), Base, FText::AsNumber(FMath::Min(InBag, Required)), FText::AsNumber(Required));
		}
		break;

	case EMTObjectiveType::Defend:
		if (!bDone)
		{
			float Elapsed = static_cast<float>(Current);
			if (const TArray<float>* Timers = DefendElapsed.Find(QuestId))
			{
				if (Timers->IsValidIndex(ObjectiveIndex))
				{
					Elapsed = FMath::Max(Elapsed, (*Timers)[ObjectiveIndex]);
				}
			}
			return FText::Format(LOCTEXT("ObjDefend", "{0} ({1} left)"), Base, MTQuestPrivate::FormatClock(Objective.Duration - Elapsed));
		}
		break;

	default:
		break;
	}

	if (bDone)
	{
		return FText::Format(LOCTEXT("ObjDone", "{0} (Done)"), Base);
	}
	return Base;
}

FText UMTQuestSubsystem::BuildDefaultObjectiveDescription(const FMTQuestObjective& Objective) const
{
	const FText Target = GetIdDisplayName(Objective.TargetId, Objective.Type);
	switch (Objective.Type)
	{
	case EMTObjectiveType::Kill:       return FText::Format(LOCTEXT("DescKill", "Defeat {0}"), Target);
	case EMTObjectiveType::Gather:     return FText::Format(LOCTEXT("DescGather", "Gather {0}"), Target);
	case EMTObjectiveType::Deliver:    return FText::Format(LOCTEXT("DescDeliver", "Deliver {0} to {1}"), Target, GetNPCDisplayName(Objective.SecondaryId));
	case EMTObjectiveType::TalkTo:     return FText::Format(LOCTEXT("DescTalk", "Talk to {0}"), Target);
	case EMTObjectiveType::Reach:      return FText::Format(LOCTEXT("DescReach", "Travel to {0}"), Target);
	case EMTObjectiveType::Defend:     return FText::Format(LOCTEXT("DescDefend", "Defend {0}"), Target);
	case EMTObjectiveType::Escort:     return FText::Format(LOCTEXT("DescEscort", "Escort {0} to {1}"), Target, GetIdDisplayName(Objective.SecondaryId, EMTObjectiveType::Reach));
	case EMTObjectiveType::Interact:   return FText::Format(LOCTEXT("DescInteract", "Examine {0}"), Target);
	case EMTObjectiveType::Puzzle:     return FText::Format(LOCTEXT("DescPuzzle", "Solve {0}"), Target);
	case EMTObjectiveType::DefeatBoss: return FText::Format(LOCTEXT("DescBoss", "Defeat {0}"), Target);
	case EMTObjectiveType::UseAbility: return FText::Format(LOCTEXT("DescAbility", "Use {0}"), Target);
	}
	return Target;
}

FText UMTQuestSubsystem::GetIdDisplayName(FName Id, EMTObjectiveType Type) const
{
	if (Id.IsNone())
	{
		return LOCTEXT("AnyTarget", "any target");
	}
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		switch (Type)
		{
		case EMTObjectiveType::Kill:
		case EMTObjectiveType::DefeatBoss:
			if (const FMTEnemyData* Enemy = Registry->FindEnemy(Id))
			{
				if (!Enemy->DisplayName.IsEmpty()) { return Enemy->DisplayName; }
			}
			break;
		case EMTObjectiveType::Gather:
		case EMTObjectiveType::Deliver:
			if (const FMTItemData* Item = Registry->FindItem(Id))
			{
				if (!Item->DisplayName.IsEmpty()) { return Item->DisplayName; }
			}
			break;
		case EMTObjectiveType::UseAbility:
			if (const FMTAbilityData* Ability = Registry->FindAbility(Id))
			{
				if (!Ability->DisplayName.IsEmpty()) { return Ability->DisplayName; }
			}
			break;
		case EMTObjectiveType::TalkTo:
		case EMTObjectiveType::Defend:
		case EMTObjectiveType::Escort:
			return GetNPCDisplayName(Id);
		default:
			break;
		}
		if (const FMTLocationData* Location = Registry->FindLocation(Id))
		{
			if (!Location->DisplayName.IsEmpty()) { return Location->DisplayName; }
		}
	}
	// Tag-style ids ("Enemy.Beast") read better as their leaf.
	FString AsString = Id.ToString();
	int32 DotIndex = INDEX_NONE;
	if (AsString.FindLastChar(TEXT('.'), DotIndex))
	{
		AsString = AsString.Mid(DotIndex + 1);
	}
	return FText::FromString(AsString);
}

FText UMTQuestSubsystem::GetNPCDisplayName(FName NpcId) const
{
	if (NpcId.IsNone())
	{
		return FText::GetEmpty();
	}
	if (const AActor* Actor = FindQuestActor(NpcId))
	{
		if (const UMTQuestGiverComponent* Giver = Actor->FindComponentByClass<UMTQuestGiverComponent>())
		{
			const FText Name = Giver->GetDisplayName();
			if (!Name.IsEmpty())
			{
				return Name;
			}
		}
	}
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		if (const FMTCharacterData* Character = Registry->FindCharacter(NpcId))
		{
			if (!Character->DisplayName.IsEmpty())
			{
				return Character->DisplayName;
			}
		}
	}
	return FText::FromName(NpcId);
}

// ---------------------------------------------------------------------------------------------
// Markers / quest actor registry
// ---------------------------------------------------------------------------------------------

void UMTQuestSubsystem::RegisterQuestActor(FName Id, AActor* Actor)
{
	if (Id.IsNone() || !Actor)
	{
		return;
	}
	TArray<TWeakObjectPtr<AActor>>& List = QuestActors.FindOrAdd(Id);
	List.RemoveAll([](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid(); });
	const TWeakObjectPtr<AActor> Weak(Actor);
	if (!List.Contains(Weak))
	{
		List.Add(Weak);
	}
}

void UMTQuestSubsystem::UnregisterQuestActor(FName Id, AActor* Actor)
{
	TArray<TWeakObjectPtr<AActor>>* List = QuestActors.Find(Id);
	if (!List)
	{
		return;
	}
	if (!Actor)
	{
		QuestActors.Remove(Id);
		return;
	}
	const TWeakObjectPtr<AActor> Weak(Actor);
	List->RemoveAll([&Weak](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid() || Entry == Weak; });
	if (List->Num() == 0)
	{
		QuestActors.Remove(Id);
	}
}

AActor* UMTQuestSubsystem::FindQuestActor(FName Id, const FVector& Near) const
{
	const TArray<TWeakObjectPtr<AActor>>* List = QuestActors.Find(Id);
	if (!List)
	{
		return nullptr;
	}
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AActor>& Entry : *List)
	{
		AActor* Actor = Entry.Get();
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
		{
			continue;
		}
		if (Near.IsZero())
		{
			return Actor;
		}
		const float DistSq = FVector::DistSquared(Near, Actor->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Actor;
		}
	}
	return Best;
}

bool UMTQuestSubsystem::ResolveIdLocation(FName Id, FVector& OutLocation) const
{
	if (Id.IsNone())
	{
		return false;
	}
	if (const AActor* Actor = FindQuestActor(Id, GetPlayerLocation()))
	{
		OutLocation = Actor->GetActorLocation();
		return true;
	}
	if (const UMTDataRegistry* Registry = GetRegistry())
	{
		if (const FMTLocationData* Location = Registry->FindLocation(Id))
		{
			OutLocation = Location->WorldLocation;
			return true;
		}
	}
	return false;
}

bool UMTQuestSubsystem::GetObjectiveMarker(FName QuestId, FVector& OutWorldLocation) const
{
	const FMTQuestData* Quest = FindQuestData(QuestId);
	if (!Quest)
	{
		return false;
	}
	const FMTQuestSaveState* State = QuestStates.Find(QuestId);
	if (!State || !State->bActive)
	{
		// Not on the quest: point at whoever offers it.
		return ResolveIdLocation(Quest->GiverNPC, OutWorldLocation);
	}
	if (AllObjectivesDone(*Quest, *State))
	{
		return ResolveIdLocation(Quest->TurnInNPC, OutWorldLocation);
	}

	const int32 Index = GetCurrentObjectiveIndex(QuestId);
	if (!Quest->Objectives.IsValidIndex(Index))
	{
		return false;
	}
	const FMTQuestObjective& Objective = Quest->Objectives[Index];
	if (ResolveIdLocation(Objective.MarkerLocationId, OutWorldLocation))
	{
		return true;
	}

	FName Primary = Objective.TargetId;
	FName Secondary = NAME_None;
	switch (Objective.Type)
	{
	case EMTObjectiveType::Deliver:
	{
		const FName DeliverTo = !Objective.SecondaryId.IsNone() ? Objective.SecondaryId
			: (!Quest->TurnInNPC.IsNone() ? Quest->TurnInNPC : Quest->GiverNPC);
		UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this);
		const bool bHasItems = Progression && Progression->GetItemCount(Objective.TargetId) >= GetRequiredProgress(Objective);
		Primary = bHasItems ? DeliverTo : Objective.TargetId;
		Secondary = DeliverTo;
		break;
	}
	case EMTObjectiveType::Escort:
		Primary = Objective.SecondaryId; // destination
		Secondary = Objective.TargetId;
		break;
	default:
		break;
	}
	return ResolveIdLocation(Primary, OutWorldLocation) || ResolveIdLocation(Secondary, OutWorldLocation);
}

// ---------------------------------------------------------------------------------------------
// Save / load
// ---------------------------------------------------------------------------------------------

void UMTQuestSubsystem::HandleGatherSaveData(FMTSaveData& Data)
{
	Data.Quests.Reset();
	for (const TPair<FName, FMTQuestSaveState>& Pair : QuestStates)
	{
		FMTQuestSaveState Copy = Pair.Value;
		Copy.QuestId = Pair.Key;
		Data.Quests.Add(Copy);
	}
	Data.TrackedQuest = TrackedQuest;
}

void UMTQuestSubsystem::HandleApplySaveData(const FMTSaveData& Data)
{
	QuestStates.Reset();
	DefendElapsed.Reset();
	ReadyNotified.Reset();

	for (const FMTQuestSaveState& Saved : Data.Quests)
	{
		if (Saved.QuestId.IsNone())
		{
			continue;
		}
		FMTQuestSaveState& State = QuestStates.Add(Saved.QuestId, Saved);
		const FMTQuestData* Quest = FindQuestData(Saved.QuestId);
		if (!Quest)
		{
			// Quest removed from data since the save: never leave a dead quest active.
			State.bActive = false;
			continue;
		}
		EnsureProgressSize(*Quest, State);
		if (State.bActive)
		{
			TArray<float>& Timers = DefendElapsed.Add(Saved.QuestId);
			Timers.SetNumZeroed(Quest->Objectives.Num());
			for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
			{
				if (Quest->Objectives[Index].Type == EMTObjectiveType::Defend)
				{
					Timers[Index] = static_cast<float>(State.Progress[Index]);
				}
			}
			if (AllObjectivesDone(*Quest, State))
			{
				ReadyNotified.Add(Saved.QuestId);
			}
		}
	}

	TrackedQuest = Data.TrackedQuest;
	RetrackIfNeeded(NAME_None);

	const TArray<FName> Active = GetActiveQuestIds();
	for (const FName& QuestId : Active)
	{
		OnQuestUpdated.Broadcast(QuestId);
	}
	UE_LOG(LogMushoku, Log, TEXT("Quest states restored: %d entries (%d active)."), QuestStates.Num(), Active.Num());
}

#undef LOCTEXT_NAMESPACE

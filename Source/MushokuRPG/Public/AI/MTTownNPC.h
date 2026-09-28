// Neutral villager (team 3) with a daily schedule and a quest-giver/talk component.
#pragma once

#include "CoreMinimal.h"
#include "Character/MTCharacterBase.h"
#include "AI/MTNPCScheduleComponent.h"
#include "MTTownNPC.generated.h"

class UMTQuestGiverComponent;

UCLASS(Blueprintable)
class MUSHOKURPG_API AMTTownNPC : public AMTCharacterBase
{
	GENERATED_BODY()

public:
	AMTTownNPC(const FObjectInitializer& ObjectInitializer);

	/** Quest / dialogue id (quest GiverNPC, TurnInNPC, TalkTo targets). Also used as GameplayId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|NPC") FName NpcId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|NPC") FText NpcDisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|NPC") float NpcMaxHealth = 150.f;

	UFUNCTION(BlueprintPure, Category = "Mushoku|NPC") EMTNPCActivity GetCurrentActivity() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|NPC") bool IsCarrying() const;

	UMTNPCScheduleComponent* GetSchedule() const { return Schedule; }
	UMTQuestGiverComponent* GetQuestGiver() const { return QuestGiver; }

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void HandleDeath(AActor* Killer) override;

	void SyncIdentity();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|NPC") TObjectPtr<UMTNPCScheduleComponent> Schedule;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|NPC") TObjectPtr<UMTQuestGiverComponent> QuestGiver;
};

// Global gameplay event bus. Combat, interaction and world systems broadcast here;
// quests, progression, achievements and UI listen. Keeps systems decoupled.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/MTTypes.h"
#include "MTGameEvents.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMTOnEnemyKilled, FName, EnemyId, FGameplayTagContainer, EnemyTags, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnItemCollected, FName, ItemId, int32, Count);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnIdEvent, FName, Id);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnAbilityUsed, FName, AbilityId, AActor*, User);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnNotification, FText, Message, FLinearColor, Color);

UCLASS()
class MUSHOKURPG_API UMTGameEvents : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UMTGameEvents* Get(const UObject* WorldContext);

	UPROPERTY(BlueprintAssignable) FMTOnEnemyKilled OnEnemyKilled;
	UPROPERTY(BlueprintAssignable) FMTOnItemCollected OnItemCollected;
	/** Player talked to an NPC (id). */
	UPROPERTY(BlueprintAssignable) FMTOnIdEvent OnNPCInteracted;
	/** Player interacted with a world object (id). */
	UPROPERTY(BlueprintAssignable) FMTOnIdEvent OnObjectInteracted;
	/** Player entered a named location (id). */
	UPROPERTY(BlueprintAssignable) FMTOnIdEvent OnLocationReached;
	UPROPERTY(BlueprintAssignable) FMTOnIdEvent OnBossDefeated;
	UPROPERTY(BlueprintAssignable) FMTOnIdEvent OnPuzzleSolved;
	/** An escorted/defended NPC died (id) - fails defend/escort objectives. */
	UPROPERTY(BlueprintAssignable) FMTOnIdEvent OnProtectedTargetLost;
	UPROPERTY(BlueprintAssignable) FMTOnAbilityUsed OnAbilityUsed;
	/** Toast messages for the HUD (level up, rank up, quest updates). */
	UPROPERTY(BlueprintAssignable) FMTOnNotification OnNotification;

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Events")
	void Notify(const FText& Message, FLinearColor Color = FLinearColor::White) { OnNotification.Broadcast(Message, Color); }
};

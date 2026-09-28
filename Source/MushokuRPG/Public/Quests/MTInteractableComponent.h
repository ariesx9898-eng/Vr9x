// Generic "press E" interaction point, plus the NPC quest-giver variant.
// The player finds the best interactable in range and calls Interact(); the HUD shows
// GetDisplayName()/GetPromptText(). Quest objectives listen on UMTGameEvents.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MTInteractableComponent.generated.h"

class AMTCharacterBase;
class UMTInteractableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnInteractSignature, AMTCharacterBase*, By);
/** Native variant that also carries the component (puzzles bind many runes to one handler). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FMTOnInteractNative, UMTInteractableComponent* /*Component*/, AMTCharacterBase* /*By*/);

UCLASS(ClassGroup = (Mushoku), Blueprintable, meta = (BlueprintSpawnableComponent))
class MUSHOKURPG_API UMTInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMTInteractableComponent();

	/** Broadcast through UMTGameEvents::OnObjectInteracted (Interact objectives). None = silent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Interaction") FName InteractionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Interaction") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Interaction") FText PromptText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Interaction", meta = (ClampMin = "0")) float InteractionRange = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Interaction") bool bSingleUse = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Interaction") bool bEnabled = true;
	/** Register the owner with the quest subsystem so objective markers can find it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Interaction") bool bRegisterAsQuestActor = true;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Interaction") virtual FText GetPromptText() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Interaction") virtual FText GetDisplayName() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Interaction") virtual bool CanInteract(AMTCharacterBase* By) const;
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Interaction") virtual void Interact(AMTCharacterBase* By);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Interaction") FVector GetInteractionLocation() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Interaction") bool HasBeenUsed() const { return bUsed; }
	/** Re-arms a single-use interactable (respawning pickups, puzzle resets). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Interaction") void ResetInteraction();

	/** Best (closest, in-range, usable) interactable for a character; optional helper for the player. */
	static UMTInteractableComponent* FindBestInteractable(AMTCharacterBase* By, float ExtraRange = 0.f);

	UPROPERTY(BlueprintAssignable) FMTOnInteractSignature OnInteracted;
	FMTOnInteractNative OnInteractedNative;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Id this component registers under for quest markers. */
	virtual FName GetQuestRegistrationId() const { return InteractionId; }
	void RegisterWithQuests();
	void UnregisterFromQuests();
	/** Shared tail of Interact(): marks used and fires delegates. */
	void FinishInteraction(AMTCharacterBase* By);

	bool bUsed = false;
	FName RegisteredId;
};

/**
 * NPC variant: talking broadcasts OnNPCInteracted(NpcId) (TalkTo / Deliver objectives) and opens
 * the NPC dialog on the interacting player's HUD.
 */
UCLASS(ClassGroup = (Mushoku), Blueprintable, meta = (BlueprintSpawnableComponent))
class MUSHOKURPG_API UMTQuestGiverComponent : public UMTInteractableComponent
{
	GENERATED_BODY()

public:
	UMTQuestGiverComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Quests") FName NpcId;

	virtual FText GetDisplayName() const override;
	virtual void Interact(AMTCharacterBase* By) override;

	/** "!" when a quest can be accepted, "?" (bTurnIn) when one can be turned in / is waiting here. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Quests")
	bool HasQuestIndicator(bool& bTurnIn) const;

	/** Seconds the NPC stops its schedule to face the player after being talked to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Quests") float ConversationPause = 8.f;

protected:
	virtual FName GetQuestRegistrationId() const override { return NpcId; }

private:
	mutable float IndicatorCacheTime = -1000.f;
	mutable bool bCachedHasIndicator = false;
	mutable bool bCachedTurnIn = false;
};

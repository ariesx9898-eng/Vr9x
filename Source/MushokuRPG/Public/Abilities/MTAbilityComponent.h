// Owns ability instances, hotbar slots, cooldowns, input buffering and mastery reporting.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/MTDataTypes.h"
#include "MTAbilityComponent.generated.h"

class UMTAbility;
class AMTCharacterBase;

/** Hotbar slots. Keys: LMB, 1-3, F, G, 4-6, 7-9, R, T. */
UENUM(BlueprintType)
enum class EMTAbilitySlot : uint8
{
	Basic,
	Character1,
	Character2,
	Character3,
	Special,
	Awakening,
	ElementA1,
	ElementA2,
	ElementA3,
	ElementB1,
	ElementB2,
	ElementB3,
	RaceActive,
	RaceTransformation,
	MAX UMETA(Hidden)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnAbilityEvent, FName, AbilityId, EMTAbilitySlot, Slot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnAbilityFailed, FName, AbilityId, FText, Reason);

UCLASS(ClassGroup = (Mushoku), meta = (BlueprintSpawnableComponent))
class MUSHOKURPG_API UMTAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMTAbilityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Puts an ability row into a slot (creates the instance). None clears. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Abilities")
	void SetSlot(EMTAbilitySlot Slot, FName AbilityId);

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Abilities")
	void ClearAllSlots();

	UFUNCTION(BlueprintPure, Category = "Mushoku|Abilities")
	FName GetSlotAbilityId(EMTAbilitySlot Slot) const;

	/** Input entry points (buffered when another ability is running). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Abilities")
	void PressSlot(EMTAbilitySlot Slot);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Abilities")
	void ReleaseSlot(EMTAbilitySlot Slot);

	/** Directly activates an ability by id (AI, sequences). Creates the instance on demand. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Abilities")
	bool ActivateAbilityById(FName AbilityId);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Abilities")
	void ReleaseAbilityById(FName AbilityId);

	/** Cancels every running ability (stagger, death). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Abilities")
	void CancelAll();

	/** True while an ability is in anticipation/action (movement abilities & new casts wait). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Abilities")
	bool IsCasting() const;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Abilities")
	float GetCooldownRemaining(FName AbilityId) const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Abilities")
	float GetCooldownFraction(FName AbilityId) const;

	UMTAbility* FindOrCreateAbility(FName AbilityId);
	UMTAbility* GetActiveAbility() const;
	UMTAbility* GetSlotAbility(EMTAbilitySlot Slot) const;
	const FMTAbilityData* GetSlotData(EMTAbilitySlot Slot) const;

	/** Called by abilities. */
	void StartCooldown(FName AbilityId, float Seconds);
	void NotifyAbilityStarted(UMTAbility* Ability);
	void NotifyAbilityEnded(UMTAbility* Ability, bool bCancelled);
	/** Called when an ability's effect connects (for mastery XP / awakening meter). */
	void NotifyAbilityHit(FName AbilityId, float DamageDealt);

	/** Awakening overrides: while active, pressing X actually fires Y. */
	void PushAbilityOverrides(const TMap<FName, FName>& Overrides);
	void PopAbilityOverrides(const TMap<FName, FName>& Overrides);
	FName ResolveOverride(FName AbilityId) const;

	AMTCharacterBase* GetOwnerCharacter() const;

	/** Seconds an input is remembered while another ability is busy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Abilities") float InputBufferTime = 0.25f;

	UPROPERTY(BlueprintAssignable) FMTOnAbilityEvent OnAbilityActivated;
	UPROPERTY(BlueprintAssignable) FMTOnAbilityEvent OnAbilityEnded;
	UPROPERTY(BlueprintAssignable) FMTOnAbilityFailed OnAbilityFailed;

	static FString SlotToKeyLabel(EMTAbilitySlot Slot);

private:
	bool TryActivateInstance(UMTAbility* Ability, EMTAbilitySlot Slot);

	UPROPERTY() TMap<FName, TObjectPtr<UMTAbility>> Instances;
	FName Slots[(int32)EMTAbilitySlot::MAX];
	TMap<FName, float> CooldownEnd;
	TMap<FName, float> CooldownDuration;
	TMap<FName, FName> ActiveOverrides;

	TWeakObjectPtr<UMTAbility> ActiveAbility;
	/** Abilities that keep running in the background (buffs, zones) are not "active". */
	UPROPERTY() TArray<TObjectPtr<UMTAbility>> Running;

	EMTAbilitySlot BufferedSlot = EMTAbilitySlot::MAX;
	float BufferedAt = -1.f;
	bool bBufferedReleased = false;
};

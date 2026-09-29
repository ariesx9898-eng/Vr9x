// Owns ability instances, hotbar slots, cooldowns, input buffering and mastery reporting.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/MTDataTypes.h"
#include "MTAbilityComponent.generated.h"

class UMTAbility;
class AMTCharacterBase;

/** Ability slots. The player's hotbar is LMB (Basic), 1-4 (Loadout1-4, chosen in the ABILITIES menu), F (Special) and
 *  G (Awakening). Character/Element/Race slots remain for AI characters and data compatibility. */
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
	Loadout1,
	Loadout2,
	Loadout3,
	Loadout4,
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

	/** Frame events from the playing clip (UMTAnimNotify_Event), forwarded to the ability that is casting. */
	void HandleAnimEvent(FName EventName, const class UAnimSequenceBase* Animation);

	/** Disturb Magic's seal: AbilityId cannot start for Seconds (its hotbar slot shows the wait as a cooldown). */
	void LockAbility(FName AbilityId, float Seconds);
	bool IsAbilityLocked(FName AbilityId) const;

	/** Follow-up window (Dragon Step -> Dragon Crush): the next start of AbilityId within Seconds gets its combo bonus. */
	void OpenComboWindow(FName AbilityId, float Seconds, AActor* Target);
	/** True (and the window closes) when AbilityId starts inside an open window; OutTarget is the enemy that opened it. */
	bool ConsumeComboWindow(FName AbilityId, AActor*& OutTarget);
	bool IsComboWindowOpen(FName AbilityId) const;

	/** Awakening overrides: while active, pressing X actually fires Y. */
	void PushAbilityOverrides(const TMap<FName, FName>& Overrides);
	void PopAbilityOverrides(const TMap<FName, FName>& Overrides);
	FName ResolveOverride(FName AbilityId) const;

	AMTCharacterBase* GetOwnerCharacter() const;

	/** Admin panel (UMTAdminSubsystem): abilities never go on cooldown. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Mushoku|Abilities") bool bNoCooldowns = false;
	/** Clears every running cooldown. */
	void ResetAllCooldowns() { CooldownEnd.Reset(); }

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

	/** Sealed abilities and the world time their seal ends. */
	TMap<FName, float> LockedUntil;
	FName ComboAbilityId;
	float ComboUntil = -1.f;
	TWeakObjectPtr<AActor> ComboTarget;

	EMTAbilitySlot BufferedSlot = EMTAbilitySlot::MAX;
	float BufferedAt = -1.f;
	bool bBufferedReleased = false;
};

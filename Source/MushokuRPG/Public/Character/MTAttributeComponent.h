// Health / Mana / Stamina / Poise, status effects and damage resolution.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Core/MTTypes.h"
#include "MTAttributeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnAttributeChanged, float, NewValue, float, MaxValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnDamaged, const FMTDamageSpec&, Spec, const FMTDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnDeath, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnStatusChanged, FName, EffectId);

UCLASS(ClassGroup = (Mushoku), meta = (BlueprintSpawnableComponent))
class MUSHOKURPG_API UMTAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMTAttributeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Sets maxima and refills. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes")
	void InitializeAttributes(float InMaxHealth, float InMaxMana, float InMaxStamina, float InMaxPoise, float InManaRegen);

	/** Resolves a hit: resistances, combo scaling, poise, reactions, death. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes")
	FMTDamageResult ApplyDamage(const FMTDamageSpec& Spec);

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes") void Heal(float Amount);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes") bool SpendMana(float Amount);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes") bool SpendStamina(float Amount);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes") void RestoreMana(float Amount);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes") void RestoreStamina(float Amount);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes") void RestorePoise(float Amount);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes") void AddAwakeningMeter(float Amount);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes") bool ConsumeAwakeningMeter(float Amount);

	/** Adds or refreshes a status effect (same Id refreshes). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes")
	void AddStatusEffect(const FMTStatusEffect& Effect);

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes")
	void RemoveStatusEffect(FName EffectId);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes")
	bool HasStatusEffect(FName EffectId) const;

	/** Combined modifiers from every active effect plus permanent passives. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") FMTStatModifier GetStatModifier() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") FMTMovementModifier GetMovementModifier() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") FGameplayTagContainer GetStatusTags() const;

	/** Permanent modifiers from character passive + race passive (set by the character). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Attributes")
	void SetPermanentModifiers(const FMTStatModifier& Stats, const FMTMovementModifier& Movement);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") bool IsAlive() const { return Health > 0.f; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") float GetMaxHealth() const { return MaxHealth; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") float GetMana() const { return Mana; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") float GetMaxMana() const { return MaxMana; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") float GetStamina() const { return Stamina; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") float GetMaxStamina() const { return MaxStamina; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") float GetPoise() const { return Poise; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") float GetMaxPoise() const { return MaxPoise; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Attributes") float GetAwakeningMeter() const { return AwakeningMeter; }
	const TArray<FMTStatusEffect>& GetStatusEffects() const { return StatusEffects; }

	/** Invulnerability frames (dodges, dash). */
	void SetInvulnerableFor(float Seconds);
	bool IsInvulnerable() const;

	/** Blocking reduces damage from the front and chips stamina. */
	void SetBlocking(bool bInBlocking) { bBlocking = bInBlocking; }
	bool IsBlocking() const { return bBlocking; }

	/** Bosses: immune to hard CC and resistant to stagger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Attributes") bool bCrowdControlImmune = false;
	/** Weak enemies are pushed by pressure auras. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Attributes") bool bIsWeak = false;
	/** Seconds of sprinting/dodging without stamina regen delay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Attributes") float StaminaRegen = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Attributes") float StaminaRegenDelay = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Attributes") float PoiseRegen = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Attributes") float HealthRegen = 0.f;

	/** Combo scaling: each extra hit from the same attacker inside the window scales down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Combat") float ComboWindow = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Combat") float ComboDecayPerHit = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Combat") float ComboMinScale = 0.3f;
	/** After this many hits in one combo the target becomes briefly hit-stun immune (no infinites). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Combat") int32 MaxComboHitsBeforeBurst = 8;

	UPROPERTY(BlueprintAssignable) FMTOnAttributeChanged OnHealthChanged;
	UPROPERTY(BlueprintAssignable) FMTOnAttributeChanged OnManaChanged;
	UPROPERTY(BlueprintAssignable) FMTOnAttributeChanged OnStaminaChanged;
	UPROPERTY(BlueprintAssignable) FMTOnDamaged OnDamaged;
	UPROPERTY(BlueprintAssignable) FMTOnDeath OnDeath;
	UPROPERTY(BlueprintAssignable) FMTOnStatusChanged OnStatusAdded;
	UPROPERTY(BlueprintAssignable) FMTOnStatusChanged OnStatusRemoved;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float Health = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float MaxHealth = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float Mana = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float MaxMana = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float ManaRegen = 10.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float Stamina = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float MaxStamina = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float Poise = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float MaxPoise = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") float AwakeningMeter = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Attributes") TArray<FMTStatusEffect> StatusEffects;

private:
	FMTStatModifier PermanentStats;
	FMTMovementModifier PermanentMovement;
	float InvulnerableUntil = -1.f;
	float LastStaminaSpendTime = -100.f;
	float LastPoiseHitTime = -100.f;
	bool bBlocking = false;

	// Combo tracking (per attacker).
	TWeakObjectPtr<AActor> ComboAttacker;
	int32 ComboHits = 0;
	float LastComboHitTime = -100.f;
	float HitStunImmuneUntil = -1.f;
};

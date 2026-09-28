// Boss: health-threshold phases with an invulnerable transition, a leashed arena, weighted
// phase-gated attack patterns, explicit recovery windows and defensive reactions.
#pragma once

#include "CoreMinimal.h"
#include "AI/MTEnemyCharacter.h"
#include "MTBossCharacter.generated.h"

class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnBossPhaseSignature, int32, NewPhase);

UCLASS(Blueprintable)
class MUSHOKURPG_API AMTBossCharacter : public AMTEnemyCharacter
{
	GENERATED_BODY()

public:
	AMTBossCharacter(const FObjectInitializer& ObjectInitializer);

	/** Arena centre (zero = where the boss spawned). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss|Arena") FVector ArenaCenter = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss|Arena") float ArenaRadius = 2500.f;
	/** The boss will not chase a target this far outside the arena. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss|Arena") float TargetLeashTolerance = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss") float PhaseTransitionDuration = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss") TSoftObjectPtr<UAnimMontage> PhaseTransitionMontage;
	/** Attacks with at least this telegraph are "big": longer, explicit recovery afterwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss") float BigAttackTelegraphThreshold = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss") float BigAttackRecoveryMultiplier = 1.6f;
	/** Chance to react (block / sidestep) when the target starts charging a spell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss") float DefensiveReactionChance = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss") float DefensiveReactionCooldown = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Boss") float SidestepChance = 0.5f;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Boss") int32 GetCurrentPhase() const { return CurrentPhase; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Boss") bool IsPhaseTransitioning() const { return bPhaseTransition; }
	/** True while the boss is punishable after an attack. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Boss") bool IsInRecoveryWindow() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Boss") FVector GetArenaCenter() const { return ResolvedArenaCenter; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Boss") bool IsLocationInsideArena(const FVector& Location, float Tolerance = 0.f) const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Boss") bool IsReturningToArena() const { return bReturningToArena; }

	virtual bool IsBoss() const override { return true; }
	virtual void GetUsableAttacks(TArray<FMTEnemyAttack>& OutAttacks) const override;
	virtual bool GetDefensiveReaction(const AMTCharacterBase* Target, bool& bOutSidestep) override;

	UPROPERTY(BlueprintAssignable) FMTOnBossPhaseSignature OnPhaseChanged;
	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Boss") void ReceivePhaseChanged(int32 NewPhase);
	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Boss") void ReceiveRecoveryWindow(float Duration);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result) override;
	virtual void HandleDeath(AActor* Killer) override;
	virtual void OnAttackActivated(const FMTEnemyAttack& Attack, bool bActivated, float CastTime) override;

	int32 ComputePhaseForHealth(float HealthFraction) const;
	void EnterPhase(int32 NewPhase);
	void EndPhaseTransition();
	void UpdateLeash();

	int32 CurrentPhase = 0;
	bool bPhaseTransition = false;
	bool bReturningToArena = false;
	bool bBossDefeatHandled = false;
	FVector ResolvedArenaCenter = FVector::ZeroVector;
	float NextDefensiveTime = 0.f;
	FTimerHandle PhaseTimer;
	FTimerHandle LeashTimer;
};

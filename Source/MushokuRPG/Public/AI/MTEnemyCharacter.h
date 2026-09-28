// Data-driven enemy. Loads FMTEnemyData (EnemyId) at BeginPlay; attacks go through a
// telegraph -> ability -> recovery pipeline so players (and Demon Eye) can read them.
#pragma once

#include "CoreMinimal.h"
#include "Character/MTCharacterBase.h"
#include "Core/MTDataTypes.h"
#include "MTEnemyCharacter.generated.h"

class AMTPickupActor;
class AMTEnemyCharacter;

/** Successful counter / perfect dodge by an enemy (Kind = "DisturbMagic", "PerfectDodge", ...). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FMTOnEnemyPerfectDefense, AMTEnemyCharacter* /*Enemy*/, FName /*Kind*/);

UCLASS(Blueprintable)
class MUSHOKURPG_API AMTEnemyCharacter : public AMTCharacterBase
{
	GENERATED_BODY()

public:
	AMTEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	/** Enemy row id. Spawners set it before BeginPlay (deferred spawn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Enemy") FName EnemyId;
	/** With a CharacterLineage row (arena Orsted/Rudeus), keep the lineage's attributes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Enemy") bool bUseLineageAttributes = true;
	/** Pickup class spawned for DropTable entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Enemy") TSubclassOf<AMTPickupActor> DropClass;
	/** Seconds the body stays after death (0 = never auto-destroy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Enemy") float CorpseLifeSpan = 12.f;
	/** Enemy kits are not meant to be mana-gated: generous pool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Enemy") float EnemyMaxMana = 1000.f;

	/** (Re)loads the row: attributes, speed, scale, tags, team, lineage. Called at BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Enemy")
	virtual void InitializeFromData();

	/**
	 * Publishes a telegraph, faces the target, waits Attack.TelegraphTime, then activates the
	 * ability and enters a recovery window. Returns false if the attack could not start.
	 */
	bool ExecuteAttack(const FMTEnemyAttack& Attack, AActor* Target);

	/** Weighted random pick among attacks usable at Distance (range, cooldown, anti-repeat). */
	virtual bool SelectAttack(const AActor* Target, float Distance, FMTEnemyAttack& OutAttack);
	/** Attack list available right now (bosses filter by phase). */
	virtual void GetUsableAttacks(TArray<FMTEnemyAttack>& OutAttacks) const;
	/** Largest MaxRange among attacks whose ability is off cooldown (0 = none ready). */
	float GetMaxReadyAttackRange() const;
	/** Aborts a telegraphed attack before it fires (stagger, counters, phase change). */
	void CancelPendingAttack();

	UFUNCTION(BlueprintPure, Category = "Mushoku|Enemy") bool IsTelegraphing() const { return bTelegraphing; }
	/** Punishable window after an attack fired. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Enemy") bool IsInRecovery() const;
	/** Telegraphing, recovering or casting: the AI must not start anything new. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Enemy") bool IsBusy() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Enemy") virtual bool IsBoss() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Enemy") float GetHealthFraction() const;
	/** Row lineage (arena AI) or the applied character id. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Enemy") FName GetLineageId() const;

	const FMTEnemyData& GetEnemyData() const { return EnemyData; }
	bool HasEnemyData() const { return bHasEnemyData; }

	/** Hook for bosses: react defensively when the target charges a spell (block/sidestep). */
	virtual bool GetDefensiveReaction(const AMTCharacterBase* Target, bool& bOutSidestep) { return false; }

	virtual void NotifyPerfectDefense(FName Kind) override;
	FMTOnEnemyPerfectDefense OnPerfectDefense;

	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Enemy") void ReceiveAttackTelegraphed(FName AttackId, float TelegraphTime);
	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Enemy") void ReceiveAttackReleased(FName AttackId, bool bActivated);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDeath(AActor* Killer) override;

	void ReleasePendingAttack();
	/** Sets the recovery window once the ability fired (bosses extend it after big attacks). */
	virtual void OnAttackActivated(const FMTEnemyAttack& Attack, bool bActivated, float CastTime);
	void SpawnDrops();
	void GrantKillRewards(AActor* Killer);
	void BuildAttacksFromLineage();
	void ApplyAIMovementSettings();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mushoku|Enemy") FMTEnemyData EnemyData;
	bool bHasEnemyData = false;
	bool bDeathHandled = false;
	bool bTelegraphing = false;
	FMTEnemyAttack PendingAttack;
	TWeakObjectPtr<AActor> PendingTarget;
	FTimerHandle AttackTimerHandle;
	float RecoveryEndTime = -1.f;
	FName LastAttackId;
	/** Attacks synthesised from a lineage kit when the row defines none. */
	TArray<FMTEnemyAttack> LineageAttacks;
};

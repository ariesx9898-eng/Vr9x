// Utility AI for enemies, bosses and lineage opponents (arena Orsted / Rudeus).
// Decisions run on a 0.25 s timer (not per frame); Orsted adds a 20 Hz reaction check that
// only looks for spells to collapse with Disturb Magic.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Core/MTTypes.h"
#include "MTEnemyAIController.generated.h"

class AMTCharacterBase;
class AMTEnemyCharacter;
class AMTProjectile;

UENUM(BlueprintType)
enum class EMTAIAction : uint8
{
	Idle,
	Patrol,
	Approach,
	Strafe,
	Attack,
	Dodge,
	Block,
	Retreat,
	Reposition,
	Counter,
	ReturnToArena
};

/** Special-cased lineage kits. */
enum class EMTAILineage : uint8
{
	Generic,
	Orsted,
	Rudeus
};

UCLASS()
class MUSHOKURPG_API AMTEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMTEnemyAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---- Tuning ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI") float EvaluationInterval = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI") float SightRadius = 2500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI") float LoseTargetRadius = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI") float PatrolRadius = 1000.f;
	/** Max melee attackers engaging one target at once; the rest circle at a ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI") int32 MaxAttackersPerTarget = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI") float RetreatHealthFraction = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI") float BlockDuration = 0.6f;
	/** Attacks with MaxRange at or below this count as melee (need an engagement slot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI") float MeleeEngageRange = 600.f;

	// ---- Orsted: Disturb Magic timing test ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Orsted") FName DisturbMagicAbilityId = TEXT("Orsted_DisturbMagic");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Orsted") float DisturbMagicRadius = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Orsted") float DisturbMinTimeToImpact = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Orsted") float DisturbMaxTimeToImpact = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Orsted") float ReactionInterval = 0.05f;

	// ---- Rudeus: ranged kit ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Rudeus") FName StoneCannonAbilityId = TEXT("Rudeus_StoneCannon");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Rudeus") FName QuagmireAbilityId = TEXT("Rudeus_Quagmire");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Rudeus") float StoneCannonHoldTime = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Rudeus") float RudeusMinRange = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Rudeus") float RudeusMaxRange = 1400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|AI|Rudeus") float QuagmireTriggerRange = 600.f;

	UFUNCTION(BlueprintPure, Category = "Mushoku|AI") AMTCharacterBase* GetCombatTarget() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|AI") EMTAIAction GetCurrentAction() const { return CurrentAction; }
	UFUNCTION(BlueprintCallable, Category = "Mushoku|AI") void SetCombatTarget(AMTCharacterBase* NewTarget);
	/** Clears target, movement, tokens and counters (arena resets). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|AI") void ResetAI();

	int32 GetDodgeCount() const { return NumDodges; }
	int32 GetBlockCount() const { return NumBlocks; }
	int32 GetDisruptAttemptCount() const { return NumDisruptAttempts; }
	int32 GetAttackCount() const { return NumAttacks; }

	/** AI controllers currently holding an engagement slot on Target. */
	static int32 GetEngagedAttackerCount(const AActor* Target);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Evaluate();
	void ReactionTick();

	UFUNCTION() void HandlePawnDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result);

	// Perception
	void RefreshLineage(AMTCharacterBase* Me);
	void UpdateTarget(AMTCharacterBase* Me, float Now);
	AMTCharacterBase* FindBestTarget(AMTCharacterBase* Me);
	bool IsAllyBlockingLineOfSight(AMTCharacterBase* Me, AActor* Target) const;
	static bool IsTargetCasting(const AMTCharacterBase* Target);

	// Behaviours
	void EvaluateGeneric(AMTCharacterBase* Me, AMTEnemyCharacter* Enemy, AMTCharacterBase* Target, float Now);
	void EvaluateRudeus(AMTCharacterBase* Me, AMTCharacterBase* Target, float Now);
	bool TryReactToThreats(AMTCharacterBase* Me, AMTEnemyCharacter* Enemy, float Now);
	void DoPatrol(AMTCharacterBase* Me, float Now);
	void DoStrafe(AMTCharacterBase* Me, AActor* Target, float Radius, float Now);
	bool MoveToFlank(AMTCharacterBase* Me, AActor* Target);
	bool StartRetreat(AMTCharacterBase* Me, AActor* Target, float Now);
	void StartBlock(AMTCharacterBase* Me, float Duration);
	void EndBlock();
	bool StartChargedCast(AMTCharacterBase* Me, FName AbilityId, float HoldTime);
	void ReleaseChargedCast();
	void ApproachTarget(AMTCharacterBase* Me, AActor* Target, float AcceptanceRadius);

	// Helpers
	bool ProjectToNav(const FVector& Point, FVector& OutPoint) const;
	/** True when a NavMesh exists around the pawn; without one, moves go straight at the goal instead of failing. */
	bool CanPathfind() const;
	FVector ComputeDodgeDirection(AMTCharacterBase* Me, const FVector& ThreatDirection, const FVector& ThreatCenter, float ThreatRadius) const;
	static bool ComputeTimeToImpact(AMTProjectile* Spell, const AMTCharacterBase* Me, float& OutTimeToImpact);
	void SetAction(EMTAIAction NewAction) { CurrentAction = NewAction; }

	// Engagement slots (anti dog-pile)
	bool TryAcquireEngageToken(AActor* Target, bool bForce);
	void ReleaseEngageToken();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mushoku|AI") EMTAIAction CurrentAction = EMTAIAction::Idle;

	TWeakObjectPtr<AMTCharacterBase> CurrentTarget;
	TWeakObjectPtr<AActor> EngagedTarget;
	TWeakObjectPtr<AActor> LastMoveGoalActor;
	bool bHoldsToken = false;
	float TokenAcquiredTime = 0.f;

	FVector HomeLocation = FVector::ZeroVector;
	EMTAILineage LineageKind = EMTAILineage::Generic;
	FName CachedLineage;
	bool bLineageResolved = false;

	FTimerHandle EvaluateTimer;
	FTimerHandle ReactionTimer;
	FTimerHandle BlockTimer;
	FTimerHandle ChargeTimer;

	float NextPerceptionTime = 0.f;
	float NextPatrolTime = 0.f;
	float NextStrafeFlipTime = 0.f;
	float NextStrafeMoveTime = 0.f;
	float NextLOSCheckTime = 0.f;
	float LastAttackTime = -100.f;
	float ActionLockUntil = 0.f;
	float RetreatUntil = 0.f;
	float NextRetreatAllowedTime = 0.f;
	float StrafeSign = 1.f;
	uint32 LastThreatKey = 0;
	bool bAllyBlockingLOS = false;
	bool bBlocking = false;
	bool bRetreating = false;
	bool bChargingCast = false;
	bool bDeadHandled = false;
	FName ChargingAbilityId;

	int32 NumDodges = 0;
	int32 NumBlocks = 0;
	int32 NumDisruptAttempts = 0;
	int32 NumAttacks = 0;
};

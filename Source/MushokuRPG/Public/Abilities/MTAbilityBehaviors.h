// Reusable ability behaviours. Every ability row maps to one of these by EMTAbilityBehavior,
// so new characters/elements are added in data, not by duplicating graphs.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/MTAbility.h"
#include "Engine/TimerHandle.h"
#include "MTAbilityBehaviors.generated.h"

class AMTProjectile;
class AMTCharacterBase;

/** Stone Cannon, Fire Burst, Water Cannon, Air Blade, basic bolts, Inferno Compression. */
UCLASS()
class MUSHOKURPG_API UMTAbility_Projectile : public UMTAbility
{
	GENERATED_BODY()
public:
	/** Spawns one projectile for any projectile row (used by barrages too). */
	static AMTProjectile* FireProjectile(AMTCharacterBase* Owner, const FMTAbilityData& Row, const FVector& From, const FVector& AimPoint, float ChargeAlpha);
protected:
	virtual void ExecuteAction() override;
};

/** Quagmire, Flame Field, Frost Prison, Cumulonimbus, Tempest Domain. */
UCLASS()
class MUSHOKURPG_API UMTAbility_Zone : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void ExecuteAction() override;
	FVector ResolveGroundTarget() const;
};

/** Elemental Barrage: timed chain of different projectile rows; caster may reposition. */
UCLASS()
class MUSHOKURPG_API UMTAbility_Sequence : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void ExecuteAction() override;
	virtual void TickAction(float DeltaTime) override;
	virtual bool IsInstantAction() const override { return false; }
	virtual void OnEnded(bool bWasCancelled) override;
	int32 NextStep = 0;
	float StepTimer = 0.f;
};

/** Dragon Step, Gale Step, racial dashes, enemy lunges. Root-motion driven, collision-safe. */
UCLASS()
class MUSHOKURPG_API UMTAbility_Dash : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void ExecuteAction() override;
	virtual void TickAction(float DeltaTime) override;
	virtual bool IsInstantAction() const override { return false; }
	virtual void OnEnded(bool bWasCancelled) override;
	uint16 RootMotionId = 0;
	float DashDuration = 0.2f;
};

/** Disturb Magic: a short timing window that collapses incoming hostile spells. */
UCLASS()
class MUSHOKURPG_API UMTAbility_Counter : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void ExecuteAction() override;
	virtual void TickAction(float DeltaTime) override;
	virtual bool IsInstantAction() const override { return false; }
	virtual void OnEnded(bool bWasCancelled) override;
	float Window = 0.3f;
	int32 Disrupted = 0;
};

/** Buffs, auras, Demon Eye and awakenings/transformations (with a transformation sequence). */
UCLASS()
class MUSHOKURPG_API UMTAbility_Buff : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void ExecuteAction() override;
	virtual void TickAction(float DeltaTime) override;
	virtual bool IsInstantAction() const override { return false; }
	virtual void OnEnded(bool bWasCancelled) override;
	void ApplyBuff();
	void ExpireBuff();
	bool bBuffApplied = false;
	FTimerHandle ExpireHandle;
};

/** Earth Fortress: raises durable walls in an arc toward the aim direction. */
UCLASS()
class MUSHOKURPG_API UMTAbility_Structure : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void ExecuteAction() override;
};

/** Orsted's palm strikes and monster melee: short sweep, lunges to a locked target. */
UCLASS()
class MUSHOKURPG_API UMTAbility_Melee : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void ExecuteAction() override;
};

namespace MTAbilityFactory
{
	MUSHOKURPG_API TSubclassOf<UMTAbility> ClassForBehavior(EMTAbilityBehavior Behavior);
}

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

/** Quagmire, Flame Wave, Inferno, Flood, Tornado, Wind Burst, Earth Spikes, Frost Prison, Cumulonimbus, Tempest Domain. */
UCLASS()
class MUSHOKURPG_API UMTAbility_Zone : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void ExecuteAction() override;
	/** Telegraphs while casting: Quagmire's spreading target ring, Earth Spikes' aim line. */
	virtual void TickAnticipation(float DeltaTime) override;
	virtual void OnEnded(bool bWasCancelled) override;
	FVector ResolveGroundTarget() const;
	/** Visual radius the zone will have if released now (charge and area stats). */
	float GetPendingRadius() const;
	TWeakObjectPtr<class AMTSpellVFX> TelegraphVFX;
	bool bTelegraphTried = false;
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
	/** Afterimages / dust trail riding with the dasher. */
	TWeakObjectPtr<class AMTSpellVFX> DashVFX;
	float DashDuration = 0.2f;
	/** Dragon Step: the enemy it is stepping beside (arrival facing, combo window). */
	TWeakObjectPtr<AActor> StepTarget;
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
	/** Aura effect for the buff's duration. */
	TWeakObjectPtr<class AMTSpellVFX> AuraVFX;
};

/** Earth Wall: fractured segments rise one after another on an arc toward the aim direction. */
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

/**
 * Elemental Barrage: four element formations orbit the caster, fire their own spells in turn (Stone -> Water -> Wind ->
 * Fire), then collapse together into one combined shot. Steps come from Data.Sequence; the step whose MontageSection is
 * "Finale" is the combined shot.
 */
UCLASS()
class MUSHOKURPG_API UMTAbility_Barrage : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void TickAnticipation(float DeltaTime) override;
	virtual void ExecuteAction() override;
	virtual void TickAction(float DeltaTime) override;
	virtual bool IsInstantAction() const override { return false; }
	virtual void OnEnded(bool bWasCancelled) override;
	virtual void OnAnimEvent(FName EventName) override;

	/** Formation slots: 0 Fire, 1 Water, 2 Earth, 3 Wind. */
	static int32 SlotForElement(EMTElement Element);
	/** Offset of a formation from the caster (local frame), with sway, bob, recoil and the finale's collapse. */
	FVector SlotOffset(int32 Slot) const;
	void SpawnFormations();
	void UpdateFormations(float DeltaTime);
	void FireShot();
	void FireFinale();
	void StopFormations();

	TWeakObjectPtr<class AMTSpellVFX> Formations[4];
	TWeakObjectPtr<class AMTSpellVFX> CollapseVFX;
	float Recoil[4] = { 0.f, 0.f, 0.f, 0.f };
	float BarrageClock = 0.f;
	float ShotTimer = 0.f;
	float SwayClock = 0.f;
	float CollapseAlpha = 0.f;
	int32 ShotIndex = 0;
	bool bFormationsSpawned = false;
	bool bCollapsing = false;
	bool bFinaleFired = false;
};

/**
 * Disturb Magic: a near-invisible pulse runs from the palm to a target. A spell still forming there collapses and that
 * spell is sealed for a few seconds; hostile spells near the pulse (or flying at the caster) collapse; persistent magic
 * at the end of the pulse is destabilised. No damage, no stun.
 */
UCLASS()
class MUSHOKURPG_API UMTAbility_Disrupt : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void TickAnticipation(float DeltaTime) override;
	virtual void ExecuteAction() override;
	virtual void TickAction(float DeltaTime) override;
	virtual bool IsInstantAction() const override { return false; }
	virtual void OnEnded(bool bWasCancelled) override;

	/** A hostile casting in front of the caster (within SearchAngle / SearchRange), or null. */
	AMTCharacterBase* FindCastingTarget() const;
	/** Collapses hostile spells within Radius of the segment A-B. */
	void CollapseSpellsNear(const FVector& A, const FVector& B, float Radius);
	/** Cancels and seals a spell still forming on Target. */
	bool DisruptCaster(AMTCharacterBase* Target);
	/** Weakens hostile zones, walls and serpents around Point. */
	void DestabilizeAround(const FVector& Point);
	/** "DisturbMagic.Collapse<Element>" at Where. */
	void SpawnCollapse(EMTElement Element, const FVector& Where);

	TWeakObjectPtr<AActor> PulseTarget;
	TWeakObjectPtr<class AMTSpellVFX> PulseVFX;
	FVector PulseStart = FVector::ZeroVector;
	FVector PulseHead = FVector::ZeroVector;
	FVector PulseDirection = FVector::ForwardVector;
	float PulseRemaining = 0.f;
	float WardWindow = 0.35f;
	int32 Disrupted = 0;
	bool bPulseArrived = false;
};

/**
 * Dragon Crush: one committed blow. Picks a primary target (combo target, lock, or the closest enemy in a cone), lunges
 * if needed, and on the release frame breaks the ground: the primary takes far more force, everyone in the shockwave is
 * launched outward, and a short hit-stop sells the weight.
 */
UCLASS()
class MUSHOKURPG_API UMTAbility_Strike : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void TickAnticipation(float DeltaTime) override;
	virtual void ExecuteAction() override;
	virtual void OnEnded(bool bWasCancelled) override;
	AMTCharacterBase* ResolvePrimary() const;
	TWeakObjectPtr<AMTCharacterBase> Primary;
	bool bPrimaryResolved = false;
	bool bLunged = false;
};

/** Water Dragon: releases a segmented serpent of water (AMTWaterSerpent) that circles, hunts and collapses. */
UCLASS()
class MUSHOKURPG_API UMTAbility_Serpent : public UMTAbility
{
	GENERATED_BODY()
protected:
	virtual void ExecuteAction() override;
};

namespace MTAbilityFactory
{
	MUSHOKURPG_API TSubclassOf<UMTAbility> ClassForBehavior(EMTAbilityBehavior Behavior);
}

// Base class for every ability. One instance per (owner, ability row). Behaviour
// subclasses implement the Action; this class owns the shared phase machine:
//   Anticipation (cast / charge) -> Action -> Recovery -> Finished
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/MTDataTypes.h"
#include "MTAbility.generated.h"

class AMTCharacterBase;
class UMTAbilityComponent;
class UAnimMontage;
class UAnimSequenceBase;

UENUM(BlueprintType)
enum class EMTAbilityPhase : uint8
{
	Idle,
	Anticipation,   // wind-up; charging happens here while the input is held
	Action,         // the effect executes (may last, e.g. a dash or barrage)
	Recovery,       // follow-through; cancellable by dodge
	Finished
};

UCLASS(Abstract, BlueprintType, Blueprintable)
class MUSHOKURPG_API UMTAbility : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UMTAbilityComponent* InComponent, const FMTAbilityData& InData);

	/** Input pressed. Returns false if the ability could not start. */
	virtual bool TryActivate();
	/** Input released (ends charge for chargeable abilities). */
	virtual void InputReleased();
	/** Called every frame while not Idle. */
	virtual void Tick(float DeltaTime);
	/** Interrupt (stagger, death, dodge-cancel). */
	virtual void Cancel();

	bool IsActive() const { return Phase != EMTAbilityPhase::Idle && Phase != EMTAbilityPhase::Finished; }
	/** True while the ability blocks other abilities (anticipation + action). */
	bool IsBlockingOtherAbilities() const { return Phase == EMTAbilityPhase::Anticipation || Phase == EMTAbilityPhase::Action; }
	EMTAbilityPhase GetPhase() const { return Phase; }
	const FMTAbilityData& GetData() const { return Data; }
	FName GetAbilityId() const { return Data.AbilityID; }
	/** 0..1 charge progress. */
	float GetChargeAlpha() const;
	AMTCharacterBase* GetOwnerCharacter() const;
	UMTAbilityComponent* GetAbilityComponent() const { return Component.Get(); }

	/** Cast time after modifiers (chantless casting, awakening). */
	float GetEffectiveCastTime() const;
	float GetEffectiveManaCost() const;
	float GetEffectiveCooldown() const;

	/** Can the ability be started right now (mana, cooldown handled by component)? */
	virtual bool CanActivate(FText* OutReason = nullptr) const;

protected:
	/** Override: perform the effect. Called once when Anticipation ends. */
	virtual void ExecuteAction() {}
	/** Override: per-frame logic during Action. Call FinishAction() when done. */
	virtual void TickAction(float DeltaTime) {}
	/** Override: cleanup. */
	virtual void OnEnded(bool bWasCancelled) {}
	/** Whether Action ends immediately after ExecuteAction (instant abilities). */
	virtual bool IsInstantAction() const { return true; }

	void EnterPhase(EMTAbilityPhase NewPhase);
	void FinishAction();

	/** Aim helpers: lock-on target or camera aim point. */
	FVector GetAimPoint() const;
	FVector GetCastLocation() const;
	FRotator GetAimRotation() const;
	AActor* GetLockedTarget() const;

	/** Plays Data.Montage (authored montage or plain sequence; section only applies to montages). Returns play length or 0. */
	float PlayMontage(FName Section = NAME_None, float PlayRate = 1.f);
	/** Plays any animation for this ability through the owner and tracks it, so Release/Cancel can stop it. */
	float PlayAbilityAnim(const TSoftObjectPtr<UAnimSequenceBase>& Anim, float PlayRate = 1.f, FName Section = NAME_None,
		int32 LoopCount = 1, float BlendIn = 0.12f, float BlendOut = 0.18f);
	/** Jumps the authored Data.Montage to a section (no-op for plain sequences / dynamic montages). */
	void JumpMontageToSection(FName Section);
	/** Stops the animation this ability started last (anticipation, hold loop or release). */
	void StopMontage(float BlendOut = 0.2f);
	/** Charging: once the anticipation clip is about to end, start the ChargeLoopAnim hold loop. */
	void UpdateChargeAnimation();
	/** Charge released: ReleaseAnim, or the authored montage's release section. */
	void PlayReleaseAnimation();
	void SpawnFX(const TSoftObjectPtr<class UNiagaraSystem>& System, const FVector& Location, const FRotator& Rotation, bool bAttachToOwner = false);
	/** This ability's runtime effect for Phase ("Impact", "Travel", "Cast", ...), or null if its preset has none. */
	class AMTSpellVFX* SpawnPhaseFX(const TCHAR* PhaseName, const FTransform& Transform, float Scale = 1.f, USceneComponent* AttachTo = nullptr, FName Socket = NAME_None);
	void PlaySound(const TSoftObjectPtr<class USoundBase>& Sound, const FVector& Location);
	void PlaySubtleCameraShake(float Scale);
	/**
	 * Hits every hostile within Radius of Center with this row's damage, stagger and knockback (melee strikes and the
	 * arrival strike of an attack dash). A target caught mid-cast is interrupted, which triggers the owner's Dragon God
	 * Knowledge. Returns the number of targets hit.
	 */
	int32 StrikeHostilesInRadius(const FVector& Center, float Radius);

	/** Returns data after applying active awakening overrides / charge scaling. */
	float GetChargedValue(float Base, float FullChargeScale) const;

	UPROPERTY() TWeakObjectPtr<UMTAbilityComponent> Component;
	UPROPERTY() FMTAbilityData Data;

	EMTAbilityPhase Phase = EMTAbilityPhase::Idle;
	float PhaseTime = 0.f;
	float ChargeTime = 0.f;
	bool bInputHeld = false;
	bool bReleasedDuringAnticipation = false;
	float CommittedManaCost = 0.f;
	/** Charge alpha frozen at release. */
	float ReleasedChargeAlpha = 0.f;

	/** Montage instance of the animation this ability started last (authored or dynamic). */
	TWeakObjectPtr<UAnimMontage> ActiveAnimMontage;
	bool bChargeLoopStarted = false;
	/** Formation effect in the hand; a looping one (charge) is stopped when the spell fires or is cancelled. */
	TWeakObjectPtr<class AMTSpellVFX> FormationVFX;
};

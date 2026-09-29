// Ground/area spell: Quagmire, Flame Wave, Inferno, Flood, Tornado, Wind Burst, Earth Spikes, Frost Prison,
// Cumulonimbus, Tempest Domain, Saint Dragon Aura. Visuals are runtime presets riding on the zone, or a projected decal
// (never a coplanar plane: no z-fighting).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MTDataTypes.h"
#include "MTZoneActor.generated.h"

class UDecalComponent;
class UNiagaraComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class AMTCharacterBase;

UCLASS()
class MUSHOKURPG_API AMTZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AMTZoneActor();

	/** AreaMultiplier comes from the caster's stat modifiers (awakening makes Quagmire larger). */
	void InitZone(const FMTAbilityData& InData, AMTCharacterBase* InOwner, float AreaMultiplier, bool bAttachToOwner);

	virtual void Tick(float DeltaSeconds) override;

	EMTZoneKind GetKind() const { return Data.ZoneKind; }
	float GetRadius() const { return Radius; }
	bool IsActiveZone() const { return !bExpired; }
	AMTCharacterBase* GetOwnerCharacter() const { return OwnerCharacter.Get(); }
	const FMTAbilityData& GetAbilityData() const { return Data; }

	/**
	 * Disturb Magic: persistent magic loses its hold instead of vanishing. Remaining time x TimeScale, strength (damage,
	 * slow, pull) x StrengthScale; a tornado also shrinks by StrengthScale.
	 */
	void Destabilize(float TimeScale, float StrengthScale);

	/** True if Location is inside any active zone of Kind in the world. */
	static bool IsInsideZoneOfKind(const UObject* WorldContext, const FVector& Location, EMTZoneKind Kind, AMTZoneActor** OutZone = nullptr);

protected:
	void Pulse();
	void ApplyMire(const TArray<AMTCharacterBase*>& Targets);
	void ApplyFreeze(const TArray<AMTCharacterBase*>& Targets);
	void ApplyStorm();
	void ApplyWindField(const TArray<AMTCharacterBase*>& Targets, float DeltaSeconds);
	void ApplyAura(const TArray<AMTCharacterBase*>& Targets);
	void Erupt();
	void EruptLine();
	void Expire();
	void UpdateDecalLook(float DeltaSeconds);
	void MoveZone(float DeltaSeconds);
	/** Damage + element status to hostiles inside, at most once per target per Data.PulseInterval. */
	void ApplyDamageField(const TArray<AMTCharacterBase*>& Targets);
	void ApplyBurst();
	void ApplyWave(const TArray<AMTCharacterBase*>& Targets);
	void HitTarget(AMTCharacterBase* Target, const FVector& From, float DamageScale = 1.f);
	/** Element interactions, the hit itself, burning and the mastery notification for a prepared spec. */
	void HitTargetWith(AMTCharacterBase* Target, FMTDamageSpec Spec);
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Flame Wave (Arc): a curved wall of fire expanding from the caster.
	void ApplyArc(float DeltaSeconds);
	// Tornado (Vortex): growth, pull, lifting small enemies.
	void UpdateVortex(float DeltaSeconds);
	void StartLift(AMTCharacterBase* Target);
	void UpdateLifts(float DeltaSeconds);
	void ReleaseAllLifts();
	// Flood (Wave): the front hits once and carries enemies along; they are thrown when it breaks.
	void CarryWave(float DeltaSeconds);
	void ReleaseCarried();
	// Inferno (Eruptions): burning ground inside the circle.
	void ApplyAreaBurn();
	/** A short additive (pull) or override (carry) root-motion push on Target, re-applied when the last one ran out. */
	void PushTarget(AMTCharacterBase* Target, const FVector& Velocity, bool bOverride, TMap<TWeakObjectPtr<AMTCharacterBase>, float>& Until);
	float GetEffectiveDuration() const { return LifeDuration; }
	/** Remaining eruptions allowed (Disturb Magic shortens the volley). */
	int32 GetPulseLimit() const { return PulseLimit >= 0 ? PulseLimit : FMath::Max(1, Data.PulseCount); }

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDecalComponent> Decal;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> LoopFX;
	/** Sustained sound bed (the row's TravelSound): follows the zone and fades out as it expires. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> BedAudio;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DecalMID;

	FMTAbilityData Data;
	TWeakObjectPtr<AMTCharacterBase> OwnerCharacter;
	float Radius = 400.f;
	float InnerRadius = 0.f;
	float Age = 0.f;
	float PulseAccumulator = 0.f;
	int32 EruptionsDone = 0;
	float NextEruptionTime = 0.f;
	bool bExpired = false;
	bool bRootApplied = false;
	TSet<TWeakObjectPtr<AMTCharacterBase>> Rooted;

	/** Runtime zone effect (loop) riding on the zone. */
	TWeakObjectPtr<class AMTSpellVFX> ZoneVFX;
	/** Moving zones and line eruptions: travel direction (horizontal) and the line's start. */
	FVector MoveDirection = FVector::ForwardVector;
	FVector LineStart = FVector::ZeroVector;
	float TrailDistance = 0.f;
	bool bBurstDone = false;
	bool bVortexSpawned = false;
	TMap<TWeakObjectPtr<AMTCharacterBase>, float> LastHitTime;
	TSet<TWeakObjectPtr<AMTCharacterBase>> SweptOnce;

	/** Strength (damage, slow, pull), 1 unless Disturb Magic destabilised the zone. */
	float Strength = 1.f;
	/** Seconds the zone lives (Data.Duration, shortened by Destabilize). */
	float LifeDuration = 3.f;
	/** AreaMultiplier at cast (charge, awakening, stats) and the visual radius it gave. */
	float SizeScale = 1.f;
	float BaseRadius = 0.f;
	/** A destabilised tornado is smaller. */
	float VortexShrink = 1.f;
	/** Eruption volleys allowed (-1 = Data.PulseCount). */
	int32 PulseLimit = -1;
	/** Where the caster stood (ground), for line eruptions and arcs. */
	FVector CastOrigin = FVector::ZeroVector;
	/** Moving zones: distance rolled so far. */
	float Travelled = 0.f;
	float NextBurnTime = 0.f;

	// Flame Wave arc.
	float ArcRadius = 0.f;
	float NextScorchRadius = 0.f;
	bool bArcStopped = false;
	TArray<TWeakObjectPtr<class AMTSpellVFX>> ArcSegments;

	// Quagmire.
	TSet<TWeakObjectPtr<AMTCharacterBase>> MireEntered;
	TMap<TWeakObjectPtr<AMTCharacterBase>, float> LastRippleTime;

	// Tornado.
	struct FLiftState
	{
		float Start = 0.f;
		float Angle = 0.f;
		float OrbitRadius = 60.f;
		float BaseZ = 0.f;
		TWeakObjectPtr<class AMTSpellVFX> FX;
	};
	TMap<TWeakObjectPtr<AMTCharacterBase>, FLiftState> Lifted;
	TMap<TWeakObjectPtr<AMTCharacterBase>, float> LiftCooldownUntil;
	TMap<TWeakObjectPtr<AMTCharacterBase>, float> PullUntil;

	// Flood.
	TMap<TWeakObjectPtr<AMTCharacterBase>, float> CarryUntil;
	TSet<TWeakObjectPtr<AMTCharacterBase>> Carried;

	// Earth Spikes: where each spike rose, when, whether it was the final one, and whether it has crumbled.
	TArray<FVector> SpikeSpots;
	TArray<float> SpikeTimes;
	TArray<bool> SpikeIsFinal;
	TArray<bool> SpikeCrumbled;
};

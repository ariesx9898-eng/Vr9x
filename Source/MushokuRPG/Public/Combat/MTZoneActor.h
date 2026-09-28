// Ground/area spell (Quagmire, Flame Field, Frost Prison, Cumulonimbus, Tempest Domain,
// Saint Dragon Aura). Visuals use a projected decal, never a coplanar plane (no z-fighting).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MTDataTypes.h"
#include "MTZoneActor.generated.h"

class UDecalComponent;
class UNiagaraComponent;
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
	void ApplyVortex(const TArray<AMTCharacterBase*>& Targets, float DeltaSeconds);
	void ApplyWave(const TArray<AMTCharacterBase*>& Targets);
	void HitTarget(AMTCharacterBase* Target, const FVector& From, float DamageScale = 1.f);
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDecalComponent> Decal;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> LoopFX;
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
};

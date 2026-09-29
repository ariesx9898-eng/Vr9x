// Data-driven spell projectile: formation -> travel -> impact -> dissipation.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MTDataTypes.h"
#include "MTProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;
class UPointLightComponent;
class UAudioComponent;
class AMTCharacterBase;

UCLASS()
class MUSHOKURPG_API AMTProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMTProjectile();

	/** Configures the projectile. ChargeAlpha 0..1 scales speed/damage/stagger. */
	void InitProjectile(const FMTAbilityData& InData, AMTCharacterBase* InOwner, float InChargeAlpha, const FVector& Direction, float SpeedMultiplier, float DamageMultiplier);

	virtual void Tick(float DeltaSeconds) override;

	/** Disturb Magic: collapse the spell without effect. */
	void Disrupt(AActor* By);
	bool IsDisruptable() const { return Data.bDisruptable && Data.Element != EMTElement::None; }
	/** Tempest Domain: bend ordinary (uncharged, light) projectiles. */
	bool IsDeflectable() const { return ChargeAlpha < 0.35f && Data.Damage < 150.f; }
	void Deflect(const FVector& NewDirection);

	AActor* GetInstigatorActor() const;
	EMTElement GetElement() const { return Data.Element; }
	const FMTAbilityData& GetAbilityData() const { return Data; }
	FMTDamageSpec GetDamageSpec() const;
	FVector GetProjectileVelocity() const;
	/** Seconds until this projectile reaches Location along its path (large if it won't). */
	float EstimateTimeToReach(const FVector& Location, float Tolerance) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION() void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Hits Other when it is a live hostile character; ignores the caster and anything touched before InitProjectile. */
	void TryHitActor(AActor* Other, const FVector& Location);
	void HitCharacter(AMTCharacterBase* Target, const FVector& Location);
	void Explode(const FVector& Location, const FVector& Normal);
	void Dissipate(bool bSpawnFX);
	void ApplyPlaceholderLook();
	/** True when the spell may continue through Target: pierce budget left and Target light enough. */
	bool CanPierce(const AMTCharacterBase* Target) const;
	/** Fire rows with BurnSeconds leave the target burning. */
	void ApplyBurn(AMTCharacterBase* Target) const;
	/** Crescent spells (ProjectileWidth > 0): box sweep across the blade from the last position to this one. */
	void SweepCrescent();
	/** Visual impact radius at this charge (AOERadius x ChargeRadiusScale). */
	float GetImpactRadius() const;
	/** Visual size multiplier at this charge (ChargeSizeScale). */
	float GetSizeScale() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> TravelFX;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	/** A looping TravelSound riding with the projectile (fireball roar, cannon-slug whirr). */
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> TravelAudio;

	FMTAbilityData Data;
	TWeakObjectPtr<AMTCharacterBase> OwnerCharacter;
	float ChargeAlpha = 0.f;
	float DamageScale = 1.f;
	float StaggerScale = 1.f;
	float Lifetime = 2.f;
	float Age = 0.f;
	FVector WaveAxis = FVector::ZeroVector;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
	bool bFinished = false;
	/** False between spawn and InitProjectile: the sphere already overlaps the caster's hand there. */
	bool bInitialized = false;
	/** Characters this spell has passed through. */
	int32 Pierced = 0;
	/** Where the last crescent sweep ended. */
	FVector LastSweepLocation = FVector::ZeroVector;
	/** Runtime travel effect riding on the projectile (stopped and left to fade when it ends). */
	TWeakObjectPtr<class AMTSpellVFX> TravelVFX;
};

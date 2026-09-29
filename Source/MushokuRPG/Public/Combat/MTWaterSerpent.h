// Water Dragon: a serpent made of moving water. It builds out of the caster's spiralling streams, circles once behind
// and above them, hunts its target, and collapses into an enormous water explosion on contact. The head and the
// segmented body are drawn here (water material, one shared dynamic instance); spray, foam and mist ride on the head as
// the "<Preset>.Travel" runtime effect.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MTDataTypes.h"
#include "MTWaterSerpent.generated.h"

class AMTCharacterBase;
class AMTSpellVFX;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(NotBlueprintable)
class MUSHOKURPG_API AMTWaterSerpent : public AActor
{
	GENERATED_BODY()

public:
	AMTWaterSerpent();

	/**
	 * Row Params: Segments (16), Length (1700 cm), HeadRadius (visual, 110), CircleTime (0.5 s), HuntTime (2.4 s),
	 * Speed (2800 cm/s), TurnRate (160 deg/s). AOERadius is the explosion's visual radius.
	 */
	void InitSerpent(const FMTAbilityData& InData, AMTCharacterBase* InCaster, AActor* InPrey, const FVector& InAimPoint, float InDamageScale);

	virtual void Tick(float DeltaSeconds) override;

	/** Disturb Magic: the water loses its shape and falls. No explosion, no damage. */
	void Collapse();
	bool IsFlying() const { return !bFinished; }
	AMTCharacterBase* GetCaster() const { return Caster.Get(); }
	const FMTAbilityData& GetAbilityData() const { return Data; }
	FVector GetHeadLocation() const { return GetActorLocation(); }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Ends the flight: the explosion (damage, knockback, wet ground) or a harmless collapse. */
	void Finish(bool bExplode, const FVector& Where);
	/** Lays the body segments along the path the head has travelled. */
	void UpdateBody();
	void SetWaterOpacity(float Opacity);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Segments;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WaterMID;

	FMTAbilityData Data;
	TWeakObjectPtr<AMTCharacterBase> Caster;
	TWeakObjectPtr<AActor> Prey;
	FVector AimPoint = FVector::ZeroVector;
	/** Where the caster stood and faced when the dragon was released (the circle is around this). */
	FVector CastOrigin = FVector::ZeroVector;
	FVector CastForward = FVector::ForwardVector;
	FVector Heading = FVector::UpVector;
	/** Head positions, newest first, a few centimetres apart. */
	TArray<FVector> Path;
	float Age = 0.f;
	float FinishedAt = -1.f;
	float DamageScale = 1.f;
	float SegmentSpacing = 106.f;
	float HeadRadius = 110.f;
	/** The kit meshes run along +Y; this turns them to face +X (the travel direction). */
	FQuat MeshToForward = FQuat::Identity;
	FVector SegmentNativeExtent = FVector(50.f);
	bool bFinished = false;
	TWeakObjectPtr<AMTSpellVFX> TravelVFX;
};

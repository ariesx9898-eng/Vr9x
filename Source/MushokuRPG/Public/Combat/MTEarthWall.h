// Earth Wall segment: rises from the ground (after a short delay, so a wall erupts piece by piece), blocks projectiles,
// movement and some magic, visibly cracks as it is damaged and crumbles back into the ground.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MTDataTypes.h"
#include "MTEarthWall.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class MUSHOKURPG_API AMTEarthWall : public AActor
{
	GENERATED_BODY()

public:
	AMTEarthWall();

	/**
	 * RiseDelay: seconds before this segment erupts (hidden and without collision until then). Variant picks the mesh
	 * (<BodyMesh>, <BodyMesh>_B, <BodyMesh>_C when authored); HeightScale stretches it (natural, uneven tops).
	 */
	void InitWall(const FMTAbilityData& InData, AActor* InOwner, float Health, float Lifetime, float RiseDelay = 0.f, int32 Variant = 0, float HeightScale = 1.f);
	void TakeStructureDamage(float Amount);
	float GetHealthFraction() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsStanding() const { return !bCrumbling; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	void Crumble();
	/** Starts the eruption (collision on, rise effect and sound). */
	void BeginRise();
	/** 0 intact, 1 cracked (below 66%), 2 badly cracked (below 33%). */
	void UpdateCracks();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Box;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

	FMTAbilityData Data;
	float MaxHealth = 200.f;
	float CurrentHealth = 200.f;
	float RiseTime = 0.35f;
	float Age = 0.f;
	float Lifetime = 15.f;
	/** Places the authored mesh so its bounds fill the collision box (rotated if its long axis differs). */
	FVector MeshBaseOffset = FVector::ZeroVector;
	FVector Extent = FVector(40.f, 160.f, 140.f);
	bool bCrumbling = false;
	/** Seconds left before the segment erupts; it is buried until then. */
	float RiseDelay = 0.f;
	float RiseVolume = 1.f;
	bool bRisen = false;
	int32 CrackStage = 0;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> RockMID;
};

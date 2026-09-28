// Earth Fortress segment: rises from the ground, blocks projectiles, has durability.
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

	void InitWall(const FMTAbilityData& InData, AActor* InOwner, float Health, float Lifetime);
	void TakeStructureDamage(float Amount);
	float GetHealthFraction() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	void Crumble();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Box;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

	FMTAbilityData Data;
	float MaxHealth = 200.f;
	float CurrentHealth = 200.f;
	float RiseTime = 0.35f;
	float Age = 0.f;
	float Lifetime = 15.f;
	FVector Extent = FVector(40.f, 160.f, 140.f);
	bool bCrumbling = false;
};

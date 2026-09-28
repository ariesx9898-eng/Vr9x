// Demon Eye: Foresight. A readable prediction of an incoming attack: a translucent
// silhouette of the attacker moving to where it will strike, plus a danger marker.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MTTypes.h"
#include "MTForesightGhost.generated.h"

class UPoseableMeshComponent;
class UDecalComponent;
class UMaterialInstanceDynamic;

UCLASS()
class MUSHOKURPG_API AMTForesightGhost : public AActor
{
	GENERATED_BODY()

public:
	AMTForesightGhost();

	void InitFromTelegraph(const FMTAttackTelegraph& Telegraph, float LeadBonus);
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Silhouette;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDecalComponent> Marker;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MarkerMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GhostMID;

	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;
	float SpawnTime = 0.f;
	float ImpactTime = 0.f;
	bool bHasSilhouette = false;
};

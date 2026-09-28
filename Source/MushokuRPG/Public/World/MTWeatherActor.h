// Regional weather and colour grading around the camera (spawned and driven by UMTRegionSubsystem): snow, ash, embers,
// dust and sand haze, pollen motes, fireflies at night and ground mist, as instanced camera-facing sprites that wrap
// around the view (no Niagara), plus an unbound post-process for the region's saturation / contrast / tint.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/MTRegionSubsystem.h"
#include "MTWeatherActor.generated.h"

class UInstancedStaticMeshComponent;
class UPostProcessComponent;

UCLASS(NotPlaceable, Transient)
class MUSHOKURPG_API AMTWeatherActor : public AActor
{
	GENERATED_BODY()

public:
	AMTWeatherActor();

	/** Called every frame with the blended atmosphere at the player. */
	void ApplyAtmosphere(const FMTRegionAtmosphere& Atmosphere, float DeltaSeconds);

private:
	struct FParticle
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Size = 1.f;
		float Phase = 0.f;
	};

	struct FLayer
	{
		int32 Kind = 0;
		UInstancedStaticMeshComponent* ISM = nullptr;
		TArray<FParticle> Particles;
		TArray<FTransform> Transforms;
		float Weight = 0.f;
		bool bSeeded = false;
		bool bVisible = false;
	};

	void BuildLayers();
	void UpdateLayer(FLayer& Layer, float Weight, const FVector& Camera, float GroundZ, float Night, const FVector& Wind, float DeltaSeconds);

	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Components;
	TArray<FLayer> Layers;
	float Time = 0.f;
	FRandomStream Random;
};

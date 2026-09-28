// A road of the generated world (World.json Roads[]): its centre line as a spline, plus a PCG component that dresses
// the roadside (milestones, fences near settlements, rocks and bushes) with /Game/LaPlace/World/PCG/PCG_Roadside.
// Placed by Content/Python/mt_build_world.py (stage "pcg"); the PCG output is generated in the editor and saved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTRoadActor.generated.h"

class USplineComponent;
class UPCGComponent;

UCLASS()
class MUSHOKURPG_API AMTRoadActor : public AActor
{
	GENERATED_BODY()

public:
	AMTRoadActor();

	/** Rebuilds the spline through WorldPoints (cm); WidthCm is the paved width (the dressing keeps clear of it). */
	UFUNCTION(BlueprintCallable, Category = "Road")
	void SetRoadPoints(const TArray<FVector>& WorldPoints, float InWidthCm);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road")
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road")
	TObjectPtr<UPCGComponent> PCG;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	float WidthCm = 600.f;
};

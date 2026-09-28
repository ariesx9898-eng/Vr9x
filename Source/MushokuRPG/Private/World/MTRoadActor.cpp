#include "World/MTRoadActor.h"

#include "Components/SplineComponent.h"
#include "PCGComponent.h"

AMTRoadActor::AMTRoadActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetMobility(EComponentMobility::Static);
	RootComponent = Spline;
	PCG = CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
	// Generated in the editor and saved with the actor (no generation cost at runtime).
	PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
}

void AMTRoadActor::SetRoadPoints(const TArray<FVector>& WorldPoints, float InWidthCm)
{
	WidthCm = InWidthCm;
	Spline->ClearSplinePoints(false);
	for (const FVector& Point : WorldPoints)
	{
		Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
	}
	for (int32 i = 0; i < WorldPoints.Num(); ++i)
	{
		Spline->SetSplinePointType(i, ESplinePointType::Curve, false);
	}
	Spline->UpdateSpline();
}

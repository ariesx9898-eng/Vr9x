#include "World/MTPlacementValidator.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "LandscapeProxy.h"

namespace MTPlacement
{
	/** Touching within this tolerance (cm) is not an overlap - keeps results stable for exactly adjacent tiles. */
	static constexpr double OverlapEpsilon = 1e-3;
	static constexpr double HugeDistance = 1e12;

	static FVector2D Rotate(const FVector2D& V, double YawDeg)
	{
		const double R = FMath::DegreesToRadians(YawDeg);
		const double C = FMath::Cos(R);
		const double S = FMath::Sin(R);
		return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C);
	}

	/** World point -> footprint local frame. */
	static FVector2D ToLocal(const FMTFootprint& F, const FVector2D& P)
	{
		return Rotate(P - F.Center, -F.YawDegrees);
	}

	static double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }

	/** Distance from P to an axis-aligned box centred at the origin. */
	static double DistancePointToAABB(const FVector2D& P, const FVector2D& H)
	{
		const double DX = FMath::Max(FMath::Abs(P.X) - H.X, 0.0);
		const double DY = FMath::Max(FMath::Abs(P.Y) - H.Y, 0.0);
		return FMath::Sqrt(DX * DX + DY * DY);
	}

	/** SAT: segment vs AABB centred at origin (both in the box's local frame). */
	static bool SegmentIntersectsAABB(const FVector2D& A, const FVector2D& B, const FVector2D& H)
	{
		if (FMath::Max(A.X, B.X) < -H.X || FMath::Min(A.X, B.X) > H.X) { return false; }
		if (FMath::Max(A.Y, B.Y) < -H.Y || FMath::Min(A.Y, B.Y) > H.Y) { return false; }
		const FVector2D D = B - A;
		const double Len = D.Size();
		if (Len < UE_KINDA_SMALL_NUMBER) { return true; } // degenerate: point already inside by the two tests
		const FVector2D N(-D.Y / Len, D.X / Len);
		const double SegProj = N.X * A.X + N.Y * A.Y;
		const double BoxR = FMath::Abs(N.X) * H.X + FMath::Abs(N.Y) * H.Y;
		return FMath::Abs(SegProj) <= BoxR;
	}

	static bool IsLandscapeActor(const AActor* Actor)
	{
		return Actor && Actor->IsA<ALandscapeProxy>();
	}

	static void BuildQueryParams(const UWorld* World, const TArray<const AActor*>& IgnoreActors, bool bIgnoreLandscape,
		FCollisionQueryParams& OutParams)
	{
		OutParams.bTraceComplex = false;
		OutParams.AddIgnoredActors(IgnoreActors);
		if (bIgnoreLandscape && World)
		{
			for (TActorIterator<ALandscapeProxy> It(const_cast<UWorld*>(World)); It; ++It)
			{
				OutParams.AddIgnoredActor(*It);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
FVector2D FMTFootprint::AxisX() const { return MTPlacement::Rotate(FVector2D(1.0, 0.0), YawDegrees); }
FVector2D FMTFootprint::AxisY() const { return MTPlacement::Rotate(FVector2D(0.0, 1.0), YawDegrees); }

FMTExclusionZone FMTExclusionZone::MakeCircle(FName InReason, const FVector2D& InCenter, float InRadius)
{
	FMTExclusionZone Z;
	Z.Reason = InReason;
	Z.Shape = EMTZoneShape::Circle;
	Z.Center = InCenter;
	Z.Radius = InRadius;
	return Z;
}

FMTExclusionZone FMTExclusionZone::MakeBox(FName InReason, const FMTFootprint& Box)
{
	FMTExclusionZone Z;
	Z.Reason = InReason;
	Z.Shape = EMTZoneShape::Box;
	Z.Center = Box.Center;
	Z.HalfExtent = Box.PaddedHalfExtent();
	Z.YawDegrees = Box.YawDegrees;
	return Z;
}

void FMTPlacementRegistry::Reset()
{
	Footprints.Reset();
	FootprintCategories.Reset();
	Circles.Reset();
	CircleCategories.Reset();
	Exclusions.Reset();
	Roads.Reset();
}

void FMTPlacementRegistry::AddFootprint(const FMTFootprint& Footprint, FName Category)
{
	Footprints.Add(Footprint);
	FootprintCategories.Add(Category);
}

void FMTPlacementRegistry::AddCircle(const FMTCircleFootprint& Circle, FName Category)
{
	Circles.Add(Circle);
	CircleCategories.Add(Category);
}

void FMTPlacementRegistry::GetCentersOfCategory(FName Category, TArray<FVector2D>& OutCenters) const
{
	for (int32 i = 0; i < Footprints.Num(); ++i)
	{
		if (FootprintCategories.IsValidIndex(i) && FootprintCategories[i] == Category) { OutCenters.Add(Footprints[i].Center); }
	}
	for (int32 i = 0; i < Circles.Num(); ++i)
	{
		if (CircleCategories.IsValidIndex(i) && CircleCategories[i] == Category) { OutCenters.Add(Circles[i].Center); }
	}
}

// ---------------------------------------------------------------------------------------------------------------------
TArray<FVector2D> UMTPlacementValidator::GetFootprintCorners(const FMTFootprint& F, bool bIncludePadding)
{
	const FVector2D H = bIncludePadding ? F.PaddedHalfExtent() : F.HalfExtent;
	const FVector2D AX = F.AxisX() * H.X;
	const FVector2D AY = F.AxisY() * H.Y;
	return { F.Center + AX + AY, F.Center - AX + AY, F.Center - AX - AY, F.Center + AX - AY };
}

bool UMTPlacementValidator::FootprintsOverlap(const FMTFootprint& A, const FMTFootprint& B)
{
	const FVector2D Axes[4] = { A.AxisX(), A.AxisY(), B.AxisX(), B.AxisY() };
	const FVector2D HA = A.PaddedHalfExtent();
	const FVector2D HB = B.PaddedHalfExtent();
	const FVector2D AX = A.AxisX(), AY = A.AxisY(), BX = B.AxisX(), BY = B.AxisY();
	const FVector2D D = B.Center - A.Center;
	for (const FVector2D& N : Axes)
	{
		const double RA = FMath::Abs(FVector2D::DotProduct(AX, N)) * HA.X + FMath::Abs(FVector2D::DotProduct(AY, N)) * HA.Y;
		const double RB = FMath::Abs(FVector2D::DotProduct(BX, N)) * HB.X + FMath::Abs(FVector2D::DotProduct(BY, N)) * HB.Y;
		const double Dist = FMath::Abs(FVector2D::DotProduct(D, N));
		if (Dist >= RA + RB - MTPlacement::OverlapEpsilon)
		{
			return false; // separating axis found
		}
	}
	return true;
}

bool UMTPlacementValidator::CircleOverlapsFootprint(const FVector2D& Center, float Radius, const FMTFootprint& Footprint)
{
	const FVector2D Local = MTPlacement::ToLocal(Footprint, Center);
	const double Dist = MTPlacement::DistancePointToAABB(Local, Footprint.PaddedHalfExtent());
	return Dist < double(Radius) - MTPlacement::OverlapEpsilon;
}

bool UMTPlacementValidator::CirclesOverlap(const FVector2D& CenterA, float RadiusA, const FVector2D& CenterB, float RadiusB)
{
	return FVector2D::Distance(CenterA, CenterB) < double(RadiusA) + double(RadiusB) - MTPlacement::OverlapEpsilon;
}

bool UMTPlacementValidator::FootprintOverlapsZone(const FMTFootprint& Footprint, const FMTExclusionZone& Zone)
{
	if (Zone.Shape == EMTZoneShape::Circle)
	{
		return CircleOverlapsFootprint(Zone.Center, Zone.Radius, Footprint);
	}
	return FootprintsOverlap(Footprint, Zone.AsFootprint());
}

bool UMTPlacementValidator::CircleOverlapsZone(const FVector2D& Center, float Radius, const FMTExclusionZone& Zone)
{
	if (Zone.Shape == EMTZoneShape::Circle)
	{
		return CirclesOverlap(Center, Radius, Zone.Center, Zone.Radius);
	}
	return CircleOverlapsFootprint(Center, Radius, Zone.AsFootprint());
}

float UMTPlacementValidator::DistancePointToSegment2D(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
	const FVector2D AB = B - A;
	const double L2 = AB.SizeSquared();
	if (L2 < UE_SMALL_NUMBER)
	{
		return float(FVector2D::Distance(P, A));
	}
	const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.0, 1.0);
	return float(FVector2D::Distance(P, A + AB * T));
}

float UMTPlacementValidator::DistancePointToPolyline2D(const FVector2D& P, const TArray<FVector2D>& Polyline)
{
	if (Polyline.Num() < 2)
	{
		return Polyline.Num() == 1 ? float(FVector2D::Distance(P, Polyline[0])) : float(MTPlacement::HugeDistance);
	}
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i + 1 < Polyline.Num(); ++i)
	{
		Best = FMath::Min(Best, DistancePointToSegment2D(P, Polyline[i], Polyline[i + 1]));
	}
	return Best;
}

float UMTPlacementValidator::DistanceFootprintToPolyline2D(const FMTFootprint& Footprint, const TArray<FVector2D>& Polyline)
{
	if (Polyline.Num() < 2)
	{
		return Polyline.Num() == 1
			? float(MTPlacement::DistancePointToAABB(MTPlacement::ToLocal(Footprint, Polyline[0]), Footprint.PaddedHalfExtent()))
			: float(MTPlacement::HugeDistance);
	}
	const FVector2D H = Footprint.PaddedHalfExtent();
	const TArray<FVector2D> Corners = GetFootprintCorners(Footprint, true);
	double Best = MTPlacement::HugeDistance;
	for (int32 i = 0; i + 1 < Polyline.Num(); ++i)
	{
		const FVector2D LA = MTPlacement::ToLocal(Footprint, Polyline[i]);
		const FVector2D LB = MTPlacement::ToLocal(Footprint, Polyline[i + 1]);
		if (MTPlacement::SegmentIntersectsAABB(LA, LB, H))
		{
			return 0.f;
		}
		// Disjoint convex shapes in 2D: the minimum distance is realised at a vertex of one of them.
		Best = FMath::Min(Best, MTPlacement::DistancePointToAABB(LA, H));
		Best = FMath::Min(Best, MTPlacement::DistancePointToAABB(LB, H));
		for (const FVector2D& C : Corners)
		{
			Best = FMath::Min(Best, double(DistancePointToSegment2D(C, Polyline[i], Polyline[i + 1])));
		}
	}
	return float(Best);
}

bool UMTPlacementValidator::MeetsMinSpacing(const FVector2D& Candidate, const TArray<FVector2D>& Existing, float MinSpacing)
{
	const double MinSq = double(MinSpacing) * double(MinSpacing);
	for (const FVector2D& E : Existing)
	{
		if (FVector2D::DistSquared(Candidate, E) < MinSq)
		{
			return false;
		}
	}
	return true;
}

EMTPlacementRejection UMTPlacementValidator::CheckFootprint(const FMTPlacementRegistry& Registry, const FMTFootprint& Footprint,
	FName Category, float MinSameKindSpacing, float RoadClearance)
{
	for (const FMTFootprint& Other : Registry.Footprints)
	{
		if (FootprintsOverlap(Footprint, Other)) { return EMTPlacementRejection::OverlapsFootprint; }
	}
	for (const FMTCircleFootprint& Circle : Registry.Circles)
	{
		if (CircleOverlapsFootprint(Circle.Center, Circle.Radius, Footprint)) { return EMTPlacementRejection::OverlapsFootprint; }
	}
	for (const FMTExclusionZone& Zone : Registry.Exclusions)
	{
		if (FootprintOverlapsZone(Footprint, Zone)) { return EMTPlacementRejection::InsideExclusionZone; }
	}
	if (MinSameKindSpacing > 0.f)
	{
		TArray<FVector2D> Same;
		Registry.GetCentersOfCategory(Category, Same);
		if (!MeetsMinSpacing(Footprint.Center, Same, MinSameKindSpacing)) { return EMTPlacementRejection::TooCloseToSameKind; }
	}
	for (const FMTPolyline2D& Road : Registry.Roads)
	{
		if (DistanceFootprintToPolyline2D(Footprint, Road.Points) < Road.HalfWidth + RoadClearance)
		{
			return EMTPlacementRejection::TooCloseToRoad;
		}
	}
	return EMTPlacementRejection::None;
}

EMTPlacementRejection UMTPlacementValidator::CheckCircle(const FMTPlacementRegistry& Registry, const FVector2D& Center, float Radius,
	FName Category, float MinSameKindSpacing, float RoadClearance)
{
	for (const FMTFootprint& Other : Registry.Footprints)
	{
		if (CircleOverlapsFootprint(Center, Radius, Other)) { return EMTPlacementRejection::OverlapsFootprint; }
	}
	for (const FMTCircleFootprint& Circle : Registry.Circles)
	{
		if (CirclesOverlap(Center, Radius, Circle.Center, Circle.Radius)) { return EMTPlacementRejection::OverlapsFootprint; }
	}
	for (const FMTExclusionZone& Zone : Registry.Exclusions)
	{
		if (CircleOverlapsZone(Center, Radius, Zone)) { return EMTPlacementRejection::InsideExclusionZone; }
	}
	if (MinSameKindSpacing > 0.f)
	{
		TArray<FVector2D> Same;
		Registry.GetCentersOfCategory(Category, Same);
		if (!MeetsMinSpacing(Center, Same, MinSameKindSpacing)) { return EMTPlacementRejection::TooCloseToSameKind; }
	}
	for (const FMTPolyline2D& Road : Registry.Roads)
	{
		if (DistancePointToPolyline2D(Center, Road.Points) < Road.HalfWidth + RoadClearance + Radius)
		{
			return EMTPlacementRejection::TooCloseToRoad;
		}
	}
	return EMTPlacementRejection::None;
}

FString UMTPlacementValidator::RejectionToString(EMTPlacementRejection Reason)
{
	switch (Reason)
	{
	case EMTPlacementRejection::None: return TEXT("None");
	case EMTPlacementRejection::OutOfBounds: return TEXT("OutOfBounds");
	case EMTPlacementRejection::OverlapsFootprint: return TEXT("OverlapsFootprint");
	case EMTPlacementRejection::InsideExclusionZone: return TEXT("InsideExclusionZone");
	case EMTPlacementRejection::TooCloseToSameKind: return TEXT("TooCloseToSameKind");
	case EMTPlacementRejection::TooCloseToRoad: return TEXT("TooCloseToRoad");
	case EMTPlacementRejection::TooSteep: return TEXT("TooSteep");
	case EMTPlacementRejection::NoGround: return TEXT("NoGround");
	case EMTPlacementRejection::BlockedByWorldGeometry: return TEXT("BlockedByWorldGeometry");
	default: return TEXT("Unknown");
	}
}

// ---------------------------------------------------------------------------------------------------------------------
bool UMTPlacementValidator::TraceGroundZ(const UWorld* World, const FVector2D& XY, float& OutZ,
	const TArray<const AActor*>& IgnoreActors, float TraceTopZ, float TraceBottomZ)
{
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTTraceGround), false);
	Params.AddIgnoredActors(IgnoreActors);
	const FVector Start(XY.X, XY.Y, TraceTopZ);
	const FVector End(XY.X, XY.Y, TraceBottomZ);

	bool bHaveFallback = false;
	float FallbackZ = 0.f;
	// Skip up to 8 non-landscape blockers (roofs, trees) so generated content never becomes "ground".
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			break;
		}
		AActor* HitActor = Hit.GetActor();
		if (MTPlacement::IsLandscapeActor(HitActor))
		{
			OutZ = float(Hit.ImpactPoint.Z);
			return true;
		}
		if (!bHaveFallback)
		{
			bHaveFallback = true;
			FallbackZ = float(Hit.ImpactPoint.Z);
		}
		if (!HitActor)
		{
			break;
		}
		Params.AddIgnoredActor(HitActor);
	}
	// No landscape under this point (e.g. a test map built from static meshes): use the first blocking hit.
	const bool bWorldHasLandscape = static_cast<bool>(TActorIterator<ALandscapeProxy>(const_cast<UWorld*>(World)));
	if (!bWorldHasLandscape && bHaveFallback)
	{
		OutZ = FallbackZ;
		return true;
	}
	return false;
}

FMTGroundSample UMTPlacementValidator::SampleGround(const UObject* WorldContextObject, const FMTFootprint& Footprint)
{
	const UWorld* World = (WorldContextObject && GEngine)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return SampleGroundIgnoring(World, Footprint, TArray<const AActor*>());
}

FMTGroundSample UMTPlacementValidator::SampleGroundIgnoring(const UWorld* World, const FMTFootprint& Footprint,
	const TArray<const AActor*>& IgnoreActors)
{
	FMTGroundSample Out;
	if (!World)
	{
		return Out;
	}
	const TArray<FVector2D> Corners = GetFootprintCorners(Footprint, false);
	float Z[4];
	for (int32 i = 0; i < 4; ++i)
	{
		if (!TraceGroundZ(World, Corners[i], Z[i], IgnoreActors))
		{
			return Out; // any corner without ground (hole, off the landscape) invalidates the sample
		}
	}
	float CenterZ = 0.f;
	if (!TraceGroundZ(World, Footprint.Center, CenterZ, IgnoreActors))
	{
		return Out;
	}
	Out.bValid = true;
	Out.MinZ = FMath::Min(FMath::Min(Z[0], Z[1]), FMath::Min(Z[2], Z[3]));
	Out.MaxZ = FMath::Max(FMath::Max(Z[0], Z[1]), FMath::Max(Z[2], Z[3]));
	Out.MinZ = FMath::Min(Out.MinZ, CenterZ);
	Out.MaxZ = FMath::Max(Out.MaxZ, CenterZ);
	Out.AverageZ = (Z[0] + Z[1] + Z[2] + Z[3]) * 0.25f;
	Out.CenterZ = CenterZ;
	Out.HeightDelta = Out.MaxZ - Out.MinZ;
	// Corners: 0 front-right, 1 front-left, 2 back-left, 3 back-right.
	const float LenX = FMath::Max(2.f * float(Footprint.HalfExtent.X), 1.f);
	const float LenY = FMath::Max(2.f * float(Footprint.HalfExtent.Y), 1.f);
	const float SlopeX = ((Z[0] + Z[3]) - (Z[1] + Z[2])) * 0.5f / LenX;
	const float SlopeY = ((Z[0] + Z[1]) - (Z[2] + Z[3])) * 0.5f / LenY;
	Out.SlopeDegrees = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(SlopeX * SlopeX + SlopeY * SlopeY)));
	return Out;
}

bool UMTPlacementValidator::PassesSlopeRule(const FMTGroundSample& Sample, const FMTSlopeRule& Rule)
{
	return Sample.bValid && Sample.HeightDelta <= Rule.MaxHeightDelta && Sample.SlopeDegrees <= Rule.MaxSlopeDegrees;
}

FMTFoundation UMTPlacementValidator::ComputeFoundation(const FMTGroundSample& Sample, float FloorClearance, float BuryDepth)
{
	FMTFoundation F;
	// Snap to the average corner height, then lift by the foundation offset so the highest corner is under the floor
	// (never sinks) and bury the plinth below the lowest corner (never floats).
	F.FoundationOffset = FMath::Max(Sample.MaxZ - Sample.AverageZ, 0.f) + FMath::Max(FloorClearance, 0.f);
	F.FloorZ = Sample.AverageZ + F.FoundationOffset;
	F.PlinthBottomZ = Sample.MinZ - FMath::Max(BuryDepth, 0.f);
	return F;
}

bool UMTPlacementValidator::IsBlockedByWorldGeometry(const UWorld* World, const FMTFootprint& Footprint, float BaseZ, float Height,
	const TArray<const AActor*>& IgnoreActors, FString* OutBlockerName, float BottomClearance)
{
	if (!World)
	{
		return false;
	}
	const float BoxHeight = FMath::Max(Height - BottomClearance, 10.f);
	const FVector HalfExtent(Footprint.HalfExtent.X, Footprint.HalfExtent.Y, BoxHeight * 0.5f);
	const FVector Center(Footprint.Center.X, Footprint.Center.Y, BaseZ + BottomClearance + BoxHeight * 0.5f);
	const FQuat Rotation(FRotator(0.f, Footprint.YawDegrees, 0.f));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTPlacementOverlap), false);
	MTPlacement::BuildQueryParams(World, IgnoreActors, /*bIgnoreLandscape*/ true, Params);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Center, Rotation, ECC_WorldStatic, FCollisionShape::MakeBox(HalfExtent), Params);
	for (const FOverlapResult& Result : Overlaps)
	{
		const AActor* Other = Result.GetActor();
		if (!Result.bBlockingHit || MTPlacement::IsLandscapeActor(Other))
		{
			continue;
		}
		if (OutBlockerName)
		{
			*OutBlockerName = Other ? Other->GetName() : TEXT("<component>");
		}
		return true;
	}
	return false;
}

bool UMTPlacementValidator::IsCapsuleBlocked(const UWorld* World, const FVector& GroundLocation, float Radius, float HalfHeight,
	const TArray<const AActor*>& IgnoreActors, FString* OutBlockerName)
{
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTCapsuleOverlap), false);
	MTPlacement::BuildQueryParams(World, IgnoreActors, /*bIgnoreLandscape*/ true, Params);
	const FVector Center = GroundLocation + FVector(0.f, 0.f, HalfHeight + 5.f);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
	for (const FOverlapResult& Result : Overlaps)
	{
		const AActor* Other = Result.GetActor();
		if (!Result.bBlockingHit || MTPlacement::IsLandscapeActor(Other) || (Other && Other->IsA<APawn>()))
		{
			continue;
		}
		if (OutBlockerName)
		{
			*OutBlockerName = Other ? Other->GetName() : TEXT("<component>");
		}
		return true;
	}
	// A point under the landscape surface is also "inside geometry".
	float GroundZ = 0.f;
	if (TraceGroundZ(World, FVector2D(GroundLocation.X, GroundLocation.Y), GroundZ, IgnoreActors) && GroundLocation.Z < GroundZ - 50.f)
	{
		if (OutBlockerName) { *OutBlockerName = TEXT("Landscape (point below surface)"); }
		return true;
	}
	return false;
}

bool UMTPlacementValidator::IsFootprintBlockedByWorld(const UObject* WorldContextObject, const FMTFootprint& Footprint, float BaseZ, float Height)
{
	const UWorld* World = (WorldContextObject && GEngine)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return IsBlockedByWorldGeometry(World, Footprint, BaseZ, Height, TArray<const AActor*>());
}

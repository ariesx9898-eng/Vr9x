// Deterministic placement rules shared by the village generator, tools and validation.
// Pure 2D maths (SAT, spacing, polyline distance) is mirrored in Tools/placement_validator_test.py.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MTPlacementValidator.generated.h"

class AActor;
class UWorld;

/** Oriented rectangle on the XY plane (cm). Local +X is the "front" (doors face +X). */
USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTFootprint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FVector2D Center = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FVector2D HalfExtent = FVector2D(100.0, 100.0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") float YawDegrees = 0.f;
	/** Extra clearance added around the rectangle for overlap tests (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") float Padding = 0.f;

	FMTFootprint() = default;
	FMTFootprint(const FVector2D& InCenter, const FVector2D& InHalfExtent, float InYaw, float InPadding = 0.f)
		: Center(InCenter), HalfExtent(InHalfExtent), YawDegrees(InYaw), Padding(InPadding) {}

	FVector2D AxisX() const;
	FVector2D AxisY() const;
	FVector2D PaddedHalfExtent() const { return FVector2D(HalfExtent.X + Padding, HalfExtent.Y + Padding); }
};

USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTCircleFootprint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FVector2D Center = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") float Radius = 100.f;
};

UENUM(BlueprintType)
enum class EMTZoneShape : uint8
{
	Circle,
	Box
};

/** Area where a category of objects must not be placed (roads use FMTPolyline2D instead). */
USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTExclusionZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FName Reason = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") EMTZoneShape Shape = EMTZoneShape::Circle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FVector2D Center = FVector2D::ZeroVector;
	/** Circle radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") float Radius = 100.f;
	/** Box half extent (cm) and yaw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FVector2D HalfExtent = FVector2D(100.0, 100.0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") float YawDegrees = 0.f;

	static FMTExclusionZone MakeCircle(FName InReason, const FVector2D& InCenter, float InRadius);
	static FMTExclusionZone MakeBox(FName InReason, const FMTFootprint& Box);
	FMTFootprint AsFootprint() const { return FMTFootprint(Center, HalfExtent, YawDegrees, 0.f); }
};

/** Road / river centre line with a half width (cm). */
USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTPolyline2D
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FName Name = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") TArray<FVector2D> Points;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") float HalfWidth = 250.f;
	/** Rivers block everything; roads may be crossed by nothing but other roads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") bool bIsRiver = false;
};

UENUM(BlueprintType)
enum class EMTPlacementRejection : uint8
{
	None,
	OutOfBounds,
	OverlapsFootprint,
	InsideExclusionZone,
	TooCloseToSameKind,
	TooCloseToRoad,
	TooSteep,
	NoGround,
	BlockedByWorldGeometry
};

USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTGroundSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Placement") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") float MinZ = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") float MaxZ = 0.f;
	/** Average of the corner heights (the snap height). */
	UPROPERTY(BlueprintReadOnly, Category = "Placement") float AverageZ = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") float CenterZ = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") float HeightDelta = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") float SlopeDegrees = 0.f;
};

USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTSlopeRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") float MaxHeightDelta = 80.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") float MaxSlopeDegrees = 10.f;
};

/** Result of snapping a building to the ground: floor never sinks, foundation never floats. */
USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTFoundation
{
	GENERATED_BODY()

	/** Floor height of the building (>= highest corner + clearance). */
	UPROPERTY(BlueprintReadOnly, Category = "Placement") float FloorZ = 0.f;
	/** Bottom of the plinth, buried below the lowest corner. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement") float PlinthBottomZ = 0.f;
	/** Foundation offset applied above the average ground height. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement") float FoundationOffset = 0.f;
};

/** Everything placed so far; the generator checks every candidate against it. */
USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTPlacementRegistry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Placement") TArray<FMTFootprint> Footprints;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") TArray<FName> FootprintCategories;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") TArray<FMTCircleFootprint> Circles;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") TArray<FName> CircleCategories;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") TArray<FMTExclusionZone> Exclusions;
	UPROPERTY(BlueprintReadOnly, Category = "Placement") TArray<FMTPolyline2D> Roads;

	void Reset();
	void AddFootprint(const FMTFootprint& Footprint, FName Category);
	void AddCircle(const FMTCircleFootprint& Circle, FName Category);
	void AddExclusion(const FMTExclusionZone& Zone) { Exclusions.Add(Zone); }
	void AddRoad(const FMTPolyline2D& Road) { Roads.Add(Road); }
	/** Centres of every footprint + circle registered with Category. */
	void GetCentersOfCategory(FName Category, TArray<FVector2D>& OutCenters) const;
};

UCLASS()
class MUSHOKURPG_API UMTPlacementValidator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---------------- pure, deterministic 2D maths ----------------
	/** Corners in CCW order (front-right, front-left, back-left, back-right). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static TArray<FVector2D> GetFootprintCorners(const FMTFootprint& Footprint, bool bIncludePadding);

	/** Separating Axis Theorem on the two padded oriented rectangles. Touching edges do not overlap. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static bool FootprintsOverlap(const FMTFootprint& A, const FMTFootprint& B);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static bool CircleOverlapsFootprint(const FVector2D& Center, float Radius, const FMTFootprint& Footprint);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static bool CirclesOverlap(const FVector2D& CenterA, float RadiusA, const FVector2D& CenterB, float RadiusB);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static bool FootprintOverlapsZone(const FMTFootprint& Footprint, const FMTExclusionZone& Zone);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static bool CircleOverlapsZone(const FVector2D& Center, float Radius, const FMTExclusionZone& Zone);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static float DistancePointToSegment2D(const FVector2D& P, const FVector2D& A, const FVector2D& B);

	/** Returns a huge value for polylines with fewer than 2 points. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static float DistancePointToPolyline2D(const FVector2D& P, const TArray<FVector2D>& Polyline);

	/** Exact distance between the (padded) footprint and the polyline; 0 when they intersect. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static float DistanceFootprintToPolyline2D(const FMTFootprint& Footprint, const TArray<FVector2D>& Polyline);

	/** True when Candidate is at least MinSpacing away from every point in Existing. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static bool MeetsMinSpacing(const FVector2D& Candidate, const TArray<FVector2D>& Existing, float MinSpacing);

	/** Registry test for a footprint: overlaps, exclusion zones, same-kind spacing and road clearance. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static EMTPlacementRejection CheckFootprint(const FMTPlacementRegistry& Registry, const FMTFootprint& Footprint,
		FName Category, float MinSameKindSpacing, float RoadClearance);

	/** Registry test for a circle (props, trees, spawn points). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static EMTPlacementRejection CheckCircle(const FMTPlacementRegistry& Registry, const FVector2D& Center, float Radius,
		FName Category, float MinSameKindSpacing, float RoadClearance);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static FString RejectionToString(EMTPlacementRejection Reason);

	// ---------------- world queries ----------------
	/**
	 * Ground height at XY: traces down and skips every hit that is not a landscape (so generated meshes never
	 * become "ground"). Falls back to the first blocking hit when the world has no landscape.
	 */
	static bool TraceGroundZ(const UWorld* World, const FVector2D& XY, float& OutZ, const TArray<const AActor*>& IgnoreActors,
		float TraceTopZ = 200000.f, float TraceBottomZ = -200000.f);

	/** Samples the 4 corners + centre of the footprint (padding ignored). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Placement", meta = (WorldContext = "WorldContextObject"))
	static FMTGroundSample SampleGround(const UObject* WorldContextObject, const FMTFootprint& Footprint);

	static FMTGroundSample SampleGroundIgnoring(const UWorld* World, const FMTFootprint& Footprint, const TArray<const AActor*>& IgnoreActors);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static bool PassesSlopeRule(const FMTGroundSample& Sample, const FMTSlopeRule& Rule);

	/** Snap to the average corner height, then lift by a foundation offset so no corner is above the floor. */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Placement")
	static FMTFoundation ComputeFoundation(const FMTGroundSample& Sample, float FloorClearance = 5.f, float BuryDepth = 20.f);

	/**
	 * Box overlap against existing WorldStatic geometry (landscape proxies are ignored). BaseZ is the bottom of the
	 * box; the box starts BottomClearance above it so resting on the ground is not a hit.
	 */
	static bool IsBlockedByWorldGeometry(const UWorld* World, const FMTFootprint& Footprint, float BaseZ, float Height,
		const TArray<const AActor*>& IgnoreActors, FString* OutBlockerName = nullptr, float BottomClearance = 15.f);

	/** Pawn-sized capsule test for NPC spawn / quest points standing on GroundZ. */
	static bool IsCapsuleBlocked(const UWorld* World, const FVector& GroundLocation, float Radius, float HalfHeight,
		const TArray<const AActor*>& IgnoreActors, FString* OutBlockerName = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Placement", meta = (WorldContext = "WorldContextObject"))
	static bool IsFootprintBlockedByWorld(const UObject* WorldContextObject, const FMTFootprint& Footprint, float BaseZ, float Height);
};

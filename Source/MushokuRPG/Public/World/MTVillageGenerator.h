// Rule-based, seed-deterministic generator for Buena Village (editor tool; also callable at runtime).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/MTPlacementValidator.h"
#include "MTVillageGenerator.generated.h"

class UStaticMesh;
class USplineComponent;
class UHierarchicalInstancedStaticMeshComponent;
class AStaticMeshActor;

USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTGenerationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Generation") int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Generation") TMap<FName, int32> Placed;
	UPROPERTY(BlueprintReadOnly, Category = "Generation") TMap<FName, int32> Rejected;
	/** "Category:Reason" -> count. */
	UPROPERTY(BlueprintReadOnly, Category = "Generation") TMap<FString, int32> RejectionReasons;
	/** Slots that fell back to engine basic shapes (BLOCKOUT placeholders). */
	UPROPERTY(BlueprintReadOnly, Category = "Generation") TArray<FString> BlockoutSlots;
	UPROPERTY(BlueprintReadOnly, Category = "Generation") bool bRoadsFromJson = false;
	UPROPERTY(BlueprintReadOnly, Category = "Generation") FString Summary;

	void Reset(int32 InSeed);
	void AddPlaced(FName Category, int32 Count = 1);
	void AddRejected(FName Category, EMTPlacementRejection Reason);
	FString BuildSummary() const;
};

/**
 * Buena Village generator. Every placement goes through UMTPlacementValidator (footprint registry, exclusion zones:
 * roads, 3 m door-front clearance, fields, well square, river; slope rules; world collision), so houses never
 * overlap houses, trees never go through buildings/roads, props never block doors and NPC spawn/quest points never
 * start inside geometry. Everything spawned is tagged "MTGenerated" (+ a per-generator tag) and ClearGenerated()
 * removes exactly that.
 *
 * Roads are NOT meshes: they are painted into the landscape "dirt_road" layer by Tools/generate_fittoa_terrain.py
 * (no coplanar road planes -> no z-fighting). The generator reads the same polylines from Content/Data/Fittoa_Roads.json
 * and stores them in USplineComponents for placement, AI and debugging.
 *
 * Mesh conventions for custom meshes: pivot at bottom centre, front (door) facing +X, 1 uu = 1 cm.
 */
UCLASS(meta = (DisplayName = "MT Village Generator"))
class MUSHOKURPG_API AMTVillageGenerator : public AActor
{
	GENERATED_BODY()

public:
	AMTVillageGenerator();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Village")
	void Generate();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Village")
	void ClearGenerated();

	/** Writes the current road polylines to Saved/MTGenerated/<name>_roads.json (feed to the terrain script --extra-roads). */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Village")
	void ExportRoadsJson();

	UFUNCTION(BlueprintPure, Category = "Village")
	FMTGenerationReport GetLastReport() const { return LastReport; }

	UFUNCTION(BlueprintPure, Category = "Village")
	TArray<FTransform> GetNPCSpawnPoints() const { return NPCSpawnPoints; }

	UFUNCTION(BlueprintPure, Category = "Village")
	TArray<FTransform> GetQuestPoints() const { return QuestPoints; }

	static const FName GeneratedTag;
	static const FName BuildingTag;

	// ---------------- rules ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	int32 Seed = 1717;

	/** Houses are placed within this radius of the actor (the square / well sits at the actor location). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules", meta = (ClampMin = "5000"))
	float VillageRadius = 26000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	float SquareRadius = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules", meta = (ClampMin = "1"))
	int32 MinHomes = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules", meta = (ClampMin = "1"))
	int32 MaxHomes = 14;

	/** Minimum distance between house centres (Buena homes are built far apart). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	float HomeMinSpacing = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	FVector2D HomeHalfExtentMin = FVector2D(450.0, 350.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	FVector2D HomeHalfExtentMax = FVector2D(650.0, 480.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	float HomeWallHeight = 550.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	int32 NumBarns = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	FVector2D BarnHalfExtent = FVector2D(900.0, 600.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	float BarnHeight = 750.f;

	/** Door-front clearance depth (cm) kept free of props, fences and trees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	float DoorClearance = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	float RoadClearance = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	FMTSlopeRule BuildingSlope;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	FMTSlopeRule FieldSlope;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Rules")
	FMTSlopeRule PropSlope;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Fields")
	int32 NumFields = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Fields")
	FVector2D FieldSizeMin = FVector2D(3000.0, 3000.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Fields")
	FVector2D FieldSizeMax = FVector2D(7000.0, 7000.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Fields")
	float FieldRingInner = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Fields")
	float FieldRingOuter = 34000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Fields", meta = (ClampMin = "60"))
	float WheatSpacing = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Fields", meta = (ClampMin = "100"))
	float FenceSegmentLength = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Fields")
	float FenceGateWidth = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Forest")
	float ForestInnerRadius = 36000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Forest")
	float ForestOuterRadius = 46000.f;

	/** World-space angle range (deg, atan2(Y,X)); default covers west -> north (north is -Y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Forest")
	FVector2D ForestArcDegrees = FVector2D(120.0, 300.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Forest", meta = (ClampMin = "200"))
	float TreeMinDistance = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Forest")
	int32 MaxTrees = 2500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Props")
	int32 MaxPropsPerYard = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Props")
	float YardRadius = 1000.f;

	// ---------------- roads ----------------
	/** Relative to the project Content directory. Empty or missing -> procedural winding roads from Seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Roads")
	FString RoadsJsonPath = TEXT("Data/Fittoa_Roads.json");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Roads")
	bool bUseRoadsJson = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Roads")
	float ProceduralRoadHalfWidth = 250.f;

	/** Extra no-build zones (quest areas, ruins...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Roads")
	TArray<FMTExclusionZone> ExtraExclusionZones;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Village|Output")
	TArray<FMTPolyline2D> Roads;

	// ---------------- meshes (unset -> BLOCKOUT engine basic shapes) ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Meshes") TArray<TSoftObjectPtr<UStaticMesh>> HouseMeshes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Meshes") TSoftObjectPtr<UStaticMesh> BarnMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Meshes") TSoftObjectPtr<UStaticMesh> WheatMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Meshes") TSoftObjectPtr<UStaticMesh> FencePanelMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Meshes") TSoftObjectPtr<UStaticMesh> FencePostMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Meshes") TArray<TSoftObjectPtr<UStaticMesh>> TreeMeshes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Meshes") TArray<TSoftObjectPtr<UStaticMesh>> PropMeshes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Village|Meshes") TSoftObjectPtr<UStaticMesh> WellMesh;

	// ---------------- output ----------------
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Village|Output")
	TArray<FTransform> NPCSpawnPoints;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Village|Output")
	TArray<FTransform> QuestPoints;

	UPROPERTY(VisibleInstanceOnly, Category = "Village|Output")
	TArray<TSoftObjectPtr<AActor>> GeneratedActors;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Village|Output")
	FMTGenerationReport LastReport;

	/** Stable per-generator id so ClearGenerated never touches another generator's output. */
	UPROPERTY(VisibleInstanceOnly, Category = "Village|Output")
	FGuid GeneratorId;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	struct FPlacedHome
	{
		FMTFootprint Footprint;
		float FloorZ = 0.f;
	};

	FName GetOwnerTag() const;
	UStaticMesh* ResolveMesh(const TSoftObjectPtr<UStaticMesh>& Slot, const TCHAR* FallbackPath, const TCHAR* SlotName, bool& bOutBlockout);
	AStaticMeshActor* SpawnMeshActor(UStaticMesh* Mesh, const FTransform& Transform, const FString& Label, bool bBuilding);
	UHierarchicalInstancedStaticMeshComponent* GetOrMakeHISM(const FString& BaseName, UStaticMesh* Mesh, bool bCollision, float CullDistance);
	USplineComponent* MakeRoadSpline(const FMTPolyline2D& Road);

	bool LoadRoadsFromJson(TArray<FMTPolyline2D>& OutRoads, TArray<FMTPolyline2D>& OutRivers) const;
	void BuildProceduralRoads(FRandomStream& Rng);
	bool FindNearestRoad(const FVector2D& P, FVector2D& OutPoint, FVector2D& OutDir, float& OutHalfWidth, float& OutDistance) const;
	float GroundZ(const FVector2D& XY, float Fallback) const;

	void PlaceSquareAndWell();
	void PlaceHomes(FRandomStream& Rng);
	void PlaceBarns(FRandomStream& Rng);
	void PlaceFields(FRandomStream& Rng);
	void BuildFieldContents(FRandomStream& Rng, const FMTFootprint& Field, int32 GateEdge);
	void PlaceProps(FRandomStream& Rng);
	void PlaceForest(FRandomStream& Rng);
	void PlaceSpawnPoints();
	bool TryAddSpawnPoint(const FVector2D& XY, float Yaw, TArray<FTransform>& Out, FName Category);
	bool PlaceBuilding(FName Category, const FMTFootprint& Footprint, float Height, UStaticMesh* Mesh, bool bBlockout,
		const FString& Label, float& OutFloorZ);

	FMTPlacementRegistry Registry;
	TArray<FPlacedHome> PlacedHomes;
	TArray<FMTFootprint> PlacedBarns;
	TArray<const AActor*> IgnoreForTraces;
	/** Transient lookup used only while Generate() runs; the components themselves are owned by this actor. */
	TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> HISMCache;
	FVector2D Origin = FVector2D::ZeroVector;
	float OriginZ = 0.f;
};

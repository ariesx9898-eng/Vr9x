// Editor-only world building for LA PLACE (Content/Python/mt_build_world.py): landscape import from the world generator's
// heightmap and weightmaps (Tools/world, Docs/LaPlace/Spec.md section 2), World Partition streaming settings and
// batch instancing of the generated foliage / city placements. Python cannot do these at this scale.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MTWorldBuildLibrary.generated.h"

class ALandscape;
class ULandscapeLayerInfoObject;
class UMaterialInterface;
class UStaticMesh;
class UWorld;
class UHLODLayer;

UCLASS()
class MUSHOKURPG_API UMTWorldBuildLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates the weight-blended layer info asset <PackageFolder>/LI_<LayerName>, or returns the existing one. The caller
	 * saves it (save_dirty_packages). Editor only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static ULandscapeLayerInfoObject* CreateLandscapeLayerInfo(const FString& PackageFolder, FName LayerName, FLinearColor DebugColor);

	/**
	 * Creates a landscape in the current editor world from a raw uint16 little-endian heightmap (VerticesX x VerticesY,
	 * rows north to south) and one 8-bit greyscale PNG weightmap per layer (same size; LayerFiles[i] for LayerInfos[i];
	 * weights sum to 255 per pixel). Location is the landscape's min corner, Scale its quad size / height scale in cm.
	 * In a World Partition map the components are then split into streaming proxies of StreamingGridSize x
	 * StreamingGridSize components; bSpatiallyLoaded = false keeps every proxy loaded (the whole terrain stays visible
	 * without HLODs). Needs a rendering-capable editor (-AllowCommandletRendering, no -nullrhi) so the edit layers can
	 * be merged. Returns null on failure (see the log). The caller saves the map and its external actors.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static ALandscape* ImportLandscape(const FString& HeightmapFile, int32 VerticesX, int32 VerticesY, int32 SectionsPerComponent,
		int32 QuadsPerSection, FVector Location, FVector Scale, UMaterialInterface* Material,
		const TArray<ULandscapeLayerInfoObject*>& LayerInfos, const TArray<FString>& LayerFiles, int32 StreamingGridSize,
		bool bSpatiallyLoaded, const FString& ActorLabel);

	/** Replaces the material of every landscape proxy in the editor world (after rebuilding the material). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static int32 SetLandscapeMaterial(UMaterialInterface* Material);

	/**
	 * Sets the cell size and loading range (cm) of every runtime partition of the editor world's World Partition
	 * (runtime hash set or legacy spatial hash grids). Returns the number of partitions changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static int32 ConfigureWorldPartitionGrid(int32 CellSizeCm, int32 LoadingRangeCm);

	/**
	 * Registers HLOD layers with the editor world's runtime hash set (UE 5.4+ World Partition only builds HLODs for
	 * layers listed in a partition's HLOD setups). Each layer gets its own setup on the first (main) partition with an
	 * LHGrid of CellSizeCm / LoadingRangeCm; layers already listed are left alone. Returns the number added.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static int32 RegisterHLODLayers(const TArray<UHLODLayer*>& Layers, int32 CellSizeCm, int32 LoadingRangeCm);

	/** Heightfield sample (cm, world space) from a raw heightmap loaded with LoadHeightmapForQueries. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static bool LoadHeightmapForQueries(const FString& HeightmapFile, int32 VerticesX, int32 VerticesY, FVector Location, FVector Scale);

	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static float GetTerrainHeight(float X, float Y);

	/**
	 * Spawns (or refills) an actor labelled Label holding one hierarchical instanced static mesh component per mesh, from
	 * a flat float array of transforms: 7 floats per instance (X, Y, Z cm, Yaw deg, Scale, Pitch deg, Roll deg), grouped
	 * by MeshIndices[i] (one mesh index per instance). CullStart/CullEnd (cm, 0 = never) apply to every component.
	 * bSpatiallyLoaded controls World Partition streaming. Collision: bCollision enables query+physics collision.
	 * Returns the actor (tagged with Tags).
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static AActor* SpawnInstancedGroup(const FString& Label, const TArray<UStaticMesh*>& Meshes, const TArray<int32>& MeshIndices,
		const TArray<float>& Transforms, float CullStart, float CullEnd, bool bCollision, bool bSpatiallyLoaded, bool bCastShadow,
		const TArray<FName>& Tags, FName HLODLayerPath);

	/**
	 * Spawns every instance group of an instance set written by the Tools/world scatter / city generators:
	 * JsonFile = {"Meshes": ["/Game/...", ...], "Groups": [{"Label", "Offset", "Count", "CullStart", "CullEnd",
	 * "Collision", "SpatiallyLoaded", "CastShadow", "Tags": [...], "HLODLayer"}, ...]} and BinFile = little-endian
	 * float32 records of 8 floats (MeshIndex, X, Y, Z cm, Yaw, Pitch, Roll deg, Scale) indexed by Offset/Count.
	 * Every spawned actor also gets SetTag (so a rebuild can DestroyActorsWithTag first). Returns the instance count
	 * (-1 on a read error).
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static int32 SpawnInstanceSetFromFile(const FString& JsonFile, const FString& BinFile, FName SetTag);

	/**
	 * Creates (replaces) a static mesh asset from triangles: Positions in cm (mesh space), Triangles as index triples,
	 * UVs per position (or empty), one material. Normals point up (+Z) unless bComputeNormals. No collision unless
	 * bCollision. Used for lake / river / ocean surfaces generated from World.json. The caller saves it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static UStaticMesh* CreateStaticMeshAsset(const FString& PackagePath, const TArray<FVector>& Positions, const TArray<int32>& Triangles,
		const TArray<FVector2D>& UVs, UMaterialInterface* Material, bool bComputeNormals, bool bCollision);

	/**
	 * Replaces a static mesh's collision with one upright capsule (tree trunks): Radius and Height in cm, standing on the
	 * pivot. Simple and complex queries both use it (leaf cards never block). The caller saves the mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static bool SetTrunkCollision(UStaticMesh* Mesh, float Radius, float Height);

	/**
	 * Assigns materials by slot name and the Nanite settings in one go, so the mesh (and its distance field) is rebuilt
	 * once instead of once per change. PreserveArea keeps foliage cards from thinning out in the distance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static bool ConfigureKitMesh(UStaticMesh* Mesh, const TMap<FName, UMaterialInterface*>& SlotMaterials, bool bNanite, bool bPreserveArea);

	/** Collision from the render mesh itself (walls, rocks): complex as simple, no simple shapes. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static bool SetComplexCollision(UStaticMesh* Mesh, bool bEnable);

	/** Compiles Material for the running shader platform and returns its compile errors (empty = OK). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static TArray<FString> GetMaterialCompileErrors(UMaterialInterface* Material);

	/** Destroys every actor in the editor world that carries Tag. Returns how many. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|World Build")
	static int32 DestroyActorsWithTag(FName Tag);
};

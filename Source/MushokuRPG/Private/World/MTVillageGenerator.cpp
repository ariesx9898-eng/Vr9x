#include "World/MTVillageGenerator.h"

#include "Core/MTTypes.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

const FName AMTVillageGenerator::GeneratedTag(TEXT("MTGenerated"));
const FName AMTVillageGenerator::BuildingTag(TEXT("MTBuilding"));

namespace MTVillage
{
	static const FName CatRoad(TEXT("Road"));
	static const FName CatWell(TEXT("Well"));
	static const FName CatHouse(TEXT("House"));
	static const FName CatBarn(TEXT("Barn"));
	static const FName CatField(TEXT("Field"));
	static const FName CatWheat(TEXT("Wheat"));
	static const FName CatFence(TEXT("FenceSegment"));
	static const FName CatProp(TEXT("Prop"));
	static const FName CatTree(TEXT("Tree"));
	static const FName CatSpawn(TEXT("NPCSpawn"));
	static const FName CatQuest(TEXT("QuestPoint"));

	static const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");         // 100 cm, pivot centre
	static const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); // 100 cm dia x 100 cm, pivot centre
	static const TCHAR* ConePath = TEXT("/Engine/BasicShapes/Cone.Cone");         // 100 cm dia x 100 cm, pivot centre

	static float YawFromDir(const FVector2D& D)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(float(D.Y), float(D.X)));
	}

	static FVector2D DirFromYaw(float YawDeg)
	{
		const float R = FMath::DegreesToRadians(YawDeg);
		return FVector2D(FMath::Cos(R), FMath::Sin(R));
	}

	static float NormalizeAngle360(float Deg)
	{
		float A = FMath::Fmod(Deg, 360.f);
		return A < 0.f ? A + 360.f : A;
	}

	static FVector2D MeshHalfExtentXY(const UStaticMesh* Mesh, const FVector2D& Fallback)
	{
		if (!Mesh)
		{
			return Fallback;
		}
		const FBox Box = Mesh->GetBoundingBox();
		const FVector Ext = Box.GetExtent();
		return (Ext.X > 1.0 && Ext.Y > 1.0) ? FVector2D(Ext.X, Ext.Y) : Fallback;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
void FMTGenerationReport::Reset(int32 InSeed)
{
	Seed = InSeed;
	Placed.Reset();
	Rejected.Reset();
	RejectionReasons.Reset();
	BlockoutSlots.Reset();
	bRoadsFromJson = false;
	Summary.Reset();
}

void FMTGenerationReport::AddPlaced(FName Category, int32 Count)
{
	Placed.FindOrAdd(Category) += Count;
}

void FMTGenerationReport::AddRejected(FName Category, EMTPlacementRejection Reason)
{
	Rejected.FindOrAdd(Category) += 1;
	RejectionReasons.FindOrAdd(Category.ToString() + TEXT(":") + UMTPlacementValidator::RejectionToString(Reason)) += 1;
}

FString FMTGenerationReport::BuildSummary() const
{
	FString Out = FString::Printf(TEXT("Village generation report (seed %d, roads from %s)\n"), Seed,
		bRoadsFromJson ? TEXT("Fittoa_Roads.json") : TEXT("procedural fallback"));
	TSet<FName> Categories;
	for (const TPair<FName, int32>& P : Placed) { Categories.Add(P.Key); }
	for (const TPair<FName, int32>& R : Rejected) { Categories.Add(R.Key); }
	for (const FName& Cat : Categories)
	{
		const int32* NumPlaced = Placed.Find(Cat);
		const int32* NumRejected = Rejected.Find(Cat);
		Out += FString::Printf(TEXT("  %-14s placed %5d  rejected %5d\n"), *Cat.ToString(), NumPlaced ? *NumPlaced : 0,
			NumRejected ? *NumRejected : 0);
	}
	for (const TPair<FString, int32>& R : RejectionReasons)
	{
		Out += FString::Printf(TEXT("    reject %-40s x%d\n"), *R.Key, R.Value);
	}
	for (const FString& Slot : BlockoutSlots)
	{
		Out += FString::Printf(TEXT("  [BLOCKOUT] %s uses an engine basic-shape PLACEHOLDER (not final art)\n"), *Slot);
	}
	return Out;
}

// ---------------------------------------------------------------------------------------------------------------------
AMTVillageGenerator::AMTVillageGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	BuildingSlope.MaxHeightDelta = 90.f;
	BuildingSlope.MaxSlopeDegrees = 10.f;
	FieldSlope.MaxHeightDelta = 500.f;
	FieldSlope.MaxSlopeDegrees = 12.f;
	PropSlope.MaxHeightDelta = 40.f;
	PropSlope.MaxSlopeDegrees = 15.f;
}

void AMTVillageGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!GeneratorId.IsValid())
	{
		GeneratorId = FGuid::NewGuid();
	}
}

FName AMTVillageGenerator::GetOwnerTag() const
{
	return FName(*FString::Printf(TEXT("MTGen_%s"), *GeneratorId.ToString()));
}

UStaticMesh* AMTVillageGenerator::ResolveMesh(const TSoftObjectPtr<UStaticMesh>& Slot, const TCHAR* FallbackPath,
	const TCHAR* SlotName, bool& bOutBlockout)
{
	bOutBlockout = false;
	UStaticMesh* Mesh = Slot.IsNull() ? nullptr : Slot.LoadSynchronous();
	if (Mesh)
	{
		return Mesh;
	}
	bOutBlockout = true;
	const FString Note = FString::Printf(TEXT("%s -> %s"), SlotName, FallbackPath);
	if (!LastReport.BlockoutSlots.Contains(Note))
	{
		LastReport.BlockoutSlots.Add(Note);
		UE_LOG(LogMushoku, Warning, TEXT("[BLOCKOUT] %s: mesh slot '%s' is empty/unloadable, using placeholder %s"),
			*GetName(), SlotName, FallbackPath);
	}
	return LoadObject<UStaticMesh>(nullptr, FallbackPath);
}

AStaticMeshActor* AMTVillageGenerator::SpawnMeshActor(UStaticMesh* Mesh, const FTransform& Transform, const FString& Label, bool bBuilding)
{
	UWorld* World = GetWorld();
	if (!World || !Mesh)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform, Params);
	if (!Actor)
	{
		return nullptr;
	}
	if (UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent())
	{
		Comp->SetStaticMesh(Mesh);
	}
	Actor->Tags.AddUnique(GeneratedTag);
	Actor->Tags.AddUnique(GetOwnerTag());
	if (bBuilding)
	{
		Actor->Tags.AddUnique(BuildingTag);
	}
	Actor->FinishSpawning(Transform);
#if WITH_EDITOR
	Actor->SetActorLabel(Label);
	Actor->SetFolderPath(FName(*FString::Printf(TEXT("MTGenerated/%s"), *GetActorNameOrLabel())));
#endif
	GeneratedActors.Add(TSoftObjectPtr<AActor>(Actor));
	IgnoreForTraces.Add(Actor);
	return Actor;
}

UHierarchicalInstancedStaticMeshComponent* AMTVillageGenerator::GetOrMakeHISM(const FString& BaseName, UStaticMesh* Mesh,
	bool bCollision, float CullDistance)
{
	if (!Mesh)
	{
		return nullptr;
	}
	const FString Key = BaseName + TEXT("|") + Mesh->GetPathName();
	if (TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Found = HISMCache.Find(Key))
	{
		return Found->Get();
	}
	const FName CompName = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), FName(*BaseName));
	UHierarchicalInstancedStaticMeshComponent* Comp =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(this, CompName, RF_Transactional);
	if (!Comp)
	{
		return nullptr;
	}
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetMobility(EComponentMobility::Static);
	Comp->SetStaticMesh(Mesh);
	Comp->ComponentTags.AddUnique(GeneratedTag);
	if (bCollision)
	{
		Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (CullDistance > 0.f)
	{
		Comp->SetCullDistances(0, int32(CullDistance));
	}
	AddInstanceComponent(Comp);
	Comp->RegisterComponent();
	HISMCache.Add(Key, Comp);
	return Comp;
}

USplineComponent* AMTVillageGenerator::MakeRoadSpline(const FMTPolyline2D& Road)
{
	if (Road.Points.Num() < 2)
	{
		return nullptr;
	}
	const FName CompName = MakeUniqueObjectName(this, USplineComponent::StaticClass(),
		FName(*FString::Printf(TEXT("Road_%s"), *Road.Name.ToString())));
	USplineComponent* Spline = NewObject<USplineComponent>(this, CompName, RF_Transactional);
	if (!Spline)
	{
		return nullptr;
	}
	Spline->SetupAttachment(GetRootComponent());
	Spline->SetMobility(EComponentMobility::Static);
	Spline->ComponentTags.AddUnique(GeneratedTag);
	Spline->ComponentTags.AddUnique(Road.Name);
	AddInstanceComponent(Spline);
	Spline->RegisterComponent();

	TArray<FVector> Points;
	Points.Reserve(Road.Points.Num());
	for (const FVector2D& P : Road.Points)
	{
		// Data only (never rendered): the visible road is the landscape dirt_road paint layer.
		Points.Add(FVector(P.X, P.Y, GroundZ(P, OriginZ) + 2.f));
	}
	Spline->SetSplinePoints(Points, ESplineCoordinateSpace::World, true);
	return Spline;
}

float AMTVillageGenerator::GroundZ(const FVector2D& XY, float Fallback) const
{
	float Z = Fallback;
	if (!UMTPlacementValidator::TraceGroundZ(GetWorld(), XY, Z, IgnoreForTraces))
	{
		return Fallback;
	}
	return Z;
}

// ---------------------------------------------------------------------------------------------------------------------
bool AMTVillageGenerator::LoadRoadsFromJson(TArray<FMTPolyline2D>& OutRoads, TArray<FMTPolyline2D>& OutRivers) const
{
	if (RoadsJsonPath.IsEmpty())
	{
		return false;
	}
	const FString Path = FPaths::ProjectContentDir() / RoadsJsonPath;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogMushoku, Warning, TEXT("VillageGenerator: roads file not found: %s"), *Path);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogMushoku, Error, TEXT("VillageGenerator: failed to parse %s"), *Path);
		return false;
	}
	auto ParseList = [](const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool bRiver, TArray<FMTPolyline2D>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (!Obj->TryGetArrayField(Field, List) || !List)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> RoadObj = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!RoadObj.IsValid())
			{
				continue;
			}
			FMTPolyline2D Line;
			FString Name;
			RoadObj->TryGetStringField(TEXT("Name"), Name);
			Line.Name = FName(*Name);
			double HalfWidth = 250.0;
			RoadObj->TryGetNumberField(TEXT("HalfWidthCm"), HalfWidth);
			Line.HalfWidth = float(HalfWidth);
			Line.bIsRiver = bRiver;
			const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
			if (RoadObj->TryGetArrayField(TEXT("Points"), Points) && Points)
			{
				for (const TSharedPtr<FJsonValue>& PV : *Points)
				{
					const TSharedPtr<FJsonObject> PO = PV.IsValid() ? PV->AsObject() : nullptr;
					double X = 0.0, Y = 0.0;
					if (PO.IsValid() && PO->TryGetNumberField(TEXT("X"), X) && PO->TryGetNumberField(TEXT("Y"), Y))
					{
						Line.Points.Add(FVector2D(X, Y));
					}
				}
			}
			if (Line.Points.Num() >= 2)
			{
				Out.Add(MoveTemp(Line));
			}
		}
	};
	ParseList(Root, TEXT("Roads"), false, OutRoads);
	ParseList(Root, TEXT("Rivers"), true, OutRivers);
	return OutRoads.Num() > 0;
}

void AMTVillageGenerator::BuildProceduralRoads(FRandomStream& Rng)
{
	// Fallback only: 4 winding radial roads from the square. These are NOT painted into the landscape; run
	// ExportRoadsJson() and re-run Tools/generate_fittoa_terrain.py --extra-roads <file> to paint them.
	const float Length = VillageRadius + FieldRingOuter * 0.5f;
	const float Step = 2000.f;
	const float BaseAngle = Rng.FRandRange(0.f, 360.f);
	for (int32 i = 0; i < 4; ++i)
	{
		FMTPolyline2D Road;
		Road.Name = FName(*FString::Printf(TEXT("Proc_Road_%d"), i));
		Road.HalfWidth = ProceduralRoadHalfWidth;
		float Heading = BaseAngle + 90.f * i + Rng.FRandRange(-20.f, 20.f);
		FVector2D P = Origin;
		Road.Points.Add(P);
		for (float Dist = 0.f; Dist < Length; Dist += Step)
		{
			Heading += Rng.FRandRange(-12.f, 12.f);
			P += MTVillage::DirFromYaw(Heading) * Step;
			Road.Points.Add(P);
		}
		Roads.Add(MoveTemp(Road));
	}
	UE_LOG(LogMushoku, Warning, TEXT("VillageGenerator %s: using PROCEDURAL roads (not painted into the landscape)."), *GetName());
}

bool AMTVillageGenerator::FindNearestRoad(const FVector2D& P, FVector2D& OutPoint, FVector2D& OutDir, float& OutHalfWidth,
	float& OutDistance) const
{
	bool bFound = false;
	double Best = TNumericLimits<double>::Max();
	for (const FMTPolyline2D& Road : Roads)
	{
		if (Road.bIsRiver)
		{
			continue;
		}
		for (int32 i = 0; i + 1 < Road.Points.Num(); ++i)
		{
			const FVector2D A = Road.Points[i];
			const FVector2D AB = Road.Points[i + 1] - A;
			const double L2 = AB.SizeSquared();
			if (L2 < 1.0)
			{
				continue;
			}
			const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.0, 1.0);
			const FVector2D Q = A + AB * T;
			const double D = FVector2D::Distance(P, Q);
			if (D < Best)
			{
				Best = D;
				OutPoint = Q;
				OutDir = AB.GetSafeNormal();
				OutHalfWidth = Road.HalfWidth;
				bFound = true;
			}
		}
	}
	OutDistance = float(Best);
	return bFound;
}

// ---------------------------------------------------------------------------------------------------------------------
void AMTVillageGenerator::Generate()
{
#if WITH_EDITOR
	FScopedTransaction Transaction(NSLOCTEXT("MTVillage", "Generate", "Generate Village"));
#endif
	ClearGenerated();
	Modify();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!GeneratorId.IsValid())
	{
		GeneratorId = FGuid::NewGuid();
	}
	FRandomStream Rng(Seed);
	LastReport.Reset(Seed);
	Registry.Reset();
	PlacedHomes.Reset();
	PlacedBarns.Reset();
	HISMCache.Reset();
	IgnoreForTraces.Reset();
	IgnoreForTraces.Add(this);

	const FVector Loc = GetActorLocation();
	Origin = FVector2D(Loc.X, Loc.Y);
	OriginZ = GroundZ(Origin, float(Loc.Z));

	// Roads (shared with the terrain script) -> registry + spline data.
	Roads.Reset();
	TArray<FMTPolyline2D> Rivers;
	if (bUseRoadsJson && LoadRoadsFromJson(Roads, Rivers))
	{
		LastReport.bRoadsFromJson = true;
	}
	else
	{
		Roads.Reset();
		BuildProceduralRoads(Rng);
	}
	for (const FMTPolyline2D& Road : Roads)
	{
		Registry.AddRoad(Road);
		if (MakeRoadSpline(Road))
		{
			LastReport.AddPlaced(MTVillage::CatRoad);
		}
	}
	for (FMTPolyline2D& River : Rivers)
	{
		Registry.AddRoad(River);
		Roads.Add(River);
	}
	for (const FMTExclusionZone& Zone : ExtraExclusionZones)
	{
		Registry.AddExclusion(Zone);
	}

	PlaceSquareAndWell();
	PlaceHomes(Rng);
	PlaceBarns(Rng);
	PlaceFields(Rng);
	PlaceProps(Rng);
	PlaceForest(Rng);
	PlaceSpawnPoints();

	HISMCache.Reset();
	LastReport.Summary = LastReport.BuildSummary();
	UE_LOG(LogMushoku, Log, TEXT("%s: %s"), *GetName(), *LastReport.Summary);
}

void AMTVillageGenerator::ClearGenerated()
{
#if WITH_EDITOR
	FScopedTransaction Transaction(NSLOCTEXT("MTVillage", "Clear", "Clear Generated Village"));
#endif
	Modify();
	UWorld* World = GetWorld();
	const FName OwnerTag = GetOwnerTag();

	TSet<AActor*> ToDestroy;
	for (const TSoftObjectPtr<AActor>& Ptr : GeneratedActors)
	{
		AActor* Actor = Ptr.Get();
		if (Actor && Actor->ActorHasTag(GeneratedTag))
		{
			ToDestroy.Add(Actor);
		}
	}
	if (World && GeneratorId.IsValid())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor->ActorHasTag(GeneratedTag) && Actor->ActorHasTag(OwnerTag))
			{
				ToDestroy.Add(Actor);
			}
		}
	}
	for (AActor* Actor : ToDestroy)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		bool bDestroyed = false;
#if WITH_EDITOR
		if (World && !World->IsGameWorld())
		{
			bDestroyed = World->EditorDestroyActor(Actor, true);
		}
#endif
		if (!bDestroyed)
		{
			Actor->Destroy();
		}
	}
	GeneratedActors.Reset();

	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->ComponentHasTag(GeneratedTag))
		{
			RemoveInstanceComponent(Comp);
			Comp->DestroyComponent();
		}
	}
	HISMCache.Reset();
	NPCSpawnPoints.Reset();
	QuestPoints.Reset();
	Roads.Reset();
}

void AMTVillageGenerator::ExportRoadsJson()
{
	FString Json = TEXT("{\n \"Comment\": \"Exported by AMTVillageGenerator. Feed to Tools/generate_fittoa_terrain.py --extra-roads\",\n \"Roads\": [\n");
	bool bFirstRoad = true;
	for (const FMTPolyline2D& Road : Roads)
	{
		if (Road.bIsRiver)
		{
			continue;
		}
		Json += FString::Printf(TEXT("%s  {\"Name\": \"%s\", \"HalfWidthCm\": %.1f, \"Points\": ["), bFirstRoad ? TEXT("") : TEXT(",\n"),
			*Road.Name.ToString(), Road.HalfWidth);
		for (int32 i = 0; i < Road.Points.Num(); ++i)
		{
			Json += FString::Printf(TEXT("%s{\"X\": %.1f, \"Y\": %.1f}"), i == 0 ? TEXT("") : TEXT(", "), Road.Points[i].X, Road.Points[i].Y);
		}
		Json += TEXT("]}");
		bFirstRoad = false;
	}
	Json += TEXT("\n ]\n}\n");
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("MTGenerated");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = Dir / (GetActorNameOrLabel() + TEXT("_roads.json"));
	if (FFileHelper::SaveStringToFile(Json, *Path))
	{
		UE_LOG(LogMushoku, Log, TEXT("VillageGenerator: wrote %s"), *Path);
	}
	else
	{
		UE_LOG(LogMushoku, Error, TEXT("VillageGenerator: could not write %s"), *Path);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
bool AMTVillageGenerator::PlaceBuilding(FName Category, const FMTFootprint& Footprint, float Height, UStaticMesh* Mesh,
	bool bBlockout, const FString& Label, float& OutFloorZ)
{
	UWorld* World = GetWorld();
	const FMTGroundSample Sample = UMTPlacementValidator::SampleGroundIgnoring(World, Footprint, IgnoreForTraces);
	if (!Sample.bValid)
	{
		LastReport.AddRejected(Category, EMTPlacementRejection::NoGround);
		return false;
	}
	if (!UMTPlacementValidator::PassesSlopeRule(Sample, BuildingSlope))
	{
		LastReport.AddRejected(Category, EMTPlacementRejection::TooSteep);
		return false;
	}
	FString Blocker;
	if (UMTPlacementValidator::IsBlockedByWorldGeometry(World, Footprint, Sample.MinZ, Height, IgnoreForTraces, &Blocker))
	{
		UE_LOG(LogMushoku, Verbose, TEXT("VillageGenerator: %s blocked by %s"), *Label, *Blocker);
		LastReport.AddRejected(Category, EMTPlacementRejection::BlockedByWorldGeometry);
		return false;
	}

	const FMTFoundation Foundation = UMTPlacementValidator::ComputeFoundation(Sample, 5.f, 20.f);
	const FRotator Rotation(0.f, Footprint.YawDegrees, 0.f);
	if (bBlockout)
	{
		// One cube from the buried plinth bottom to the roof line: never floats, never sinks.
		const float Bottom = Foundation.PlinthBottomZ;
		const float Top = Foundation.FloorZ + Height;
		const FVector Scale(Footprint.HalfExtent.X * 2.0 / 100.0, Footprint.HalfExtent.Y * 2.0 / 100.0, (Top - Bottom) / 100.f);
		const FTransform T(Rotation, FVector(Footprint.Center.X, Footprint.Center.Y, (Bottom + Top) * 0.5f), Scale);
		SpawnMeshActor(Mesh, T, TEXT("BLOCKOUT_") + Label, true);
	}
	else
	{
		const FTransform T(Rotation, FVector(Footprint.Center.X, Footprint.Center.Y, Foundation.FloorZ));
		SpawnMeshActor(Mesh, T, Label, true);
		if (Foundation.FloorZ - Sample.MinZ > 2.f)
		{
			bool bCubeBlockout = true;
			UStaticMesh* Cube = ResolveMesh(TSoftObjectPtr<UStaticMesh>(), MTVillage::CubePath, TEXT("Foundation plinth"), bCubeBlockout);
			// Inset 5 cm and stop 1 cm under the floor: no coplanar faces with the building.
			const float Top = Foundation.FloorZ - 1.f;
			const float Bottom = Foundation.PlinthBottomZ;
			const FVector Scale(FMath::Max(Footprint.HalfExtent.X - 5.0, 10.0) * 2.0 / 100.0,
				FMath::Max(Footprint.HalfExtent.Y - 5.0, 10.0) * 2.0 / 100.0, FMath::Max(Top - Bottom, 1.f) / 100.f);
			const FTransform PT(Rotation, FVector(Footprint.Center.X, Footprint.Center.Y, (Top + Bottom) * 0.5f), Scale);
			if (AStaticMeshActor* Plinth = SpawnMeshActor(Cube, PT, Label + TEXT("_Foundation"), false))
			{
				Plinth->Tags.AddUnique(TEXT("MTFoundation"));
			}
		}
	}
	Registry.AddFootprint(Footprint, Category);
	LastReport.AddPlaced(Category);
	OutFloorZ = Foundation.FloorZ;
	return true;
}

void AMTVillageGenerator::PlaceSquareAndWell()
{
	Registry.AddExclusion(FMTExclusionZone::MakeCircle(TEXT("WellSquare"), Origin, SquareRadius));
	bool bBlockout = false;
	UStaticMesh* Mesh = ResolveMesh(WellMesh, MTVillage::CylinderPath, TEXT("WellMesh"), bBlockout);
	const float WellRadius = 160.f;
	FMTCircleFootprint WellCircle;
	WellCircle.Center = Origin;
	WellCircle.Radius = WellRadius;

	const FMTFootprint WellBox(Origin, FVector2D(WellRadius, WellRadius), 0.f, 0.f);
	const FMTGroundSample Sample = UMTPlacementValidator::SampleGroundIgnoring(GetWorld(), WellBox, IgnoreForTraces);
	const float BaseZ = Sample.bValid ? Sample.MinZ : OriginZ;
	FTransform T;
	if (bBlockout)
	{
		T = FTransform(FRotator::ZeroRotator, FVector(Origin.X, Origin.Y, BaseZ - 20.f + 60.f), FVector(3.2f, 3.2f, 1.2f));
	}
	else
	{
		T = FTransform(FRotator::ZeroRotator, FVector(Origin.X, Origin.Y, BaseZ - 5.f));
	}
	if (SpawnMeshActor(Mesh, T, bBlockout ? TEXT("BLOCKOUT_Buena_Well") : TEXT("Buena_Well"), true))
	{
		LastReport.AddPlaced(MTVillage::CatWell);
	}
	Registry.AddCircle(WellCircle, MTVillage::CatWell);
}

void AMTVillageGenerator::PlaceHomes(FRandomStream& Rng)
{
	const int32 Target = Rng.RandRange(FMath::Min(MinHomes, MaxHomes), FMath::Max(MinHomes, MaxHomes));
	const int32 MaxAttempts = Target * 60;
	const float InnerR = SquareRadius + 800.f;
	for (int32 Attempt = 0; Attempt < MaxAttempts && PlacedHomes.Num() < Target; ++Attempt)
	{
		// Uniform-by-area sample in the village annulus, then snap to the nearest road frontage.
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::Sqrt(Rng.FRandRange(InnerR * InnerR, VillageRadius * VillageRadius));
		const FVector2D P = Origin + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;

		// Mesh + size (drawn before validation so the random sequence is identical for every outcome).
		const int32 MeshIndex = HouseMeshes.Num() > 0 ? Rng.RandRange(0, HouseMeshes.Num() - 1) : -1;
		const FVector2D RandomHalf(Rng.FRandRange(HomeHalfExtentMin.X, HomeHalfExtentMax.X),
			Rng.FRandRange(HomeHalfExtentMin.Y, HomeHalfExtentMax.Y));
		const float Setback = Rng.FRandRange(400.f, 900.f);
		const float YawJitter = Rng.FRandRange(-6.f, 6.f);

		FVector2D RoadPoint, RoadDir;
		float HalfWidth = 0.f, RoadDist = 0.f;
		if (!FindNearestRoad(P, RoadPoint, RoadDir, HalfWidth, RoadDist))
		{
			LastReport.AddRejected(MTVillage::CatHouse, EMTPlacementRejection::OutOfBounds);
			continue;
		}
		bool bBlockout = false;
		UStaticMesh* Mesh = ResolveMesh(MeshIndex >= 0 ? HouseMeshes[MeshIndex] : TSoftObjectPtr<UStaticMesh>(),
			MTVillage::CubePath, TEXT("HouseMeshes"), bBlockout);
		const FVector2D Half = bBlockout ? RandomHalf : MTVillage::MeshHalfExtentXY(Mesh, RandomHalf);

		const FVector2D Normal(-RoadDir.Y, RoadDir.X);
		const float Side = FVector2D::DotProduct(P - RoadPoint, Normal) >= 0.0 ? 1.f : -1.f;
		const FVector2D Center = RoadPoint + Normal * Side * (HalfWidth + Setback + Half.X);
		const float Yaw = MTVillage::YawFromDir(-Normal * Side) + YawJitter; // front (+X, door) faces the road
		const FMTFootprint Footprint(Center, Half, Yaw, 200.f);

		if (FVector2D::Distance(Center, Origin) > VillageRadius)
		{
			LastReport.AddRejected(MTVillage::CatHouse, EMTPlacementRejection::OutOfBounds);
			continue;
		}
		const EMTPlacementRejection Reason = UMTPlacementValidator::CheckFootprint(Registry, Footprint, MTVillage::CatHouse,
			HomeMinSpacing, RoadClearance);
		if (Reason != EMTPlacementRejection::None)
		{
			LastReport.AddRejected(MTVillage::CatHouse, Reason);
			continue;
		}
		// Door-front clearance must itself be free (no footprint may sit in front of the door).
		const FMTFootprint Door(Center + Footprint.AxisX() * (Half.X + DoorClearance * 0.5f),
			FVector2D(DoorClearance * 0.5f, 150.0), Yaw, 0.f);
		bool bDoorBlocked = false;
		for (const FMTFootprint& Other : Registry.Footprints)
		{
			bDoorBlocked |= UMTPlacementValidator::FootprintsOverlap(Door, Other);
		}
		if (bDoorBlocked)
		{
			LastReport.AddRejected(MTVillage::CatHouse, EMTPlacementRejection::InsideExclusionZone);
			continue;
		}
		float FloorZ = 0.f;
		const FString Label = FString::Printf(TEXT("Buena_House_%02d"), PlacedHomes.Num() + 1);
		if (!PlaceBuilding(MTVillage::CatHouse, Footprint, HomeWallHeight, Mesh, bBlockout, Label, FloorZ))
		{
			continue;
		}
		Registry.AddExclusion(FMTExclusionZone::MakeBox(TEXT("DoorClearance"), Door));
		FPlacedHome Home;
		Home.Footprint = Footprint;
		Home.FloorZ = FloorZ;
		PlacedHomes.Add(Home);
	}
	if (PlacedHomes.Num() < MinHomes)
	{
		UE_LOG(LogMushoku, Warning, TEXT("VillageGenerator: only %d/%d homes fit the rules (see report)."), PlacedHomes.Num(), Target);
	}
}

void AMTVillageGenerator::PlaceBarns(FRandomStream& Rng)
{
	bool bBlockout = false;
	UStaticMesh* Mesh = ResolveMesh(BarnMesh, MTVillage::CubePath, TEXT("BarnMesh"), bBlockout);
	const FVector2D Half = bBlockout ? BarnHalfExtent : MTVillage::MeshHalfExtentXY(Mesh, BarnHalfExtent);
	const float InnerR = VillageRadius * 0.5f;
	const float OuterR = VillageRadius + 4000.f;
	for (int32 Attempt = 0; Attempt < NumBarns * 80 && PlacedBarns.Num() < NumBarns; ++Attempt)
	{
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::Sqrt(Rng.FRandRange(InnerR * InnerR, OuterR * OuterR));
		const float Setback = Rng.FRandRange(600.f, 1200.f);
		const float YawJitter = Rng.FRandRange(-4.f, 4.f);
		const FVector2D P = Origin + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		FVector2D RoadPoint, RoadDir;
		float HalfWidth = 0.f, RoadDist = 0.f;
		if (!FindNearestRoad(P, RoadPoint, RoadDir, HalfWidth, RoadDist))
		{
			LastReport.AddRejected(MTVillage::CatBarn, EMTPlacementRejection::OutOfBounds);
			continue;
		}
		const FVector2D Normal(-RoadDir.Y, RoadDir.X);
		const float Side = FVector2D::DotProduct(P - RoadPoint, Normal) >= 0.0 ? 1.f : -1.f;
		const FVector2D Center = RoadPoint + Normal * Side * (HalfWidth + Setback + Half.X);
		const float Yaw = MTVillage::YawFromDir(-Normal * Side) + YawJitter;
		const FMTFootprint Footprint(Center, Half, Yaw, 300.f);
		const EMTPlacementRejection Reason = UMTPlacementValidator::CheckFootprint(Registry, Footprint, MTVillage::CatBarn,
			3000.f, RoadClearance);
		if (Reason != EMTPlacementRejection::None)
		{
			LastReport.AddRejected(MTVillage::CatBarn, Reason);
			continue;
		}
		float FloorZ = 0.f;
		const FString Label = FString::Printf(TEXT("Buena_Barn_%02d"), PlacedBarns.Num() + 1);
		if (!PlaceBuilding(MTVillage::CatBarn, Footprint, BarnHeight, Mesh, bBlockout, Label, FloorZ))
		{
			continue;
		}
		const FMTFootprint Door(Center + Footprint.AxisX() * (Half.X + DoorClearance * 0.5f),
			FVector2D(DoorClearance * 0.5f, 250.0), Yaw, 0.f);
		Registry.AddExclusion(FMTExclusionZone::MakeBox(TEXT("DoorClearance"), Door));
		PlacedBarns.Add(Footprint);
	}
}

void AMTVillageGenerator::PlaceFields(FRandomStream& Rng)
{
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < NumFields * 40 && Placed < NumFields; ++Attempt)
	{
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::Sqrt(Rng.FRandRange(FieldRingInner * FieldRingInner, FieldRingOuter * FieldRingOuter));
		const FVector2D Size(Rng.FRandRange(FieldSizeMin.X, FieldSizeMax.X), Rng.FRandRange(FieldSizeMin.Y, FieldSizeMax.Y));
		const float YawJitter = Rng.FRandRange(-4.f, 4.f);
		const FVector2D P = Origin + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		FVector2D RoadPoint, RoadDir;
		float HalfWidth = 0.f, RoadDist = 0.f;
		if (!FindNearestRoad(P, RoadPoint, RoadDir, HalfWidth, RoadDist))
		{
			LastReport.AddRejected(MTVillage::CatField, EMTPlacementRejection::OutOfBounds);
			continue;
		}
		const FVector2D Half = Size * 0.5;
		const FVector2D Normal(-RoadDir.Y, RoadDir.X);
		const float Side = FVector2D::DotProduct(P - RoadPoint, Normal) >= 0.0 ? 1.f : -1.f;
		const FVector2D Center = RoadPoint + Normal * Side * (HalfWidth + 400.f + Half.Y);
		const FMTFootprint Field(Center, Half, MTVillage::YawFromDir(RoadDir) + YawJitter, 300.f);

		EMTPlacementRejection Reason = UMTPlacementValidator::CheckFootprint(Registry, Field, MTVillage::CatField, 0.f, RoadClearance);
		if (Reason == EMTPlacementRejection::None)
		{
			const FMTGroundSample Sample = UMTPlacementValidator::SampleGroundIgnoring(GetWorld(), Field, IgnoreForTraces);
			if (!Sample.bValid)
			{
				Reason = EMTPlacementRejection::NoGround;
			}
			else if (!UMTPlacementValidator::PassesSlopeRule(Sample, FieldSlope))
			{
				Reason = EMTPlacementRejection::TooSteep;
			}
			else if (UMTPlacementValidator::IsBlockedByWorldGeometry(GetWorld(), Field, Sample.MinZ, 150.f, IgnoreForTraces))
			{
				Reason = EMTPlacementRejection::BlockedByWorldGeometry;
			}
		}
		if (Reason != EMTPlacementRejection::None)
		{
			LastReport.AddRejected(MTVillage::CatField, Reason);
			continue;
		}
		Registry.AddFootprint(Field, MTVillage::CatField);
		LastReport.AddPlaced(MTVillage::CatField);
		++Placed;
		// Corner order 0:+X+Y 1:-X+Y 2:-X-Y 3:+X-Y; edge e runs corner e -> e+1. The road lies on local -Y*Side.
		const int32 GateEdge = Side > 0.f ? 2 : 0;
		BuildFieldContents(Rng, Field, GateEdge);
	}
}

void AMTVillageGenerator::BuildFieldContents(FRandomStream& Rng, const FMTFootprint& Field, int32 GateEdge)
{
	// ---- wheat (HISM, no collision) ----
	bool bWheatBlockout = false;
	UStaticMesh* Wheat = ResolveMesh(WheatMesh, MTVillage::ConePath, TEXT("WheatMesh"), bWheatBlockout);
	UHierarchicalInstancedStaticMeshComponent* WheatHISM = GetOrMakeHISM(TEXT("Wheat"), Wheat, false, 15000.f);
	const FVector2D AX = Field.AxisX();
	const FVector2D AY = Field.AxisY();
	const float Inset = 100.f;
	int32 WheatCount = 0;
	if (WheatHISM)
	{
		for (float LX = -Field.HalfExtent.X + Inset; LX <= Field.HalfExtent.X - Inset; LX += WheatSpacing)
		{
			for (float LY = -Field.HalfExtent.Y + Inset; LY <= Field.HalfExtent.Y - Inset; LY += WheatSpacing)
			{
				const float JX = Rng.FRandRange(-0.3f, 0.3f) * WheatSpacing;
				const float JY = Rng.FRandRange(-0.3f, 0.3f) * WheatSpacing;
				const float Yaw = Rng.FRandRange(0.f, 360.f);
				const float S = Rng.FRandRange(0.85f, 1.15f);
				const FVector2D P = Field.Center + AX * (LX + JX) + AY * (LY + JY);
				float Z = 0.f;
				if (!UMTPlacementValidator::TraceGroundZ(GetWorld(), P, Z, IgnoreForTraces))
				{
					LastReport.AddRejected(MTVillage::CatWheat, EMTPlacementRejection::NoGround);
					continue;
				}
				const FVector Scale = bWheatBlockout ? FVector(0.25f * S, 0.25f * S, 1.0f * S) : FVector(S);
				const float ZOffset = bWheatBlockout ? 50.f * S - 5.f : -5.f; // cone pivot is centred; sink 5 cm
				WheatHISM->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, Z + ZOffset), Scale), true);
				++WheatCount;
			}
		}
	}
	LastReport.AddPlaced(MTVillage::CatWheat, WheatCount);

	// ---- fences: posts snapped per segment, last post of each segment skipped (next segment/edge owns it) ----
	bool bPanelBlockout = false, bPostBlockout = false;
	UStaticMesh* Panel = ResolveMesh(FencePanelMesh, MTVillage::CubePath, TEXT("FencePanelMesh"), bPanelBlockout);
	UStaticMesh* Post = ResolveMesh(FencePostMesh, MTVillage::CylinderPath, TEXT("FencePostMesh"), bPostBlockout);
	UHierarchicalInstancedStaticMeshComponent* PanelHISM = GetOrMakeHISM(TEXT("FencePanels"), Panel, true, 20000.f);
	UHierarchicalInstancedStaticMeshComponent* PostHISM = GetOrMakeHISM(TEXT("FencePosts"), Post, true, 20000.f);
	if (!PanelHISM || !PostHISM)
	{
		return;
	}
	const float PanelMeshLength = bPanelBlockout ? 100.f : FMath::Max(float(Panel->GetBoundingBox().GetSize().X), 1.f);
	const TArray<FVector2D> Corners = UMTPlacementValidator::GetFootprintCorners(Field, false);

	auto SegmentAllowed = [this](const FVector2D& Mid) -> bool
	{
		for (const FMTExclusionZone& Zone : Registry.Exclusions)
		{
			if (UMTPlacementValidator::CircleOverlapsZone(Mid, 30.f, Zone)) { return false; }
		}
		for (const FMTPolyline2D& Road : Registry.Roads)
		{
			if (UMTPlacementValidator::DistancePointToPolyline2D(Mid, Road.Points) < Road.HalfWidth + 50.f) { return false; }
		}
		return true;
	};

	for (int32 Edge = 0; Edge < 4; ++Edge)
	{
		const FVector2D A = Corners[Edge];
		const FVector2D B = Corners[(Edge + 1) % 4];
		const float Length = float(FVector2D::Distance(A, B));
		if (Length < 1.f)
		{
			continue;
		}
		const FVector2D Dir = (B - A) / Length;
		const int32 NumSegments = FMath::Max(1, FMath::RoundToInt(Length / FenceSegmentLength));
		const float SegLen = Length / NumSegments;
		const float GateStart = Length * 0.5f - FenceGateWidth * 0.5f;
		const float GateEnd = Length * 0.5f + FenceGateWidth * 0.5f;

		auto IsSkipped = [&](int32 J) -> bool
		{
			if (J < 0 || J >= NumSegments) { return false; }
			const float S0 = J * SegLen;
			const float S1 = S0 + SegLen;
			const bool bGate = Edge == GateEdge && S1 > GateStart && S0 < GateEnd;
			return bGate || !SegmentAllowed(A + Dir * ((S0 + S1) * 0.5f));
		};

		for (int32 J = 0; J < NumSegments; ++J)
		{
			if (IsSkipped(J))
			{
				LastReport.AddRejected(MTVillage::CatFence, EMTPlacementRejection::InsideExclusionZone);
				continue;
			}
			const FVector2D P0 = A + Dir * (J * SegLen);
			const FVector2D P1 = A + Dir * ((J + 1) * SegLen);
			float Z0 = 0.f, Z1 = 0.f;
			if (!UMTPlacementValidator::TraceGroundZ(GetWorld(), P0, Z0, IgnoreForTraces) ||
				!UMTPlacementValidator::TraceGroundZ(GetWorld(), P1, Z1, IgnoreForTraces))
			{
				LastReport.AddRejected(MTVillage::CatFence, EMTPlacementRejection::NoGround);
				continue;
			}
			// Start post only; the end post is the next segment's start (or the next edge's corner post).
			auto AddPost = [&](const FVector2D& PP, float PZ)
			{
				const FTransform T = bPostBlockout
					? FTransform(FRotator::ZeroRotator, FVector(PP.X, PP.Y, PZ + 60.f - 15.f), FVector(0.12f, 0.12f, 1.2f))
					: FTransform(FRotator::ZeroRotator, FVector(PP.X, PP.Y, PZ - 10.f));
				PostHISM->AddInstance(T, true);
			};
			AddPost(P0, Z0);
			const bool bNextOpen = (J + 1 < NumSegments) && IsSkipped(J + 1);
			if (bNextOpen)
			{
				AddPost(P1, Z1); // close the fence at a gate / exclusion gap
			}
			const float Rise = Z1 - Z0;
			const float Sloped = FMath::Sqrt(SegLen * SegLen + Rise * Rise);
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, SegLen));
			const FRotator Rot(Pitch, MTVillage::YawFromDir(Dir), 0.f);
			const FVector2D Mid = (P0 + P1) * 0.5;
			const float MidZ = (Z0 + Z1) * 0.5f;
			if (bPanelBlockout)
			{
				// Two rails, 4 cm shorter than the post spacing so rail ends hide inside the posts.
				for (const float RailHeight : { 45.f, 95.f })
				{
					PanelHISM->AddInstance(FTransform(Rot, FVector(Mid.X, Mid.Y, MidZ + RailHeight),
						FVector((Sloped - 4.f) / 100.f, 0.05f, 0.12f)), true);
				}
			}
			else
			{
				PanelHISM->AddInstance(FTransform(Rot, FVector(Mid.X, Mid.Y, MidZ - 5.f),
					FVector(Sloped / PanelMeshLength, 1.f, 1.f)), true);
			}
			LastReport.AddPlaced(MTVillage::CatFence);
		}
	}
}

void AMTVillageGenerator::PlaceProps(FRandomStream& Rng)
{
	struct FPropKind { UStaticMesh* Mesh; bool bBlockout; float Radius; FVector BlockoutScale; };
	TArray<FPropKind> Kinds;
	for (int32 i = 0; i < PropMeshes.Num(); ++i)
	{
		bool bBlockout = false;
		UStaticMesh* Mesh = ResolveMesh(PropMeshes[i], MTVillage::CubePath, TEXT("PropMeshes"), bBlockout);
		if (Mesh && !bBlockout)
		{
			const FVector Ext = Mesh->GetBoundingBox().GetExtent();
			Kinds.Add({ Mesh, false, float(FMath::Max(Ext.X, Ext.Y)), FVector::OneVector });
		}
	}
	if (Kinds.Num() == 0)
	{
		bool bB1 = false, bB2 = false;
		UStaticMesh* Cube = ResolveMesh(TSoftObjectPtr<UStaticMesh>(), MTVillage::CubePath, TEXT("PropMeshes (crate/cart)"), bB1);
		UStaticMesh* Cyl = ResolveMesh(TSoftObjectPtr<UStaticMesh>(), MTVillage::CylinderPath, TEXT("PropMeshes (hay bale)"), bB2);
		Kinds.Add({ Cube, true, 75.f, FVector(1.0f, 1.0f, 0.8f) });  // crate
		Kinds.Add({ Cyl, true, 70.f, FVector(1.3f, 1.3f, 1.0f) });   // hay bale
		Kinds.Add({ Cube, true, 140.f, FVector(2.4f, 1.3f, 1.0f) }); // cart
	}

	for (const FPlacedHome& Home : PlacedHomes)
	{
		const FMTFootprint& F = Home.Footprint;
		const int32 Count = Rng.RandRange(1, FMath::Max(1, MaxPropsPerYard));
		for (int32 k = 0; k < Count; ++k)
		{
			bool bPlaced = false;
			EMTPlacementRejection LastReason = EMTPlacementRejection::None;
			for (int32 Try = 0; Try < 8 && !bPlaced; ++Try)
			{
				const FPropKind& Kind = Kinds[Rng.RandRange(0, Kinds.Num() - 1)];
				const float RelAngle = Rng.FRandRange(60.f, 300.f); // never the door side (front +/-60 deg)
				const float Dist = FMath::Max(F.HalfExtent.X, F.HalfExtent.Y) + Kind.Radius + Rng.FRandRange(100.f, YardRadius);
				const float Yaw = Rng.FRandRange(0.f, 360.f);
				const FVector2D P = F.Center + MTVillage::DirFromYaw(F.YawDegrees + RelAngle) * Dist;

				LastReason = UMTPlacementValidator::CheckCircle(Registry, P, Kind.Radius + 30.f, MTVillage::CatProp, 0.f, 100.f);
				if (LastReason != EMTPlacementRejection::None)
				{
					continue;
				}
				const FMTFootprint Box(P, FVector2D(Kind.Radius, Kind.Radius), 0.f, 0.f);
				const FMTGroundSample Sample = UMTPlacementValidator::SampleGroundIgnoring(GetWorld(), Box, IgnoreForTraces);
				if (!Sample.bValid) { LastReason = EMTPlacementRejection::NoGround; continue; }
				if (!UMTPlacementValidator::PassesSlopeRule(Sample, PropSlope)) { LastReason = EMTPlacementRejection::TooSteep; continue; }
				if (UMTPlacementValidator::IsBlockedByWorldGeometry(GetWorld(), Box, Sample.MinZ, 150.f, IgnoreForTraces))
				{
					LastReason = EMTPlacementRejection::BlockedByWorldGeometry;
					continue;
				}
				UHierarchicalInstancedStaticMeshComponent* HISM = GetOrMakeHISM(TEXT("Props"), Kind.Mesh, true, 12000.f);
				if (!HISM)
				{
					break;
				}
				const float Z = Kind.bBlockout ? Sample.MinZ + Kind.BlockoutScale.Z * 50.f - 3.f : Sample.MinZ - 3.f;
				HISM->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, Z), Kind.BlockoutScale), true);
				FMTCircleFootprint Circle;
				Circle.Center = P;
				Circle.Radius = Kind.Radius;
				Registry.AddCircle(Circle, MTVillage::CatProp);
				LastReport.AddPlaced(MTVillage::CatProp);
				bPlaced = true;
			}
			if (!bPlaced)
			{
				LastReport.AddRejected(MTVillage::CatProp, LastReason);
			}
		}
	}
}

void AMTVillageGenerator::PlaceForest(FRandomStream& Rng)
{
	const float Inner = FMath::Min(ForestInnerRadius, ForestOuterRadius);
	const float Outer = FMath::Max(ForestInnerRadius, ForestOuterRadius);
	const float R = FMath::Max(TreeMinDistance, 200.f);
	const float ArcMin = MTVillage::NormalizeAngle360(ForestArcDegrees.X);
	const float ArcMax = MTVillage::NormalizeAngle360(ForestArcDegrees.Y);
	auto InBand = [&](const FVector2D& Q) -> bool
	{
		const FVector2D D = Q - Origin;
		const double Dist = D.Size();
		if (Dist < Inner || Dist > Outer) { return false; }
		if (FMath::IsNearlyEqual(ArcMin, ArcMax)) { return true; }
		const float A = MTVillage::NormalizeAngle360(MTVillage::YawFromDir(D));
		return ArcMin <= ArcMax ? (A >= ArcMin && A <= ArcMax) : (A >= ArcMin || A <= ArcMax);
	};

	// Bridson Poisson-disc sampling on a grid covering the band's bounding square (deterministic via Rng).
	const float Cell = R / UE_SQRT_2;
	const int32 GridN = FMath::Max(1, FMath::CeilToInt(2.f * Outer / Cell));
	const FVector2D GridMin = Origin - FVector2D(Outer, Outer);
	TArray<int32> Grid;
	Grid.Init(INDEX_NONE, GridN * GridN);
	TArray<FVector2D> Samples;
	TArray<int32> Active;
	auto CellOf = [&](const FVector2D& Q, int32& CX, int32& CY)
	{
		CX = FMath::Clamp(int32((Q.X - GridMin.X) / Cell), 0, GridN - 1);
		CY = FMath::Clamp(int32((Q.Y - GridMin.Y) / Cell), 0, GridN - 1);
	};
	auto FarEnough = [&](const FVector2D& Q) -> bool
	{
		int32 CX, CY;
		CellOf(Q, CX, CY);
		for (int32 Y = FMath::Max(0, CY - 2); Y <= FMath::Min(GridN - 1, CY + 2); ++Y)
		{
			for (int32 X = FMath::Max(0, CX - 2); X <= FMath::Min(GridN - 1, CX + 2); ++X)
			{
				const int32 Idx = Grid[Y * GridN + X];
				if (Idx != INDEX_NONE && FVector2D::DistSquared(Samples[Idx], Q) < double(R) * R) { return false; }
			}
		}
		return true;
	};
	auto AddSample = [&](const FVector2D& Q)
	{
		int32 CX, CY;
		CellOf(Q, CX, CY);
		Grid[CY * GridN + CX] = Samples.Num();
		Active.Add(Samples.Num());
		Samples.Add(Q);
	};
	for (int32 Try = 0; Try < 200 && Samples.Num() == 0; ++Try)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float D = Rng.FRandRange(Inner, Outer);
		const FVector2D Q = Origin + FVector2D(FMath::Cos(A), FMath::Sin(A)) * D;
		if (InBand(Q)) { AddSample(Q); }
	}
	const int32 SampleCap = FMath::Max(MaxTrees, 0) * 3;
	while (Active.Num() > 0 && Samples.Num() < SampleCap)
	{
		const int32 ActiveIdx = Rng.RandRange(0, Active.Num() - 1);
		const FVector2D Base = Samples[Active[ActiveIdx]];
		bool bFound = false;
		for (int32 k = 0; k < 30; ++k)
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI);
			const float D = Rng.FRandRange(R, 2.f * R);
			const FVector2D Q = Base + FVector2D(FMath::Cos(A), FMath::Sin(A)) * D;
			if (InBand(Q) && FarEnough(Q))
			{
				AddSample(Q);
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Active.RemoveAtSwap(ActiveIdx);
		}
	}

	// Resolve meshes.
	TArray<UStaticMesh*> Custom;
	for (const TSoftObjectPtr<UStaticMesh>& Slot : TreeMeshes)
	{
		bool bBlockout = false;
		UStaticMesh* M = ResolveMesh(Slot, MTVillage::ConePath, TEXT("TreeMeshes"), bBlockout);
		if (M && !bBlockout) { Custom.Add(M); }
	}
	bool bB1 = false, bB2 = false;
	UStaticMesh* Trunk = Custom.Num() == 0 ? ResolveMesh(TSoftObjectPtr<UStaticMesh>(), MTVillage::CylinderPath, TEXT("TreeMeshes (trunk)"), bB1) : nullptr;
	UStaticMesh* Canopy = Custom.Num() == 0 ? ResolveMesh(TSoftObjectPtr<UStaticMesh>(), MTVillage::ConePath, TEXT("TreeMeshes (canopy)"), bB2) : nullptr;

	const float TrunkClearance = 250.f;
	int32 Placed = 0;
	for (const FVector2D& Q : Samples)
	{
		if (Placed >= MaxTrees)
		{
			break;
		}
		const int32 MeshPick = Custom.Num() > 0 ? Rng.RandRange(0, Custom.Num() - 1) : 0;
		const float S = Rng.FRandRange(0.8f, 1.3f);
		const float Yaw = Rng.FRandRange(0.f, 360.f);
		EMTPlacementRejection Reason = UMTPlacementValidator::CheckCircle(Registry, Q, TrunkClearance, MTVillage::CatTree, 0.f, 300.f);
		FMTGroundSample Sample;
		if (Reason == EMTPlacementRejection::None)
		{
			Sample = UMTPlacementValidator::SampleGroundIgnoring(GetWorld(), FMTFootprint(Q, FVector2D(50.0, 50.0), Yaw, 0.f), IgnoreForTraces);
			if (!Sample.bValid)
			{
				Reason = EMTPlacementRejection::NoGround;
			}
			else if (Sample.HeightDelta > 80.f)
			{
				Reason = EMTPlacementRejection::TooSteep;
			}
			else if (UMTPlacementValidator::IsBlockedByWorldGeometry(GetWorld(), FMTFootprint(Q, FVector2D(40.0, 40.0), Yaw, 0.f),
				Sample.MinZ, 400.f, IgnoreForTraces))
			{
				Reason = EMTPlacementRejection::BlockedByWorldGeometry;
			}
		}
		if (Reason != EMTPlacementRejection::None)
		{
			LastReport.AddRejected(MTVillage::CatTree, Reason);
			continue;
		}
		const float BaseZ = Sample.MinZ; // lowest trunk sample: roots never float on slopes
		if (Custom.Num() > 0)
		{
			if (UHierarchicalInstancedStaticMeshComponent* HISM = GetOrMakeHISM(TEXT("Trees"), Custom[MeshPick], true, 60000.f))
			{
				HISM->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Q.X, Q.Y, BaseZ - 10.f), FVector(S)), true);
			}
		}
		else
		{
			UHierarchicalInstancedStaticMeshComponent* TrunkHISM = GetOrMakeHISM(TEXT("TreeTrunks"), Trunk, true, 60000.f);
			UHierarchicalInstancedStaticMeshComponent* CanopyHISM = GetOrMakeHISM(TEXT("TreeCanopies"), Canopy, false, 60000.f);
			if (TrunkHISM)
			{
				TrunkHISM->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Q.X, Q.Y, BaseZ - 20.f + 200.f * S),
					FVector(0.4f * S, 0.4f * S, 4.0f * S)), true);
			}
			if (CanopyHISM)
			{
				CanopyHISM->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Q.X, Q.Y, BaseZ + 550.f * S),
					FVector(3.5f * S, 3.5f * S, 6.0f * S)), true);
			}
		}
		FMTCircleFootprint Circle;
		Circle.Center = Q;
		Circle.Radius = TrunkClearance * 0.5f;
		Registry.AddCircle(Circle, MTVillage::CatTree);
		LastReport.AddPlaced(MTVillage::CatTree);
		++Placed;
	}
}

bool AMTVillageGenerator::TryAddSpawnPoint(const FVector2D& XY, float Yaw, TArray<FTransform>& Out, FName Category)
{
	UWorld* World = GetWorld();
	// Ignore nothing: houses, props, fences and trees must all count as "geometry" here. Pawns are skipped inside.
	const TArray<const AActor*> NoIgnore;
	static const FVector2D Offsets[] = { FVector2D(0, 0), FVector2D(100, 0), FVector2D(-100, 0), FVector2D(0, 100),
		FVector2D(0, -100), FVector2D(150, 150), FVector2D(-150, -150) };
	for (const FVector2D& Off : Offsets)
	{
		const FVector2D P = XY + Off;
		float Z = 0.f;
		if (!UMTPlacementValidator::TraceGroundZ(World, P, Z, IgnoreForTraces))
		{
			continue;
		}
		if (UMTPlacementValidator::IsCapsuleBlocked(World, FVector(P.X, P.Y, Z), 42.f, 96.f, NoIgnore))
		{
			continue;
		}
		Out.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, Z + 2.f)));
		LastReport.AddPlaced(Category);
		return true;
	}
	LastReport.AddRejected(Category, EMTPlacementRejection::BlockedByWorldGeometry);
	return false;
}

void AMTVillageGenerator::PlaceSpawnPoints()
{
	NPCSpawnPoints.Reset();
	QuestPoints.Reset();
	for (const FPlacedHome& Home : PlacedHomes)
	{
		const FMTFootprint& F = Home.Footprint;
		// Inside the (kept-free) door clearance, facing away from the door.
		const FVector2D P = F.Center + F.AxisX() * (F.HalfExtent.X + DoorClearance * 0.5f);
		TryAddSpawnPoint(P, F.YawDegrees, NPCSpawnPoints, MTVillage::CatSpawn);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const float A = 45.f + 90.f * i;
		const FVector2D P = Origin + MTVillage::DirFromYaw(A) * (SquareRadius * 0.55f);
		TryAddSpawnPoint(P, A + 180.f, NPCSpawnPoints, MTVillage::CatSpawn);
	}
	TryAddSpawnPoint(Origin + FVector2D(350.0, 0.0), 180.f, QuestPoints, MTVillage::CatQuest); // at the well
	for (const FMTFootprint& Barn : PlacedBarns)
	{
		TryAddSpawnPoint(Barn.Center + Barn.AxisX() * (Barn.HalfExtent.X + DoorClearance * 0.5f), Barn.YawDegrees,
			QuestPoints, MTVillage::CatQuest);
	}
}

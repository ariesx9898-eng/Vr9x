#include "World/MTWorldBuildLibrary.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "LandscapeSubsystem.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/Package.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMTWorldBuild, Log, All);

namespace MTWorldBuild
{
	UWorld* EditorWorld()
	{
#if WITH_EDITOR
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
#else
		return nullptr;
#endif
	}

	/** Sets an integer or floating point property by name (reflection: the partition classes keep these protected). */
	bool SetNumber(UStruct* Type, void* Container, const TCHAR* Name, double Value)
	{
		FNumericProperty* Prop = Type ? CastField<FNumericProperty>(Type->FindPropertyByName(Name)) : nullptr;
		if (!Prop || !Container)
		{
			return false;
		}
		void* Ptr = Prop->ContainerPtrToValuePtr<void>(Container);
		if (Prop->IsFloatingPoint())
		{
			Prop->SetFloatingPointPropertyValue(Ptr, Value);
		}
		else
		{
			Prop->SetIntPropertyValue(Ptr, static_cast<int64>(Value));
		}
		return true;
	}

	/** Heightmap cache for GetTerrainHeight. */
	TArray<uint16> QueryHeights;
	int32 QuerySizeX = 0;
	int32 QuerySizeY = 0;
	FVector QueryLocation = FVector::ZeroVector;
	FVector QueryScale = FVector::OneVector;

	bool LoadRawHeights(const FString& File, int32 SizeX, int32 SizeY, TArray<uint16>& Out)
	{
		TArray<uint8> Raw;
		if (!FFileHelper::LoadFileToArray(Raw, *File) || Raw.Num() != SizeX * SizeY * 2)
		{
			UE_LOG(LogMTWorldBuild, Error, TEXT("Heightmap %s: missing or not %d x %d uint16 (%d bytes)"), *File, SizeX, SizeY, Raw.Num());
			return false;
		}
		Out.SetNumUninitialized(SizeX * SizeY);
		const uint8* Src = Raw.GetData();
		for (int32 i = 0; i < Out.Num(); ++i)
		{
			Out[i] = static_cast<uint16>(Src[2 * i] | (Src[2 * i + 1] << 8)); // little endian on disk
		}
		return true;
	}

	/** One tag / label per spawned group; instances in world space. */
	AActor* SpawnGroup(UWorld* World, const FString& Label, const TArray<UStaticMesh*>& Meshes, const TArray<TArray<FTransform>>& PerMesh,
		float CullStart, float CullEnd, bool bCollision, bool bSpatiallyLoaded, bool bCastShadow, const TArray<FName>& Tags, UHLODLayer* HLODLayer)
	{
#if WITH_EDITOR
		FBox Bounds(ForceInit);
		for (const TArray<FTransform>& Set : PerMesh)
		{
			for (const FTransform& T : Set)
			{
				Bounds += T.GetLocation();
			}
		}
		if (!Bounds.IsValid)
		{
			return nullptr;
		}
		const FVector Origin(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);

		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Origin));
		if (!Actor)
		{
			return nullptr;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"), RF_Transactional);
		Root->SetMobility(EComponentMobility::Static);
		Actor->SetRootComponent(Root);
		Actor->AddInstanceComponent(Root);
		Root->RegisterComponent();

		for (int32 MeshIndex = 0; MeshIndex < Meshes.Num(); ++MeshIndex)
		{
			if (!Meshes[MeshIndex] || !PerMesh.IsValidIndex(MeshIndex) || PerMesh[MeshIndex].IsEmpty())
			{
				continue;
			}
			const FName CompName = MakeUniqueObjectName(Actor, UHierarchicalInstancedStaticMeshComponent::StaticClass(),
				*FString::Printf(TEXT("HISM_%s"), *Meshes[MeshIndex]->GetName()));
			UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor, CompName, RF_Transactional);
			HISM->SetMobility(EComponentMobility::Static);
			HISM->SetStaticMesh(Meshes[MeshIndex]);
			HISM->SetupAttachment(Root);
			HISM->SetCullDistances(FMath::RoundToInt(CullStart), FMath::RoundToInt(CullEnd));
			HISM->SetCastShadow(bCastShadow);
			HISM->bAffectDistanceFieldLighting = bCastShadow;
			if (bCollision)
			{
				HISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
				HISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
			else
			{
				HISM->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			HISM->SetCanEverAffectNavigation(false);
			Actor->AddInstanceComponent(HISM);
			HISM->RegisterComponent();
			HISM->AddInstances(PerMesh[MeshIndex], /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true, /*bUpdateNavigation=*/false);
			HISM->BuildTreeIfOutdated(/*Async=*/false, /*ForceUpdate=*/true);
		}

		Actor->SetActorLabel(Label);
		Actor->SetIsSpatiallyLoaded(bSpatiallyLoaded);
		Actor->Tags.Append(Tags);
		if (HLODLayer)
		{
			Actor->SetHLODLayer(HLODLayer);
		}
		Actor->MarkPackageDirty();
		return Actor;
#else
		return nullptr;
#endif
	}
}

ULandscapeLayerInfoObject* UMTWorldBuildLibrary::CreateLandscapeLayerInfo(const FString& PackageFolder, FName LayerName, FLinearColor DebugColor)
{
#if WITH_EDITOR
	const FString AssetName = FString::Printf(TEXT("LI_%s"), *LayerName.ToString());
	const FString PackageName = PackageFolder / AssetName;
	if (FPackageName::DoesPackageExist(PackageName))
	{
		if (ULandscapeLayerInfoObject* Existing = LoadObject<ULandscapeLayerInfoObject>(nullptr, *(PackageName + TEXT(".") + AssetName)))
		{
			return Existing;
		}
	}
	UPackage* Package = CreatePackage(*PackageName);
	ULandscapeLayerInfoObject* Info = NewObject<ULandscapeLayerInfoObject>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Info->SetLayerName(LayerName, /*bInModify=*/false);
	Info->SetBlendMethod(ELandscapeTargetLayerBlendMethod::FinalWeightBlending, /*bInModify=*/false);
	Info->SetLayerUsageDebugColor(DebugColor, /*bInModify=*/false, EPropertyChangeType::ValueSet);
	FAssetRegistryModule::AssetCreated(Info);
	Package->MarkPackageDirty();
	return Info;
#else
	return nullptr;
#endif
}

ALandscape* UMTWorldBuildLibrary::ImportLandscape(const FString& HeightmapFile, int32 VerticesX, int32 VerticesY, int32 SectionsPerComponent,
	int32 QuadsPerSection, FVector Location, FVector Scale, UMaterialInterface* Material, const TArray<ULandscapeLayerInfoObject*>& LayerInfos,
	const TArray<FString>& LayerFiles, int32 StreamingGridSize, bool bSpatiallyLoaded, const FString& ActorLabel)
{
#if WITH_EDITOR
	UWorld* World = MTWorldBuild::EditorWorld();
	const int32 QuadsPerComponent = SectionsPerComponent * QuadsPerSection;
	if (!World || QuadsPerComponent <= 0 || (VerticesX - 1) % QuadsPerComponent != 0 || (VerticesY - 1) % QuadsPerComponent != 0
		|| LayerInfos.Num() != LayerFiles.Num())
	{
		UE_LOG(LogMTWorldBuild, Error, TEXT("ImportLandscape: bad arguments (world %d, %d x %d vertices, %d quads per component, %d layer infos, %d files)"),
			World != nullptr, VerticesX, VerticesY, QuadsPerComponent, LayerInfos.Num(), LayerFiles.Num());
		return nullptr;
	}

	TArray<uint16> Heights;
	if (!MTWorldBuild::LoadRawHeights(HeightmapFile, VerticesX, VerticesY, Heights))
	{
		return nullptr;
	}

	TArray<FLandscapeImportLayerInfo> ImportLayers;
	for (int32 i = 0; i < LayerInfos.Num(); ++i)
	{
		ULandscapeLayerInfoObject* Info = LayerInfos[i];
		FImage Image;
		if (!Info || !FImageUtils::LoadImage(*LayerFiles[i], Image) || Image.SizeX != VerticesX || Image.SizeY != VerticesY)
		{
			UE_LOG(LogMTWorldBuild, Error, TEXT("ImportLandscape: weightmap %s missing, unreadable or not %d x %d"), *LayerFiles[i], VerticesX, VerticesY);
			return nullptr;
		}
		// Weights are raw 0..255 values: treat any source as linear so no gamma curve is applied.
		Image.GammaSpace = EGammaSpace::Linear;
		if (Image.Format != ERawImageFormat::G8)
		{
			Image.ChangeFormat(ERawImageFormat::G8, EGammaSpace::Linear);
		}
		FLandscapeImportLayerInfo& Layer = ImportLayers.Emplace_GetRef(Info->GetLayerName());
		Layer.LayerInfo = Info;
		Layer.SourceFilePath = LayerFiles[i];
		Layer.LayerData.SetNumUninitialized(VerticesX * VerticesY);
		FMemory::Memcpy(Layer.LayerData.GetData(), Image.RawData.GetData(), Layer.LayerData.Num());
	}

	ALandscape* Landscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);
	if (!Landscape)
	{
		UE_LOG(LogMTWorldBuild, Error, TEXT("ImportLandscape: could not spawn the landscape actor"));
		return nullptr;
	}
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->bAreNewLandscapeActorsSpatiallyLoaded = bSpatiallyLoaded;

	TMap<FGuid, TArray<uint16>> HeightData;
	HeightData.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayerData;
	LayerData.Add(FGuid(), MoveTemp(ImportLayers));
	Landscape->Import(FGuid::NewGuid(), 0, 0, VerticesX - 1, VerticesY - 1, SectionsPerComponent, QuadsPerSection, HeightData, *HeightmapFile,
		LayerData, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		UE_LOG(LogMTWorldBuild, Error, TEXT("ImportLandscape: no landscape info after import"));
		return nullptr;
	}
	Landscape->SetActorLabel(ActorLabel.IsEmpty() ? FString(TEXT("Landscape")) : ActorLabel);
	// Layers with no weight anywhere were skipped by Import; they still need target layers so they can be painted.
	for (ULandscapeLayerInfoObject* LayerInfo : LayerInfos)
	{
		if (!Landscape->HasTargetLayer(LayerInfo->GetLayerName()))
		{
			Landscape->AddTargetLayer(LayerInfo->GetLayerName(), FLandscapeTargetLayerSettings(LayerInfo));
		}
	}
	Info->UpdateLayerInfoMap(Landscape);

	ULandscapeSubsystem* Subsystem = World->GetSubsystem<ULandscapeSubsystem>();
	if (Subsystem && Subsystem->IsGridBased() && StreamingGridSize > 0)
	{
		Subsystem->ChangeGridSize(Info, static_cast<uint32>(StreamingGridSize)); // also forces the edit layers merge
	}
	else if (FApp::CanEverRender())
	{
		Landscape->ForceLayersFullUpdate();
	}
	int32 Proxies = 0;
	Info->ForEachLandscapeProxy([&Proxies](ALandscapeProxy*) { ++Proxies; return true; });
	UE_LOG(LogMTWorldBuild, Display, TEXT("ImportLandscape: %d x %d vertices, %d components, %d actors (grid %d, spatially loaded %d), can render %d"),
		VerticesX, VerticesY, Info->XYtoComponentMap.Num(), Proxies, StreamingGridSize, bSpatiallyLoaded, FApp::CanEverRender());
	return Landscape;
#else
	return nullptr;
#endif
}

int32 UMTWorldBuildLibrary::SetLandscapeMaterial(UMaterialInterface* Material)
{
#if WITH_EDITOR
	UWorld* World = MTWorldBuild::EditorWorld();
	if (!World)
	{
		return 0;
	}
	int32 Count = 0;
	FProperty* MaterialProperty = ALandscapeProxy::StaticClass()->FindPropertyByName(TEXT("LandscapeMaterial"));
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		It->Modify();
		It->LandscapeMaterial = Material;
		FPropertyChangedEvent Event(MaterialProperty);
		It->PostEditChangeProperty(Event);
		++Count;
	}
	return Count;
#else
	return 0;
#endif
}

int32 UMTWorldBuildLibrary::ConfigureWorldPartitionGrid(int32 CellSizeCm, int32 LoadingRangeCm)
{
#if WITH_EDITOR
	UWorld* World = MTWorldBuild::EditorWorld();
	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	UWorldPartitionRuntimeHash* Hash = WorldPartition ? WorldPartition->RuntimeHash.Get() : nullptr;
	if (!Hash)
	{
		UE_LOG(LogMTWorldBuild, Warning, TEXT("ConfigureWorldPartitionGrid: no World Partition runtime hash"));
		return 0;
	}
	Hash->Modify();
	int32 Changed = 0;
	// Runtime hash set (UE 5.4+ default): RuntimePartitions[].MainLayer is a URuntimePartition (LHGrid has CellSize).
	// Legacy spatial hash: Grids[] of FSpatialHashRuntimeGrid {CellSize, LoadingRange}.
	for (const TCHAR* ArrayName : { TEXT("RuntimePartitions"), TEXT("Grids") })
	{
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Hash->GetClass()->FindPropertyByName(ArrayName));
		FStructProperty* Inner = ArrayProp ? CastField<FStructProperty>(ArrayProp->Inner) : nullptr;
		if (!Inner)
		{
			continue;
		}
		FScriptArrayHelper Array(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Hash));
		for (int32 i = 0; i < Array.Num(); ++i)
		{
			void* Element = Array.GetRawPtr(i);
			if (FObjectProperty* MainProp = CastField<FObjectProperty>(Inner->Struct->FindPropertyByName(TEXT("MainLayer"))))
			{
				if (UObject* Partition = MainProp->GetObjectPropertyValue_InContainer(Element))
				{
					Partition->Modify();
					const bool bCell = MTWorldBuild::SetNumber(Partition->GetClass(), Partition, TEXT("CellSize"), CellSizeCm);
					const bool bRange = MTWorldBuild::SetNumber(Partition->GetClass(), Partition, TEXT("LoadingRange"), LoadingRangeCm);
					Changed += (bCell || bRange) ? 1 : 0;
				}
			}
			else
			{
				const bool bCell = MTWorldBuild::SetNumber(Inner->Struct, Element, TEXT("CellSize"), CellSizeCm);
				const bool bRange = MTWorldBuild::SetNumber(Inner->Struct, Element, TEXT("LoadingRange"), LoadingRangeCm);
				Changed += (bCell || bRange) ? 1 : 0;
			}
		}
	}
	Hash->MarkPackageDirty();
	UE_LOG(LogMTWorldBuild, Display, TEXT("ConfigureWorldPartitionGrid: %s, %d partition(s) set to cell %d cm, loading range %d cm"),
		*Hash->GetClass()->GetName(), Changed, CellSizeCm, LoadingRangeCm);
	return Changed;
#else
	return 0;
#endif
}

int32 UMTWorldBuildLibrary::RegisterHLODLayers(const TArray<UHLODLayer*>& Layers, int32 CellSizeCm, int32 LoadingRangeCm)
{
#if WITH_EDITOR
	UWorld* World = MTWorldBuild::EditorWorld();
	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	UWorldPartitionRuntimeHash* Hash = WorldPartition ? WorldPartition->RuntimeHash.Get() : nullptr;
	FArrayProperty* ArrayProp = Hash ? CastField<FArrayProperty>(Hash->GetClass()->FindPropertyByName(TEXT("RuntimePartitions"))) : nullptr;
	UClass* GridClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.RuntimePartitionLHGrid"));
	if (!ArrayProp || !GridClass)
	{
		UE_LOG(LogMTWorldBuild, Warning, TEXT("RegisterHLODLayers: the world does not use a runtime hash set"));
		return 0;
	}
	FScriptArrayHelper Array(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Hash));
	if (Array.Num() == 0)
	{
		return 0;
	}
	Hash->Modify();
	FRuntimePartitionDesc& Desc = *reinterpret_cast<FRuntimePartitionDesc*>(Array.GetRawPtr(0));
	int32 Added = 0;
	for (UHLODLayer* Layer : Layers)
	{
		if (!Layer || Desc.HLODSetups.ContainsByPredicate([Layer](const FRuntimePartitionHLODSetup& Setup) { return Setup.HLODLayers.Contains(Layer); }))
		{
			continue;
		}
		FRuntimePartitionHLODSetup& Setup = Desc.HLODSetups.AddDefaulted_GetRef();
		Setup.Name = Layer->GetFName();
		Setup.HLODLayers = { Layer };
		// Always loaded: distant cities and forests stay visible across the whole world. (5.8 takes this from the
		// partition's setup; UHLODLayer::IsSpatiallyLoaded is deprecated.)
		Setup.bIsSpatiallyLoaded = false;
		UObject* Grid = NewObject<UObject>(Hash, GridClass, NAME_None, RF_Transactional);
		MTWorldBuild::SetNumber(GridClass, Grid, TEXT("CellSize"), CellSizeCm);
		MTWorldBuild::SetNumber(GridClass, Grid, TEXT("LoadingRange"), LoadingRangeCm);
		MTWorldBuild::SetNumber(GridClass, Grid, TEXT("HLODIndex"), 0);
		MTWorldBuild::SetNumber(GridClass, Grid, TEXT("Priority"), 0);
		if (FNameProperty* NameProp = CastField<FNameProperty>(GridClass->FindPropertyByName(TEXT("Name"))))
		{
			NameProp->SetPropertyValue_InContainer(Grid, Setup.Name);
		}
		if (FBoolProperty* ClientOnly = CastField<FBoolProperty>(GridClass->FindPropertyByName(TEXT("bClientOnlyVisible"))))
		{
			ClientOnly->SetPropertyValue_InContainer(Grid, true);
		}
		Setup.PartitionLayer = static_cast<URuntimePartition*>(Grid);
		++Added;
	}
	Hash->MarkPackageDirty();
	UE_LOG(LogMTWorldBuild, Display, TEXT("RegisterHLODLayers: %d added (%d setups on %s)"), Added, Desc.HLODSetups.Num(), *Desc.Name.ToString());
	return Added;
#else
	return 0;
#endif
}

bool UMTWorldBuildLibrary::LoadHeightmapForQueries(const FString& HeightmapFile, int32 VerticesX, int32 VerticesY, FVector Location, FVector Scale)
{
	if (!MTWorldBuild::LoadRawHeights(HeightmapFile, VerticesX, VerticesY, MTWorldBuild::QueryHeights))
	{
		return false;
	}
	MTWorldBuild::QuerySizeX = VerticesX;
	MTWorldBuild::QuerySizeY = VerticesY;
	MTWorldBuild::QueryLocation = Location;
	MTWorldBuild::QueryScale = Scale;
	return true;
}

float UMTWorldBuildLibrary::GetTerrainHeight(float X, float Y)
{
	using namespace MTWorldBuild;
	if (QueryHeights.IsEmpty())
	{
		return 0.f;
	}
	const float U = FMath::Clamp((X - QueryLocation.X) / QueryScale.X, 0.f, QuerySizeX - 1.001f);
	const float V = FMath::Clamp((Y - QueryLocation.Y) / QueryScale.Y, 0.f, QuerySizeY - 1.001f);
	const int32 X0 = FMath::FloorToInt(U);
	const int32 Y0 = FMath::FloorToInt(V);
	const float FX = U - X0;
	const float FY = V - Y0;
	auto H = [](int32 IX, int32 IY) { return static_cast<float>(QueryHeights[IY * QuerySizeX + IX]); };
	const float Top = FMath::Lerp(H(X0, Y0), H(X0 + 1, Y0), FX);
	const float Bottom = FMath::Lerp(H(X0, Y0 + 1), H(X0 + 1, Y0 + 1), FX);
	return QueryLocation.Z + (FMath::Lerp(Top, Bottom, FY) - 32768.f) * QueryScale.Z / 128.f;
}

AActor* UMTWorldBuildLibrary::SpawnInstancedGroup(const FString& Label, const TArray<UStaticMesh*>& Meshes, const TArray<int32>& MeshIndices,
	const TArray<float>& Transforms, float CullStart, float CullEnd, bool bCollision, bool bSpatiallyLoaded, bool bCastShadow,
	const TArray<FName>& Tags, FName HLODLayerPath)
{
	UWorld* World = MTWorldBuild::EditorWorld();
	if (!World || Transforms.Num() != MeshIndices.Num() * 7)
	{
		UE_LOG(LogMTWorldBuild, Error, TEXT("SpawnInstancedGroup %s: %d transform floats for %d instances (7 each expected)"), *Label, Transforms.Num(), MeshIndices.Num());
		return nullptr;
	}
	TArray<TArray<FTransform>> PerMesh;
	PerMesh.SetNum(Meshes.Num());
	for (int32 i = 0; i < MeshIndices.Num(); ++i)
	{
		if (!PerMesh.IsValidIndex(MeshIndices[i]))
		{
			continue;
		}
		const float* T = &Transforms[i * 7];
		PerMesh[MeshIndices[i]].Emplace(FRotator(T[5], T[3], T[6]), FVector(T[0], T[1], T[2]), FVector(T[4]));
	}
	UHLODLayer* HLODLayer = HLODLayerPath.IsNone() ? nullptr : LoadObject<UHLODLayer>(nullptr, *HLODLayerPath.ToString());
	return MTWorldBuild::SpawnGroup(World, Label, Meshes, PerMesh, CullStart, CullEnd, bCollision, bSpatiallyLoaded, bCastShadow, Tags, HLODLayer);
}

int32 UMTWorldBuildLibrary::SpawnInstanceSetFromFile(const FString& JsonFile, const FString& BinFile, FName SetTag)
{
	UWorld* World = MTWorldBuild::EditorWorld();
	FString JsonText;
	TArray<uint8> Bin;
	TSharedPtr<FJsonObject> Root;
	if (!World || !FFileHelper::LoadFileToString(JsonText, *JsonFile) || !FFileHelper::LoadFileToArray(Bin, *BinFile)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Root) || !Root.IsValid() || Bin.Num() % (8 * sizeof(float)) != 0)
	{
		UE_LOG(LogMTWorldBuild, Error, TEXT("SpawnInstanceSetFromFile: cannot read %s / %s"), *JsonFile, *BinFile);
		return -1;
	}
	const float* Records = reinterpret_cast<const float*>(Bin.GetData()); // little endian float32 (host order on Mac / x64)
	const int32 NumRecords = Bin.Num() / (8 * sizeof(float));

	TArray<UStaticMesh*> Meshes;
	for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("Meshes")))
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Value->AsString());
		if (!Mesh)
		{
			UE_LOG(LogMTWorldBuild, Warning, TEXT("SpawnInstanceSetFromFile: missing mesh %s (its instances are skipped)"), *Value->AsString());
		}
		Meshes.Add(Mesh);
	}

	TMap<FString, UHLODLayer*> HLODLayers;
	int32 Total = 0;
	for (const TSharedPtr<FJsonValue>& GroupValue : Root->GetArrayField(TEXT("Groups")))
	{
		const TSharedPtr<FJsonObject>& Group = GroupValue->AsObject();
		const int32 Offset = static_cast<int32>(Group->GetNumberField(TEXT("Offset")));
		const int32 Count = static_cast<int32>(Group->GetNumberField(TEXT("Count")));
		if (Offset < 0 || Count <= 0 || Offset + Count > NumRecords)
		{
			continue;
		}
		TArray<TArray<FTransform>> PerMesh;
		PerMesh.SetNum(Meshes.Num());
		for (int32 i = Offset; i < Offset + Count; ++i)
		{
			const float* R = Records + i * 8;
			const int32 MeshIndex = static_cast<int32>(R[0]);
			if (Meshes.IsValidIndex(MeshIndex) && Meshes[MeshIndex])
			{
				PerMesh[MeshIndex].Emplace(FRotator(R[5], R[4], R[6]), FVector(R[1], R[2], R[3]), FVector(R[7]));
				++Total;
			}
		}
		TArray<FName> Tags;
		Tags.Add(SetTag);
		const TArray<TSharedPtr<FJsonValue>>* TagValues = nullptr;
		if (Group->TryGetArrayField(TEXT("Tags"), TagValues))
		{
			for (const TSharedPtr<FJsonValue>& Tag : *TagValues)
			{
				Tags.Add(FName(*Tag->AsString()));
			}
		}
		UHLODLayer* HLODLayer = nullptr;
		FString HLODPath;
		if (Group->TryGetStringField(TEXT("HLODLayer"), HLODPath) && !HLODPath.IsEmpty())
		{
			UHLODLayer*& Cached = HLODLayers.FindOrAdd(HLODPath);
			if (!Cached)
			{
				Cached = LoadObject<UHLODLayer>(nullptr, *HLODPath);
			}
			HLODLayer = Cached;
		}
		bool bCollision = true, bSpatiallyLoaded = true, bCastShadow = true;
		double CullStart = 0.0, CullEnd = 0.0;
		Group->TryGetBoolField(TEXT("Collision"), bCollision);
		Group->TryGetBoolField(TEXT("SpatiallyLoaded"), bSpatiallyLoaded);
		Group->TryGetBoolField(TEXT("CastShadow"), bCastShadow);
		Group->TryGetNumberField(TEXT("CullStart"), CullStart);
		Group->TryGetNumberField(TEXT("CullEnd"), CullEnd);
		MTWorldBuild::SpawnGroup(World, Group->GetStringField(TEXT("Label")), Meshes, PerMesh, static_cast<float>(CullStart), static_cast<float>(CullEnd),
			bCollision, bSpatiallyLoaded, bCastShadow, Tags, HLODLayer);
	}
	UE_LOG(LogMTWorldBuild, Display, TEXT("SpawnInstanceSetFromFile %s: %d instances, %d meshes"), *FPaths::GetBaseFilename(JsonFile), Total, Meshes.Num());
	return Total;
}

UStaticMesh* UMTWorldBuildLibrary::CreateStaticMeshAsset(const FString& PackagePath, const TArray<FVector>& Positions, const TArray<int32>& Triangles,
	const TArray<FVector2D>& UVs, UMaterialInterface* Material, bool bComputeNormals, bool bCollision)
{
#if WITH_EDITOR
	if (Positions.Num() < 3 || Triangles.Num() < 3 || Triangles.Num() % 3 != 0 || (UVs.Num() != 0 && UVs.Num() != Positions.Num()))
	{
		UE_LOG(LogMTWorldBuild, Error, TEXT("CreateStaticMeshAsset %s: bad geometry (%d positions, %d indices, %d uvs)"), *PackagePath,
			Positions.Num(), Triangles.Num(), UVs.Num());
		return nullptr;
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	UPackage* Package = CreatePackage(*PackagePath);
	Package->FullyLoad();
	UStaticMesh* Mesh = FindObject<UStaticMesh>(Package, *AssetName);
	if (Mesh)
	{
		Mesh->Modify();
		Mesh->GetStaticMaterials().Reset();
		Mesh->SetNumSourceModels(0);
	}
	else
	{
		Mesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Mesh);
	}

	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> TexCoords = Attributes.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	TexCoords.SetNumChannels(1);

	Description.ReserveNewVertices(Positions.Num());
	Description.ReserveNewVertexInstances(Triangles.Num());
	Description.ReserveNewTriangles(Triangles.Num() / 3);
	TArray<FVertexID> VertexIds;
	VertexIds.Reserve(Positions.Num());
	for (const FVector& P : Positions)
	{
		const FVertexID Id = Description.CreateVertex();
		VertexPositions[Id] = FVector3f(P);
		VertexIds.Add(Id);
	}
	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	SlotNames[Group] = TEXT("Surface");
	for (int32 i = 0; i + 2 < Triangles.Num(); i += 3)
	{
		TArray<FVertexInstanceID, TFixedAllocator<3>> Corners;
		for (int32 c = 0; c < 3; ++c)
		{
			const int32 Index = Triangles[i + c];
			if (!VertexIds.IsValidIndex(Index))
			{
				continue;
			}
			const FVertexInstanceID Inst = Description.CreateVertexInstance(VertexIds[Index]);
			Normals[Inst] = FVector3f::UpVector;
			Tangents[Inst] = FVector3f::ForwardVector;
			BinormalSigns[Inst] = 1.f;
			TexCoords.Set(Inst, 0, UVs.Num() ? FVector2f(UVs[Index]) : FVector2f(Positions[Index].X / 100.f, Positions[Index].Y / 100.f));
			Corners.Add(Inst);
		}
		if (Corners.Num() == 3)
		{
			Description.CreateTriangle(Group, Corners);
		}
	}

	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Surface"), TEXT("Surface")));
	FStaticMeshSourceModel& Source = Mesh->AddSourceModel();
	Source.BuildSettings.bRecomputeNormals = bComputeNormals;
	Source.BuildSettings.bRecomputeTangents = true;
	Source.BuildSettings.bGenerateLightmapUVs = false;
	Source.BuildSettings.bRemoveDegenerates = true;
	Source.BuildSettings.bUseMikkTSpace = true;
	Mesh->CreateMeshDescription(0, MoveTemp(Description));
	Mesh->CommitMeshDescription(0);
	Mesh->SetLightMapCoordinateIndex(0);
	Mesh->CreateBodySetup();
	if (UBodySetup* Body = Mesh->GetBodySetup())
	{
		Body->CollisionTraceFlag = bCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;
		Body->DefaultInstance.SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	}
	Mesh->Build(/*bInSilent=*/true);
	Mesh->PostEditChange();
	Package->MarkPackageDirty();
	return Mesh;
#else
	return nullptr;
#endif
}

bool UMTWorldBuildLibrary::SetTrunkCollision(UStaticMesh* Mesh, float Radius, float Height)
{
#if WITH_EDITOR
	if (!Mesh || Radius <= 0.f || Height <= 2.f * Radius)
	{
		return false;
	}
	Mesh->Modify();
	Mesh->CreateBodySetup();
	UBodySetup* Body = Mesh->GetBodySetup();
	Body->Modify();
	Body->RemoveSimpleCollision();
	FKSphylElem Capsule(Radius, Height - 2.f * Radius);
	Capsule.Center = FVector(0.f, 0.f, Height * 0.5f);
	Body->AggGeom.SphylElems.Add(Capsule);
	Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Body->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Body->InvalidatePhysicsData();
	Body->CreatePhysicsMeshes();
	Mesh->SetCustomizedCollision(true);
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UMTWorldBuildLibrary::ConfigureKitMesh(UStaticMesh* Mesh, const TMap<FName, UMaterialInterface*>& SlotMaterials, bool bNanite, bool bPreserveArea)
{
#if WITH_EDITOR
	if (!Mesh)
	{
		return false;
	}
	Mesh->Modify();
	for (FStaticMaterial& Slot : Mesh->GetStaticMaterials())
	{
		if (UMaterialInterface* const* Found = SlotMaterials.Find(Slot.MaterialSlotName))
		{
			if (*Found)
			{
				Slot.MaterialInterface = *Found;
			}
		}
	}
	FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings();
	Nanite.bEnabled = bNanite;
	Nanite.ShapePreservation = bPreserveArea ? ENaniteShapePreservation::PreserveArea : ENaniteShapePreservation::None;
	Mesh->SetNaniteSettings(Nanite);
	Mesh->PostEditChange(); // one rebuild for everything above
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UMTWorldBuildLibrary::SetComplexCollision(UStaticMesh* Mesh, bool bEnable)
{
#if WITH_EDITOR
	if (!Mesh)
	{
		return false;
	}
	Mesh->Modify();
	Mesh->CreateBodySetup();
	UBodySetup* Body = Mesh->GetBodySetup();
	Body->Modify();
	Body->RemoveSimpleCollision();
	Body->CollisionTraceFlag = bEnable ? CTF_UseComplexAsSimple : CTF_UseDefault;
	Body->DefaultInstance.SetCollisionProfileName(bEnable ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Body->InvalidatePhysicsData();
	Body->CreatePhysicsMeshes();
	Mesh->SetCustomizedCollision(true);
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

TArray<FString> UMTWorldBuildLibrary::GetMaterialCompileErrors(UMaterialInterface* Material)
{
	TArray<FString> Errors;
	UMaterial* Base = Material ? Material->GetMaterial() : nullptr;
	if (!Base)
	{
		Errors.Add(TEXT("no material"));
		return Errors;
	}
	if (FMaterialResource* Resource = Base->GetMaterialResource(GMaxRHIShaderPlatform))
	{
		Resource->FinishCompilation();
		Errors = Resource->GetCompileErrors();
	}
	return Errors;
}

int32 UMTWorldBuildLibrary::DestroyActorsWithTag(FName Tag)
{
	int32 Count = 0;
#if WITH_EDITOR
	if (UWorld* World = MTWorldBuild::EditorWorld())
	{
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag))
			{
				ToDestroy.Add(*It);
			}
		}
		for (AActor* Actor : ToDestroy)
		{
			Count += World->EditorDestroyActor(Actor, /*bShouldModifyLevel=*/true) ? 1 : 0;
		}
	}
#endif
	return Count;
}

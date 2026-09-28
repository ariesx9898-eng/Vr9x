#include "World/MTValidateWorldCommandlet.h"

#include "Core/MTTypes.h"

#if WITH_EDITOR
#include "World/MTWorldValidationLibrary.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#endif // WITH_EDITOR

UMTValidateWorldCommandlet::UMTValidateWorldCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UMTValidateWorldCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogMushoku, Error, TEXT("MTValidateWorld requires an editor build (UnrealEditor-Cmd)."));
	return 2;
#else
	FString MapPath;
	if (!FParse::Value(*Params, TEXT("map="), MapPath) || MapPath.IsEmpty())
	{
		MapPath = TEXT("/Game/Maps/L_Fittoa");
	}
	UE_LOG(LogMushoku, Display, TEXT("MTValidateWorld: loading %s"), *MapPath);

	UPackage* Package = LoadPackage(nullptr, *MapPath, LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	if (!World)
	{
		UE_LOG(LogMushoku, Error, TEXT("MTValidateWorld: could not load map %s"), *MapPath);
		return 2;
	}

	World->WorldType = EWorldType::Editor;
	World->AddToRoot();
	if (!World->bIsWorldInitialized)
	{
		UWorld::InitializationValues IVS;
		IVS.RequiresHitProxies(false);
		IVS.ShouldSimulatePhysics(false);
		IVS.EnableTraceCollision(true);
		IVS.CreateNavigation(false);
		IVS.CreateAISystem(false);
		IVS.AllowAudioPlayback(false);
		IVS.CreatePhysicsScene(true);
		World->InitWorld(IVS);
	}
	World->PersistentLevel->UpdateModelComponents();
	World->UpdateWorldComponents(true, false);

	// World Partition: actors are external and unloaded by default -> load everything for the scan.
	FLoaderAdapterShape* Loader = nullptr;
	if (UWorldPartition* WorldPartition = World->GetWorldPartition())
	{
		if (!WorldPartition->IsInitialized())
		{
			WorldPartition->Initialize(World, FTransform::Identity);
		}
		const FBox Everything(FVector(-2.0e7), FVector(2.0e7)); // +-200 km: every actor of the map
		Loader = new FLoaderAdapterShape(World, Everything, TEXT("MTValidateWorld"));
		Loader->Load();
		World->UpdateWorldComponents(true, false);
	}

	const TArray<FMTValidationIssue> Issues = UMTWorldValidationLibrary::RunAllChecksForWorld(World);
	const int32 Errors = UMTWorldValidationLibrary::CountIssuesOfSeverity(Issues, EMTValidationSeverity::Error);
	const int32 Warnings = UMTWorldValidationLibrary::CountIssuesOfSeverity(Issues, EMTValidationSeverity::Warning);
	for (const FMTValidationIssue& Issue : Issues)
	{
		if (Issue.Severity == EMTValidationSeverity::Error)
		{
			UE_LOG(LogMushoku, Error, TEXT("[%s] %s"), *Issue.Category.ToString(), *Issue.Message);
		}
		else
		{
			UE_LOG(LogMushoku, Warning, TEXT("[%s] %s"), *Issue.Category.ToString(), *Issue.Message);
		}
	}

	const FString MapName = FPackageName::GetShortName(MapPath);
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Validation");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString OutPath = Dir / (MapName + TEXT(".json"));
	if (!FFileHelper::SaveStringToFile(UMTWorldValidationLibrary::IssuesToJson(Issues, MapPath), *OutPath))
	{
		UE_LOG(LogMushoku, Error, TEXT("MTValidateWorld: could not write %s"), *OutPath);
	}
	UE_LOG(LogMushoku, Display, TEXT("MTValidateWorld: %d errors, %d warnings -> %s"), Errors, Warnings, *OutPath);

	delete Loader; // unloads the actors it loaded
	// Tear the world down before exit: the World Partition initialised above must be uninitialised and the world's
	// subsystems deinitialised, or UWorldPartition::BeginDestroy asserts during the exit garbage collection.
	if (UWorldPartition* WorldPartition = World->GetWorldPartition())
	{
		if (WorldPartition->IsInitialized())
		{
			WorldPartition->Uninitialize();
		}
	}
	World->DestroyWorld(false); // CleanupWorld (subsystems) and RemoveFromRoot
	return Errors > 0 ? 1 : 0;
#endif // WITH_EDITOR
}

// World validation checks (editor + runtime safe). Used by MTValidateWorldCommandlet and Content/Python/mt_validate_world.py.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MTWorldValidationLibrary.generated.h"

class AActor;
class UWorld;

UENUM(BlueprintType)
enum class EMTValidationSeverity : uint8
{
	Info,
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTValidationIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Validation") EMTValidationSeverity Severity = EMTValidationSeverity::Warning;
	UPROPERTY(BlueprintReadOnly, Category = "Validation") FName Category;
	UPROPERTY(BlueprintReadOnly, Category = "Validation") FString Message;
	/** Weak (not Blueprint-visible: UHT rejects exposed weak pointers); use ActorAName in scripts. */
	UPROPERTY() TWeakObjectPtr<AActor> ActorA;
	UPROPERTY() TWeakObjectPtr<AActor> ActorB;
	UPROPERTY(BlueprintReadOnly, Category = "Validation") FString ActorAName;
	UPROPERTY(BlueprintReadOnly, Category = "Validation") FString ActorBName;
	UPROPERTY(BlueprintReadOnly, Category = "Validation") FVector Location = FVector::ZeroVector;
};

/**
 * Z-fighting / overlap / grounding checks. All functions are read-only scans; none of them modify the world.
 * Tags used: MTBuilding (buildings), MTSpawner / MTQuest / MTSpawnPoint (points that must be clear), MTAllowFloating.
 */
UCLASS()
class MUSHOKURPG_API UMTWorldValidationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Same static mesh with transforms within 1 cm / 1 degree / 1 % scale (actors and ISM/HISM instances). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Validation", meta = (WorldContext = "WorldContextObject"))
	static TArray<FMTValidationIssue> FindDuplicateMeshes(const UObject* WorldContextObject);

	/** Thin (<2 cm) parallel bounds within 0.5 cm of the same plane that overlap in the other two axes. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Validation", meta = (WorldContext = "WorldContextObject"))
	static TArray<FMTValidationIssue> FindCoplanarOverlaps(const UObject* WorldContextObject);

	/** Actors tagged MTBuilding whose oriented footprints overlap (SAT). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Validation", meta = (WorldContext = "WorldContextObject"))
	static TArray<FMTValidationIssue> FindInterpenetratingBuildings(const UObject* WorldContextObject);

	/** Pawns, player starts and actors tagged MTSpawner/MTQuest/MTSpawnPoint that start inside blocking geometry. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Validation", meta = (WorldContext = "WorldContextObject"))
	static TArray<FMTValidationIssue> FindActorsInsideGeometry(const UObject* WorldContextObject);

	/** Relevant actors whose location is more than 50 cm below the landscape surface. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Validation", meta = (WorldContext = "WorldContextObject"))
	static TArray<FMTValidationIssue> FindBelowLandscape(const UObject* WorldContextObject);

	/** Static mesh actors whose bounds bottom is more than 10 cm above the ground below them. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Validation", meta = (WorldContext = "WorldContextObject"))
	static TArray<FMTValidationIssue> FindFloatingProps(const UObject* WorldContextObject);

	/** Runs every check above. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Validation", meta = (WorldContext = "WorldContextObject"))
	static TArray<FMTValidationIssue> RunAllChecks(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Validation")
	static int32 CountIssuesOfSeverity(const TArray<FMTValidationIssue>& Issues, EMTValidationSeverity Severity);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Validation")
	static FString IssuesToJson(const TArray<FMTValidationIssue>& Issues, const FString& MapName);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Validation")
	static FString SeverityToString(EMTValidationSeverity Severity);

	// C++ entry points taking the world directly (commandlet).
	static TArray<FMTValidationIssue> RunAllChecksForWorld(UWorld* World);
	static TArray<FMTValidationIssue> FindDuplicateMeshesInWorld(UWorld* World);
	static TArray<FMTValidationIssue> FindCoplanarOverlapsInWorld(UWorld* World);
	static TArray<FMTValidationIssue> FindInterpenetratingBuildingsInWorld(UWorld* World);
	static TArray<FMTValidationIssue> FindActorsInsideGeometryInWorld(UWorld* World);
	static TArray<FMTValidationIssue> FindBelowLandscapeInWorld(UWorld* World);
	static TArray<FMTValidationIssue> FindFloatingPropsInWorld(UWorld* World);
};

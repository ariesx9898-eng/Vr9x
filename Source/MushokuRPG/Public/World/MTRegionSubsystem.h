// LA PLACE regions at runtime: which region the player is in (Content/Data/WorldRegions.png from the world generator),
// the region names (World.json) and a smoothly blended per-region atmosphere (Content/Data/RegionAtmosphere.json):
// fog density / tint (applied by AMTDayNightController), colour grading and weather (applied by AMTWeatherActor, which
// this subsystem spawns), and the region-name banner the HUD shows on entering a new region. Game worlds only; inactive
// on maps outside the generated world.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MTRegionSubsystem.generated.h"

class AMTWeatherActor;

USTRUCT(BlueprintType)
struct MUSHOKURPG_API FMTRegionAtmosphere
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Region") float FogDensityScale = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Region") FLinearColor FogTint = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Saturation = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Contrast = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Region") FLinearColor Gain = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Wind = 1.f;
	// Weather weights, 0..1.
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Snow = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Ash = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Embers = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Dust = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Pollen = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Fireflies = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Region") float Mist = 0.f;

	/** Exponential approach of every field toward Target (Alpha in 0..1 per call). */
	void BlendToward(const FMTRegionAtmosphere& Target, float Alpha);
};

struct FMTRegionInfo
{
	FText Name;
	FText Continent;
	bool bBanner = true;
	FMTRegionAtmosphere Atmosphere;
};

UCLASS()
class MUSHOKURPG_API UMTRegionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMTRegionSubsystem* Get(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** True once the region map loaded and the player is inside the generated world. */
	bool IsActive() const { return bLoaded; }
	/** Region id at a world position (0 = sea, or outside the map). */
	int32 GetRegionAt(const FVector& Location) const;
	int32 GetCurrentRegion() const { return CurrentRegion; }
	const FMTRegionInfo* GetRegionInfo(int32 Id) const { return Regions.Find(Id); }
	/** Blended atmosphere at the player (fog, grading, weather weights). */
	const FMTRegionAtmosphere& GetAtmosphere() const { return Blended; }
	/** Region to announce and the world time it was entered; INDEX_NONE until the player has settled somewhere. */
	int32 GetBannerRegion(double& OutSince) const { OutSince = BannerSince; return BannerRegion; }

private:
	bool LoadData();
	FVector ViewLocation(bool& bOutValid) const;

	TArray<uint8> RegionMap;
	int32 MapWidth = 0;
	int32 MapHeight = 0;
	FVector2D MapMin = FVector2D::ZeroVector;
	FVector2D MapSize = FVector2D::UnitVector;
	TMap<int32, FMTRegionInfo> Regions;
	float SnowAboveCm = 60000.f;

	FMTRegionAtmosphere Blended;
	int32 CurrentRegion = INDEX_NONE;
	int32 CandidateRegion = INDEX_NONE;
	float CandidateTime = 0.f;
	int32 BannerRegion = INDEX_NONE;
	double BannerSince = 0.0;
	bool bLoaded = false;
	bool bFirstBlend = true;

	UPROPERTY(Transient) TObjectPtr<AMTWeatherActor> Weather;
};

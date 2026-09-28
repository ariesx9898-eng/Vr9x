// Drives sun / moon / sky light / height fog from UMTTimeOfDaySubsystem. Place one per level.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Curves/RichCurve.h"
#include "MTDayNightController.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnNightChanged, bool, bIsNight);

/**
 * Lighting rules (see Docs/World_Fittoa.md):
 *  - Sun follows a tilted circular orbit: rises in the east (+X), peaks NoonElevationDegrees in the south (+Y),
 *    sets in the west. North is -Y (matches the Fittoa heightmap convention).
 *  - Colour temperature ~2600 K at the horizon, ~6000 K at noon (FRichCurve built in code).
 *  - No crushed blacks: moonlight never drops below MoonFloorLux at night, the sky light never below
 *    SkyLightNightMinIntensity. With no moon light assigned, the sun light itself becomes the moon.
 *  - SkyLight recapture: at most every RecaptureMinGameMinutes in-game minutes AND only when the change is
 *    significant (elevation / intensity / night flag). Real-time-capture sky lights are never recaptured.
 *  - Updates run at UpdateIntervalSeconds (not per frame).
 */
UCLASS(Blueprintable, meta = (DisplayName = "MT Day Night Controller"))
class MUSHOKURPG_API AMTDayNightController : public AActor
{
	GENERATED_BODY()

public:
	AMTDayNightController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// ---- references (same level, always-loaded actors) ----
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DayNight|References")
	TObjectPtr<ADirectionalLight> SunLight;

	/** Optional. When empty the sun light is re-aimed as moonlight at night. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DayNight|References")
	TObjectPtr<ADirectionalLight> MoonLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DayNight|References")
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DayNight|References")
	TObjectPtr<AExponentialHeightFog> HeightFog;

	/** Find the first sun / sky light / fog in the level at BeginPlay when a reference is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|References")
	bool bAutoFindReferences = true;

	// ---- sun & moon ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Sun", meta = (ClampMin = "5", ClampMax = "89"))
	float NoonElevationDegrees = 62.f;

	/** Rotates the whole sky around Z if the level's north is not -Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Sun")
	float NorthYawOffsetDegrees = 0.f;

	/** Sun intensity (lux) at full elevation. UE default directional light = 10 lux with auto exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Sun", meta = (ClampMin = "0"))
	float SunMaxIntensityLux = 10.f;

	/** Moonlight floor: night never gets darker than this (lux-equivalent on the same scale as the sun). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Moon", meta = (ClampMin = "0"))
	float MoonFloorLux = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Moon", meta = (ClampMin = "0"))
	float MoonMaxLux = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Moon")
	float MoonTemperatureKelvin = 7800.f;

	// ---- sky light & fog ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Sky", meta = (ClampMin = "0"))
	float SkyLightDayIntensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Sky", meta = (ClampMin = "0"))
	float SkyLightNightMinIntensity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Fog", meta = (ClampMin = "0"))
	float FogDensityDay = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Fog", meta = (ClampMin = "0"))
	float FogDensityNight = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Fog")
	FLinearColor FogColorDay = FLinearColor(0.45f, 0.55f, 0.70f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Fog")
	FLinearColor FogColorNight = FLinearColor(0.04f, 0.06f, 0.12f);

	// ---- sky recapture throttling ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Recapture", meta = (ClampMin = "1"))
	float RecaptureMinGameMinutes = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Recapture", meta = (ClampMin = "0"))
	float RecaptureMinElevationDelta = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Recapture", meta = (ClampMin = "0"))
	float RecaptureMinIntensityRatio = 0.15f;

	// ---- update / preview ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Update", meta = (ClampMin = "0.02"))
	float UpdateIntervalSeconds = 0.1f;

	/** Editor-only preview hour used by PreviewNow() (and OnConstruction when bLivePreviewInEditor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Preview", meta = (ClampMin = "0", ClampMax = "24"))
	float PreviewHour = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Preview")
	bool bLivePreviewInEditor = false;

	/** Toggle ULightComponents on actors tagged MTNightLight (street lamps, windows) on night change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNight|Night")
	bool bAutoToggleTaggedNightLights = true;

	/** Fired when night starts (true) / ends (false), and once at BeginPlay. For lamps, windows, NPC schedules. */
	UPROPERTY(BlueprintAssignable, Category = "DayNight")
	FMTOnNightChanged OnNightChanged;

	/** Applies lighting for Hours. bAllowRecapture=false never touches the sky capture. */
	UFUNCTION(BlueprintCallable, Category = "DayNight")
	void ApplyTimeOfDay(float Hours, bool bAllowRecapture);

	UFUNCTION(CallInEditor, Category = "DayNight|Preview")
	void PreviewNow();

	UFUNCTION(BlueprintPure, Category = "DayNight")
	bool IsNightNow() const { return bIsNight; }

	/** Unit vector pointing TO the sun for Hours (Z up). */
	UFUNCTION(BlueprintPure, Category = "DayNight")
	static FVector ComputeSunDirection(float Hours, float NoonElevationDeg, float NorthYawOffsetDeg);

	UFUNCTION(BlueprintPure, Category = "DayNight")
	float GetSunElevationDegrees(float Hours) const;

	static const FName NightLightTag;

private:
	void BuildCurves();
	void AutoFindReferences();
	void MaybeRecaptureSky(float Hours, float SunElevation, float SkyIntensity, bool bNight);
	void SetNightState(bool bNight, bool bForceBroadcast);
	float ReadCurrentHours() const;

	FRichCurve SunIntensityCurve;   // elevation (deg) -> 0..1
	FRichCurve SunTemperatureCurve; // elevation (deg) -> Kelvin
	FRichCurve SkyIntensityCurve;   // elevation (deg) -> 0..1 (blend night-min .. day)
	bool bCurvesBuilt = false;

	bool bIsNight = false;
	bool bNightStateInitialised = false;
	float LastAppliedHours = -100.f;

	bool bHasCaptured = false;
	float LastCaptureHours = -100.f;
	float LastCaptureElevation = 0.f;
	float LastCaptureSkyIntensity = 0.f;
	bool bLastCaptureNight = false;
};

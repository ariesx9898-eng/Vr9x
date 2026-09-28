#include "World/MTDayNightController.h"
#include "World/MTRegionSubsystem.h"

#include "World/MTTimeOfDaySubsystem.h"
#include "Core/MTTypes.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"

const FName AMTDayNightController::NightLightTag(TEXT("MTNightLight"));

namespace MTDayNight
{
	static float SmoothStep01(float Edge0, float Edge1, float X)
	{
		if (FMath::IsNearlyEqual(Edge0, Edge1))
		{
			return X < Edge0 ? 0.f : 1.f;
		}
		const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	static void AddSmoothKey(FRichCurve& Curve, float Time, float Value)
	{
		const FKeyHandle Handle = Curve.AddKey(Time, Value);
		Curve.SetKeyInterpMode(Handle, RCIM_Cubic);
	}
}

AMTDayNightController::AMTDayNightController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 0.1f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AMTDayNightController::BuildCurves()
{
	if (bCurvesBuilt)
	{
		return;
	}
	SunIntensityCurve.Reset();
	SunTemperatureCurve.Reset();
	SkyIntensityCurve.Reset();

	// Sun intensity factor by elevation: 0 below -4 deg, soft ramp through the golden hour.
	MTDayNight::AddSmoothKey(SunIntensityCurve, -4.f, 0.f);
	MTDayNight::AddSmoothKey(SunIntensityCurve, 0.f, 0.12f);
	MTDayNight::AddSmoothKey(SunIntensityCurve, 10.f, 0.5f);
	MTDayNight::AddSmoothKey(SunIntensityCurve, 30.f, 0.88f);
	MTDayNight::AddSmoothKey(SunIntensityCurve, 60.f, 1.f);
	MTDayNight::AddSmoothKey(SunIntensityCurve, 90.f, 1.f);

	// Colour temperature: warm dawn/dusk, neutral noon.
	MTDayNight::AddSmoothKey(SunTemperatureCurve, -6.f, 2200.f);
	MTDayNight::AddSmoothKey(SunTemperatureCurve, 0.f, 2600.f);
	MTDayNight::AddSmoothKey(SunTemperatureCurve, 10.f, 3800.f);
	MTDayNight::AddSmoothKey(SunTemperatureCurve, 25.f, 5200.f);
	MTDayNight::AddSmoothKey(SunTemperatureCurve, 45.f, 6000.f);
	MTDayNight::AddSmoothKey(SunTemperatureCurve, 90.f, 6200.f);

	// Sky light blend factor (0 = night minimum, 1 = day).
	MTDayNight::AddSmoothKey(SkyIntensityCurve, -12.f, 0.f);
	MTDayNight::AddSmoothKey(SkyIntensityCurve, -2.f, 0.35f);
	MTDayNight::AddSmoothKey(SkyIntensityCurve, 10.f, 0.85f);
	MTDayNight::AddSmoothKey(SkyIntensityCurve, 30.f, 1.f);

	SunIntensityCurve.AutoSetTangents();
	SunTemperatureCurve.AutoSetTangents();
	SkyIntensityCurve.AutoSetTangents();
	bCurvesBuilt = true;
}

void AMTDayNightController::AutoFindReferences()
{
	UWorld* World = GetWorld();
	if (!World || !bAutoFindReferences)
	{
		return;
	}
	if (!SunLight)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			if (*It != MoonLight)
			{
				SunLight = *It;
				break;
			}
		}
	}
	if (!SkyLight)
	{
		TActorIterator<ASkyLight> It(World);
		SkyLight = It ? *It : nullptr;
	}
	if (!HeightFog)
	{
		TActorIterator<AExponentialHeightFog> It(World);
		HeightFog = It ? *It : nullptr;
	}
	if (!SunLight)
	{
		UE_LOG(LogMushoku, Warning, TEXT("DayNight: no sun DirectionalLight found for %s"), *GetName());
	}
}

void AMTDayNightController::BeginPlay()
{
	Super::BeginPlay();
	BuildCurves();
	AutoFindReferences();
	SetActorTickInterval(FMath::Max(UpdateIntervalSeconds, 0.02f));

	const float Hours = ReadCurrentHours();
	ApplyTimeOfDay(Hours, true);
	SetNightState(bIsNight, true);
}

void AMTDayNightController::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const UWorld* World = GetWorld();
	if (bLivePreviewInEditor && World && !World->IsGameWorld())
	{
		ApplyTimeOfDay(PreviewHour, false);
	}
}

void AMTDayNightController::PreviewNow()
{
	ApplyTimeOfDay(PreviewHour, true);
}

float AMTDayNightController::ReadCurrentHours() const
{
	if (const UMTTimeOfDaySubsystem* Clock = UMTTimeOfDaySubsystem::Get(this))
	{
		return Clock->GetTimeOfDayHours();
	}
	return PreviewHour;
}

void AMTDayNightController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Hours = ReadCurrentHours();
	// Skip work when the clock moved less than ~15 in-game seconds (paused clock, very long days).
	float Delta = FMath::Abs(Hours - LastAppliedHours);
	Delta = FMath::Min(Delta, 24.f - Delta);
	if (Delta < (0.25f / 60.f))
	{
		return;
	}
	ApplyTimeOfDay(Hours, true);
}

FVector AMTDayNightController::ComputeSunDirection(float Hours, float NoonElevationDeg, float NorthYawOffsetDeg)
{
	// Circular orbit tilted by the noon elevation: 06:00 east (+X), 12:00 south (+Y) & up, 18:00 west (-X).
	const float Theta = 2.f * PI * (Hours - 6.f) / 24.f;
	const float Tilt = FMath::DegreesToRadians(FMath::Clamp(NoonElevationDeg, 1.f, 89.f));
	FVector ToSun(FMath::Cos(Theta), FMath::Sin(Theta) * FMath::Cos(Tilt), FMath::Sin(Theta) * FMath::Sin(Tilt));
	ToSun = ToSun.RotateAngleAxis(NorthYawOffsetDeg, FVector::UpVector);
	return ToSun.GetSafeNormal();
}

float AMTDayNightController::GetSunElevationDegrees(float Hours) const
{
	const FVector ToSun = ComputeSunDirection(Hours, NoonElevationDegrees, NorthYawOffsetDegrees);
	return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(ToSun.Z, -1.f, 1.f)));
}

void AMTDayNightController::ApplyTimeOfDay(float Hours, bool bAllowRecapture)
{
	BuildCurves();
	Hours = FMath::Fmod(Hours, 24.f);
	if (Hours < 0.f)
	{
		Hours += 24.f;
	}
	LastAppliedHours = Hours;

	const FVector ToSun = ComputeSunDirection(Hours, NoonElevationDegrees, NorthYawOffsetDegrees);
	const float Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(ToSun.Z, -1.f, 1.f)));
	const FVector ToMoon = -ToSun; // opposite the sun: highest at midnight
	const float MoonElevation = -Elevation;

	const float SunFactor = FMath::Clamp(SunIntensityCurve.Eval(Elevation, 0.f), 0.f, 1.f);
	const float SunTemperature = FMath::Clamp(SunTemperatureCurve.Eval(Elevation, 6000.f), 1700.f, 12000.f);
	// Moon fades in after sunset and never drops below the floor once it is night.
	const float MoonFade = MTDayNight::SmoothStep01(-2.f, -8.f, Elevation);
	const float MoonLux = MoonFade * FMath::Lerp(MoonFloorLux, MoonMaxLux, FMath::Clamp(MoonElevation / 45.f, 0.f, 1.f));

	const UMTTimeOfDaySubsystem* Clock = UMTTimeOfDaySubsystem::Get(this);
	const bool bNight = Clock ? Clock->IsNight() : (Elevation < -4.f);

	// ---- sun / moon ----
	ULightComponent* SunComp = SunLight ? SunLight->GetLightComponent() : nullptr;
	ULightComponent* MoonComp = MoonLight ? MoonLight->GetLightComponent() : nullptr;
	if (SunComp)
	{
		const bool bSunActsAsMoon = !MoonComp && Elevation < -4.f;
		const FVector LightTravel = bSunActsAsMoon ? -ToMoon : -ToSun; // light forward = direction light travels
		SunLight->SetActorRotation(LightTravel.Rotation());
		SunComp->SetUseTemperature(true);
		if (bSunActsAsMoon)
		{
			SunComp->SetTemperature(MoonTemperatureKelvin);
			SunComp->SetIntensity(FMath::Max(MoonLux, MoonFloorLux * MoonFade));
		}
		else
		{
			SunComp->SetTemperature(SunTemperature);
			SunComp->SetIntensity(SunMaxIntensityLux * SunFactor);
		}
	}
	if (MoonComp)
	{
		MoonLight->SetActorRotation((-ToMoon).Rotation());
		MoonComp->SetUseTemperature(true);
		MoonComp->SetTemperature(MoonTemperatureKelvin);
		MoonComp->SetIntensity(MoonLux);
	}

	// ---- sky light: never below the night minimum ----
	const float SkyBlend = FMath::Clamp(SkyIntensityCurve.Eval(Elevation, 1.f), 0.f, 1.f);
	const float SkyIntensity = FMath::Max(FMath::Lerp(SkyLightNightMinIntensity, SkyLightDayIntensity, SkyBlend),
		SkyLightNightMinIntensity);
	if (USkyLightComponent* SkyComp = SkyLight ? SkyLight->GetLightComponent() : nullptr)
	{
		SkyComp->SetIntensity(SkyIntensity);
	}

	// ---- fog ----
	if (UExponentialHeightFogComponent* FogComp = HeightFog ? HeightFog->GetComponent() : nullptr)
	{
		const float NightAlpha = MTDayNight::SmoothStep01(2.f, -8.f, Elevation);
		// Region atmosphere (LA PLACE world): thicker fog in the north and the Great Forest, red haze on the Demon
		// Continent, warm dust over Begaritt.
		float RegionDensity = 1.f;
		FLinearColor RegionTint = FLinearColor::White;
		if (const UMTRegionSubsystem* Regions = UMTRegionSubsystem::Get(this))
		{
			if (Regions->IsActive())
			{
				RegionDensity = Regions->GetAtmosphere().FogDensityScale;
				RegionTint = Regions->GetAtmosphere().FogTint;
			}
		}
		FogComp->SetFogDensity(FMath::Lerp(FogDensityDay, FogDensityNight, NightAlpha) * RegionDensity);
		FogComp->SetFogInscatteringColor(FMath::Lerp(FogColorDay, FogColorNight, NightAlpha) * RegionTint);
	}

	if (bAllowRecapture)
	{
		MaybeRecaptureSky(Hours, Elevation, SkyIntensity, bNight);
	}
	SetNightState(bNight, false);
}

void AMTDayNightController::MaybeRecaptureSky(float Hours, float SunElevation, float SkyIntensity, bool bNight)
{
	USkyLightComponent* SkyComp = SkyLight ? SkyLight->GetLightComponent() : nullptr;
	if (!SkyComp || SkyComp->bRealTimeCapture)
	{
		return; // real-time capture updates itself; recapturing would only cost a hitch
	}
	if (bHasCaptured)
	{
		float MinutesSince = FMath::Abs(Hours - LastCaptureHours);
		MinutesSince = FMath::Min(MinutesSince, 24.f - MinutesSince) * 60.f;
		if (MinutesSince < RecaptureMinGameMinutes)
		{
			return;
		}
		const bool bElevationChanged = FMath::Abs(SunElevation - LastCaptureElevation) >= RecaptureMinElevationDelta;
		const bool bIntensityChanged = FMath::Abs(SkyIntensity - LastCaptureSkyIntensity) >=
			RecaptureMinIntensityRatio * FMath::Max(LastCaptureSkyIntensity, 0.01f);
		const bool bNightFlipped = bNight != bLastCaptureNight;
		if (!bElevationChanged && !bIntensityChanged && !bNightFlipped)
		{
			return;
		}
	}
	SkyComp->RecaptureSky();
	bHasCaptured = true;
	LastCaptureHours = Hours;
	LastCaptureElevation = SunElevation;
	LastCaptureSkyIntensity = SkyIntensity;
	bLastCaptureNight = bNight;
}

void AMTDayNightController::SetNightState(bool bNight, bool bForceBroadcast)
{
	if (bNightStateInitialised && bNight == bIsNight && !bForceBroadcast)
	{
		return;
	}
	bIsNight = bNight;
	bNightStateInitialised = true;

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return; // editor preview: never flip lamps or fire gameplay events
	}
	if (bAutoToggleTaggedNightLights)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || !Actor->ActorHasTag(NightLightTag))
			{
				continue;
			}
			TArray<ULightComponent*> Lights;
			Actor->GetComponents<ULightComponent>(Lights);
			for (ULightComponent* Light : Lights)
			{
				if (Light)
				{
					Light->SetVisibility(bNight);
				}
			}
		}
	}
	OnNightChanged.Broadcast(bNight);
}

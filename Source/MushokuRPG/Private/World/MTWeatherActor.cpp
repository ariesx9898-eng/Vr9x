#include "World/MTWeatherActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VFX/MTVFXLibrary.h"
#include "World/MTTimeOfDaySubsystem.h"

namespace MTWeather
{
	enum EKind : int32 { Snow, Ash, Embers, Dust, Haze, Pollen, Fireflies, Mist, Num };

	/** One particle layer. Heights are relative to the camera, or to the ground under the player (bGround). */
	struct FSpec
	{
		int32 Max;
		float Radius;         // half width of the wrap box (cm)
		float MinZ, MaxZ;     // wrap box height range (cm)
		bool bGround;
		FVector Velocity;     // base velocity (cm/s), before wind
		float WindFactor;     // how much the region wind pushes it
		float Jitter;         // random velocity spread
		float Sway;           // sideways sway amplitude (cm/s)
		float SizeMin, SizeMax;
		FLinearColor Color;
		float Alpha;
		float Intensity;      // emissive boost for glowing kinds
		bool bSmoke;          // alpha-blended (smoke material) instead of the glowing sprite material
		bool bPuff;           // soft puff texture instead of the dot
		float DayAmount;      // visibility by day
		float NightAmount;    // visibility by night
	};

	const FSpec Specs[Num] = {
		// Max  Radius MinZ   MaxZ  Ground Velocity                 Wind  Jit   Sway  Size         Color                                  Alpha Int   Smoke  Puff   Day   Night
		{ 1200, 3200, -1200.f, 2600.f, false, FVector(0, 0, -150),  0.6f,  35.f, 45.f, 3.f, 6.5f,   FLinearColor(1.f, 1.f, 1.f),           0.9f, 1.0f, false, false, 1.0f, 0.4f },  // Snow
		{ 700,  3000, -1000.f, 2400.f, false, FVector(0, 0, -55),   0.8f,  20.f, 30.f, 4.f, 9.f,    FLinearColor(0.2f, 0.18f, 0.17f),      0.8f, 1.0f, true,  false, 1.0f, 0.8f },  // Ash
		{ 180,  2200, -400.f,  1600.f, false, FVector(0, 0, 70),    0.5f,  40.f, 30.f, 2.5f, 4.5f,  FLinearColor(1.f, 0.42f, 0.08f),       1.0f, 8.0f, false, false, 0.7f, 1.0f },  // Embers
		{ 500,  2600, -400.f,  1200.f, false, FVector(0, 0, 0),     1.0f,  150.f, 40.f, 2.f, 5.f,   FLinearColor(0.78f, 0.64f, 0.45f),     0.55f, 1.0f, true,  false, 1.0f, 0.6f }, // Dust
		{ 45,   3200, -200.f,  700.f,  true,  FVector(0, 0, 0),     0.8f,  60.f, 20.f, 500.f, 1100.f, FLinearColor(0.86f, 0.72f, 0.52f),   0.07f, 1.0f, true,  true,  1.0f, 0.5f }, // Haze
		{ 260,  2000, -200.f,  800.f,  true,  FVector(0, 0, 5),     0.2f,  25.f, 25.f, 1.5f, 3.f,   FLinearColor(1.f, 0.95f, 0.72f),       0.85f, 1.6f, false, false, 1.0f, 0.0f }, // Pollen
		{ 150,  2600, 20.f,    350.f,  true,  FVector(0, 0, 0),     0.1f,  45.f, 40.f, 4.f, 6.f,    FLinearColor(0.72f, 1.f, 0.35f),       1.0f, 10.f, false, false, 0.0f, 1.0f },  // Fireflies
		{ 36,   3600, -150.f,  250.f,  true,  FVector(0, 0, 0),     0.3f,  25.f, 10.f, 900.f, 1600.f, FLinearColor(0.86f, 0.9f, 0.92f),    0.07f, 1.0f, true,  true,  0.8f, 1.0f }, // Mist
	};

	const FName ParamColor(TEXT("Color"));
	const FName ParamIntensity(TEXT("Intensity"));
	const FName ParamOpacity(TEXT("Opacity"));
	const FName ParamTexture(TEXT("Texture"));

	float WrapInto(float Value, float Min, float Max)
	{
		const float Range = Max - Min;
		return Range > 0.f ? Min + FMath::Fmod(FMath::Fmod(Value - Min, Range) + Range, Range) : Min;
	}
}

AMTWeatherActor::AMTWeatherActor()
{
	PrimaryActorTick.bCanEverTick = false; // driven by UMTRegionSubsystem
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("RegionGrading"));
	Post->SetupAttachment(RootComponent);
	Post->bUnbound = true;
	Post->Priority = 5.f;
	Post->BlendWeight = 1.f;
	Random.Initialize(0x1A7C3);
}

void AMTWeatherActor::BuildLayers()
{
	UStaticMesh* Plane = MTVFX::LoadMesh(MTVFX::Paths::Plane);
	for (int32 Kind = 0; Kind < MTWeather::Num; ++Kind)
	{
		const MTWeather::FSpec& Spec = MTWeather::Specs[Kind];
		FLayer& Layer = Layers.AddDefaulted_GetRef();
		Layer.Kind = Kind;
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		ISM->SetStaticMesh(Plane);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCastShadow(false);
		ISM->SetGenerateOverlapEvents(false);
		ISM->bReceivesDecals = false;
		ISM->SetupAttachment(RootComponent);
		ISM->SetNumCustomDataFloats(4);
		ISM->RegisterComponent();
		if (UMaterialInterface* Base = MTVFX::LoadMaterial(Spec.bSmoke ? MTVFX::Paths::MatSmoke : MTVFX::Paths::MatSprite))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			if (UTexture* Tex = MTVFX::LoadTexture(Spec.bPuff ? MTVFX::Paths::TexPuff : MTVFX::Paths::TexDot))
			{
				MID->SetTextureParameterValue(MTWeather::ParamTexture, Tex);
			}
			MID->SetScalarParameterValue(MTWeather::ParamIntensity, Spec.Intensity);
			MID->SetScalarParameterValue(MTWeather::ParamOpacity, 1.f);
			MID->SetVectorParameterValue(MTWeather::ParamColor, FLinearColor::White);
			ISM->SetMaterial(0, MID);
		}
		Layer.Particles.SetNum(Spec.Max);
		Layer.Transforms.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(KINDA_SMALL_NUMBER)), Spec.Max);
		ISM->AddInstances(Layer.Transforms, false, true);
		ISM->SetVisibility(false);
		Layer.ISM = ISM;
		Components.Add(ISM);
	}
}

void AMTWeatherActor::ApplyAtmosphere(const FMTRegionAtmosphere& A, float DeltaSeconds)
{
	Time += DeltaSeconds;
	// Colour grading.
	FPostProcessSettings& S = Post->Settings;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(A.Saturation, A.Saturation, A.Saturation, 1.f);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(A.Contrast, A.Contrast, A.Contrast, 1.f);
	S.bOverride_ColorGain = true;
	S.ColorGain = FVector4(A.Gain.R, A.Gain.G, A.Gain.B, 1.f);

	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
	const APawn* Pawn = PC->GetPawn();
	const float GroundZ = Pawn ? Pawn->GetActorLocation().Z - Pawn->GetSimpleCollisionHalfHeight() : Camera.Z - 170.f;

	// 0 by day, 1 at night, with an hour of dusk / dawn.
	float Night = 0.f;
	if (const UMTTimeOfDaySubsystem* Clock = UMTTimeOfDaySubsystem::Get(this))
	{
		const float H = Clock->GetTimeOfDayHours();
		Night = 1.f - FMath::SmoothStep(4.8f, 6.3f, H) * (1.f - FMath::SmoothStep(18.2f, 19.7f, H));
	}
	const FVector Wind = FVector(1.f, 0.35f, 0.f).GetSafeNormal() * 180.f * A.Wind;

	if (Layers.IsEmpty())
	{
		BuildLayers();
	}
	const float Weights[MTWeather::Num] = { A.Snow, A.Ash, A.Embers, A.Dust, A.Dust, A.Pollen, A.Fireflies, A.Mist };
	for (FLayer& Layer : Layers)
	{
		UpdateLayer(Layer, Weights[Layer.Kind], Camera, GroundZ, Night, Wind, DeltaSeconds);
	}
}

void AMTWeatherActor::UpdateLayer(FLayer& Layer, float Weight, const FVector& Camera, float GroundZ, float Night, const FVector& Wind, float Dt)
{
	const MTWeather::FSpec& Spec = MTWeather::Specs[Layer.Kind];
	const float Visible = Weight * FMath::Lerp(Spec.DayAmount, Spec.NightAmount, Night);
	Layer.Weight = FMath::FInterpTo(Layer.Weight, Visible, Dt, 1.5f);
	const int32 Active = FMath::Clamp(FMath::RoundToInt(Layer.Weight * Spec.Max), 0, Spec.Max);
	if (Active == 0)
	{
		if (Layer.bVisible)
		{
			Layer.ISM->SetVisibility(false);
			Layer.bVisible = false;
		}
		return;
	}
	if (!Layer.bVisible)
	{
		Layer.ISM->SetVisibility(true);
		Layer.bVisible = true;
	}
	const FVector Center(Camera.X, Camera.Y, Spec.bGround ? GroundZ : Camera.Z);
	if (!Layer.bSeeded)
	{
		for (FParticle& P : Layer.Particles)
		{
			P.Position = Center + FVector(Random.FRandRange(-Spec.Radius, Spec.Radius), Random.FRandRange(-Spec.Radius, Spec.Radius),
				Random.FRandRange(Spec.MinZ, Spec.MaxZ));
			P.Velocity = Spec.Velocity + FVector(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f), Random.FRandRange(-0.5f, 0.5f)) * Spec.Jitter;
			P.Size = Random.FRandRange(Spec.SizeMin, Spec.SizeMax);
			P.Phase = Random.FRandRange(0.f, 2.f * PI);
		}
		Layer.bSeeded = true;
	}

	float Custom[4];
	for (int32 i = 0; i < Spec.Max; ++i)
	{
		FParticle& P = Layer.Particles[i];
		if (i >= Active)
		{
			Layer.Transforms[i] = FTransform(FQuat::Identity, Center, FVector(KINDA_SMALL_NUMBER));
			continue;
		}
		const float SwayT = Time * (0.6f + 0.3f * FMath::Sin(P.Phase * 3.f)) + P.Phase;
		const FVector Sway(FMath::Sin(SwayT) * Spec.Sway, FMath::Cos(SwayT * 0.8f) * Spec.Sway, 0.f);
		P.Position += (P.Velocity + Wind * Spec.WindFactor + Sway) * Dt;
		// Keep every particle inside the box that travels with the view.
		FVector Rel = P.Position - Center;
		Rel.X = MTWeather::WrapInto(Rel.X, -Spec.Radius, Spec.Radius);
		Rel.Y = MTWeather::WrapInto(Rel.Y, -Spec.Radius, Spec.Radius);
		Rel.Z = MTWeather::WrapInto(Rel.Z, Spec.MinZ, Spec.MaxZ);
		P.Position = Center + Rel;

		const FVector ToCamera = (Camera - P.Position).GetSafeNormal();
		const FQuat Facing = FRotationMatrix::MakeFromZ(ToCamera).ToQuat();
		Layer.Transforms[i] = FTransform(Facing, P.Position, FVector(P.Size / 100.f, P.Size / 100.f, 1.f));

		// Fade near the box edges (no popping when particles wrap) and blink fireflies.
		const float Edge = FMath::Clamp((Spec.Radius - FMath::Max(FMath::Abs(Rel.X), FMath::Abs(Rel.Y))) / (Spec.Radius * 0.2f), 0.f, 1.f)
			* FMath::Clamp(FMath::Min(Rel.Z - Spec.MinZ, Spec.MaxZ - Rel.Z) / ((Spec.MaxZ - Spec.MinZ) * 0.15f), 0.f, 1.f);
		float Alpha = Spec.Alpha * Edge;
		if (Layer.Kind == MTWeather::Fireflies)
		{
			Alpha *= FMath::Clamp(FMath::Sin(Time * 1.7f + P.Phase * 5.f) * 1.4f, 0.f, 1.f);
		}
		const FLinearColor C = Spec.Color;
		Custom[0] = C.R;
		Custom[1] = C.G;
		Custom[2] = C.B;
		Custom[3] = Alpha;
		Layer.ISM->SetCustomData(i, Custom, false);
	}
	Layer.ISM->BatchUpdateInstancesTransforms(0, Layer.Transforms, true, true, true);
}

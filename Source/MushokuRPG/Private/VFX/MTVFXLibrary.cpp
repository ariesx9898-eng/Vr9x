#include "VFX/MTVFXLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"
#include "Core/MTTypes.h"

namespace MTVFX
{
	namespace Paths
	{
		const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
		const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
		const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
		const TCHAR* Plane = TEXT("/Engine/BasicShapes/Plane.Plane");
		const TCHAR* Ring = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Ring.SM_VFX_Ring");
		const TCHAR* ShockRing = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_ShockRing.SM_VFX_ShockRing");
		const TCHAR* Disc = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Disc.SM_VFX_Disc");
		const TCHAR* Crescent = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Crescent.SM_VFX_Crescent");
		const TCHAR* Spike = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Spike_A.SM_VFX_Spike_A");
		const TCHAR* RockChunk = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_RockChunk_A.SM_VFX_RockChunk_A");
		const TCHAR* RockChunkB = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_RockChunk_C.SM_VFX_RockChunk_C");
		const TCHAR* Funnel = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Funnel.SM_VFX_Funnel");
		const TCHAR* Flame = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Flame.SM_VFX_Flame");
		const TCHAR* DragonHead = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_DragonHead.SM_VFX_DragonHead");
		const TCHAR* DragonSegment = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_DragonSegment.SM_VFX_DragonSegment");
		const TCHAR* Beam = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Beam.SM_VFX_Beam");
		const TCHAR* EarthWall = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_EarthWall.SM_VFX_EarthWall");
		const TCHAR* Crystal = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Crystal.SM_VFX_Crystal");

		const TCHAR* MatGlow = TEXT("/Game/LaPlace/VFX/Materials/M_VFX_Glow.M_VFX_Glow");
		const TCHAR* MatSprite = TEXT("/Game/LaPlace/VFX/Materials/M_VFX_Sprite.M_VFX_Sprite");
		const TCHAR* MatSmoke = TEXT("/Game/LaPlace/VFX/Materials/M_VFX_Smoke.M_VFX_Smoke");
		const TCHAR* MatWater = TEXT("/Game/LaPlace/VFX/Materials/M_VFX_Water.M_VFX_Water");
		const TCHAR* MatAir = TEXT("/Game/LaPlace/VFX/Materials/M_VFX_Air.M_VFX_Air");
		const TCHAR* MatRock = TEXT("/Game/LaPlace/VFX/Materials/M_VFX_Rock.M_VFX_Rock");
		const TCHAR* MatGhost = TEXT("/Game/LaPlace/VFX/Materials/M_VFX_Ghost.M_VFX_Ghost");
		const TCHAR* DecalScorch = TEXT("/Game/LaPlace/VFX/Materials/M_Decal_Scorch.M_Decal_Scorch");
		const TCHAR* DecalCracks = TEXT("/Game/LaPlace/VFX/Materials/M_Decal_Cracks.M_Decal_Cracks");
		const TCHAR* DecalWet = TEXT("/Game/LaPlace/VFX/Materials/M_Decal_Wet.M_Decal_Wet");
		const TCHAR* DecalCircle = TEXT("/Game/LaPlace/VFX/Materials/M_Decal_Circle.M_Decal_Circle");

		const TCHAR* TexDot = TEXT("/Game/LaPlace/VFX/Textures/T_VFX_Dot.T_VFX_Dot");
		const TCHAR* TexPuff = TEXT("/Game/LaPlace/VFX/Textures/T_VFX_Puff.T_VFX_Puff");
		const TCHAR* TexStreak = TEXT("/Game/LaPlace/VFX/Textures/T_VFX_Streak.T_VFX_Streak");
	}

	namespace
	{
		template <typename T>
		T* LoadCached(const FString& Path)
		{
			static TMap<FString, TWeakObjectPtr<UObject>> Cache;
			static TSet<FString> Missing;
			if (Path.IsEmpty() || Missing.Contains(Path))
			{
				return nullptr;
			}
			if (TWeakObjectPtr<UObject>* Found = Cache.Find(Path))
			{
				if (UObject* Obj = Found->Get())
				{
					return Cast<T>(Obj);
				}
			}
			T* Loaded = LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Loaded)
			{
				Missing.Add(Path);
				return nullptr;
			}
			Cache.Add(Path, Loaded);
			return Loaded;
		}

		/** Kit meshes may not be imported yet: fall back to the closest engine basic shape. */
		const TCHAR* FallbackMesh(const FString& Path)
		{
			if (Path.Contains(TEXT("Ring")) || Path.Contains(TEXT("Disc")))
			{
				return Paths::Cylinder;
			}
			if (Path.Contains(TEXT("Spike")) || Path.Contains(TEXT("Funnel")) || Path.Contains(TEXT("Flame")) || Path.Contains(TEXT("Crystal")))
			{
				return Paths::Cone;
			}
			if (Path.Contains(TEXT("Beam")) || Path.Contains(TEXT("Segment")))
			{
				return Paths::Cylinder;
			}
			if (Path.Contains(TEXT("EarthWall")))
			{
				return Paths::Cube;
			}
			if (Path.Contains(TEXT("Crescent")))
			{
				return Paths::Plane;
			}
			return Paths::Sphere;
		}
	}

	UStaticMesh* LoadMesh(const FString& Path)
	{
		if (UStaticMesh* Mesh = LoadCached<UStaticMesh>(Path))
		{
			return Mesh;
		}
		if (UStaticMesh* Mesh = LoadCached<UStaticMesh>(FallbackMesh(Path)))
		{
			return Mesh;
		}
		return LoadCached<UStaticMesh>(Paths::Sphere);
	}

	UMaterialInterface* LoadMaterial(const FString& Path)
	{
		if (UMaterialInterface* Mat = LoadCached<UMaterialInterface>(Path))
		{
			return Mat;
		}
		// The VFX materials are created by the editor setup; until then the older emissive spell material.
		return LoadCached<UMaterialInterface>(TEXT("/Game/Materials/M_MT_SpellBody.M_MT_SpellBody"));
	}

	UTexture* LoadTexture(const FString& Path)
	{
		return LoadCached<UTexture>(Path);
	}

	// -----------------------------------------------------------------------------------------
	// Building blocks
	// -----------------------------------------------------------------------------------------
	namespace
	{
		using FC = FLinearColor;

		// Palette (linear). Emissive layers multiply these by their intensity.
		const FC FireCore(1.f, 0.85f, 0.55f);
		const FC FireOrange(1.f, 0.38f, 0.07f);
		const FC FireRed(0.85f, 0.1f, 0.02f);
		const FC Smoke(0.09f, 0.085f, 0.08f);
		const FC SmokeLight(0.35f, 0.33f, 0.31f);
		const FC WaterBlue(0.18f, 0.45f, 0.75f);
		const FC WaterFoam(0.85f, 0.93f, 1.f);
		const FC WindWhite(0.85f, 0.95f, 0.9f);
		const FC WindGreen(0.55f, 0.8f, 0.6f);
		const FC Dust(0.42f, 0.35f, 0.26f);
		const FC DustLight(0.62f, 0.55f, 0.45f);
		const FC Earth(0.36f, 0.27f, 0.18f);
		const FC Mana(0.25f, 0.85f, 1.f);
		const FC Mud(0.12f, 0.08f, 0.05f);
		const FC Teal(0.15f, 0.75f, 0.7f);
		const FC Silver(0.85f, 0.9f, 1.f);
		const FC Gold(1.f, 0.78f, 0.35f);
		const FC Leaf(0.3f, 0.45f, 0.15f);

		FMTVFXMeshLayer Layer(const TCHAR* Mesh, const TCHAR* Material, const FVector& Size, const FC& Color, float Intensity,
			const FMTVFXCurve& Alpha = FMTVFXCurve::FadeInOut(0.1f, 0.3f), const FMTVFXCurve& ScaleCurve = FMTVFXCurve())
		{
			FMTVFXMeshLayer L;
			L.MeshPath = Mesh;
			L.MaterialPath = Material;
			L.Size = Size;
			L.Color = Color;
			L.Intensity = Intensity;
			L.AlphaOverLife = Alpha;
			L.ScaleOverLife = ScaleCurve;
			return L;
		}

		FMTVFXMeshLayer Orb(float Radius, const FC& Color, float Intensity, float FresnelMix = 0.f, float Noise = 0.3f)
		{
			FMTVFXMeshLayer L = Layer(Paths::Sphere, Paths::MatGlow, FVector(Radius), Color, Intensity, FMTVFXCurve::FadeInOut(0.1f, 0.25f));
			L.Scalars.Add(TEXT("FresnelMix"), FresnelMix);
			L.Scalars.Add(TEXT("NoiseAmount"), Noise);
			return L;
		}

		/** Expanding flat ring (shockwave). */
		FMTVFXMeshLayer ShockwaveRing(float FromRadius, float ToRadius, float Duration, const FC& Color, float Intensity, float Delay = 0.f, float Thickness = 0.12f)
		{
			FMTVFXMeshLayer L = Layer(Paths::ShockRing, Paths::MatGlow, FVector(ToRadius, ToRadius, ToRadius * Thickness * 0.2f), Color, Intensity,
				FMTVFXCurve({ { 0.f, 1.f }, { 0.5f, 0.7f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, FromRadius / ToRadius }, { 1.f, 1.f } }));
			L.Delay = Delay;
			L.Duration = Duration;
			L.Scalars.Add(TEXT("FresnelMix"), 0.f);
			L.Scalars.Add(TEXT("NoiseAmount"), 0.35f);
			return L;
		}

		/** Expanding air-distortion sphere/ring (pressure, heat). */
		FMTVFXMeshLayer AirPulse(const TCHAR* Mesh, float FromRadius, float ToRadius, float Duration, float Strength, float Delay = 0.f, float FlatZ = 1.f)
		{
			FMTVFXMeshLayer L = Layer(Mesh, Paths::MatAir, FVector(ToRadius, ToRadius, ToRadius * FlatZ), FC::White, 1.f,
				FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, FromRadius / ToRadius }, { 1.f, 1.f } }));
			L.Delay = Delay;
			L.Duration = Duration;
			L.Scalars.Add(TEXT("Distortion"), Strength);
			return L;
		}

		FMTVFXEmitter Sprites(EMTVFXRender Render, int32 Burst, float Rate, float LifeMin, float LifeMax, float SizeMin, float SizeMax,
			const FC& From, const FC& To, float Intensity = 4.f)
		{
			FMTVFXEmitter E;
			E.Render = Render;
			E.Burst = Burst;
			E.Rate = Rate;
			E.LifeMin = LifeMin;
			E.LifeMax = LifeMax;
			E.SizeMin = SizeMin;
			E.SizeMax = SizeMax;
			E.ColorStart = From;
			E.ColorEnd = To;
			E.Intensity = Intensity;
			E.MaxParticles = FMath::Clamp(Burst + FMath::CeilToInt(Rate * LifeMax * 1.2f) + 4, 8, 400);
			return E;
		}

		FMTVFXEmitter Flames(int32 Burst, float Rate, float SizeMin, float SizeMax, float Life = 0.45f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::SpriteAdd, Burst, Rate, Life * 0.7f, Life * 1.2f, SizeMin, SizeMax, FireOrange, FireRed, 6.f);
			E.Texture = Paths::TexPuff;
			E.Buoyancy = 260.f;
			E.Drag = 2.f;
			E.SpinMax = 90.f;
			E.SizeOverLife = FMTVFXCurve({ { 0.f, 0.6f }, { 0.25f, 1.f }, { 1.f, 0.35f } });
			E.AlphaOverLife = FMTVFXCurve::FadeInOut(0.1f, 0.55f);
			return E;
		}

		FMTVFXEmitter Sparks(int32 Burst, float Rate, float SpeedMin, float SpeedMax, const FC& Color, float Gravity = 900.f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Stretched, Burst, Rate, 0.35f, 0.9f, 3.f, 7.f, Color, Color * 0.4f, 10.f);
			E.SpeedMin = SpeedMin;
			E.SpeedMax = SpeedMax;
			E.Gravity = Gravity;
			E.Drag = 1.2f;
			E.bRadial = true;
			E.Shape = EMTVFXShape::Sphere;
			E.Radius = 10.f;
			E.Stretch = 5.f;
			E.AlphaOverLife = FMTVFXCurve::FadeInOut(0.02f, 0.5f);
			return E;
		}

		FMTVFXEmitter SmokePuffs(int32 Burst, float Rate, float SizeMin, float SizeMax, const FC& Color, float Alpha = 0.55f, float Life = 1.6f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::SpriteSmoke, Burst, Rate, Life * 0.7f, Life * 1.3f, SizeMin, SizeMax, Color, Color * 1.2f, 1.f);
			E.Buoyancy = 90.f;
			E.Drag = 1.5f;
			E.SpinMax = 40.f;
			E.SizeOverLife = FMTVFXCurve({ { 0.f, 0.45f }, { 1.f, 1.6f } });
			E.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.12f, Alpha }, { 0.6f, Alpha * 0.6f }, { 1.f, 0.f } });
			E.SpeedMin = 40.f;
			E.SpeedMax = 140.f;
			return E;
		}

		FMTVFXEmitter Debris(int32 Burst, float SpeedMin, float SpeedMax, float SizeMin = 8.f, float SizeMax = 22.f, const TCHAR* Mesh = Paths::RockChunk)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Mesh, Burst, 0.f, 1.4f, 2.4f, SizeMin, SizeMax, FC::White, FC::White, 1.f);
			E.MeshPath = Mesh;
			E.MaterialPath = Paths::MatRock;
			E.SpeedMin = SpeedMin;
			E.SpeedMax = SpeedMax;
			E.Direction = FVector::UpVector;
			E.ConeDeg = 55.f;
			E.Gravity = 980.f;
			E.SpinMax = 540.f;
			E.bBounce = true;
			E.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.85f, 1.f }, { 1.f, 0.f } });
			E.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.8f, 1.f }, { 1.f, 0.1f } });
			return E;
		}

		FMTVFXEmitter Droplets(int32 Burst, float Rate, float SpeedMin, float SpeedMax)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Stretched, Burst, Rate, 0.4f, 0.9f, 3.f, 8.f, WaterFoam, WaterBlue, 1.6f);
			E.SpeedMin = SpeedMin;
			E.SpeedMax = SpeedMax;
			E.Gravity = 980.f;
			E.Stretch = 3.5f;
			E.Direction = FVector::UpVector;
			E.ConeDeg = 60.f;
			E.AlphaOverLife = FMTVFXCurve::FadeInOut(0.02f, 0.4f);
			return E;
		}

		FMTVFXEmitter Motes(float Rate, float Radius, const FC& Color, float Intensity = 6.f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::SpriteAdd, 0, Rate, 0.8f, 1.6f, 3.f, 7.f, Color, Color, Intensity);
			E.Shape = EMTVFXShape::Sphere;
			E.Radius = Radius;
			E.SpeedMin = 20.f;
			E.SpeedMax = 70.f;
			E.Buoyancy = 60.f;
			E.Drag = 1.f;
			return E;
		}

		FMTVFXLight Glow(const FC& Color, float Intensity, float Radius, float Flicker = 0.f, const FMTVFXCurve& Curve = FMTVFXCurve::FadeInOut(0.05f, 0.5f))
		{
			FMTVFXLight L;
			L.Color = Color;
			L.Intensity = Intensity;
			L.Radius = Radius;
			L.Flicker = Flicker;
			L.IntensityOverLife = Curve;
			return L;
		}

		FMTVFXDecal Decal(const TCHAR* Material, float Size, float Lifetime, const FC& Color = FC::White, float Intensity = 1.f, float Delay = 0.f);

		/** Burn mark: charred ground that lasts, plus embers that glow for a moment. */
		void AddScorch(FMTVFXDesc& D, float Size, float Lifetime, float EmberSeconds = 1.6f)
		{
			FMTVFXDecal Char = Decal(Paths::DecalScorch, Size, Lifetime, FC(1.f, 0.4f, 0.08f), 0.f);
			Char.Scalars.Add(TEXT("Char"), 1.f);
			D.Decals.Add(Char);
			FMTVFXDecal Embers = Decal(Paths::DecalScorch, Size, EmberSeconds, FC(1.f, 0.35f, 0.06f), 2.5f);
			Embers.Scalars.Add(TEXT("Char"), 0.f);
			Embers.FadeOut = 1.2f;
			D.Decals.Add(Embers);
		}

		FMTVFXDecal Decal(const TCHAR* Material, float Size, float Lifetime, const FC& Color, float Intensity, float Delay)
		{
			FMTVFXDecal D;
			D.MaterialPath = Material;
			D.Size = Size;
			D.Depth = FMath::Max(150.f, Size);
			D.Lifetime = Lifetime;
			D.Color = Color;
			D.Intensity = Intensity;
			D.Delay = Delay;
			return D;
		}

		FMTVFXShake Shake(float Strength, float Duration = 0.35f, float Delay = 0.f)
		{
			FMTVFXShake S;
			S.Strength = Strength;
			S.Duration = Duration;
			S.Delay = Delay;
			return S;
		}

		FMTVFXDesc Desc(float Duration, bool bLoop = false, float FadeOut = 0.35f)
		{
			FMTVFXDesc D;
			D.Duration = Duration;
			D.bLoop = bLoop;
			D.FadeOut = FadeOut;
			return D;
		}

		/** Particles swirling inward to the hand (formations). */
		FMTVFXEmitter Gather(float Rate, float Radius, const FC& Color, EMTVFXRender Render = EMTVFXRender::SpriteAdd, float Intensity = 5.f)
		{
			FMTVFXEmitter E = Sprites(Render, 0, Rate, 0.3f, 0.5f, 6.f, 14.f, Color, Color * 1.3f, Intensity);
			E.Shape = EMTVFXShape::SphereShell;
			E.Radius = Radius;
			E.SpeedMin = 0.f;
			E.SpeedMax = 20.f;
			E.Attract = 2600.f;
			E.Orbit = 420.f;
			E.Drag = 3.f;
			E.bWorldSpace = false;
			E.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.4f } });
			return E;
		}

		// -------------------------------------------------------------------------------------
		// Fire
		// -------------------------------------------------------------------------------------
		void AddFire(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				FMTVFXDesc D = Desc(0.55f);
				FMTVFXMeshLayer Core = Orb(16.f, FireCore, 14.f, 0.f, 0.4f);
				Core.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 0.9f);
				Core.bWobble = true;
				D.Meshes.Add(Core);
				D.Emitters.Add(Gather(90.f, 55.f, FireOrange));
				FMTVFXEmitter Swirl = Flames(0, 40.f, 10.f, 20.f, 0.3f);
				Swirl.bWorldSpace = false;
				Swirl.Orbit = 500.f;
				Swirl.Shape = EMTVFXShape::Ring;
				Swirl.Radius = 20.f;
				D.Emitters.Add(Swirl);
				D.Lights.Add(Glow(FireOrange, 40.f, 400.f, 0.3f));
				Out.Add(TEXT("Fireball.Formation"), D);
			}
			{
				FMTVFXDesc D = Desc(0.25f, true);
				FMTVFXMeshLayer Core = Orb(17.f, FireCore, 16.f, 0.f, 0.3f);
				Core.bWobble = true;
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Core);
				FMTVFXMeshLayer Shell = Orb(30.f, FireOrange, 5.f, 0.65f, 0.9f);
				Shell.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				Shell.SpinSpeed = 300.f;
				Shell.SpinAxis = FVector(1.f, 0.3f, 0.2f);
				D.Meshes.Add(Shell);
				FMTVFXEmitter Trail = Flames(0, 120.f, 22.f, 44.f, 0.4f);
				Trail.Shape = EMTVFXShape::Sphere;
				Trail.Radius = 12.f;
				Trail.SpeedMin = 20.f;
				Trail.SpeedMax = 80.f;
				D.Emitters.Add(Trail);
				FMTVFXEmitter Embers = Sparks(0, 45.f, 60.f, 220.f, FireOrange, 250.f);
				Embers.LifeMin = 0.4f;
				Embers.LifeMax = 0.9f;
				D.Emitters.Add(Embers);
				D.Emitters.Add(SmokePuffs(0, 22.f, 25.f, 45.f, Smoke, 0.35f, 1.2f));
				D.Lights.Add(Glow(FireOrange, 70.f, 700.f, 0.35f));
				Out.Add(TEXT("Fireball.Travel"), D);
			}
			{
				FMTVFXDesc D = Desc(1.8f);
				FMTVFXMeshLayer Flash = Orb(120.f, FireCore, 10.f, 0.4f, 0.3f);
				Flash.Duration = 0.18f;
				Flash.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.15f }, { 1.f, 1.f } });
				Flash.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Flash);
				FMTVFXMeshLayer Ball = Orb(190.f, FireOrange, 2.2f, 1.f, 1.f);
				Ball.Scalars.Add(TEXT("NoisePower"), 2.2f);
				Ball.Scalars.Add(TEXT("UVScaleX"), 2.5f);
				Ball.Scalars.Add(TEXT("UVScaleY"), 2.5f);
				Ball.Duration = 0.7f;
				Ball.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.3f }, { 0.4f, 0.9f }, { 1.f, 1.f } });
				Ball.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.35f, 0.9f }, { 1.f, 0.f } });
				D.Meshes.Add(Ball);
				D.Meshes.Add(ShockwaveRing(40.f, 380.f, 0.35f, FireCore, 8.f));
				FMTVFXEmitter Burst = Flames(45, 0.f, 45.f, 95.f, 0.7f);
				Burst.bRadial = true;
				Burst.Shape = EMTVFXShape::Sphere;
				Burst.Radius = 30.f;
				Burst.SpeedMin = 250.f;
				Burst.SpeedMax = 700.f;
				Burst.Drag = 3.5f;
				D.Emitters.Add(Burst);
				FMTVFXEmitter Spk = Sparks(60, 0.f, 500.f, 1400.f, FireOrange);
				Spk.bBounce = true;
				D.Emitters.Add(Spk);
				FMTVFXEmitter Plume = SmokePuffs(16, 0.f, 90.f, 180.f, Smoke, 0.6f, 2.4f);
				Plume.Delay = 0.15f;
				Plume.Buoyancy = 160.f;
				Plume.Shape = EMTVFXShape::Sphere;
				Plume.Radius = 80.f;
				D.Emitters.Add(Plume);
				D.Lights.Add(Glow(FireOrange, 700.f, 1400.f, 0.2f, FMTVFXCurve({ { 0.f, 1.f }, { 0.15f, 0.8f }, { 1.f, 0.f } })));
				AddScorch(D, 190.f, 10.f);
				D.Shakes.Add(Shake(0.35f));
				Out.Add(TEXT("Fireball.Impact"), D);
			}
			{
				FMTVFXDesc D = Desc(0.5f);
				FMTVFXEmitter Puff = SmokePuffs(8, 0.f, 25.f, 50.f, Smoke, 0.4f, 0.8f);
				D.Emitters.Add(Puff);
				D.Emitters.Add(Sparks(10, 0.f, 100.f, 300.f, FireOrange, 300.f));
				Out.Add(TEXT("Fireball.Dissipation"), D);
			}
			{
				// Flame Wave: a rolling wall of fire that travels with its zone.
				FMTVFXDesc D = Desc(0.3f, true, 0.6f);
				FMTVFXEmitter Wall = Flames(20, 260.f, 55.f, 120.f, 0.55f);
				Wall.Shape = EMTVFXShape::Line;
				Wall.Extent = FVector(0.f, 300.f, 0.f);
				Wall.SpeedMin = 120.f;
				Wall.SpeedMax = 320.f;
				Wall.ConeDeg = 20.f;
				Wall.bFollowGround = true;
				Wall.Offset = FVector(0.f, 0.f, 40.f);
				D.Emitters.Add(Wall);
				FMTVFXEmitter Embers = Sparks(0, 80.f, 150.f, 450.f, FireOrange, 150.f);
				Embers.Shape = EMTVFXShape::Line;
				Embers.Extent = FVector(0.f, 300.f, 0.f);
				Embers.bRadial = false;
				Embers.Direction = FVector::UpVector;
				Embers.ConeDeg = 35.f;
				D.Emitters.Add(Embers);
				FMTVFXEmitter Plume = SmokePuffs(0, 40.f, 90.f, 200.f, Smoke, 0.45f, 1.8f);
				Plume.Shape = EMTVFXShape::Line;
				Plume.Extent = FVector(0.f, 300.f, 0.f);
				Plume.Offset = FVector(-60.f, 0.f, 160.f);
				D.Emitters.Add(Plume);
				FMTVFXMeshLayer Haze = Layer(Paths::Plane, Paths::MatAir, FVector(320.f, 160.f, 1.f), FC::White, 1.f, FMTVFXCurve::Grow(0.f, 0.4f));
				Haze.Rotation = FRotator(90.f, 0.f, 0.f);
				Haze.Offset = FVector(0.f, 0.f, 150.f);
				Haze.Scalars.Add(TEXT("Distortion"), 0.6f);
				D.Meshes.Add(Haze);
				D.Lights.Add(Glow(FireOrange, 260.f, 1100.f, 0.35f, FMTVFXCurve::Grow(0.f, 0.4f)));
				Out.Add(TEXT("FlameWave.Zone"), D);
			}
			{
				FMTVFXDesc D = Desc(0.4f);
				AddScorch(D, 150.f, 9.f, 1.2f);
				Out.Add(TEXT("FlameWave.Scorch"), D);
			}
			{
				FMTVFXDesc D = Desc(0.5f);
				FMTVFXEmitter Swipe = Flames(30, 0.f, 25.f, 55.f, 0.4f);
				Swipe.Shape = EMTVFXShape::Line;
				Swipe.Extent = FVector(0.f, 60.f, 0.f);
				Swipe.Direction = FVector::ForwardVector;
				Swipe.SpeedMin = 200.f;
				Swipe.SpeedMax = 500.f;
				D.Emitters.Add(Swipe);
				D.Lights.Add(Glow(FireOrange, 80.f, 500.f, 0.3f));
				Out.Add(TEXT("FlameWave.Formation"), D);
			}
			{
				// Inferno: warning circle under the target, then pillars and a vortex.
				FMTVFXDesc D = Desc(0.5f, true, 0.8f);
				FMTVFXDecal Circle = Decal(Paths::DecalCircle, 520.f, 3.2f, FireOrange, 8.f);
				Circle.SpinSpeed = 35.f;
				Circle.bRandomYaw = false;
				Circle.FadeIn = 0.35f;
				Circle.FadeOut = 0.8f;
				D.Decals.Add(Circle);
				FMTVFXEmitter Rise = Motes(70.f, 10.f, FireOrange, 8.f);
				Rise.Shape = EMTVFXShape::Disc;
				Rise.Radius = 480.f;
				Rise.Buoyancy = 180.f;
				Rise.bFollowGround = true;
				D.Emitters.Add(Rise);
				D.Lights.Add(Glow(FireOrange, 120.f, 900.f, 0.2f, FMTVFXCurve::Grow(0.f, 0.5f)));
				Out.Add(TEXT("Inferno.Zone"), D);
			}
			{
				FMTVFXDesc D = Desc(1.3f);
				FMTVFXMeshLayer Pillar = Layer(Paths::Beam, Paths::MatGlow, FVector(30.f, 30.f, 340.f), FireOrange, 0.7f,
					FMTVFXCurve({ { 0.f, 0.f }, { 0.08f, 1.f }, { 0.6f, 0.8f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 0.2f }, { 0.15f, 1.f }, { 1.f, 1.1f } }));
				Pillar.Offset = FVector::ZeroVector;
				Pillar.Scalars.Add(TEXT("FresnelMix"), 0.7f);
				Pillar.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Pillar.Scalars.Add(TEXT("UVScaleX"), 2.f);
				Pillar.Scalars.Add(TEXT("UVScaleY"), 0.5f);
				Pillar.Scalars.Add(TEXT("PanX"), 0.3f);
				Pillar.Scalars.Add(TEXT("PanY"), -1.6f);
				Pillar.Scalars.Add(TEXT("NoisePower"), 2.2f);
				Pillar.SpinSpeed = 220.f;
				D.Meshes.Add(Pillar);
				FMTVFXEmitter Jet = Flames(20, 180.f, 60.f, 130.f, 0.7f);
				Jet.EmitDuration = 0.7f;
				Jet.Shape = EMTVFXShape::Disc;
				Jet.Radius = 60.f;
				Jet.Direction = FVector::UpVector;
				Jet.ConeDeg = 8.f;
				Jet.SpeedMin = 700.f;
				Jet.SpeedMax = 1300.f;
				Jet.Rate = 240.f;
				Jet.Drag = 1.2f;
				D.Emitters.Add(Jet);
				D.Emitters.Add(Sparks(40, 0.f, 400.f, 1200.f, FireOrange));
				FMTVFXEmitter Column = SmokePuffs(6, 25.f, 110.f, 220.f, Smoke, 0.55f, 2.5f);
				Column.EmitDuration = 0.8f;
				Column.Offset = FVector(0.f, 0.f, 500.f);
				Column.Buoyancy = 220.f;
				D.Emitters.Add(Column);
				D.Lights.Add(Glow(FireOrange, 900.f, 1500.f, 0.25f, FMTVFXCurve({ { 0.f, 0.f }, { 0.08f, 1.f }, { 1.f, 0.f } })));
				AddScorch(D, 170.f, 12.f);
				D.Shakes.Add(Shake(0.28f, 0.3f));
				Out.Add(TEXT("Inferno.Eruption"), D);
			}
			{
				FMTVFXDesc D = Desc(2.6f);
				FMTVFXMeshLayer Vortex = Layer(Paths::Funnel, Paths::MatGlow, FVector(260.f, 260.f, 520.f), FireOrange, 1.4f,
					FMTVFXCurve({ { 0.f, 0.f }, { 0.15f, 1.f }, { 0.75f, 0.9f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 0.3f }, { 0.3f, 1.f }, { 1.f, 1.15f } }));
				Vortex.Offset = FVector::ZeroVector;
				Vortex.SpinSpeed = 320.f;
				Vortex.Scalars.Add(TEXT("FresnelMix"), 0.6f);
				Vortex.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Vortex.Scalars.Add(TEXT("UVScaleX"), 3.f);
				Vortex.Scalars.Add(TEXT("UVScaleY"), 0.6f);
				Vortex.Scalars.Add(TEXT("PanX"), 1.1f);
				Vortex.Scalars.Add(TEXT("PanY"), -0.8f);
				Vortex.Scalars.Add(TEXT("NoisePower"), 2.5f);
				D.Meshes.Add(Vortex);
				FMTVFXEmitter Swirl = Flames(0, 160.f, 60.f, 120.f, 0.9f);
				Swirl.EmitDuration = 2.f;
				Swirl.Shape = EMTVFXShape::Ring;
				Swirl.Radius = 220.f;
				Swirl.Orbit = 260.f;
				Swirl.Direction = FVector::UpVector;
				Swirl.ConeDeg = 10.f;
				Swirl.SpeedMin = 300.f;
				Swirl.SpeedMax = 600.f;
				D.Emitters.Add(Swirl);
				FMTVFXEmitter Plume = SmokePuffs(0, 30.f, 160.f, 320.f, Smoke, 0.6f, 3.f);
				Plume.EmitDuration = 2.f;
				Plume.Offset = FVector(0.f, 0.f, 900.f);
				Plume.Shape = EMTVFXShape::Disc;
				Plume.Radius = 250.f;
				Plume.Buoyancy = 200.f;
				D.Emitters.Add(Plume);
				D.Lights.Add(Glow(FireOrange, 1200.f, 2200.f, 0.3f));
				D.Shakes.Add(Shake(0.45f, 0.6f));
				Out.Add(TEXT("Inferno.Vortex"), D);
			}
			{
				FMTVFXDesc D = Desc(0.6f);
				FMTVFXEmitter Up = Flames(24, 0.f, 20.f, 40.f, 0.5f);
				Up.Shape = EMTVFXShape::Ring;
				Up.Radius = 30.f;
				Up.Direction = FVector::UpVector;
				Up.SpeedMin = 200.f;
				Up.SpeedMax = 450.f;
				D.Emitters.Add(Up);
				D.Lights.Add(Glow(FireOrange, 120.f, 600.f, 0.3f));
				Out.Add(TEXT("Inferno.Formation"), D);
			}
		}

		// -------------------------------------------------------------------------------------
		// Water
		// -------------------------------------------------------------------------------------
		FMTVFXMeshLayer WaterBody(const TCHAR* Mesh, const FVector& Size, float Wobble = 1.f)
		{
			FMTVFXMeshLayer L = Layer(Mesh, Paths::MatWater, Size, WaterBlue, 1.f, FMTVFXCurve::Grow(0.f, 0.25f));
			L.bWobble = Wobble > 0.f;
			return L;
		}

		FMTVFXEmitter Mist(int32 Burst, float Rate, float SizeMin, float SizeMax, float Alpha = 0.3f, float Life = 1.2f)
		{
			FMTVFXEmitter E = SmokePuffs(Burst, Rate, SizeMin, SizeMax, WaterFoam, Alpha, Life);
			E.Buoyancy = 30.f;
			return E;
		}

		void AddWater(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				FMTVFXDesc D = Desc(0.5f);
				FMTVFXMeshLayer Ball = WaterBody(Paths::Sphere, FVector(15.f));
				Ball.ScaleOverLife = FMTVFXCurve::Grow(0.15f, 0.9f);
				D.Meshes.Add(Ball);
				FMTVFXEmitter In = Gather(80.f, 60.f, WaterFoam, EMTVFXRender::Stretched, 1.5f);
				D.Emitters.Add(In);
				D.Emitters.Add(Mist(0, 20.f, 15.f, 35.f, 0.25f, 0.6f));
				Out.Add(TEXT("WaterBullet.Formation"), D);
			}
			{
				FMTVFXDesc D = Desc(0.25f, true);
				FMTVFXMeshLayer Ball = WaterBody(Paths::Sphere, FVector(22.f, 17.f, 17.f));
				Ball.SpinSpeed = 540.f;
				Ball.SpinAxis = FVector::ForwardVector;
				D.Meshes.Add(Ball);
				FMTVFXMeshLayer Sheen = Orb(20.f, WaterFoam, 1.2f, 0.85f, 0.6f);
				Sheen.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Sheen);
				FMTVFXEmitter Spray = Droplets(0, 90.f, 60.f, 200.f);
				Spray.Direction = -FVector::ForwardVector;
				Spray.ConeDeg = 35.f;
				D.Emitters.Add(Spray);
				D.Emitters.Add(Mist(0, 35.f, 18.f, 40.f, 0.28f, 0.8f));
				Out.Add(TEXT("WaterBullet.Travel"), D);
			}
			{
				FMTVFXDesc D = Desc(1.2f);
				FMTVFXEmitter Splash = Droplets(70, 0.f, 250.f, 850.f);
				Splash.bBounce = true;
				Splash.Shape = EMTVFXShape::Sphere;
				Splash.Radius = 20.f;
				D.Emitters.Add(Splash);
				D.Meshes.Add(ShockwaveRing(30.f, 260.f, 0.4f, WaterFoam, 1.5f));
				FMTVFXMeshLayer Crown = WaterBody(Paths::Sphere, FVector(90.f, 90.f, 60.f), 1.f);
				Crown.Duration = 0.35f;
				Crown.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.3f }, { 1.f, 1.f } });
				Crown.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Crown);
				D.Emitters.Add(Mist(14, 0.f, 50.f, 130.f, 0.4f, 1.3f));
				D.Decals.Add(Decal(Paths::DecalWet, 170.f, 12.f));
				D.Shakes.Add(Shake(0.15f));
				Out.Add(TEXT("WaterBullet.Impact"), D);
			}
			{
				FMTVFXDesc D = Desc(0.5f);
				D.Emitters.Add(Droplets(20, 0.f, 50.f, 200.f));
				D.Emitters.Add(Mist(6, 0.f, 20.f, 50.f));
				Out.Add(TEXT("WaterBullet.Dissipation"), D);
			}
			{
				// Water Dragon: head leads, the tail is water left along its path, shrinking behind it.
				FMTVFXDesc D = Desc(0.3f, true, 0.5f);
				FMTVFXMeshLayer Head = WaterBody(Paths::DragonHead, FVector(90.f, 160.f, 70.f));
				Head.Rotation = FRotator(0.f, -90.f, 0.f);
				D.Meshes.Add(Head);
				for (const float Side : { -1.f, 1.f })
				{
					FMTVFXMeshLayer Eye = Orb(9.f, Mana, 14.f, 0.f, 0.f);
					Eye.Offset = FVector(95.f, Side * 32.f, 38.f);
					Eye.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
					D.Meshes.Add(Eye);
				}
				FMTVFXEmitter Tail = Sprites(EMTVFXRender::Mesh, 0, 70.f, 0.7f, 0.85f, 60.f, 78.f, FC::White, FC::White, 1.f);
				Tail.MeshPath = Paths::Sphere;
				Tail.MaterialPath = Paths::MatWater;
				Tail.SpeedMin = 0.f;
				Tail.SpeedMax = 10.f;
				Tail.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.15f } });
				Tail.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.8f, 1.f }, { 1.f, 0.f } });
				Tail.MaxParticles = 70;
				D.Emitters.Add(Tail);
				FMTVFXEmitter Spray = Droplets(0, 120.f, 80.f, 300.f);
				Spray.Shape = EMTVFXShape::Sphere;
				Spray.Radius = 60.f;
				D.Emitters.Add(Spray);
				D.Emitters.Add(Mist(0, 40.f, 40.f, 90.f, 0.3f, 1.2f));
				D.Lights.Add(Glow(WaterBlue, 60.f, 700.f));
				Out.Add(TEXT("WaterDragon.Travel"), D);
			}
			{
				FMTVFXDesc D = Desc(0.9f);
				FMTVFXEmitter Spiral = Droplets(0, 160.f, 200.f, 400.f);
				Spiral.Shape = EMTVFXShape::Ring;
				Spiral.Radius = 120.f;
				Spiral.Orbit = 380.f;
				Spiral.Direction = FVector::UpVector;
				Spiral.ConeDeg = 10.f;
				Spiral.Gravity = 200.f;
				Spiral.bWorldSpace = false;
				D.Emitters.Add(Spiral);
				D.Emitters.Add(Mist(0, 30.f, 40.f, 90.f, 0.3f, 0.9f));
				FMTVFXMeshLayer Column = WaterBody(Paths::Cylinder, FVector(60.f, 60.f, 160.f));
				Column.Offset = FVector(0.f, 0.f, 160.f);
				Column.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.1f }, { 0.7f, 1.f }, { 1.f, 0.6f } });
				Column.AlphaOverLife = FMTVFXCurve::FadeInOut(0.2f, 0.3f);
				Column.SpinSpeed = 360.f;
				D.Meshes.Add(Column);
				Out.Add(TEXT("WaterDragon.Formation"), D);
			}
			{
				FMTVFXDesc D = Desc(2.f);
				FMTVFXEmitter Splash = Droplets(150, 0.f, 400.f, 1300.f);
				Splash.Shape = EMTVFXShape::Sphere;
				Splash.Radius = 80.f;
				Splash.bBounce = true;
				Splash.MaxParticles = 170;
				D.Emitters.Add(Splash);
				D.Meshes.Add(ShockwaveRing(80.f, 750.f, 0.55f, WaterFoam, 2.f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 100.f, 600.f, 0.45f, 0.8f));
				FMTVFXMeshLayer Crown = WaterBody(Paths::Sphere, FVector(300.f, 300.f, 200.f));
				Crown.Duration = 0.6f;
				Crown.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
				Crown.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Crown);
				D.Emitters.Add(Mist(24, 0.f, 120.f, 300.f, 0.45f, 2.f));
				D.Decals.Add(Decal(Paths::DecalWet, 450.f, 14.f));
				D.Lights.Add(Glow(WaterBlue, 300.f, 1400.f, 0.f, FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } })));
				D.Shakes.Add(Shake(0.5f, 0.5f));
				Out.Add(TEXT("WaterDragon.Impact"), D);
			}
			{
				// Flood: a breaking wave that rolls forward with its zone.
				FMTVFXDesc D = Desc(0.4f, true, 0.7f);
				FMTVFXMeshLayer Wave = WaterBody(Paths::Cylinder, FVector(170.f, 420.f, 170.f));
				Wave.Rotation = FRotator(0.f, 0.f, 90.f);
				Wave.Offset = FVector(0.f, 0.f, 60.f);
				D.Meshes.Add(Wave);
				FMTVFXEmitter Foam = SmokePuffs(10, 140.f, 50.f, 110.f, WaterFoam, 0.55f, 0.9f);
				Foam.Shape = EMTVFXShape::Line;
				Foam.Extent = FVector(0.f, 420.f, 0.f);
				Foam.Offset = FVector(60.f, 0.f, 200.f);
				Foam.Direction = FVector(1.f, 0.f, 0.5f);
				Foam.SpeedMin = 100.f;
				Foam.SpeedMax = 260.f;
				D.Emitters.Add(Foam);
				FMTVFXEmitter Spray = Droplets(0, 140.f, 200.f, 600.f);
				Spray.Shape = EMTVFXShape::Line;
				Spray.Extent = FVector(0.f, 420.f, 0.f);
				Spray.Offset = FVector(80.f, 0.f, 220.f);
				Spray.Direction = FVector(1.f, 0.f, 1.f);
				D.Emitters.Add(Spray);
				Out.Add(TEXT("Flood.Zone"), D);
			}
			{
				FMTVFXDesc D = Desc(0.4f);
				D.Decals.Add(Decal(Paths::DecalWet, 260.f, 12.f));
				Out.Add(TEXT("Flood.Wet"), D);
			}
			{
				FMTVFXDesc D = Desc(0.7f);
				FMTVFXEmitter Rise = Droplets(0, 120.f, 150.f, 350.f);
				Rise.Shape = EMTVFXShape::Line;
				Rise.Extent = FVector(0.f, 150.f, 0.f);
				D.Emitters.Add(Rise);
				D.Emitters.Add(Mist(10, 0.f, 40.f, 90.f));
				Out.Add(TEXT("Flood.Formation"), D);
			}
		}

		// -------------------------------------------------------------------------------------
		// Wind
		// -------------------------------------------------------------------------------------
		FMTVFXEmitter DustCloud(int32 Burst, float Rate, float SizeMin, float SizeMax, float Alpha = 0.45f, float Life = 1.4f)
		{
			FMTVFXEmitter E = SmokePuffs(Burst, Rate, SizeMin, SizeMax, DustLight, Alpha, Life);
			E.Buoyancy = 40.f;
			return E;
		}

		FMTVFXEmitter Leaves(int32 Burst, float Rate)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::SpriteSmoke, Burst, Rate, 0.8f, 1.6f, 6.f, 12.f, Leaf, Leaf * 1.4f, 1.f);
			E.Texture = Paths::TexDot;
			E.Gravity = 120.f;
			E.Drag = 1.2f;
			E.SpinMax = 600.f;
			E.AlphaOverLife = FMTVFXCurve::FadeInOut(0.05f, 0.3f);
			return E;
		}

		void AddWind(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				FMTVFXDesc D = Desc(0.4f);
				FMTVFXEmitter Swirl = Gather(60.f, 50.f, WindWhite, EMTVFXRender::SpriteSmoke, 1.f);
				Swirl.AlphaOverLife = FMTVFXCurve::FadeInOut(0.2f, 0.4f);
				D.Emitters.Add(Swirl);
				Out.Add(TEXT("WindBlade.Formation"), D);
			}
			{
				FMTVFXDesc D = Desc(0.2f, true);
				FMTVFXMeshLayer Blade = Layer(Paths::Crescent, Paths::MatAir, FVector(50.f, 135.f, 10.f), FC::White, 1.f, FMTVFXCurve::Grow(0.f, 0.3f));
				Blade.Scalars.Add(TEXT("Distortion"), 1.2f);
				D.Meshes.Add(Blade);
				FMTVFXMeshLayer Edge = Layer(Paths::Crescent, Paths::MatGlow, FVector(48.f, 130.f, 8.f), WindWhite, 5.f, FMTVFXCurve::Grow(0.f, 0.3f));
				Edge.Scalars.Add(TEXT("FresnelMix"), 0.8f);
				Edge.Scalars.Add(TEXT("NoiseAmount"), 0.6f);
				D.Meshes.Add(Edge);
				FMTVFXEmitter Trail = DustCloud(0, 45.f, 25.f, 60.f, 0.28f, 0.7f);
				Trail.Shape = EMTVFXShape::Line;
				Trail.Extent = FVector(0.f, 90.f, 0.f);
				D.Emitters.Add(Trail);
				FMTVFXEmitter Streaks = Sprites(EMTVFXRender::Stretched, 0, 60.f, 0.2f, 0.35f, 4.f, 8.f, WindWhite, WindGreen, 2.f);
				Streaks.Shape = EMTVFXShape::Line;
				Streaks.Extent = FVector(0.f, 100.f, 0.f);
				Streaks.Direction = -FVector::ForwardVector;
				Streaks.ConeDeg = 5.f;
				Streaks.SpeedMin = 300.f;
				Streaks.SpeedMax = 600.f;
				Streaks.Stretch = 8.f;
				D.Emitters.Add(Streaks);
				D.Emitters.Add(Leaves(0, 12.f));
				Out.Add(TEXT("WindBlade.Travel"), D);
			}
			{
				FMTVFXDesc D = Desc(0.8f);
				D.Meshes.Add(ShockwaveRing(20.f, 220.f, 0.25f, WindWhite, 3.f, 0.f, 0.06f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 30.f, 200.f, 0.3f, 1.f));
				D.Emitters.Add(DustCloud(12, 0.f, 40.f, 100.f, 0.4f, 0.9f));
				FMTVFXEmitter Foliage = Leaves(14, 0.f);
				Foliage.bRadial = true;
				Foliage.Shape = EMTVFXShape::Sphere;
				Foliage.Radius = 30.f;
				Foliage.SpeedMin = 200.f;
				Foliage.SpeedMax = 500.f;
				D.Emitters.Add(Foliage);
				D.Shakes.Add(Shake(0.12f, 0.2f));
				Out.Add(TEXT("WindBlade.Impact"), D);
			}
			{
				// Tornado: a spinning funnel that drags dust, leaves and debris up and around.
				FMTVFXDesc D = Desc(0.6f, true, 0.8f);
				FMTVFXMeshLayer Funnel = Layer(Paths::Funnel, Paths::MatAir, FVector(240.f, 240.f, 480.f), FC::White, 1.f, FMTVFXCurve::Grow(0.2f, 0.8f));
				Funnel.Offset = FVector::ZeroVector;
				Funnel.SpinSpeed = 480.f;
				Funnel.Scalars.Add(TEXT("Distortion"), 1.4f);
				D.Meshes.Add(Funnel);
				FMTVFXMeshLayer Streaks = Layer(Paths::Funnel, Paths::MatGlow, FVector(230.f, 230.f, 470.f), FC(0.62f, 0.58f, 0.5f), 0.45f, FMTVFXCurve::Grow(0.f, 0.8f));
				Streaks.Offset = FVector::ZeroVector;
				Streaks.SpinSpeed = 620.f;
				Streaks.Scalars.Add(TEXT("FresnelMix"), 0.4f);
				Streaks.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Streaks.Scalars.Add(TEXT("UVScaleX"), 3.f);
				Streaks.Scalars.Add(TEXT("UVScaleY"), 0.6f);
				Streaks.Scalars.Add(TEXT("PanX"), 1.4f);
				Streaks.Scalars.Add(TEXT("PanY"), 0.3f);
				Streaks.Scalars.Add(TEXT("NoisePower"), 3.f);
				D.Meshes.Add(Streaks);
				FMTVFXEmitter Swirl = DustCloud(0, 70.f, 60.f, 140.f, 0.45f, 1.4f);
				Swirl.Shape = EMTVFXShape::Ring;
				Swirl.Radius = 140.f;
				Swirl.Orbit = 300.f;
				Swirl.Direction = FVector::UpVector;
				Swirl.ConeDeg = 15.f;
				Swirl.SpeedMin = 250.f;
				Swirl.SpeedMax = 500.f;
				Swirl.bWorldSpace = false;
				D.Emitters.Add(Swirl);
				FMTVFXEmitter Chunks = Debris(0, 250.f, 500.f, 6.f, 16.f);
				Chunks.Rate = 10.f;
				Chunks.Shape = EMTVFXShape::Ring;
				Chunks.Radius = 150.f;
				Chunks.Orbit = 360.f;
				Chunks.Gravity = -60.f;
				Chunks.bBounce = false;
				Chunks.bWorldSpace = false;
				Chunks.MaxParticles = 30;
				D.Emitters.Add(Chunks);
				FMTVFXEmitter Foliage = Leaves(0, 30.f);
				Foliage.Shape = EMTVFXShape::Ring;
				Foliage.Radius = 180.f;
				Foliage.Orbit = 400.f;
				Foliage.Gravity = -150.f;
				Foliage.bWorldSpace = false;
				D.Emitters.Add(Foliage);
				FMTVFXEmitter Ground = DustCloud(0, 40.f, 80.f, 160.f, 0.35f, 1.f);
				Ground.Shape = EMTVFXShape::Ring;
				Ground.Radius = 120.f;
				Ground.bRadial = true;
				Ground.SpeedMin = 300.f;
				Ground.SpeedMax = 600.f;
				Ground.bFollowGround = true;
				D.Emitters.Add(Ground);
				Out.Add(TEXT("Tornado.Zone"), D);
			}
			{
				FMTVFXDesc D = Desc(0.6f);
				FMTVFXEmitter Rise = DustCloud(0, 80.f, 40.f, 90.f, 0.35f, 0.7f);
				Rise.Shape = EMTVFXShape::Ring;
				Rise.Radius = 80.f;
				Rise.Orbit = 500.f;
				Rise.bWorldSpace = false;
				D.Emitters.Add(Rise);
				Out.Add(TEXT("Tornado.Formation"), D);
			}
			{
				// Wind Burst: compressed air released outward in rings.
				FMTVFXDesc D = Desc(1.1f);
				for (int32 i = 0; i < 3; ++i)
				{
					D.Meshes.Add(ShockwaveRing(40.f, 700.f - i * 130.f, 0.5f, WindWhite, 5.f - i * 1.2f, i * 0.07f, 0.09f));
				}
				D.Meshes.Add(AirPulse(Paths::Sphere, 60.f, 520.f, 0.4f, 1.3f));
				FMTVFXEmitter Ring = DustCloud(44, 0.f, 90.f, 190.f, 0.6f, 1.3f);
				Ring.Shape = EMTVFXShape::Ring;
				Ring.Radius = 60.f;
				Ring.bRadial = true;
				Ring.SpeedMin = 700.f;
				Ring.SpeedMax = 1200.f;
				Ring.Drag = 3.f;
				Ring.bFollowGround = true;
				D.Emitters.Add(Ring);
				FMTVFXEmitter Foliage = Leaves(24, 0.f);
				Foliage.Shape = EMTVFXShape::Ring;
				Foliage.Radius = 80.f;
				Foliage.bRadial = true;
				Foliage.SpeedMin = 500.f;
				Foliage.SpeedMax = 900.f;
				D.Emitters.Add(Foliage);
				D.Shakes.Add(Shake(0.25f, 0.3f));
				Out.Add(TEXT("WindBurst.Impact"), D);
			}
		}

		// -------------------------------------------------------------------------------------
		// Earth (and Rudeus's signature spells)
		// -------------------------------------------------------------------------------------
		FMTVFXMeshLayer RockBody(const TCHAR* Mesh, const FVector& Size, float GlowAmount = 0.f, const FC& GlowColor = Mana)
		{
			FMTVFXMeshLayer L = Layer(Mesh, Paths::MatRock, Size, GlowColor, GlowAmount, FMTVFXCurve::Grow(1.f, 0.01f));
			L.Scalars.Add(TEXT("Glow"), GlowAmount);
			return L;
		}

		void AddEarthImpact(FMTVFXDesc& D, float Radius, int32 Chunks, float ShakeStrength, bool bCrater)
		{
			D.Emitters.Add(Debris(Chunks, 300.f, 900.f * FMath::Sqrt(Radius / 150.f), 6.f, 20.f + Radius * 0.04f));
			FMTVFXEmitter Cloud = DustCloud(FMath::RoundToInt(Radius / 12.f), 0.f, Radius * 0.5f, Radius * 1.3f, 0.6f, 2.f);
			Cloud.Shape = EMTVFXShape::Sphere;
			Cloud.Radius = Radius * 0.3f;
			Cloud.bRadial = true;
			Cloud.SpeedMin = 150.f;
			Cloud.SpeedMax = 450.f;
			D.Emitters.Add(Cloud);
			FMTVFXEmitter Grit = Sparks(30, 0.f, 300.f, 900.f, Dust * 3.f, 980.f);
			Grit.Intensity = 1.5f;
			Grit.Render = EMTVFXRender::Stretched;
			D.Emitters.Add(Grit);
			D.Meshes.Add(ShockwaveRing(Radius * 0.2f, Radius * 2.2f, 0.4f, DustLight, 1.2f));
			D.Decals.Add(Decal(bCrater ? Paths::DecalCracks : Paths::DecalCracks, Radius * (bCrater ? 1.6f : 1.1f), 12.f, FC(0.08f, 0.06f, 0.05f)));
			D.Shakes.Add(Shake(ShakeStrength, 0.4f));
		}

		void AddEarth(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				FMTVFXDesc D = Desc(0.35f);
				FMTVFXEmitter In = Gather(50.f, 45.f, DustLight, EMTVFXRender::SpriteSmoke, 1.f);
				D.Emitters.Add(In);
				FMTVFXMeshLayer Seed = RockBody(Paths::Crystal, FVector(6.f, 6.f, 11.f));
				Seed.ScaleOverLife = FMTVFXCurve::Grow(0.1f, 0.9f);
				D.Meshes.Add(Seed);
				Out.Add(TEXT("StoneBullet.Formation"), D);
			}
			{
				FMTVFXDesc D = Desc(0.2f, true);
				FMTVFXMeshLayer Slug = RockBody(Paths::Crystal, FVector(13.f, 13.f, 24.f));
				Slug.Rotation = FRotator(-90.f, 0.f, 0.f);
				Slug.SpinSpeed = 1440.f;
				Slug.SpinAxis = FVector::ForwardVector;
				D.Meshes.Add(Slug);
				FMTVFXEmitter Trail = DustCloud(0, 40.f, 12.f, 30.f, 0.35f, 0.5f);
				D.Emitters.Add(Trail);
				FMTVFXEmitter Grit = Sprites(EMTVFXRender::Stretched, 0, 40.f, 0.15f, 0.3f, 2.f, 4.f, Dust * 3.f, Dust, 1.5f);
				Grit.Direction = -FVector::ForwardVector;
				Grit.ConeDeg = 10.f;
				Grit.SpeedMin = 200.f;
				Grit.SpeedMax = 500.f;
				D.Emitters.Add(Grit);
				Out.Add(TEXT("StoneBullet.Travel"), D);
			}
			{
				FMTVFXDesc D = Desc(0.8f);
				D.Emitters.Add(Debris(10, 200.f, 500.f, 4.f, 9.f));
				D.Emitters.Add(DustCloud(8, 0.f, 30.f, 70.f, 0.5f, 0.9f));
				D.Meshes.Add(ShockwaveRing(10.f, 90.f, 0.2f, DustLight, 1.f));
				D.Shakes.Add(Shake(0.06f, 0.15f));
				Out.Add(TEXT("StoneBullet.Impact"), D);
			}
			for (const TCHAR* Family : { TEXT("StoneCannon"), TEXT("RudeusCannon") })
			{
				const bool bRudeus = FCString::Strcmp(Family, TEXT("RudeusCannon")) == 0;
				const float ManaGlow = bRudeus ? 6.f : 0.f;
				{
					// Charge: rock gathers, compresses and spins up; Rudeus's version hums with mana.
					FMTVFXDesc D = Desc(1.2f, true, 0.2f);
					FMTVFXMeshLayer Shell = RockBody(Paths::Crystal, FVector(12.f, 12.f, bRudeus ? 22.f : 19.f), ManaGlow);
					Shell.ScaleOverLife = FMTVFXCurve::Grow(0.15f, 1.f);
					Shell.SpinSpeed = 900.f;
					Shell.SpinAxis = FVector::ForwardVector;
					Shell.Rotation = FRotator(-90.f, 0.f, 0.f);
					D.Meshes.Add(Shell);
					FMTVFXEmitter Stones = Debris(0, 0.f, 20.f, 3.f, 7.f);
					Stones.Rate = 26.f;
					Stones.Shape = EMTVFXShape::SphereShell;
					Stones.Radius = 70.f;
					Stones.Attract = 1800.f;
					Stones.Orbit = 520.f;
					Stones.Gravity = 0.f;
					Stones.bBounce = false;
					Stones.bWorldSpace = false;
					Stones.LifeMin = 0.35f;
					Stones.LifeMax = 0.5f;
					Stones.MaxParticles = 24;
					D.Emitters.Add(Stones);
					D.Emitters.Add(Gather(40.f, 60.f, DustLight, EMTVFXRender::SpriteSmoke, 1.f));
					FMTVFXMeshLayer Pressure = AirPulse(Paths::ShockRing, 60.f, 20.f, 0.4f, 1.f, 0.f, 0.05f);
					Pressure.Duration = -1.f;
					Pressure.SpinSpeed = 200.f;
					Pressure.Rotation = FRotator(90.f, 0.f, 0.f);
					Pressure.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.6f);
					D.Meshes.Add(Pressure);
					if (bRudeus)
					{
						FMTVFXMeshLayer Core = Orb(14.f, Mana, 8.f, 0.3f, 0.5f);
						Core.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 1.f);
						D.Meshes.Add(Core);
						D.Emitters.Add(Gather(50.f, 55.f, Mana, EMTVFXRender::SpriteAdd, 6.f));
						D.Lights.Add(Glow(Mana, 60.f, 450.f, 0.2f, FMTVFXCurve::Grow(0.f, 1.f)));
					}
					Out.Add(FName(FString(Family) + TEXT(".Formation")), D);
				}
				{
					FMTVFXDesc D = Desc(0.2f, true);
					FMTVFXMeshLayer Shell = RockBody(Paths::Crystal, FVector(18.f, 18.f, bRudeus ? 36.f : 30.f), ManaGlow);
					Shell.Rotation = FRotator(-90.f, 0.f, 0.f);
					Shell.SpinSpeed = 1800.f;
					Shell.SpinAxis = FVector::ForwardVector;
					D.Meshes.Add(Shell);
					for (int32 i = 0; i < 2; ++i)
					{
						FMTVFXMeshLayer Ring = AirPulse(Paths::ShockRing, 30.f, 60.f + i * 25.f, 0.2f, 1.2f, 0.f, 0.05f);
						Ring.Duration = -1.f;
						Ring.Offset = FVector(-50.f - i * 45.f, 0.f, 0.f);
						Ring.Rotation = FRotator(90.f, 0.f, 0.f);
						Ring.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.5f);
						D.Meshes.Add(Ring);
					}
					FMTVFXEmitter Trail = DustCloud(0, 70.f, 20.f, 50.f, 0.4f, 0.7f);
					D.Emitters.Add(Trail);
					FMTVFXEmitter Grit = Sprites(EMTVFXRender::Stretched, 0, 70.f, 0.2f, 0.35f, 2.f, 5.f, Dust * 3.f, Dust, 1.5f);
					Grit.Direction = -FVector::ForwardVector;
					Grit.ConeDeg = 12.f;
					Grit.SpeedMin = 300.f;
					Grit.SpeedMax = 700.f;
					D.Emitters.Add(Grit);
					if (bRudeus)
					{
						FMTVFXEmitter Sparkle = Sprites(EMTVFXRender::SpriteAdd, 0, 50.f, 0.2f, 0.4f, 4.f, 8.f, Mana, Mana, 8.f);
						Sparkle.Direction = -FVector::ForwardVector;
						Sparkle.ConeDeg = 20.f;
						D.Emitters.Add(Sparkle);
						D.Lights.Add(Glow(Mana, 50.f, 450.f));
					}
					Out.Add(FName(FString(Family) + TEXT(".Travel")), D);
				}
				{
					FMTVFXDesc D = Desc(2.2f);
					AddEarthImpact(D, bRudeus ? 210.f : 160.f, bRudeus ? 32 : 22, bRudeus ? 0.45f : 0.3f, true);
					D.Meshes.Add(AirPulse(Paths::Sphere, 40.f, bRudeus ? 420.f : 320.f, 0.35f, 1.2f));
					if (bRudeus)
					{
						FMTVFXMeshLayer Flash = Orb(120.f, Mana, 10.f, 0.4f, 0.3f);
						Flash.Duration = 0.2f;
						Flash.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
						Flash.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
						D.Meshes.Add(Flash);
						D.Lights.Add(Glow(Mana, 400.f, 1200.f, 0.f, FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } })));
					}
					Out.Add(FName(FString(Family) + TEXT(".Impact")), D);
				}
			}
			{
				// Earth Spikes: one eruption per point along the line.
				FMTVFXDesc D = Desc(1.6f);
				FMTVFXMeshLayer Spike = RockBody(Paths::Spike, FVector(55.f, 55.f, 150.f));
				Spike.Offset = FVector(0.f, 0.f, -10.f);
				Spike.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.05f }, { 0.08f, 1.f }, { 0.8f, 1.f }, { 1.f, 0.05f } });
				Spike.AlphaOverLife = FMTVFXCurve();
				Spike.Rotation = FRotator(0.f, 0.f, 0.f);
				D.Meshes.Add(Spike);
				AddEarthImpact(D, 110.f, 12, 0.18f, false);
				Out.Add(TEXT("EarthSpikes.Eruption"), D);
			}
			{
				FMTVFXDesc D = Desc(0.5f);
				FMTVFXEmitter Crack = DustCloud(12, 0.f, 25.f, 60.f, 0.5f, 0.6f);
				Crack.Shape = EMTVFXShape::Line;
				Crack.Extent = FVector(0.f, 40.f, 0.f);
				D.Emitters.Add(Crack);
				Out.Add(TEXT("EarthSpikes.Formation"), D);
			}
			{
				// Earth Wall: dust and falling pebbles as the slab rises (the slab itself is AMTEarthWall).
				FMTVFXDesc D = Desc(1.6f);
				FMTVFXEmitter Base = DustCloud(24, 0.f, 70.f, 160.f, 0.6f, 1.6f);
				Base.Shape = EMTVFXShape::Line;
				Base.Extent = FVector(0.f, 180.f, 0.f);
				Base.bRadial = true;
				Base.SpeedMin = 150.f;
				Base.SpeedMax = 350.f;
				Base.bFollowGround = true;
				D.Emitters.Add(Base);
				FMTVFXEmitter Pebbles = Debris(0, 0.f, 50.f, 4.f, 9.f);
				Pebbles.Rate = 30.f;
				Pebbles.EmitDuration = 1.2f;
				Pebbles.Delay = 0.2f;
				Pebbles.Shape = EMTVFXShape::Line;
				Pebbles.Extent = FVector(0.f, 170.f, 0.f);
				Pebbles.Offset = FVector(0.f, 0.f, 280.f);
				Pebbles.Direction = -FVector::UpVector;
				D.Emitters.Add(Pebbles);
				D.Decals.Add(Decal(Paths::DecalCracks, 220.f, 14.f, FC(0.08f, 0.06f, 0.05f)));
				D.Shakes.Add(Shake(0.2f, 0.5f));
				Out.Add(TEXT("EarthWall.Rise"), D);
			}
			{
				// Quagmire: bubbling mud and teal ripples over the zone decal.
				FMTVFXDesc D = Desc(0.5f, true, 0.8f);
				FMTVFXEmitter Bubbles = SmokePuffs(0, 30.f, 20.f, 45.f, Mud, 0.7f, 0.9f);
				Bubbles.Shape = EMTVFXShape::Disc;
				Bubbles.Radius = 380.f;
				Bubbles.Buoyancy = 20.f;
				Bubbles.SpeedMin = 10.f;
				Bubbles.SpeedMax = 40.f;
				Bubbles.bFollowGround = true;
				D.Emitters.Add(Bubbles);
				FMTVFXEmitter Ripples = Sprites(EMTVFXRender::Mesh, 0, 3.f, 1.2f, 1.6f, 150.f, 260.f, FC::White, FC::White, 1.f);
				Ripples.MeshPath = Paths::ShockRing;
				Ripples.MaterialPath = Paths::MatGlow;
				Ripples.bFlat = true;
				Ripples.Shape = EMTVFXShape::Disc;
				Ripples.Radius = 250.f;
				Ripples.SpeedMin = 0.f;
				Ripples.SpeedMax = 0.f;
				Ripples.bFollowGround = true;
				Ripples.Offset = FVector(0.f, 0.f, 8.f);
				Ripples.SizeOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
				Ripples.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.2f, 0.5f }, { 1.f, 0.f } });
				Ripples.ColorStart = Teal;
				Ripples.ColorEnd = Teal;
				Ripples.MaxParticles = 8;
				D.Emitters.Add(Ripples);
				D.Emitters.Add(Motes(10.f, 300.f, Teal, 3.f));
				Out.Add(TEXT("Quagmire.Zone"), D);
			}
			{
				FMTVFXDesc D = Desc(0.5f);
				FMTVFXMeshLayer Orb1 = Orb(12.f, Teal, 6.f, 0.3f, 0.5f);
				Orb1.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 0.8f);
				D.Meshes.Add(Orb1);
				D.Emitters.Add(Gather(50.f, 45.f, Teal));
				Out.Add(TEXT("Quagmire.Formation"), D);
			}
		}

		// -------------------------------------------------------------------------------------
		// Rudeus: foresight, barrage, awakening
		// -------------------------------------------------------------------------------------
		void AddRudeus(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				FMTVFXDesc D = Desc(0.8f);
				FMTVFXMeshLayer Flash = Orb(18.f, Mana, 12.f, 0.4f, 0.3f);
				Flash.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 0.3f, 1.f }, { 1.f, 1.4f } });
				D.Meshes.Add(Flash);
				D.Meshes.Add(ShockwaveRing(10.f, 120.f, 0.45f, Mana, 5.f, 0.05f, 0.08f));
				D.Emitters.Add(Motes(40.f, 70.f, Mana));
				D.Lights.Add(Glow(Mana, 80.f, 500.f));
				Out.Add(TEXT("DemonEye.Cast"), D);
			}
			{
				FMTVFXDesc D = Desc(0.5f, true, 0.6f);
				FMTVFXEmitter Orbit = Motes(14.f, 40.f, Mana, 5.f);
				Orbit.Orbit = 180.f;
				Orbit.bWorldSpace = false;
				D.Emitters.Add(Orbit);
				Out.Add(TEXT("DemonEye.Aura"), D);
			}
			{
				FMTVFXDesc D = Desc(0.9f);
				const FC Colors[4] = { DustLight, WindWhite, WaterFoam, FireOrange };
				for (const FC& C : Colors)
				{
					FMTVFXEmitter E = Gather(18.f, 90.f, C, EMTVFXRender::SpriteAdd, 4.f);
					D.Emitters.Add(E);
				}
				Out.Add(TEXT("Barrage.Formation"), D);
			}
			{
				// Quagmire Magician: a maelstrom of mud, stone and mana around Rudeus.
				FMTVFXDesc D = Desc(1.8f);
				FMTVFXMeshLayer Pillar = Layer(Paths::Beam, Paths::MatGlow, FVector(70.f, 70.f, 520.f), Mana, 1.f,
					FMTVFXCurve({ { 0.f, 0.f }, { 0.2f, 0.6f }, { 0.85f, 1.f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 0.3f }, { 0.85f, 1.f }, { 1.f, 1.6f } }));
				Pillar.Offset = FVector::ZeroVector;
				Pillar.Scalars.Add(TEXT("FresnelMix"), 0.7f);
				Pillar.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Pillar.Scalars.Add(TEXT("UVScaleX"), 2.f);
				Pillar.Scalars.Add(TEXT("UVScaleY"), 0.4f);
				Pillar.Scalars.Add(TEXT("PanY"), 1.3f);
				Pillar.Scalars.Add(TEXT("NoisePower"), 2.f);
				Pillar.SpinSpeed = 180.f;
				D.Meshes.Add(Pillar);
				FMTVFXEmitter Stones = Debris(0, 100.f, 300.f, 6.f, 14.f);
				Stones.Rate = 30.f;
				Stones.EmitDuration = 1.5f;
				Stones.Shape = EMTVFXShape::Ring;
				Stones.Radius = 200.f;
				Stones.Orbit = 300.f;
				Stones.Gravity = -80.f;
				Stones.bBounce = false;
				Stones.bWorldSpace = false;
				Stones.MaxParticles = 60;
				D.Emitters.Add(Stones);
				FMTVFXEmitter MudSwirl = SmokePuffs(0, 60.f, 60.f, 130.f, Mud, 0.6f, 1.2f);
				MudSwirl.EmitDuration = 1.5f;
				MudSwirl.Shape = EMTVFXShape::Ring;
				MudSwirl.Radius = 180.f;
				MudSwirl.Orbit = 340.f;
				MudSwirl.bWorldSpace = false;
				D.Emitters.Add(MudSwirl);
				D.Emitters.Add(Motes(60.f, 220.f, Mana, 7.f));
				D.Meshes.Add(ShockwaveRing(50.f, 900.f, 0.5f, Mana, 5.f, 1.5f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 80.f, 700.f, 0.45f, 1.2f, 1.5f));
				D.Lights.Add(Glow(Mana, 500.f, 1500.f, 0.2f));
				D.Decals.Add(Decal(Paths::DecalCracks, 380.f, 14.f, FC(0.06f, 0.05f, 0.04f), 1.f, 1.5f));
				D.Shakes.Add(Shake(0.45f, 0.5f, 1.5f));
				Out.Add(TEXT("QuagmireMagician.Cast"), D);
			}
			{
				FMTVFXDesc D = Desc(0.6f, true, 0.8f);
				FMTVFXEmitter Orbit = Debris(0, 0.f, 10.f, 4.f, 8.f);
				Orbit.Rate = 6.f;
				Orbit.Shape = EMTVFXShape::Ring;
				Orbit.Radius = 90.f;
				Orbit.Orbit = 160.f;
				Orbit.Gravity = 0.f;
				Orbit.bBounce = false;
				Orbit.bWorldSpace = false;
				Orbit.LifeMin = 1.5f;
				Orbit.LifeMax = 2.f;
				Orbit.Offset = FVector(0.f, 0.f, 60.f);
				Orbit.MaxParticles = 14;
				D.Emitters.Add(Orbit);
				D.Emitters.Add(Motes(20.f, 80.f, Mana, 5.f));
				Out.Add(TEXT("QuagmireMagician.Aura"), D);
			}
		}

		// -------------------------------------------------------------------------------------
		// Orsted: force, precision, shockwaves. Muted colours: the power reads through impact.
		// -------------------------------------------------------------------------------------
		void AddOrsted(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				FMTVFXDesc D = Desc(0.5f);
				FMTVFXMeshLayer Ring = AirPulse(Paths::ShockRing, 15.f, 150.f, 0.22f, 1.4f, 0.f, 0.06f);
				Ring.Rotation = FRotator(90.f, 0.f, 0.f);
				D.Meshes.Add(Ring);
				FMTVFXMeshLayer Edge = ShockwaveRing(15.f, 160.f, 0.25f, Silver, 4.f, 0.f, 0.05f);
				Edge.Rotation = FRotator(90.f, 0.f, 0.f);
				D.Meshes.Add(Edge);
				D.Meshes.Add(AirPulse(Paths::Sphere, 10.f, 90.f, 0.18f, 1.2f));
				FMTVFXEmitter Puff = DustCloud(6, 0.f, 25.f, 55.f, 0.35f, 0.6f);
				Puff.Direction = FVector::ForwardVector;
				Puff.ConeDeg = 30.f;
				Puff.SpeedMin = 200.f;
				Puff.SpeedMax = 400.f;
				D.Emitters.Add(Puff);
				D.Shakes.Add(Shake(0.12f, 0.15f));
				Out.Add(TEXT("PalmStrike.Impact"), D);
			}
			{
				FMTVFXDesc D = Desc(0.45f);
				FMTVFXMeshLayer Ring = ShockwaveRing(20.f, 60.f, 0.45f, Gold, 4.f, 0.f, 0.08f);
				Ring.Rotation = FRotator(90.f, 0.f, 0.f);
				Ring.SpinSpeed = 360.f;
				Ring.SpinAxis = FVector::ForwardVector;
				D.Meshes.Add(Ring);
				FMTVFXMeshLayer Palm = Orb(10.f, Silver, 8.f, 0.5f, 0.3f);
				D.Meshes.Add(Palm);
				D.Lights.Add(Glow(Gold, 40.f, 300.f));
				Out.Add(TEXT("DisturbMagic.Cast"), D);
			}
			{
				// A spell collapsing: shards of broken light.
				FMTVFXDesc D = Desc(0.9f);
				FMTVFXMeshLayer Flash = Orb(70.f, Silver, 14.f, 0.3f, 0.2f);
				Flash.Duration = 0.15f;
				Flash.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				Flash.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.3f }, { 1.f, 1.f } });
				D.Meshes.Add(Flash);
				FMTVFXEmitter Shards = Sparks(45, 0.f, 300.f, 900.f, Gold, 500.f);
				Shards.Stretch = 3.f;
				Shards.SizeMin = 5.f;
				Shards.SizeMax = 11.f;
				D.Emitters.Add(Shards);
				FMTVFXEmitter Glitter = Sparks(30, 0.f, 150.f, 500.f, Silver, 200.f);
				D.Emitters.Add(Glitter);
				D.Meshes.Add(ShockwaveRing(20.f, 220.f, 0.3f, Silver, 3.f, 0.f, 0.05f));
				D.Lights.Add(Glow(Silver, 250.f, 700.f, 0.f, FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } })));
				Out.Add(TEXT("DisturbMagic.Impact"), D);
			}
			{
				// Dragon Step: afterimages, dust along the ground, a tear of air behind.
				FMTVFXDesc D = Desc(0.3f, true, 0.3f);
				D.Afterimages.Count = 6;
				D.Afterimages.Interval = 0.045f;
				D.Afterimages.Lifetime = 0.4f;
				D.Afterimages.Color = Silver;
				D.Afterimages.Intensity = 4.f;
				FMTVFXEmitter Kick = DustCloud(0, 70.f, 40.f, 90.f, 0.45f, 0.9f);
				Kick.bFollowGround = true;
				Kick.Offset = FVector(-40.f, 0.f, 10.f);
				Kick.Direction = FVector(-1.f, 0.f, 0.3f);
				Kick.ConeDeg = 35.f;
				D.Emitters.Add(Kick);
				FMTVFXEmitter Streaks = Sprites(EMTVFXRender::Stretched, 0, 50.f, 0.15f, 0.3f, 5.f, 9.f, Silver, Silver, 1.8f);
				Streaks.Shape = EMTVFXShape::Box;
				Streaks.Extent = FVector(20.f, 30.f, 80.f);
				Streaks.Offset = FVector(0.f, 0.f, 90.f);
				Streaks.Direction = -FVector::ForwardVector;
				Streaks.ConeDeg = 4.f;
				Streaks.SpeedMin = 800.f;
				Streaks.SpeedMax = 1400.f;
				Streaks.Stretch = 10.f;
				D.Emitters.Add(Streaks);
				Out.Add(TEXT("DragonStep.Travel"), D);
			}
			{
				FMTVFXDesc D = Desc(0.6f);
				D.Meshes.Add(AirPulse(Paths::Sphere, 30.f, 220.f, 0.25f, 1.3f, 0.f));
				FMTVFXEmitter Ring = DustCloud(16, 0.f, 50.f, 110.f, 0.5f, 0.8f);
				Ring.Shape = EMTVFXShape::Ring;
				Ring.Radius = 40.f;
				Ring.bRadial = true;
				Ring.SpeedMin = 400.f;
				Ring.SpeedMax = 700.f;
				Ring.Drag = 3.f;
				Ring.bFollowGround = true;
				D.Emitters.Add(Ring);
				D.Shakes.Add(Shake(0.12f, 0.2f));
				Out.Add(TEXT("DragonStep.Launch"), D);
			}
			{
				// Dragon Crush: the ground breaks. Force, not colour.
				FMTVFXDesc D = Desc(2.4f);
				for (int32 i = 0; i < 3; ++i)
				{
					D.Meshes.Add(ShockwaveRing(60.f, 950.f - i * 200.f, 0.55f, DustLight, 1.6f - i * 0.3f, i * 0.08f, 0.05f));
					D.Meshes.Add(AirPulse(Paths::ShockRing, 60.f, 900.f - i * 200.f, 0.5f, 1.4f, i * 0.08f, 0.06f));
				}
				D.Meshes.Add(AirPulse(Paths::Sphere, 50.f, 450.f, 0.35f, 1.5f));
				FMTVFXEmitter Chunks = Debris(45, 500.f, 1300.f, 10.f, 34.f);
				Chunks.ConeDeg = 40.f;
				Chunks.MaxParticles = 50;
				D.Emitters.Add(Chunks);
				FMTVFXEmitter Ring = DustCloud(40, 0.f, 120.f, 260.f, 0.65f, 2.2f);
				Ring.Shape = EMTVFXShape::Ring;
				Ring.Radius = 80.f;
				Ring.bRadial = true;
				Ring.SpeedMin = 900.f;
				Ring.SpeedMax = 1500.f;
				Ring.Drag = 2.8f;
				Ring.bFollowGround = true;
				D.Emitters.Add(Ring);
				FMTVFXEmitter Column = DustCloud(14, 0.f, 150.f, 300.f, 0.55f, 2.4f);
				Column.Direction = FVector::UpVector;
				Column.ConeDeg = 25.f;
				Column.SpeedMin = 300.f;
				Column.SpeedMax = 600.f;
				D.Emitters.Add(Column);
				D.Decals.Add(Decal(Paths::DecalCracks, 480.f, 16.f, FC(0.05f, 0.045f, 0.04f)));
				D.Shakes.Add(Shake(0.75f, 0.6f));
				Out.Add(TEXT("DragonCrush.Impact"), D);
			}
			{
				FMTVFXDesc D = Desc(0.5f);
				FMTVFXEmitter Gathering = Gather(40.f, 70.f, Silver, EMTVFXRender::SpriteSmoke, 1.f);
				D.Emitters.Add(Gathering);
				Out.Add(TEXT("DragonCrush.Formation"), D);
			}
			{
				// Saint Dragon Aura: pressure ripples and a faint pale-gold shimmer.
				FMTVFXDesc D = Desc(1.2f);
				D.Meshes.Add(ShockwaveRing(40.f, 600.f, 0.8f, Gold, 2.5f, 0.f, 0.05f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 60.f, 500.f, 0.7f, 1.4f));
				D.Emitters.Add(Motes(60.f, 120.f, Gold, 5.f));
				D.Lights.Add(Glow(Gold, 120.f, 800.f));
				D.Shakes.Add(Shake(0.2f, 0.4f));
				Out.Add(TEXT("DragonAura.Cast"), D);
			}
			{
				FMTVFXDesc D = Desc(0.8f, true, 0.8f);
				FMTVFXEmitter Ripples = Sprites(EMTVFXRender::Mesh, 0, 1.6f, 1.4f, 1.8f, 350.f, 500.f, Gold, Gold, 1.f);
				Ripples.MeshPath = Paths::ShockRing;
				Ripples.MaterialPath = Paths::MatGlow;
				Ripples.Intensity = 1.6f;
				Ripples.bFlat = true;
				Ripples.bWorldSpace = false;
				Ripples.SpeedMin = 0.f;
				Ripples.SpeedMax = 0.f;
				Ripples.Offset = FVector(0.f, 0.f, 20.f);
				Ripples.SizeOverLife = FMTVFXCurve({ { 0.f, 0.15f }, { 1.f, 1.f } });
				Ripples.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.15f, 1.f }, { 1.f, 0.f } });
				Ripples.MaxParticles = 6;
				D.Emitters.Add(Ripples);
				FMTVFXEmitter Rise = Motes(18.f, 80.f, Gold, 4.f);
				Rise.Offset = FVector(0.f, 0.f, 60.f);
				Rise.bWorldSpace = false;
				D.Emitters.Add(Rise);
				FMTVFXMeshLayer Shimmer = Layer(Paths::Cylinder, Paths::MatAir, FVector(70.f, 70.f, 120.f), FC::White, 1.f, FMTVFXCurve::Grow(0.f, 0.5f));
				Shimmer.Offset = FVector(0.f, 0.f, 100.f);
				Shimmer.Scalars.Add(TEXT("Distortion"), 0.5f);
				D.Meshes.Add(Shimmer);
				Out.Add(TEXT("DragonAura.Aura"), D);
			}
			{
				// Dragon God awakening: the aura condenses into him, then releases.
				FMTVFXDesc D = Desc(1.6f);
				FMTVFXEmitter Condense = Sprites(EMTVFXRender::Mesh, 0, 5.f, 0.6f, 0.7f, 500.f, 700.f, Gold, Gold, 1.f);
				Condense.MeshPath = Paths::ShockRing;
				Condense.MaterialPath = Paths::MatGlow;
				Condense.bFlat = true;
				Condense.SpeedMin = 0.f;
				Condense.SpeedMax = 0.f;
				Condense.EmitDuration = 1.1f;
				Condense.Offset = FVector(0.f, 0.f, 20.f);
				Condense.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.05f } });
				Condense.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.3f, 0.8f }, { 1.f, 0.f } });
				Condense.Intensity = 3.f;
				Condense.MaxParticles = 10;
				D.Emitters.Add(Condense);
				FMTVFXEmitter Inward = Gather(60.f, 300.f, Gold, EMTVFXRender::SpriteAdd, 6.f);
				Inward.EmitDuration = 1.1f;
				Inward.Attract = 900.f;
				D.Emitters.Add(Inward);
				FMTVFXMeshLayer Flash = Orb(160.f, Gold, 12.f, 0.5f, 0.3f);
				Flash.Delay = 1.15f;
				Flash.Duration = 0.3f;
				Flash.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
				Flash.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Flash);
				D.Meshes.Add(ShockwaveRing(80.f, 1100.f, 0.5f, Gold, 3.f, 1.15f, 0.05f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 100.f, 800.f, 0.45f, 1.6f, 1.15f));
				FMTVFXEmitter Blast = DustCloud(36, 0.f, 120.f, 260.f, 0.55f, 1.8f);
				Blast.Delay = 1.15f;
				Blast.Shape = EMTVFXShape::Ring;
				Blast.Radius = 80.f;
				Blast.bRadial = true;
				Blast.SpeedMin = 900.f;
				Blast.SpeedMax = 1500.f;
				Blast.Drag = 2.8f;
				Blast.bFollowGround = true;
				D.Emitters.Add(Blast);
				FMTVFXLight Light = Glow(Gold, 900.f, 1600.f, 0.f, FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } }));
				Light.Delay = 1.15f;
				Light.Duration = 0.45f;
				D.Lights.Add(Light);
				D.Decals.Add(Decal(Paths::DecalCracks, 420.f, 16.f, FC(0.05f, 0.045f, 0.04f), 1.f, 1.15f));
				D.Shakes.Add(Shake(0.65f, 0.6f, 1.15f));
				Out.Add(TEXT("DragonGod.Cast"), D);
			}
			{
				FMTVFXDesc D = Desc(0.6f, true, 1.f);
				D.Emitters.Add(Motes(26.f, 90.f, Gold, 6.f));
				FMTVFXMeshLayer Shimmer = Layer(Paths::Cylinder, Paths::MatAir, FVector(80.f, 80.f, 130.f), FC::White, 1.f, FMTVFXCurve::Grow(0.f, 0.5f));
				Shimmer.Offset = FVector(0.f, 0.f, 100.f);
				Shimmer.Scalars.Add(TEXT("Distortion"), 0.8f);
				D.Meshes.Add(Shimmer);
				Out.Add(TEXT("DragonGod.Aura"), D);
			}
		}

		// -------------------------------------------------------------------------------------
		// Shared hit reactions and fallbacks
		// -------------------------------------------------------------------------------------
		void AddShared(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				FMTVFXDesc D = Desc(0.4f);
				D.Emitters.Add(Sparks(14, 0.f, 200.f, 600.f, Silver, 600.f));
				D.Meshes.Add(ShockwaveRing(10.f, 70.f, 0.15f, Silver, 2.f, 0.f, 0.06f));
				Out.Add(TEXT("Hit.Light"), D);
			}
			{
				FMTVFXDesc D = Desc(0.7f);
				D.Emitters.Add(Sparks(26, 0.f, 300.f, 900.f, Silver, 700.f));
				D.Emitters.Add(DustCloud(6, 0.f, 30.f, 70.f, 0.4f, 0.8f));
				D.Meshes.Add(ShockwaveRing(10.f, 130.f, 0.2f, Silver, 2.5f, 0.f, 0.06f));
				D.Shakes.Add(Shake(0.15f, 0.2f));
				Out.Add(TEXT("Hit.Heavy"), D);
			}
			{
				FMTVFXDesc D = Desc(0.4f);
				FMTVFXMeshLayer Flash = Orb(10.f, Mana, 8.f, 0.3f, 0.3f);
				Flash.ScaleOverLife = FMTVFXCurve::Grow(0.3f, 0.5f);
				D.Meshes.Add(Flash);
				D.Emitters.Add(Gather(40.f, 40.f, Mana));
				Out.Add(TEXT("Cast.Generic"), D);
			}
			{
				FMTVFXDesc D = Desc(0.5f);
				FMTVFXEmitter Kick = DustCloud(8, 0.f, 35.f, 70.f, 0.4f, 0.7f);
				Kick.bFollowGround = true;
				Kick.Direction = FVector(-1.f, 0.f, 0.4f);
				D.Emitters.Add(Kick);
				Out.Add(TEXT("Dodge.Dust"), D);
			}
		}

		TMap<FName, FMTVFXDesc> BuildLibrary()
		{
			TMap<FName, FMTVFXDesc> Out;
			AddFire(Out);
			AddWater(Out);
			AddWind(Out);
			AddEarth(Out);
			AddRudeus(Out);
			AddOrsted(Out);
			AddShared(Out);
			return Out;
		}

		const TMap<FName, FMTVFXDesc>& Library()
		{
			static const TMap<FName, FMTVFXDesc> Presets = BuildLibrary();
			return Presets;
		}
	}

	const FMTVFXDesc* FindPreset(FName Name)
	{
		return Name.IsNone() ? nullptr : Library().Find(Name);
	}

	TArray<FName> GetPresetNames()
	{
		TArray<FName> Names;
		Library().GetKeys(Names);
		Names.Sort(FNameLexicalLess());
		return Names;
	}
}

#include "VFX/MTVFXLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/SoftObjectPath.h"

// Every preset is authored at the base visual size of Docs/Ability_Overhaul.md section 5.3 (scale 1 = that size) and
// registered with a literal Out.Add(TEXT("<Preset>.<Phase>"), ...) so Tools/vfx/check_presets.py can list them.
// Docs/LaPlace/VFX.md describes what each one shows, beat by beat.

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
		const TCHAR* SpikeB = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Spike_B.SM_VFX_Spike_B");
		const TCHAR* RockChunk = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_RockChunk_A.SM_VFX_RockChunk_A");
		const TCHAR* RockChunkB = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_RockChunk_C.SM_VFX_RockChunk_C");
		const TCHAR* RockChunkLong = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_RockChunk_B.SM_VFX_RockChunk_B");
		const TCHAR* RockChunkD = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_RockChunk_D.SM_VFX_RockChunk_D");
		const TCHAR* Funnel = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Funnel.SM_VFX_Funnel");
		const TCHAR* Flame = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Flame.SM_VFX_Flame");
		const TCHAR* DragonHead = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_DragonHead.SM_VFX_DragonHead");
		const TCHAR* DragonSegment = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_DragonSegment.SM_VFX_DragonSegment");
		const TCHAR* Beam = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Beam.SM_VFX_Beam");
		const TCHAR* EarthWall = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_EarthWall.SM_VFX_EarthWall");
		const TCHAR* EarthWallB = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_EarthWall_B.SM_VFX_EarthWall_B");
		const TCHAR* EarthWallC = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_EarthWall_C.SM_VFX_EarthWall_C");
		const TCHAR* Crystal = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Crystal.SM_VFX_Crystal");
		const TCHAR* Slug = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Slug.SM_VFX_Slug");
		const TCHAR* Spiral = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Spiral.SM_VFX_Spiral");
		const TCHAR* WaveSheet = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_WaveSheet.SM_VFX_WaveSheet");
		const TCHAR* Slab = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Slab.SM_VFX_Slab");
		const TCHAR* Glyph = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Glyph.SM_VFX_Glyph");
		const TCHAR* ConeShell = TEXT("/Game/LaPlace/Kit/VFX/SM_VFX_Cone.SM_VFX_Cone");

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
		const TCHAR* DecalMud = TEXT("/Game/LaPlace/VFX/Materials/M_Decal_Mud.M_Decal_Mud");
		const TCHAR* DecalCrater = TEXT("/Game/LaPlace/VFX/Materials/M_Decal_Crater.M_Decal_Crater");
		const TCHAR* DecalAimLine = TEXT("/Game/LaPlace/VFX/Materials/M_Decal_AimLine.M_Decal_AimLine");

		const TCHAR* TexDot = TEXT("/Game/LaPlace/VFX/Textures/T_VFX_Dot.T_VFX_Dot");
		const TCHAR* TexPuff = TEXT("/Game/LaPlace/VFX/Textures/T_VFX_Puff.T_VFX_Puff");
		const TCHAR* TexStreak = TEXT("/Game/LaPlace/VFX/Textures/T_VFX_Streak.T_VFX_Streak");
		const TCHAR* TexFlame = TEXT("/Game/LaPlace/VFX/Textures/T_VFX_Flame.T_VFX_Flame");
		const TCHAR* TexFirePuff = TEXT("/Game/LaPlace/VFX/Textures/T_VFX_FirePuff.T_VFX_FirePuff");
		const TCHAR* TexLeaf = TEXT("/Game/LaPlace/VFX/Textures/T_VFX_Leaf.T_VFX_Leaf");
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
			if (Path.Contains(TEXT("Ring")) || Path.Contains(TEXT("Disc")) || Path.Contains(TEXT("Glyph")))
			{
				return Paths::Cylinder;
			}
			if (Path.Contains(TEXT("Spike")) || Path.Contains(TEXT("Funnel")) || Path.Contains(TEXT("Flame")) || Path.Contains(TEXT("Crystal"))
				|| Path.Contains(TEXT("_Cone")))
			{
				return Paths::Cone;
			}
			if (Path.Contains(TEXT("Beam")) || Path.Contains(TEXT("Segment")))
			{
				return Paths::Cylinder;
			}
			if (Path.Contains(TEXT("EarthWall")) || Path.Contains(TEXT("Slab")))
			{
				return Paths::Cube;
			}
			if (Path.Contains(TEXT("Crescent")))
			{
				return Paths::Plane;
			}
			// Slugs, spirals and wave sheets read best as stretched spheres until the kit is imported.
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

	UMaterialInterface* LoadDecalMaterial(const FString& Path)
	{
		if (UMaterialInterface* Mat = LoadCached<UMaterialInterface>(Path))
		{
			return Mat;
		}
		// Newer decals fall back to the closest older one (never to a surface material, which cannot project).
		const TCHAR* Fallback = Path.Contains(TEXT("Mud")) ? Paths::DecalWet
			: Path.Contains(TEXT("Crater")) ? Paths::DecalCracks
			: Path.Contains(TEXT("AimLine")) ? Paths::DecalCircle : nullptr;
		return Fallback ? LoadCached<UMaterialInterface>(Fallback) : nullptr;
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
		const FC FireWhite(1.f, 0.93f, 0.8f);
		const FC FireCore(1.f, 0.85f, 0.55f);
		const FC FireYellow(1.f, 0.62f, 0.18f);
		const FC FireOrange(1.f, 0.38f, 0.07f);
		const FC FireRed(0.85f, 0.1f, 0.02f);
		const FC Smoke(0.09f, 0.085f, 0.08f);
		const FC Soot(0.04f, 0.036f, 0.032f);
		const FC WaterBlue(0.18f, 0.45f, 0.75f);
		const FC WaterFoam(0.85f, 0.93f, 1.f);
		const FC MistWhite(0.78f, 0.84f, 0.88f);
		const FC WindWhite(0.88f, 0.97f, 1.f);
		const FC WindCyan(0.6f, 0.9f, 1.f);
		const FC Dust(0.42f, 0.35f, 0.26f);
		const FC DustLight(0.62f, 0.55f, 0.45f);
		const FC Dirt(0.2f, 0.15f, 0.1f);
		const FC Amber(1.f, 0.62f, 0.28f);
		const FC Mana(0.25f, 0.85f, 1.f);
		const FC ManaWhite(0.8f, 0.96f, 1.f);
		const FC Mud(0.12f, 0.08f, 0.05f);
		const FC MudWet(0.22f, 0.15f, 0.09f);
		const FC Teal(0.15f, 0.75f, 0.7f);
		const FC Silver(0.85f, 0.9f, 1.f);
		const FC Gold(1.f, 0.78f, 0.35f);
		const FC PaleGold(1.f, 0.88f, 0.62f);
		const FC DragonWhite(0.93f, 0.96f, 1.f);
		const FC DeepBlue(0.07f, 0.15f, 0.6f);
		const FC ArcaneBlue(0.6f, 0.8f, 1.f);
		const FC VoidBlack(0.015f, 0.015f, 0.025f);
		const FC Leaf(0.3f, 0.45f, 0.15f);

		// ------------------------------------------------------------------------------ mesh layers

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

		/** M_VFX_Glow noise flow: fire licking upward, wind streaking along a ring. */
		void Flow(FMTVFXMeshLayer& L, float UVScaleX, float UVScaleY, float PanX, float PanY, float NoisePower)
		{
			L.Scalars.Add(TEXT("UVScaleX"), UVScaleX);
			L.Scalars.Add(TEXT("UVScaleY"), UVScaleY);
			L.Scalars.Add(TEXT("PanX"), PanX);
			L.Scalars.Add(TEXT("PanY"), PanY);
			L.Scalars.Add(TEXT("NoisePower"), NoisePower);
		}

		/** A burst sphere: expands from 20% to full over Duration while fading out. */
		FMTVFXMeshLayer Flash(float Radius, const FC& Color, float Intensity, float Duration, float Delay = 0.f)
		{
			FMTVFXMeshLayer L = Orb(Radius, Color, Intensity, 0.35f, 0.25f);
			L.Duration = Duration;
			L.Delay = Delay;
			L.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
			L.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
			return L;
		}

		/** Expanding flat ring (shockwave) in the local XY plane (contracting when From > To). */
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

		/** Expanding ring facing X: a pressure ring around the aim or the travel direction. */
		FMTVFXMeshLayer FacingRing(float FromRadius, float ToRadius, float Duration, const FC& Color, float Intensity, float Delay = 0.f, float Thickness = 0.1f)
		{
			FMTVFXMeshLayer L = ShockwaveRing(FromRadius, ToRadius, Duration, Color, Intensity, Delay, Thickness);
			L.Rotation = FRotator(90.f, 0.f, 0.f);
			return L;
		}

		/** A thin glowing torus (SM_VFX_Ring) of Radius in the local XY plane; stays for loops. */
		FMTVFXMeshLayer ThinRing(float Radius, const FC& Color, float Intensity)
		{
			FMTVFXMeshLayer L = Layer(Paths::Ring, Paths::MatGlow, FVector(Radius, Radius, Radius * 0.035f), Color, Intensity, FMTVFXCurve::Grow(0.f, 0.3f));
			L.Scalars.Add(TEXT("FresnelMix"), 0.2f);
			L.Scalars.Add(TEXT("NoiseAmount"), 0.5f);
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

		/** A ring of warped air facing X (pressure rings along the aim, sonic booms). */
		FMTVFXMeshLayer FacingAir(float FromRadius, float ToRadius, float Duration, float Strength, float Delay = 0.f)
		{
			FMTVFXMeshLayer L = AirPulse(Paths::ShockRing, FromRadius, ToRadius, Duration, Strength, Delay, 0.05f);
			L.Rotation = FRotator(90.f, 0.f, 0.f);
			return L;
		}

		/** A standing distortion (heat haze, compressed air) that fades in and stays: loops. */
		FMTVFXMeshLayer Haze(const TCHAR* Mesh, const FVector& Size, float Strength)
		{
			FMTVFXMeshLayer L = Layer(Mesh, Paths::MatAir, Size, FC::White, 1.f, FMTVFXCurve::Grow(0.f, 0.4f));
			L.Scalars.Add(TEXT("Distortion"), Strength);
			return L;
		}

		/** A ring of black, smoky counter-magic (Disturb Magic), facing X unless bFlat. */
		FMTVFXMeshLayer VoidRing(float FromRadius, float ToRadius, float Duration, float Delay = 0.f, bool bFlat = false)
		{
			FMTVFXMeshLayer L = Layer(Paths::ShockRing, Paths::MatSmoke, FVector(ToRadius, ToRadius, 1.f), VoidBlack, 1.f,
				FMTVFXCurve({ { 0.f, 0.f }, { 0.25f, 0.75f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, FromRadius / ToRadius }, { 1.f, 1.f } }));
			L.Duration = Duration;
			L.Delay = Delay;
			if (!bFlat)
			{
				L.Rotation = FRotator(90.f, 0.f, 0.f);
			}
			return L;
		}

		/** The thin dragon-line glyph (SM_VFX_Glyph), facing X unless bFlat. */
		FMTVFXMeshLayer GlyphLayer(float Radius, const FC& Color, float Intensity, bool bFlat = false)
		{
			FMTVFXMeshLayer L = Layer(Paths::Glyph, Paths::MatGlow, FVector(Radius, Radius, 1.f), Color, Intensity, FMTVFXCurve::Grow(0.f, 0.3f));
			L.Scalars.Add(TEXT("FresnelMix"), 0.f);
			L.Scalars.Add(TEXT("NoiseAmount"), 0.35f);
			if (!bFlat)
			{
				L.Rotation = FRotator(90.f, 0.f, 0.f);
			}
			return L;
		}

		FMTVFXMeshLayer WaterBody(const TCHAR* Mesh, const FVector& Size, const FC& Color = WaterBlue)
		{
			FMTVFXMeshLayer L = Layer(Mesh, Paths::MatWater, Size, Color, 1.f, FMTVFXCurve::Grow(0.f, 0.25f));
			L.bWobble = true;
			return L;
		}

		FMTVFXMeshLayer RockBody(const TCHAR* Mesh, const FVector& Size, float GlowAmount = 0.f, const FC& GlowColor = Mana)
		{
			FMTVFXMeshLayer L = Layer(Mesh, Paths::MatRock, Size, GlowColor, GlowAmount, FMTVFXCurve::Grow(1.f, 0.01f));
			L.Scalars.Add(TEXT("Glow"), GlowAmount);
			return L;
		}

		/** Opaque rock layers cannot fade: this pops them up (with an overshoot), holds, and sinks them away. */
		FMTVFXCurve RiseHoldSink(float RiseAt, float SinkFrom)
		{
			return FMTVFXCurve({ { 0.f, 0.05f }, { RiseAt, 1.08f }, { RiseAt * 1.8f, 1.f }, { SinkFrom, 1.f }, { 1.f, 0.02f } });
		}

		/** Slabs of ground heaved up around a crater (SM_VFX_Slab), their inner edges lifted. */
		void AddRaisedSlabs(FMTVFXDesc& D, int32 Count, float Radius, float SlabHalfWidth, float Tilt, float Duration, int32 Seed)
		{
			FRandomStream Stream(Seed);
			for (int32 i = 0; i < Count; ++i)
			{
				const float Angle = 360.f * (static_cast<float>(i) + Stream.FRandRange(-0.3f, 0.3f)) / static_cast<float>(Count);
				const float Distance = Radius * Stream.FRandRange(0.75f, 1.1f);
				const float HalfWidth = SlabHalfWidth * Stream.FRandRange(0.7f, 1.2f);
				FMTVFXMeshLayer Piece = RockBody(Paths::Slab, FVector(HalfWidth, HalfWidth * Stream.FRandRange(0.7f, 1.f), HalfWidth * 0.28f));
				Piece.Offset = FVector(FMath::Cos(FMath::DegreesToRadians(Angle)) * Distance, FMath::Sin(FMath::DegreesToRadians(Angle)) * Distance, -HalfWidth * 0.12f);
				Piece.Rotation = FRotator(-Tilt * Stream.FRandRange(0.7f, 1.2f), Angle + Stream.FRandRange(-25.f, 25.f), Stream.FRandRange(-12.f, 12.f));
				Piece.Duration = Duration;
				Piece.Delay = Stream.FRandRange(0.f, 0.05f);
				Piece.ScaleOverLife = RiseHoldSink(0.05f, 0.82f);
				Piece.Scalars.Add(TEXT("Crack"), 0.6f);
				D.Meshes.Add(Piece);
			}
		}

		// ------------------------------------------------------------------------------ particles

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
			E.MaxParticles = 0; // sized from burst, rate and life when the library is built (AutoCapacity)
			return E;
		}

		/** Billowing fire bodies (fireball explosions, rolling fire): textured, additive, rising. */
		FMTVFXEmitter FireBody(int32 Burst, float Rate, float SizeMin, float SizeMax, float Life = 0.5f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::SpriteAdd, Burst, Rate, Life * 0.7f, Life * 1.2f, SizeMin, SizeMax, FireYellow, FireRed, 5.f);
			E.Texture = Paths::TexFirePuff;
			E.Buoyancy = 240.f;
			E.Drag = 2.f;
			E.SpinMax = 70.f;
			E.SizeOverLife = FMTVFXCurve({ { 0.f, 0.5f }, { 0.2f, 1.f }, { 1.f, 0.4f } });
			E.AlphaOverLife = FMTVFXCurve::FadeInOut(0.08f, 0.55f);
			return E;
		}

		/** Flame tongues: flame-wisp sprites stretched along their motion (they lick upward as they rise). */
		FMTVFXEmitter FlameWisps(int32 Burst, float Rate, float SizeMin, float SizeMax, float Life = 0.4f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Stretched, Burst, Rate, Life * 0.7f, Life * 1.25f, SizeMin, SizeMax, FireYellow, FireRed, 5.5f);
			E.Texture = Paths::TexFlame;
			E.Stretch = 2.2f;
			E.Buoyancy = 320.f;
			E.Drag = 1.6f;
			E.ConeDeg = 18.f;
			E.SpeedMin = 150.f;
			E.SpeedMax = 380.f;
			E.SizeOverLife = FMTVFXCurve({ { 0.f, 0.55f }, { 0.25f, 1.f }, { 1.f, 0.3f } });
			E.AlphaOverLife = FMTVFXCurve::FadeInOut(0.1f, 0.5f);
			return E;
		}

		/** Glowing embers drifting up with the heat. */
		FMTVFXEmitter EmberDrift(int32 Burst, float Rate, float Radius, float Life = 1.4f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::SpriteAdd, Burst, Rate, Life * 0.6f, Life, 2.5f, 5.f, FireYellow, FireRed, 9.f);
			E.Shape = EMTVFXShape::Sphere;
			E.Radius = Radius;
			E.Buoyancy = 140.f;
			E.Drag = 1.2f;
			E.SpeedMin = 30.f;
			E.SpeedMax = 140.f;
			E.ConeDeg = 60.f;
			E.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.1f, 1.f }, { 0.7f, 0.8f }, { 1.f, 0.f } });
			E.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.5f } });
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

		FMTVFXEmitter DustCloud(int32 Burst, float Rate, float SizeMin, float SizeMax, float Alpha = 0.45f, float Life = 1.4f)
		{
			FMTVFXEmitter E = SmokePuffs(Burst, Rate, SizeMin, SizeMax, DustLight, Alpha, Life);
			E.Buoyancy = 40.f;
			return E;
		}

		FMTVFXEmitter Debris(int32 Burst, float SpeedMin, float SpeedMax, float SizeMin = 8.f, float SizeMax = 22.f, const TCHAR* Mesh = Paths::RockChunk)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Mesh, Burst, 0.f, 1.4f, 2.4f, SizeMin, SizeMax, FC::White, FC::White, 1.f);
			E.MeshPath = Mesh;
			E.MaterialPath = Paths::MatRock;
			E.SpeedMin = SpeedMin;
			E.SpeedMax = SpeedMax;
			E.ConeDeg = 55.f;
			E.Gravity = 980.f;
			E.SpinMax = 540.f;
			E.bBounce = true;
			E.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.85f, 1.f }, { 1.f, 0.f } });
			E.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.8f, 1.f }, { 1.f, 0.1f } });
			return E;
		}

		/** Dark clods of soil thrown by earth hits (lit, not glowing). */
		FMTVFXEmitter DirtClods(int32 Burst, float SpeedMin, float SpeedMax, float SizeMin = 3.f, float SizeMax = 7.f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Stretched, Burst, 0.f, 0.5f, 1.1f, SizeMin, SizeMax, Dirt, Dust, 1.f);
			E.MaterialPath = Paths::MatSmoke;
			E.Texture = Paths::TexDot;
			E.Stretch = 2.2f;
			E.SpeedMin = SpeedMin;
			E.SpeedMax = SpeedMax;
			E.ConeDeg = 50.f;
			E.Gravity = 980.f;
			E.Drag = 0.5f;
			E.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.75f, 1.f }, { 1.f, 0.f } });
			return E;
		}

		FMTVFXEmitter Droplets(int32 Burst, float Rate, float SpeedMin, float SpeedMax)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Stretched, Burst, Rate, 0.4f, 0.9f, 3.f, 8.f, WaterFoam, WaterBlue, 1.6f);
			E.SpeedMin = SpeedMin;
			E.SpeedMax = SpeedMax;
			E.Gravity = 980.f;
			E.Stretch = 3.5f;
			E.ConeDeg = 60.f;
			E.AlphaOverLife = FMTVFXCurve::FadeInOut(0.02f, 0.4f);
			return E;
		}

		/** Real water: refractive, glossy blobs (mesh particles) flung and falling. */
		FMTVFXEmitter WaterBlobs(int32 Burst, float Rate, float SpeedMin, float SpeedMax, float SizeMin = 5.f, float SizeMax = 12.f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Mesh, Burst, Rate, 0.5f, 1.f, SizeMin, SizeMax, WaterBlue, WaterFoam, 1.f);
			E.MeshPath = Paths::Sphere;
			E.MaterialPath = Paths::MatWater;
			E.SpeedMin = SpeedMin;
			E.SpeedMax = SpeedMax;
			E.Gravity = 980.f;
			E.Drag = 0.6f;
			E.ConeDeg = 55.f;
			E.SpinMax = 200.f;
			E.SizeOverLife = FMTVFXCurve({ { 0.f, 0.6f }, { 0.15f, 1.f }, { 1.f, 0.35f } });
			E.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.8f, 1.f }, { 1.f, 0.f } });
			return E;
		}

		/** White water: churned foam that falls back rather than rising like smoke. */
		FMTVFXEmitter Foam(int32 Burst, float Rate, float SizeMin, float SizeMax, float Alpha = 0.55f, float Life = 0.9f)
		{
			FMTVFXEmitter E = SmokePuffs(Burst, Rate, SizeMin, SizeMax, WaterFoam, Alpha, Life);
			E.Buoyancy = 0.f;
			E.Gravity = 250.f;
			E.Drag = 2.2f;
			return E;
		}

		FMTVFXEmitter Mist(int32 Burst, float Rate, float SizeMin, float SizeMax, float Alpha = 0.3f, float Life = 1.2f)
		{
			FMTVFXEmitter E = SmokePuffs(Burst, Rate, SizeMin, SizeMax, MistWhite, Alpha, Life);
			E.Buoyancy = 30.f;
			return E;
		}

		FMTVFXEmitter Leaves(int32 Burst, float Rate)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::SpriteSmoke, Burst, Rate, 0.8f, 1.6f, 8.f, 16.f, Leaf, Leaf * 1.4f, 1.f);
			E.Texture = Paths::TexLeaf;
			E.Gravity = 120.f;
			E.Drag = 1.2f;
			E.SpinMax = 600.f;
			E.AlphaOverLife = FMTVFXCurve::FadeInOut(0.05f, 0.3f);
			return E;
		}

		/** Wind made visible: white streaks of condensed air (lit, not glowing), stretched along the flow. */
		FMTVFXEmitter Condensation(int32 Burst, float Rate, float SizeMin, float SizeMax, float Alpha = 0.35f, float Life = 0.35f)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Stretched, Burst, Rate, Life * 0.7f, Life * 1.2f, SizeMin, SizeMax, WindWhite, WindWhite, 1.f);
			E.MaterialPath = Paths::MatSmoke;
			E.Texture = Paths::TexStreak;
			E.Stretch = 9.f;
			E.Drag = 2.f;
			E.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.15f, Alpha }, { 0.6f, Alpha * 0.7f }, { 1.f, 0.f } });
			return E;
		}

		/** Faint pale-cyan glints riding the air flow. */
		FMTVFXEmitter WindStreaks(int32 Burst, float Rate, float SpeedMin, float SpeedMax, const FVector& Along)
		{
			FMTVFXEmitter E = Sprites(EMTVFXRender::Stretched, Burst, Rate, 0.15f, 0.3f, 3.f, 6.f, WindWhite, WindCyan, 1.6f);
			E.Stretch = 12.f;
			E.SpeedMin = SpeedMin;
			E.SpeedMax = SpeedMax;
			E.Direction = Along;
			E.ConeDeg = 6.f;
			E.AlphaOverLife = FMTVFXCurve::FadeInOut(0.1f, 0.5f);
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

		/** Particles swirling inward to the hand (formations): the magic being drawn in and shaped. */
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

		/** Turns an emitter into an inward rush from a shell of Radius (streams converging on a point). */
		void Converge(FMTVFXEmitter& E, float Radius, float Pull)
		{
			E.Shape = EMTVFXShape::SphereShell;
			E.Radius = Radius;
			E.bRadial = true;
			E.SpeedMin = -Radius * 2.5f;
			E.SpeedMax = -Radius * 1.5f;
			E.Attract = Pull;
			E.Gravity = 0.f;
			E.Buoyancy = 0.f;
			E.Drag = 0.5f;
			E.bWorldSpace = false;
			E.bBounce = false;
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

		/** A short light flash that decays over Duration. */
		FMTVFXLight FlashLight(const FC& Color, float Intensity, float Radius, float Duration)
		{
			FMTVFXLight L = Glow(Color, Intensity, Radius, 0.f, FMTVFXCurve({ { 0.f, 1.f }, { 0.3f, 0.45f }, { 1.f, 0.f } }));
			L.Duration = Duration;
			return L;
		}

		// ------------------------------------------------------------------------------ ground marks

		FMTVFXDecal Decal(const TCHAR* Material, float Size, float Lifetime, const FC& Color = FC::White, float Intensity = 1.f, float Delay = 0.f)
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

		/** Burn mark: charred ground that lasts, plus embers that glow for a moment. */
		void AddScorch(FMTVFXDesc& D, float Size, float Lifetime, float EmberSeconds = 1.6f)
		{
			FMTVFXDecal Char = Decal(Paths::DecalScorch, Size, Lifetime, FC(1.f, 0.4f, 0.08f), 0.f);
			Char.Scalars.Add(TEXT("Char"), 1.f);
			D.Decals.Add(Char);
			FMTVFXDecal Embers = Decal(Paths::DecalScorch, Size, EmberSeconds, FC(1.f, 0.35f, 0.06f), 2.5f);
			Embers.Scalars.Add(TEXT("Char"), 0.f);
			Embers.FadeOut = 1.2f;
			Embers.SortOrder = 1;
			D.Decals.Add(Embers);
		}

		/** A dug crater bowl with a thrown-up rim (M_Decal_Crater), under the cracks. */
		void AddCrater(FMTVFXDesc& D, float Size, float Lifetime)
		{
			FMTVFXDecal Bowl = Decal(Paths::DecalCrater, Size, Lifetime, FC(0.3f, 0.25f, 0.2f));
			Bowl.FadeOut = 3.f;
			D.Decals.Add(Bowl);
		}

		void AddCracks(FMTVFXDesc& D, float Size, float Lifetime, const FC& Color = FC(0.06f, 0.05f, 0.04f), float Delay = 0.f)
		{
			FMTVFXDecal Broken = Decal(Paths::DecalCracks, Size, Lifetime, Color, 1.f, Delay);
			Broken.FadeOut = 3.f;
			Broken.SortOrder = 1;
			D.Decals.Add(Broken);
		}

		void AddWet(FMTVFXDesc& D, float Size, float Lifetime)
		{
			FMTVFXDecal Soaked = Decal(Paths::DecalWet, Size, Lifetime);
			Soaked.FadeOut = 4.f;
			D.Decals.Add(Soaked);
		}

		// ------------------------------------------------------------------------------ camera tiers (section 5.4)

		FMTVFXShake Shake(float Strength, float Duration, float Radius, float Delay = 0.f)
		{
			FMTVFXShake S;
			S.Strength = Strength;
			S.Duration = Duration;
			S.Radius = Radius;
			S.Delay = Delay;
			return S;
		}

		FMTVFXShake MinorShake(float Strength = 0.1f, float Delay = 0.f)
		{
			return Shake(Strength, 0.2f, 2500.f, Delay);
		}

		FMTVFXShake HeavyShake(float Strength = 0.38f, float Delay = 0.f)
		{
			return Shake(Strength, 0.35f, 4000.f, Delay);
		}

		FMTVFXShake UltimateShake(float Strength, float FOVDegrees, float Delay = 0.f)
		{
			FMTVFXShake S = Shake(Strength, 0.5f, 6000.f, Delay);
			S.FOVKick = FOVDegrees;
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

		/** Impacts and ground phases: Z stays up whatever rotation the spawner passes. */
		FMTVFXDesc ImpactDesc(float Duration)
		{
			FMTVFXDesc D = Desc(Duration);
			D.bUpright = true;
			return D;
		}

		/** Ground burst for earth hits (Radius = damage radius): rock chunks, soil clods, a dust cloud, a dust skirt and ring. */
		void AddRockBurst(FMTVFXDesc& D, float Radius, int32 Chunks, float Upward = 1.f)
		{
			const float Reach = FMath::Sqrt(Radius / 150.f);
			FMTVFXEmitter Rocks = Debris(Chunks, 300.f * Upward, 900.f * Upward * Reach, 5.f, 14.f + Radius * 0.04f);
			Rocks.Shape = EMTVFXShape::Sphere;
			Rocks.Radius = Radius * 0.15f;
			Rocks.bHero = true;
			D.Emitters.Add(Rocks);
			FMTVFXEmitter Slabs = Debris(FMath::Max(2, Chunks / 4), 250.f * Upward, 650.f * Upward * Reach, 10.f + Radius * 0.03f, 20.f + Radius * 0.06f, Paths::RockChunkB);
			D.Emitters.Add(Slabs);
			D.Emitters.Add(DirtClods(Chunks, 400.f, 1100.f * Reach));
			FMTVFXEmitter Cloud = DustCloud(FMath::Clamp(FMath::RoundToInt(Radius / 10.f), 8, 40), 0.f, Radius * 0.5f, Radius * 1.2f, 0.55f, 2.2f);
			Cloud.Shape = EMTVFXShape::Sphere;
			Cloud.Radius = Radius * 0.3f;
			Cloud.bRadial = true;
			Cloud.SpeedMin = 150.f;
			Cloud.SpeedMax = 150.f + Radius * 1.5f;
			Cloud.Drag = 2.4f;
			D.Emitters.Add(Cloud);
			FMTVFXEmitter Skirt = DustCloud(FMath::Clamp(FMath::RoundToInt(Radius / 14.f), 8, 36), 0.f, Radius * 0.35f, Radius * 0.8f, 0.45f, 1.6f);
			Skirt.Shape = EMTVFXShape::Ring;
			Skirt.Radius = Radius * 0.2f;
			Skirt.bRadial = true;
			Skirt.SpeedMin = Radius * 2.f;
			Skirt.SpeedMax = Radius * 4.f;
			Skirt.Drag = 3.2f;
			Skirt.bFollowGround = true;
			Skirt.Offset = FVector(0.f, 0.f, 15.f);
			D.Emitters.Add(Skirt);
			D.Meshes.Add(ShockwaveRing(Radius * 0.15f, Radius * 1.15f, 0.4f, DustLight, 1.1f));
		}

		/**
		 * Orsted's style for the element formations ("<Preset>.Formation@Orsted"): the same element, cleaner (half the
		 * particles, dimmer lights, calm hands), with a faint Dragon God accent: a thin pale-gold ring, a deep-blue rim and a
		 * few pale-gold motes.
		 */
		FMTVFXDesc OrstedStyle(const FMTVFXDesc& Base, float AccentRadius)
		{
			FMTVFXDesc D = Base;
			for (FMTVFXEmitter& E : D.Emitters)
			{
				E.Burst = E.Burst > 0 ? FMath::Max(1, FMath::RoundToInt(static_cast<float>(E.Burst) * 0.5f)) : 0;
				E.Rate *= 0.5f;
				if (E.MaxParticles > 0)
				{
					E.MaxParticles = FMath::Max(4, FMath::CeilToInt(static_cast<float>(E.MaxParticles) * 0.6f));
				}
			}
			for (FMTVFXLight& L : D.Lights)
			{
				L.Intensity *= 0.7f;
			}
			D.Charge.Vibration *= 0.3f;
			FMTVFXMeshLayer Accent = ThinRing(AccentRadius, PaleGold, 1.8f);
			Accent.Rotation = FRotator(90.f, 0.f, 0.f);
			Accent.SpinSpeed = 70.f;
			D.Meshes.Add(Accent);
			FMTVFXMeshLayer Rim = Orb(AccentRadius * 0.8f, DeepBlue, 0.9f, 1.f, 0.4f);
			Rim.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.4f);
			D.Meshes.Add(Rim);
			FMTVFXEmitter Specks = Motes(10.f, AccentRadius, PaleGold, 5.f);
			Specks.bWorldSpace = false;
			Specks.SizeMin = 2.f;
			Specks.SizeMax = 4.f;
			D.Emitters.Add(Specks);
			return D;
		}

		// =====================================================================================
		// Fire
		// =====================================================================================
		void AddFire(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Fireball formation: a compressed sphere of fire turning inward in the palm.
				// Charge: bigger, faster, brighter, orange -> yellow -> an almost white core.
				FMTVFXDesc D = Desc(0.45f, true, 0.12f);
				D.Charge.SizeScale = 1.4f;
				D.Charge.IntensityScale = 1.9f;
				D.Charge.SpinScale = 2.3f;
				D.Charge.RateScale = 1.8f;
				D.Charge.TintAtFull = FC(1.f, 0.95f, 0.84f, 0.85f);
				D.Charge.Vibration = 0.5f;
				FMTVFXMeshLayer Core = Orb(10.f, FireCore, 13.f, 0.f, 0.45f);
				Core.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 0.7f);
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.25f);
				Core.bWobble = true;
				D.Meshes.Add(Core);
				FMTVFXMeshLayer Inner = Orb(16.f, FireYellow, 5.f, 0.55f, 1.f);
				Flow(Inner, 2.4f, 2.4f, 1.6f, -0.5f, 2.f);
				Inner.SpinSpeed = 380.f;
				Inner.SpinAxis = FVector(0.25f, 0.2f, 1.f);
				Inner.ScaleOverLife = FMTVFXCurve::Grow(0.3f, 0.8f);
				Inner.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Inner);
				// The outer shell starts wide and closes onto the core: fire drawn inward, not flickering out.
				FMTVFXMeshLayer Shell = Orb(23.f, FireOrange, 2.3f, 0.85f, 1.f);
				Flow(Shell, 2.8f, 2.8f, -1.2f, 0.8f, 2.6f);
				Shell.SpinSpeed = -260.f;
				Shell.SpinAxis = FVector(1.f, 0.3f, 0.2f);
				Shell.ScaleOverLife = FMTVFXCurve({ { 0.f, 1.7f }, { 0.9f, 1.f }, { 1.f, 1.f } });
				Shell.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.4f);
				D.Meshes.Add(Shell);
				for (int32 i = 0; i < 2; ++i)
				{
					// Compression bands snap on as the charge builds.
					FMTVFXMeshLayer Band = ThinRing(27.f + i * 5.f, FireYellow, 3.f);
					Band.Rotation = FRotator(90.f, i * 60.f, 0.f);
					Band.SpinSpeed = i == 0 ? 420.f : -360.f;
					Band.ChargeThreshold = i == 0 ? 0.5f : 0.85f;
					D.Meshes.Add(Band);
				}
				FMTVFXEmitter Inward = FlameWisps(0, 55.f, 7.f, 14.f, 0.32f);
				Inward.Shape = EMTVFXShape::SphereShell;
				Inward.Radius = 48.f;
				Inward.bRadial = true;
				Inward.SpeedMin = -60.f;
				Inward.SpeedMax = -20.f;
				Inward.Attract = 2400.f;
				Inward.Orbit = 480.f;
				Inward.Buoyancy = 0.f;
				Inward.Drag = 2.5f;
				Inward.bWorldSpace = false;
				Inward.bHero = true;
				Inward.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.3f } });
				D.Emitters.Add(Inward);
				FMTVFXEmitter Specks = Gather(36.f, 65.f, FireYellow, EMTVFXRender::SpriteAdd, 9.f);
				Specks.SizeMin = 2.f;
				Specks.SizeMax = 4.f;
				Specks.ColorEnd = FireWhite;
				D.Emitters.Add(Specks);
				D.Meshes.Add(Haze(Paths::Sphere, FVector(34.f), 0.5f));
				D.Lights.Add(Glow(FireOrange, 45.f, 380.f, 0.25f, FMTVFXCurve::Grow(0.f, 0.4f)));
				Out.Add(TEXT("Fireball.Formation"), D);
			}
			{
				// Release: the throw. A flash and a pressure ring at the palm, flame tongues and sparks thrown forward.
				FMTVFXDesc D = Desc(0.45f);
				D.Meshes.Add(Flash(26.f, FireWhite, 10.f, 0.09f));
				D.Meshes.Add(FacingRing(10.f, 95.f, 0.2f, FireYellow, 4.f));
				D.Meshes.Add(FacingAir(10.f, 120.f, 0.22f, 0.9f));
				FMTVFXEmitter Thrown = FlameWisps(14, 0.f, 12.f, 24.f, 0.32f);
				Thrown.Direction = FVector::ForwardVector;
				Thrown.ConeDeg = 18.f;
				Thrown.SpeedMin = 500.f;
				Thrown.SpeedMax = 1100.f;
				Thrown.Drag = 4.f;
				Thrown.Buoyancy = 120.f;
				D.Emitters.Add(Thrown);
				FMTVFXEmitter Spk = Sparks(16, 0.f, 600.f, 1500.f, FireYellow, 300.f);
				Spk.bRadial = false;
				Spk.Direction = FVector::ForwardVector;
				Spk.ConeDeg = 25.f;
				D.Emitters.Add(Spk);
				D.Emitters.Add(SmokePuffs(4, 0.f, 20.f, 45.f, Smoke, 0.3f, 0.8f));
				D.Lights.Add(FlashLight(FireOrange, 160.f, 500.f, 0.3f));
				FMTVFXShake Kick = MinorShake(0.08f);
				Kick.ScaleStrength = 1.4f;
				D.Shakes.Add(Kick);
				Out.Add(TEXT("Fireball.Release"), D);
			}
			{
				// Travel: a white-hot core inside turning shells of fire, licking a trail of flame, embers and smoke.
				FMTVFXDesc D = Desc(0.25f, true, 0.2f);
				FMTVFXMeshLayer Core = Orb(14.f, FireCore, 14.f, 0.f, 0.35f);
				Core.bWobble = true;
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Core);
				FMTVFXMeshLayer Inner = Orb(22.f, FireYellow, 4.5f, 0.5f, 1.f);
				Flow(Inner, 2.2f, 2.2f, 1.8f, -0.6f, 2.f);
				Inner.SpinSpeed = 420.f;
				Inner.SpinAxis = FVector(1.f, 0.3f, 0.2f);
				Inner.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Inner);
				FMTVFXMeshLayer Shell = Orb(30.f, FireOrange, 2.2f, 0.85f, 1.f);
				Shell.Size = FVector(36.f, 30.f, 30.f);
				Flow(Shell, 2.8f, 2.8f, -1.4f, 0.9f, 2.4f);
				Shell.SpinSpeed = -300.f;
				Shell.SpinAxis = FVector::ForwardVector;
				Shell.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Shell);
				D.Meshes.Add(Haze(Paths::Sphere, FVector(46.f, 36.f, 36.f), 0.6f));
				FMTVFXEmitter Tongues = FlameWisps(0, 90.f, 16.f, 32.f, 0.38f);
				Tongues.Shape = EMTVFXShape::Sphere;
				Tongues.Radius = 14.f;
				Tongues.Direction = -FVector::ForwardVector;
				Tongues.ConeDeg = 25.f;
				Tongues.SpeedMin = 60.f;
				Tongues.SpeedMax = 220.f;
				Tongues.Buoyancy = 200.f;
				Tongues.bHero = true;
				D.Emitters.Add(Tongues);
				FMTVFXEmitter Body = FireBody(0, 60.f, 22.f, 42.f, 0.35f);
				Body.Shape = EMTVFXShape::Sphere;
				Body.Radius = 12.f;
				Body.SpeedMin = 20.f;
				Body.SpeedMax = 80.f;
				D.Emitters.Add(Body);
				FMTVFXEmitter Spk = Sparks(0, 40.f, 60.f, 220.f, FireYellow, 250.f);
				Spk.LifeMin = 0.4f;
				Spk.LifeMax = 0.9f;
				D.Emitters.Add(Spk);
				D.Emitters.Add(SmokePuffs(0, 18.f, 25.f, 50.f, Smoke, 0.3f, 1.1f));
				D.Lights.Add(Glow(FireOrange, 80.f, 700.f, 0.35f));
				Out.Add(TEXT("Fireball.Travel"), D);
			}
			{
				// Impact (VR 320): flash, pressure sphere and ring, a fireball of billowing flame, flame tongues and bouncing
				// sparks, a smoke plume and a soot ring, embers drifting up, a burst of light, a scorch mark.
				FMTVFXDesc D = ImpactDesc(2.2f);
				D.Meshes.Add(Flash(120.f, FireWhite, 12.f, 0.12f));
				FMTVFXMeshLayer Ball = Orb(200.f, FireOrange, 2.4f, 1.f, 1.f);
				Flow(Ball, 2.5f, 2.5f, 0.13f, 0.41f, 2.2f);
				Ball.Duration = 0.75f;
				Ball.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.3f }, { 0.35f, 0.9f }, { 1.f, 1.05f } });
				Ball.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.35f, 0.9f }, { 1.f, 0.f } });
				D.Meshes.Add(Ball);
				FMTVFXMeshLayer Heart = Orb(120.f, FireYellow, 6.f, 0.3f, 0.8f);
				Heart.Duration = 0.35f;
				Heart.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.3f }, { 1.f, 1.f } });
				Heart.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Heart);
				D.Meshes.Add(AirPulse(Paths::Sphere, 40.f, 380.f, 0.3f, 1.1f));
				D.Meshes.Add(ShockwaveRing(40.f, 360.f, 0.35f, FireYellow, 6.f, 0.f, 0.1f));
				D.Meshes.Add(AirPulse(Paths::ShockRing, 60.f, 440.f, 0.4f, 1.2f, 0.02f, 0.05f));
				FMTVFXEmitter Body = FireBody(34, 0.f, 60.f, 120.f, 0.7f);
				Body.bRadial = true;
				Body.Shape = EMTVFXShape::Sphere;
				Body.Radius = 40.f;
				Body.SpeedMin = 300.f;
				Body.SpeedMax = 800.f;
				Body.Drag = 3.5f;
				Body.bHero = true;
				D.Emitters.Add(Body);
				FMTVFXEmitter Tongues = FlameWisps(26, 0.f, 20.f, 40.f, 0.45f);
				Tongues.bRadial = true;
				Tongues.Shape = EMTVFXShape::Sphere;
				Tongues.Radius = 30.f;
				Tongues.SpeedMin = 700.f;
				Tongues.SpeedMax = 1400.f;
				Tongues.Drag = 3.f;
				D.Emitters.Add(Tongues);
				FMTVFXEmitter Spk = Sparks(50, 0.f, 500.f, 1400.f, FireYellow);
				Spk.bBounce = true;
				D.Emitters.Add(Spk);
				FMTVFXEmitter Rising = EmberDrift(0, 40.f, 200.f, 1.6f);
				Rising.Delay = 0.3f;
				Rising.EmitDuration = 1.f;
				Rising.bFollowGround = true;
				Rising.Offset = FVector(0.f, 0.f, 20.f);
				D.Emitters.Add(Rising);
				FMTVFXEmitter Plume = SmokePuffs(14, 0.f, 100.f, 200.f, Smoke, 0.55f, 2.4f);
				Plume.Delay = 0.15f;
				Plume.Buoyancy = 170.f;
				Plume.Shape = EMTVFXShape::Sphere;
				Plume.Radius = 90.f;
				D.Emitters.Add(Plume);
				FMTVFXEmitter SootRing = DustCloud(18, 0.f, 80.f, 150.f, 0.35f, 1.4f);
				SootRing.ColorStart = Soot;
				SootRing.ColorEnd = Smoke;
				SootRing.Shape = EMTVFXShape::Ring;
				SootRing.Radius = 60.f;
				SootRing.bRadial = true;
				SootRing.SpeedMin = 300.f;
				SootRing.SpeedMax = 650.f;
				SootRing.Drag = 2.5f;
				SootRing.bFollowGround = true;
				SootRing.Offset = FVector(0.f, 0.f, 20.f);
				D.Emitters.Add(SootRing);
				D.Lights.Add(Glow(FireOrange, 700.f, 1400.f, 0.2f, FMTVFXCurve({ { 0.f, 1.f }, { 0.15f, 0.8f }, { 1.f, 0.f } })));
				AddScorch(D, 220.f, 12.f);
				FMTVFXShake Kick = MinorShake(0.14f);
				Kick.ScaleStrength = 2.2f; // a full charge (x1.75) reaches the heavy tier
				D.Shakes.Add(Kick);
				Out.Add(TEXT("Fireball.Impact"), D);
			}
			{
				// Dissipation: the fire gutters out in a puff of smoke and a few sparks.
				FMTVFXDesc D = Desc(0.6f);
				D.Meshes.Add(Flash(24.f, FireYellow, 4.f, 0.12f));
				FMTVFXEmitter Puff = SmokePuffs(8, 0.f, 25.f, 55.f, Smoke, 0.4f, 0.9f);
				Puff.Shape = EMTVFXShape::Sphere;
				Puff.Radius = 15.f;
				D.Emitters.Add(Puff);
				D.Emitters.Add(Sparks(12, 0.f, 100.f, 320.f, FireYellow, 300.f));
				FMTVFXEmitter Gutter = FireBody(5, 0.f, 20.f, 36.f, 0.25f);
				Gutter.SpeedMin = 30.f;
				Gutter.SpeedMax = 90.f;
				D.Emitters.Add(Gutter);
				Out.Add(TEXT("Fireball.Dissipation"), D);
			}
			{
				// Flame Wave formation: fire gathers along the sweeping arm in a burning arc.
				FMTVFXDesc D = Desc(0.4f, true, 0.15f);
				FMTVFXMeshLayer Arc = Layer(Paths::Crescent, Paths::MatGlow, FVector(26.f, 70.f, 5.f), FireOrange, 3.f, FMTVFXCurve::Grow(0.f, 0.5f),
					FMTVFXCurve::Grow(0.3f, 0.8f));
				Arc.Scalars.Add(TEXT("FresnelMix"), 0.4f);
				Arc.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Flow(Arc, 3.f, 1.f, 2.2f, 0.f, 2.2f);
				Arc.Offset = FVector(20.f, 0.f, 0.f);
				Arc.SpinSpeed = 90.f;
				D.Meshes.Add(Arc);
				FMTVFXEmitter Lick = FlameWisps(0, 70.f, 10.f, 22.f, 0.3f);
				Lick.Shape = EMTVFXShape::Line;
				Lick.Extent = FVector(0.f, 60.f, 0.f);
				Lick.bHero = true;
				D.Emitters.Add(Lick);
				FMTVFXEmitter Drawn = Gather(40.f, 80.f, FireYellow, EMTVFXRender::SpriteAdd, 8.f);
				Drawn.SizeMin = 2.5f;
				Drawn.SizeMax = 5.f;
				D.Emitters.Add(Drawn);
				D.Meshes.Add(Haze(Paths::Sphere, FVector(60.f, 90.f, 40.f), 0.5f));
				D.Lights.Add(Glow(FireOrange, 90.f, 500.f, 0.3f, FMTVFXCurve::Grow(0.f, 0.4f)));
				Out.Add(TEXT("FlameWave.Formation"), D);
			}
			{
				// One of the 11 pieces of the rolling wall of fire, moved along the expanding arc every frame (X = outward,
				// Y = along the arc; 300 wide, 260 high, 180 deep, ground at Z = 0): a curling sheet of flame with a brighter
				// heart, licking tongues along its foot, fire rolling off its crest, embers, smoke and heat haze.
				FMTVFXDesc D = Desc(0.3f, true, 0.5f);
				D.bUpright = true;
				FMTVFXMeshLayer Face = Layer(Paths::WaveSheet, Paths::MatGlow, FVector(90.f, 165.f, 130.f), FireOrange, 2.2f,
					FMTVFXCurve::Grow(0.f, 0.3f), FMTVFXCurve::Grow(0.25f, 0.9f));
				Face.Scalars.Add(TEXT("FresnelMix"), 0.45f);
				Face.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Flow(Face, 2.f, 1.2f, 0.25f, -1.5f, 2.4f);
				Face.bWobble = true;
				D.Meshes.Add(Face);
				FMTVFXMeshLayer Heart = Layer(Paths::WaveSheet, Paths::MatGlow, FVector(60.f, 150.f, 100.f), FireYellow, 3.2f,
					FMTVFXCurve::Grow(0.f, 0.3f), FMTVFXCurve::Grow(0.25f, 0.9f));
				Heart.Scalars.Add(TEXT("FresnelMix"), 0.2f);
				Heart.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Flow(Heart, 2.4f, 1.4f, -0.3f, -1.9f, 2.8f);
				Heart.Offset = FVector(-20.f, 0.f, 0.f);
				D.Meshes.Add(Heart);
				D.Meshes.Add(Haze(Paths::WaveSheet, FVector(100.f, 175.f, 160.f), 0.8f));
				FMTVFXEmitter Tongues = FlameWisps(10, 90.f, 40.f, 85.f, 0.55f);
				Tongues.Shape = EMTVFXShape::Line;
				Tongues.Extent = FVector(0.f, 150.f, 0.f);
				Tongues.Offset = FVector(20.f, 0.f, 30.f);
				Tongues.SpeedMin = 250.f;
				Tongues.SpeedMax = 600.f;
				Tongues.bHero = true;
				D.Emitters.Add(Tongues);
				FMTVFXEmitter Roll = FireBody(0, 45.f, 60.f, 110.f, 0.5f);
				Roll.Shape = EMTVFXShape::Line;
				Roll.Extent = FVector(0.f, 150.f, 0.f);
				Roll.Offset = FVector(70.f, 0.f, 90.f);
				Roll.Direction = FVector(1.f, 0.f, 0.6f);
				Roll.ConeDeg = 25.f;
				Roll.SpeedMin = 150.f;
				Roll.SpeedMax = 350.f;
				D.Emitters.Add(Roll);
				FMTVFXEmitter Rising = EmberDrift(0, 45.f, 0.f, 1.2f);
				Rising.Shape = EMTVFXShape::Line;
				Rising.Extent = FVector(0.f, 150.f, 0.f);
				Rising.Offset = FVector(-20.f, 0.f, 150.f);
				Rising.SpeedMin = 100.f;
				Rising.SpeedMax = 300.f;
				D.Emitters.Add(Rising);
				FMTVFXEmitter Plume = SmokePuffs(0, 20.f, 110.f, 220.f, Smoke, 0.4f, 1.8f);
				Plume.Shape = EMTVFXShape::Line;
				Plume.Extent = FVector(0.f, 150.f, 0.f);
				Plume.Offset = FVector(-70.f, 0.f, 220.f);
				Plume.Buoyancy = 180.f;
				D.Emitters.Add(Plume);
				D.Lights.Add(Glow(FireOrange, 140.f, 800.f, 0.35f, FMTVFXCurve::Grow(0.f, 0.4f)));
				D.Shakes.Add(HeavyShake(0.32f));
				Out.Add(TEXT("FlameWave.Segment"), D);
			}
			{
				// Trail (pooled; 250 wide): a charred scorch with glowing embers settling on it, a few licks of flame and smoke.
				FMTVFXDesc D = ImpactDesc(0.5f);
				FMTVFXDecal Char = Decal(Paths::DecalScorch, 125.f, 10.f, FireOrange, 0.f);
				Char.Scalars.Add(TEXT("Char"), 1.f);
				D.Decals.Add(Char);
				FMTVFXEmitter Glowing = EmberDrift(10, 0.f, 0.f, 2.2f);
				Glowing.Shape = EMTVFXShape::Disc;
				Glowing.Radius = 110.f;
				Glowing.bFollowGround = true;
				Glowing.Offset = FVector(0.f, 0.f, 4.f);
				Glowing.Buoyancy = 0.f;
				Glowing.SpeedMin = 0.f;
				Glowing.SpeedMax = 10.f;
				D.Emitters.Add(Glowing);
				FMTVFXEmitter Rising = EmberDrift(0, 12.f, 0.f, 1.2f);
				Rising.EmitDuration = 0.5f;
				Rising.Shape = EMTVFXShape::Disc;
				Rising.Radius = 100.f;
				Rising.bFollowGround = true;
				D.Emitters.Add(Rising);
				FMTVFXEmitter Licks = FlameWisps(6, 0.f, 18.f, 36.f, 0.3f);
				Licks.Shape = EMTVFXShape::Disc;
				Licks.Radius = 90.f;
				Licks.bFollowGround = true;
				D.Emitters.Add(Licks);
				D.Emitters.Add(SmokePuffs(3, 0.f, 60.f, 110.f, Smoke, 0.25f, 1.6f));
				Out.Add(TEXT("FlameWave.Trail"), D);
			}
			{
				// Dissipation (at the caster's ground point, X = the arc's centre, scale = arc radius / 1300): along the
				// whole 120 degree arc the wall collapses into billowing smoke, shrinking flames and falling embers.
				FMTVFXDesc D = ImpactDesc(1.f);
				FMTVFXEmitter Billow = SmokePuffs(18, 0.f, 150.f, 280.f, Smoke, 0.45f, 2.2f);
				Billow.Shape = EMTVFXShape::Ring;
				Billow.Radius = 1300.f;
				Billow.ArcDegrees = 120.f;
				Billow.bFollowGround = true;
				Billow.Offset = FVector(0.f, 0.f, 140.f);
				D.Emitters.Add(Billow);
				FMTVFXEmitter Dying = FlameWisps(26, 0.f, 40.f, 80.f, 0.4f);
				Dying.Shape = EMTVFXShape::Ring;
				Dying.Radius = 1300.f;
				Dying.ArcDegrees = 120.f;
				Dying.bFollowGround = true;
				Dying.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.2f } });
				D.Emitters.Add(Dying);
				FMTVFXEmitter Falling = EmberDrift(30, 0.f, 0.f, 1.4f);
				Falling.Shape = EMTVFXShape::Ring;
				Falling.Radius = 1300.f;
				Falling.ArcDegrees = 120.f;
				Falling.bFollowGround = true;
				Falling.Offset = FVector(0.f, 0.f, 180.f);
				Falling.Buoyancy = -60.f;
				D.Emitters.Add(Falling);
				Out.Add(TEXT("FlameWave.Dissipation"), D);
			}
			{
				// Inferno formation: fire spirals up the raised arm into an intense core; the ground around the caster heats.
				FMTVFXDesc D = Desc(0.8f, true, 0.2f);
				FMTVFXMeshLayer Core = Orb(12.f, FireCore, 12.f, 0.f, 0.4f);
				Core.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 0.8f);
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.2f);
				Core.bWobble = true;
				D.Meshes.Add(Core);
				FMTVFXMeshLayer Crown = Orb(22.f, FireOrange, 2.5f, 0.85f, 1.f);
				Flow(Crown, 2.5f, 2.5f, 0.4f, -1.4f, 2.4f);
				Crown.ScaleOverLife = FMTVFXCurve::Grow(0.3f, 0.9f);
				Crown.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				Crown.SpinSpeed = 260.f;
				D.Meshes.Add(Crown);
				FMTVFXEmitter Spiral = FlameWisps(0, 60.f, 12.f, 26.f, 0.45f);
				Spiral.Shape = EMTVFXShape::Ring;
				Spiral.Radius = 45.f;
				Spiral.Orbit = 420.f;
				Spiral.Attract = 400.f;
				Spiral.SpeedMin = 80.f;
				Spiral.SpeedMax = 200.f;
				Spiral.bWorldSpace = false;
				Spiral.bHero = true;
				D.Emitters.Add(Spiral);
				FMTVFXEmitter Heat = EmberDrift(0, 40.f, 0.f, 1.6f);
				Heat.Shape = EMTVFXShape::Disc;
				Heat.Radius = 260.f;
				Heat.bFollowGround = true;
				Heat.Buoyancy = 220.f;
				D.Emitters.Add(Heat);
				FMTVFXEmitter Drawn = Gather(40.f, 120.f, FireYellow, EMTVFXRender::SpriteAdd, 8.f);
				Drawn.SizeMin = 2.5f;
				Drawn.SizeMax = 5.f;
				Drawn.Attract = 1800.f;
				D.Emitters.Add(Drawn);
				D.Meshes.Add(Haze(Paths::Sphere, FVector(60.f), 0.6f));
				D.Lights.Add(Glow(FireOrange, 120.f, 700.f, 0.3f, FMTVFXCurve::Grow(0.f, 0.45f)));
				Out.Add(TEXT("Inferno.Formation"), D);
			}
			{
				// Zone (VR 1100): two counter-turning rune circles, a ring of fire at the boundary and an inner ring, embers
				// rising across the area, a wall of smoke at the edge, heat shimmer, a big warm light; the ultimate shake
				// lands with the first pillars (0.6 s).
				FMTVFXDesc D = Desc(0.6f, true, 1.f);
				D.bUpright = true;
				FMTVFXDecal Outer = Decal(Paths::DecalCircle, 1100.f, 0.f, FireOrange, 6.f);
				Outer.bUntilStop = true;
				Outer.SpinSpeed = 22.f;
				Outer.bRandomYaw = false;
				Outer.FadeIn = 0.4f;
				Outer.FadeOut = 1.f;
				Outer.Depth = 500.f;
				D.Decals.Add(Outer);
				FMTVFXDecal Inner = Decal(Paths::DecalCircle, 640.f, 0.f, FireYellow, 5.f, 0.15f);
				Inner.bUntilStop = true;
				Inner.SpinSpeed = -38.f;
				Inner.bRandomYaw = false;
				Inner.FadeIn = 0.3f;
				Inner.FadeOut = 0.8f;
				Inner.Depth = 400.f;
				Inner.SortOrder = 1;
				D.Decals.Add(Inner);
				FMTVFXEmitter Boundary = FlameWisps(0, 110.f, 30.f, 70.f, 0.55f);
				Boundary.Shape = EMTVFXShape::Ring;
				Boundary.Radius = 1100.f;
				Boundary.bFollowGround = true;
				Boundary.Delay = 0.1f;
				Boundary.SpeedMin = 150.f;
				Boundary.SpeedMax = 400.f;
				Boundary.bHero = true;
				Boundary.MaxParticles = 90;
				D.Emitters.Add(Boundary);
				FMTVFXEmitter InnerRing = FlameWisps(0, 45.f, 20.f, 45.f, 0.45f);
				InnerRing.Shape = EMTVFXShape::Ring;
				InnerRing.Radius = 640.f;
				InnerRing.bFollowGround = true;
				InnerRing.Delay = 0.2f;
				D.Emitters.Add(InnerRing);
				FMTVFXEmitter Rising = EmberDrift(0, 60.f, 0.f, 1.8f);
				Rising.Shape = EMTVFXShape::Disc;
				Rising.Radius = 1050.f;
				Rising.bFollowGround = true;
				Rising.Buoyancy = 220.f;
				D.Emitters.Add(Rising);
				FMTVFXEmitter Wall = SmokePuffs(0, 10.f, 150.f, 300.f, Smoke, 0.35f, 2.5f);
				Wall.Shape = EMTVFXShape::Ring;
				Wall.Radius = 1100.f;
				Wall.bFollowGround = true;
				Wall.Offset = FVector(0.f, 0.f, 160.f);
				Wall.Buoyancy = 150.f;
				D.Emitters.Add(Wall);
				FMTVFXMeshLayer Shimmer = Haze(Paths::Disc, FVector(1100.f, 1100.f, 2.f), 0.3f);
				Shimmer.Offset = FVector(0.f, 0.f, 40.f);
				D.Meshes.Add(Shimmer);
				D.Lights.Add(Glow(FireOrange, 180.f, 1800.f, 0.3f, FMTVFXCurve::Grow(0.f, 0.4f)));
				D.Shakes.Add(UltimateShake(0.6f, 4.5f, 0.6f));
				Out.Add(TEXT("Inferno.Zone"), D);
			}
			{
				// Vortex (centre): a turning funnel of fire with a brighter core, flames and embers spiralling up, a glowing
				// base ring, a smoke plume at the top and a charred centre.
				FMTVFXDesc D = ImpactDesc(4.2f);
				D.FadeOut = 0.6f;
				const FMTVFXCurve VortexAlpha({ { 0.f, 0.f }, { 0.1f, 1.f }, { 0.85f, 0.9f }, { 1.f, 0.f } });
				const FMTVFXCurve VortexScale({ { 0.f, 0.3f }, { 0.2f, 1.f }, { 1.f, 1.15f } });
				FMTVFXMeshLayer Vortex = Layer(Paths::Funnel, Paths::MatGlow, FVector(260.f, 260.f, 520.f), FireOrange, 1.4f, VortexAlpha, VortexScale);
				Vortex.SpinSpeed = 320.f;
				Vortex.Scalars.Add(TEXT("FresnelMix"), 0.6f);
				Vortex.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Flow(Vortex, 3.f, 0.6f, 1.1f, -0.8f, 2.5f);
				D.Meshes.Add(Vortex);
				FMTVFXMeshLayer Heart = Layer(Paths::Funnel, Paths::MatGlow, FVector(150.f, 150.f, 470.f), FireYellow, 2.f, VortexAlpha, VortexScale);
				Heart.SpinSpeed = 480.f;
				Heart.Scalars.Add(TEXT("FresnelMix"), 0.3f);
				Heart.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Flow(Heart, 3.f, 0.8f, 1.6f, -1.3f, 2.8f);
				D.Meshes.Add(Heart);
				FMTVFXMeshLayer Footing = ThinRing(260.f, FireYellow, 4.f);
				Footing.AlphaOverLife = FMTVFXCurve::FadeInOut(0.08f, 0.2f);
				Footing.SpinSpeed = 120.f;
				Footing.Offset = FVector(0.f, 0.f, 15.f);
				D.Meshes.Add(Footing);
				FMTVFXEmitter Swirl = FlameWisps(0, 150.f, 50.f, 110.f, 0.8f);
				Swirl.EmitDuration = 3.6f;
				Swirl.Shape = EMTVFXShape::Ring;
				Swirl.Radius = 200.f;
				Swirl.Orbit = 280.f;
				Swirl.ConeDeg = 10.f;
				Swirl.SpeedMin = 350.f;
				Swirl.SpeedMax = 700.f;
				Swirl.Buoyancy = 150.f;
				Swirl.bHero = true;
				Swirl.MaxParticles = 140;
				D.Emitters.Add(Swirl);
				FMTVFXEmitter Sparkles = EmberDrift(0, 70.f, 0.f, 1.6f);
				Sparkles.EmitDuration = 3.6f;
				Sparkles.Shape = EMTVFXShape::Ring;
				Sparkles.Radius = 150.f;
				Sparkles.Orbit = 400.f;
				Sparkles.ConeDeg = 15.f;
				Sparkles.SpeedMin = 300.f;
				Sparkles.SpeedMax = 700.f;
				D.Emitters.Add(Sparkles);
				FMTVFXEmitter Plume = SmokePuffs(0, 30.f, 160.f, 320.f, Smoke, 0.6f, 3.f);
				Plume.EmitDuration = 3.6f;
				Plume.Offset = FVector(0.f, 0.f, 900.f);
				Plume.Shape = EMTVFXShape::Disc;
				Plume.Radius = 250.f;
				Plume.Buoyancy = 200.f;
				D.Emitters.Add(Plume);
				D.Lights.Add(Glow(FireOrange, 1200.f, 2200.f, 0.3f, FMTVFXCurve::FadeInOut(0.1f, 0.2f)));
				AddScorch(D, 360.f, 16.f);
				Out.Add(TEXT("Inferno.Vortex"), D);
			}
			{
				// Eruption (pillar VR 190, 900 high): the ground bursts, a spinning column of fire with a white-hot heart
				// shoots up and mushrooms at the top; sparks, chips, a smoke column, a flash of light and a scorch.
				FMTVFXDesc D = ImpactDesc(1.4f);
				const FMTVFXCurve PillarAlpha({ { 0.f, 0.f }, { 0.06f, 1.f }, { 0.6f, 0.8f }, { 1.f, 0.f } });
				const FMTVFXCurve PillarScale({ { 0.f, 0.15f }, { 0.12f, 1.f }, { 1.f, 1.1f } });
				FMTVFXMeshLayer Pillar = Layer(Paths::Beam, Paths::MatGlow, FVector(70.f, 70.f, 450.f), FireOrange, 0.8f, PillarAlpha, PillarScale);
				Pillar.Scalars.Add(TEXT("FresnelMix"), 0.7f);
				Pillar.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Flow(Pillar, 2.f, 0.5f, 0.3f, -2.2f, 2.2f);
				Pillar.SpinSpeed = 220.f;
				D.Meshes.Add(Pillar);
				FMTVFXMeshLayer Heart = Layer(Paths::Beam, Paths::MatGlow, FVector(32.f, 32.f, 470.f), FireYellow, 2.2f, PillarAlpha, PillarScale);
				Heart.Scalars.Add(TEXT("FresnelMix"), 0.2f);
				Heart.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Flow(Heart, 2.f, 0.6f, -0.4f, -2.8f, 2.6f);
				Heart.SpinSpeed = -300.f;
				D.Meshes.Add(Heart);
				D.Meshes.Add(Flash(90.f, FireWhite, 8.f, 0.1f));
				D.Meshes.Add(ShockwaveRing(30.f, 220.f, 0.3f, FireYellow, 4.f));
				FMTVFXEmitter Jet = FlameWisps(16, 220.f, 50.f, 110.f, 0.7f);
				Jet.EmitDuration = 0.7f;
				Jet.Shape = EMTVFXShape::Disc;
				Jet.Radius = 70.f;
				Jet.ConeDeg = 8.f;
				Jet.SpeedMin = 900.f;
				Jet.SpeedMax = 1600.f;
				Jet.Drag = 1.f;
				Jet.bHero = true;
				Jet.MaxParticles = 170;
				D.Emitters.Add(Jet);
				FMTVFXEmitter Body = FireBody(8, 80.f, 80.f, 150.f, 0.7f);
				Body.EmitDuration = 0.6f;
				Body.Shape = EMTVFXShape::Disc;
				Body.Radius = 60.f;
				Body.ConeDeg = 10.f;
				Body.SpeedMin = 600.f;
				Body.SpeedMax = 1100.f;
				D.Emitters.Add(Body);
				FMTVFXEmitter Crown = FireBody(10, 0.f, 90.f, 160.f, 0.8f);
				Crown.Delay = 0.12f;
				Crown.Offset = FVector(0.f, 0.f, 820.f);
				Crown.bRadial = true;
				Crown.Shape = EMTVFXShape::Sphere;
				Crown.Radius = 40.f;
				Crown.SpeedMin = 200.f;
				Crown.SpeedMax = 450.f;
				D.Emitters.Add(Crown);
				D.Emitters.Add(Sparks(40, 0.f, 400.f, 1200.f, FireYellow));
				D.Emitters.Add(Debris(8, 300.f, 700.f, 5.f, 12.f));
				FMTVFXEmitter Column = SmokePuffs(6, 25.f, 110.f, 220.f, Smoke, 0.55f, 2.5f);
				Column.EmitDuration = 0.8f;
				Column.Offset = FVector(0.f, 0.f, 500.f);
				Column.Buoyancy = 220.f;
				D.Emitters.Add(Column);
				D.Lights.Add(Glow(FireOrange, 900.f, 1500.f, 0.25f, FMTVFXCurve({ { 0.f, 0.f }, { 0.06f, 1.f }, { 1.f, 0.f } })));
				AddScorch(D, 190.f, 12.f);
				D.Shakes.Add(MinorShake(0.12f));
				Out.Add(TEXT("Inferno.Eruption"), D);
			}
			{
				// Dissipation (VR 1100): the circle smoulders out: smoke over the area, the boundary flames shrinking, last embers.
				FMTVFXDesc D = ImpactDesc(1.2f);
				FMTVFXEmitter Smoulder = SmokePuffs(20, 8.f, 200.f, 400.f, Smoke, 0.4f, 3.f);
				Smoulder.EmitDuration = 1.f;
				Smoulder.Shape = EMTVFXShape::Disc;
				Smoulder.Radius = 1000.f;
				Smoulder.bFollowGround = true;
				Smoulder.Offset = FVector(0.f, 0.f, 60.f);
				Smoulder.Buoyancy = 120.f;
				D.Emitters.Add(Smoulder);
				FMTVFXEmitter Dying = FlameWisps(40, 0.f, 25.f, 55.f, 0.4f);
				Dying.Shape = EMTVFXShape::Ring;
				Dying.Radius = 1100.f;
				Dying.bFollowGround = true;
				Dying.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.2f } });
				D.Emitters.Add(Dying);
				FMTVFXEmitter Last = EmberDrift(40, 0.f, 0.f, 1.6f);
				Last.Shape = EMTVFXShape::Disc;
				Last.Radius = 1000.f;
				Last.bFollowGround = true;
				D.Emitters.Add(Last);
				Out.Add(TEXT("Inferno.Dissipation"), D);
			}
		}

		// =====================================================================================
		// Water: real water (refractive bodies, droplets, blobs, foam, mist, wet ground), never blue light.
		// =====================================================================================
		void AddWater(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Water Bullet formation: water drawn in and squeezed into a dense sphere above the palm, a pressure ring
				// spinning around it.
				FMTVFXDesc D = Desc(0.32f);
				D.FadeOut = 0.12f;
				FMTVFXMeshLayer Ball = WaterBody(Paths::Sphere, FVector(13.f));
				Ball.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 0.8f);
				D.Meshes.Add(Ball);
				FMTVFXMeshLayer Squeeze = WaterBody(Paths::Sphere, FVector(22.f));
				Squeeze.ScaleOverLife = FMTVFXCurve({ { 0.f, 1.6f }, { 1.f, 0.85f } });
				Squeeze.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.4f, 0.8f }, { 1.f, 0.5f } });
				D.Meshes.Add(Squeeze);
				FMTVFXMeshLayer Band = ThinRing(24.f, WaterFoam, 1.1f);
				Band.Rotation = FRotator(90.f, 0.f, 0.f);
				Band.SpinSpeed = 520.f;
				D.Meshes.Add(Band);
				FMTVFXMeshLayer Press = Haze(Paths::ShockRing, FVector(30.f, 30.f, 1.f), 1.f);
				Press.Rotation = FRotator(90.f, 0.f, 0.f);
				D.Meshes.Add(Press);
				FMTVFXEmitter Drawn = Gather(80.f, 60.f, WaterFoam, EMTVFXRender::Stretched, 1.5f);
				Drawn.bHero = true;
				D.Emitters.Add(Drawn);
				D.Emitters.Add(Mist(0, 20.f, 15.f, 35.f, 0.25f, 0.6f));
				Out.Add(TEXT("WaterBullet.Formation"), D);
			}
			{
				// Release: pressure rings snapping forward, a jet of spray and water beads, a puff of vapour.
				FMTVFXDesc D = Desc(0.5f);
				D.Meshes.Add(FacingAir(10.f, 130.f, 0.22f, 1.2f));
				D.Meshes.Add(FacingRing(8.f, 80.f, 0.18f, WaterFoam, 1.6f));
				FMTVFXMeshLayer Ahead = FacingAir(10.f, 90.f, 0.2f, 1.f, 0.05f);
				Ahead.Velocity = FVector(900.f, 0.f, 0.f);
				D.Meshes.Add(Ahead);
				FMTVFXEmitter Spray = Droplets(28, 0.f, 900.f, 1800.f);
				Spray.Direction = FVector::ForwardVector;
				Spray.ConeDeg = 12.f;
				Spray.Gravity = 600.f;
				Spray.Drag = 2.f;
				Spray.bHero = true;
				D.Emitters.Add(Spray);
				FMTVFXEmitter Puff = Mist(8, 0.f, 30.f, 70.f, 0.35f, 0.9f);
				Puff.Direction = FVector::ForwardVector;
				Puff.ConeDeg = 35.f;
				Puff.SpeedMin = 200.f;
				Puff.SpeedMax = 500.f;
				D.Emitters.Add(Puff);
				FMTVFXEmitter Beads = WaterBlobs(6, 0.f, 500.f, 1000.f, 4.f, 8.f);
				Beads.Direction = FVector::ForwardVector;
				Beads.ConeDeg = 20.f;
				D.Emitters.Add(Beads);
				D.Shakes.Add(MinorShake(0.08f));
				Out.Add(TEXT("WaterBullet.Release"), D);
			}
			{
				// Travel (lance radius 20): a spinning lance of water with a foam sheen, a vapour cone and pressure rings
				// trailing it, spray and mist behind.
				FMTVFXDesc D = Desc(0.2f, true, 0.15f);
				FMTVFXMeshLayer Lance = WaterBody(Paths::Sphere, FVector(52.f, 17.f, 17.f));
				Lance.SpinSpeed = 540.f;
				Lance.SpinAxis = FVector::ForwardVector;
				D.Meshes.Add(Lance);
				FMTVFXMeshLayer Sheen = Orb(18.f, WaterFoam, 0.9f, 0.9f, 0.6f);
				Sheen.Size = FVector(50.f, 16.f, 16.f);
				Sheen.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Sheen);
				FMTVFXMeshLayer Vapour = Haze(Paths::ConeShell, FVector(30.f, 30.f, 45.f), 1.1f);
				Vapour.Rotation = FRotator(90.f, 0.f, 0.f); // apex at the nose, opening backward
				Vapour.Offset = FVector(40.f, 0.f, 0.f);
				D.Meshes.Add(Vapour);
				for (int32 i = 0; i < 2; ++i)
				{
					const float BandRadius = 34.f + i * 10.f;
					FMTVFXMeshLayer Band = Haze(Paths::ShockRing, FVector(BandRadius, BandRadius, 1.f), 1.1f);
					Band.Rotation = FRotator(90.f, 0.f, 0.f);
					Band.Offset = FVector(-50.f - i * 45.f, 0.f, 0.f);
					D.Meshes.Add(Band);
				}
				FMTVFXEmitter Spray = Droplets(0, 110.f, 60.f, 220.f);
				Spray.Direction = -FVector::ForwardVector;
				Spray.ConeDeg = 30.f;
				D.Emitters.Add(Spray);
				FMTVFXEmitter Trail = Mist(0, 40.f, 18.f, 40.f, 0.28f, 0.8f);
				Trail.bHero = true;
				D.Emitters.Add(Trail);
				Out.Add(TEXT("WaterBullet.Travel"), D);
			}
			{
				// Pierce: water blasting through and out of the target, a little back-spray, a ring.
				FMTVFXDesc D = Desc(0.6f);
				FMTVFXEmitter Through = Droplets(26, 0.f, 500.f, 1100.f);
				Through.Direction = FVector::ForwardVector;
				Through.ConeDeg = 25.f;
				Through.bHero = true;
				D.Emitters.Add(Through);
				FMTVFXEmitter Back = Droplets(8, 0.f, 200.f, 500.f);
				Back.Direction = -FVector::ForwardVector;
				Back.ConeDeg = 40.f;
				D.Emitters.Add(Back);
				FMTVFXEmitter Beads = WaterBlobs(4, 0.f, 300.f, 700.f, 4.f, 8.f);
				Beads.Direction = FVector::ForwardVector;
				Beads.ConeDeg = 30.f;
				D.Emitters.Add(Beads);
				D.Emitters.Add(Mist(5, 0.f, 30.f, 60.f, 0.3f, 0.8f));
				D.Meshes.Add(FacingRing(8.f, 80.f, 0.2f, WaterFoam, 1.4f));
				Out.Add(TEXT("WaterBullet.Pierce"), D);
			}
			{
				// Impact (VR 150): a splash crown and dome of water, a foam ring, droplets and blobs flung and bouncing,
				// foam skidding out, mist, wet ground.
				FMTVFXDesc D = ImpactDesc(1.3f);
				D.Meshes.Add(Flash(40.f, WaterFoam, 2.f, 0.08f));
				D.Meshes.Add(ShockwaveRing(30.f, 170.f, 0.3f, WaterFoam, 1.2f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 20.f, 160.f, 0.25f, 0.9f));
				FMTVFXMeshLayer Crown = WaterBody(Paths::Beam, FVector(90.f, 90.f, 45.f));
				Crown.Duration = 0.45f;
				Crown.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.3f }, { 1.f, 1.f } });
				Crown.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Crown);
				FMTVFXMeshLayer Dome = WaterBody(Paths::Sphere, FVector(70.f, 70.f, 50.f));
				Dome.Duration = 0.3f;
				Dome.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
				Dome.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Dome);
				FMTVFXEmitter Splash = Droplets(60, 0.f, 300.f, 900.f);
				Splash.bBounce = true;
				Splash.Shape = EMTVFXShape::Sphere;
				Splash.Radius = 20.f;
				Splash.bHero = true;
				D.Emitters.Add(Splash);
				D.Emitters.Add(WaterBlobs(10, 0.f, 250.f, 650.f));
				FMTVFXEmitter Froth = Foam(10, 0.f, 40.f, 90.f);
				Froth.bRadial = true;
				Froth.Shape = EMTVFXShape::Ring;
				Froth.Radius = 30.f;
				Froth.SpeedMin = 150.f;
				Froth.SpeedMax = 350.f;
				Froth.bFollowGround = true;
				Froth.Offset = FVector(0.f, 0.f, 10.f);
				D.Emitters.Add(Froth);
				D.Emitters.Add(Mist(12, 0.f, 50.f, 130.f, 0.35f, 1.4f));
				AddWet(D, 150.f, 12.f);
				D.Shakes.Add(MinorShake(0.1f));
				Out.Add(TEXT("WaterBullet.Impact"), D);
			}
			{
				// Dissipation: the lance loses cohesion and falls as drops.
				FMTVFXDesc D = Desc(0.6f);
				FMTVFXEmitter Fall = Droplets(22, 0.f, 50.f, 220.f);
				Fall.Shape = EMTVFXShape::Sphere;
				Fall.Radius = 15.f;
				Fall.Direction = -FVector::UpVector;
				Fall.ConeDeg = 80.f;
				D.Emitters.Add(Fall);
				D.Emitters.Add(WaterBlobs(4, 0.f, 50.f, 150.f, 4.f, 8.f));
				D.Emitters.Add(Mist(6, 0.f, 20.f, 50.f, 0.3f, 1.f));
				Out.Add(TEXT("WaterBullet.Dissipation"), D);
			}
			{
				// Water Dragon formation (loop on the caster, 0.75 s): twin spiral streams of water wind up around the
				// caster while the dragon's mass gathers above and behind, droplets and beads spiralling up, mist at the feet.
				FMTVFXDesc D = Desc(0.75f, true, 0.4f);
				for (int32 i = 0; i < 2; ++i)
				{
					FMTVFXMeshLayer Stream = WaterBody(Paths::Spiral, FVector(200.f, 130.f, 130.f));
					Stream.Rotation = FRotator(90.f, i * 180.f, 0.f); // the helix axis upright
					Stream.Offset = FVector(0.f, 0.f, 110.f);
					Stream.ScaleOverLife = FMTVFXCurve::Grow(0.15f, 0.85f);
					Stream.SpinSpeed = 320.f;
					D.Meshes.Add(Stream);
				}
				FMTVFXMeshLayer Mass = WaterBody(Paths::Sphere, FVector(70.f, 70.f, 60.f));
				Mass.Offset = FVector(-40.f, 0.f, 260.f);
				Mass.ScaleOverLife = FMTVFXCurve::Grow(0.1f, 1.f);
				D.Meshes.Add(Mass);
				FMTVFXEmitter Rise = Droplets(0, 160.f, 200.f, 450.f);
				Rise.Shape = EMTVFXShape::Ring;
				Rise.Radius = 130.f;
				Rise.Offset = FVector(0.f, 0.f, -80.f);
				Rise.Orbit = 380.f;
				Rise.ConeDeg = 10.f;
				Rise.Gravity = 200.f;
				Rise.bWorldSpace = false;
				Rise.bHero = true;
				D.Emitters.Add(Rise);
				FMTVFXEmitter Beads = WaterBlobs(0, 30.f, 150.f, 350.f, 5.f, 10.f);
				Beads.Shape = EMTVFXShape::Ring;
				Beads.Radius = 120.f;
				Beads.Offset = FVector(0.f, 0.f, -60.f);
				Beads.Orbit = 320.f;
				Beads.Gravity = 150.f;
				Beads.ConeDeg = 15.f;
				Beads.bWorldSpace = false;
				D.Emitters.Add(Beads);
				FMTVFXEmitter Base = Mist(0, 25.f, 60.f, 130.f, 0.3f, 1.2f);
				Base.Shape = EMTVFXShape::Ring;
				Base.Radius = 140.f;
				Base.bFollowGround = true;
				D.Emitters.Add(Base);
				D.Lights.Add(Glow(WaterFoam, 50.f, 700.f, 0.f, FMTVFXCurve::Grow(0.f, 0.45f)));
				Out.Add(TEXT("WaterDragon.Formation"), D);
			}
			{
				// Travel (loop on the serpent's head, radius 110; the head and body are drawn by AMTWaterSerpent): spray
				// thrown off the head, foam and mist streaming back, drips falling, a ribbon of water blobs left in its wake,
				// glowing eyes and a cool light.
				FMTVFXDesc D = Desc(0.3f, true, 0.5f);
				FMTVFXEmitter Spray = Droplets(0, 140.f, 100.f, 350.f);
				Spray.Shape = EMTVFXShape::Sphere;
				Spray.Radius = 90.f;
				Spray.Direction = FVector(-1.f, 0.f, 0.3f);
				Spray.ConeDeg = 50.f;
				Spray.bHero = true;
				Spray.MaxParticles = 150;
				D.Emitters.Add(Spray);
				FMTVFXEmitter Froth = Foam(0, 30.f, 40.f, 90.f, 0.5f, 0.8f);
				Froth.Shape = EMTVFXShape::SphereShell;
				Froth.Radius = 90.f;
				Froth.Direction = -FVector::ForwardVector;
				Froth.ConeDeg = 40.f;
				Froth.SpeedMin = 80.f;
				Froth.SpeedMax = 200.f;
				Froth.Gravity = 150.f;
				D.Emitters.Add(Froth);
				FMTVFXEmitter Vapour = Mist(0, 45.f, 60.f, 140.f, 0.3f, 1.2f);
				Vapour.Shape = EMTVFXShape::Sphere;
				Vapour.Radius = 80.f;
				D.Emitters.Add(Vapour);
				FMTVFXEmitter Drips = Droplets(0, 45.f, 20.f, 90.f);
				Drips.Shape = EMTVFXShape::Sphere;
				Drips.Radius = 100.f;
				Drips.Direction = -FVector::UpVector;
				Drips.ConeDeg = 20.f;
				D.Emitters.Add(Drips);
				FMTVFXEmitter Ribbon = WaterBlobs(0, 36.f, 20.f, 90.f, 10.f, 24.f);
				Ribbon.Shape = EMTVFXShape::Sphere;
				Ribbon.Radius = 50.f;
				Ribbon.Offset = FVector(-80.f, 0.f, 0.f);
				Ribbon.Direction = -FVector::ForwardVector;
				Ribbon.ConeDeg = 30.f;
				Ribbon.Gravity = 350.f;
				Ribbon.LifeMin = 0.6f;
				Ribbon.LifeMax = 0.9f;
				D.Emitters.Add(Ribbon);
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					FMTVFXMeshLayer Eye = Orb(8.f, ManaWhite, 12.f, 0.f, 0.f);
					Eye.Offset = FVector(95.f, Side * 32.f, 38.f);
					Eye.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
					D.Meshes.Add(Eye);
				}
				D.Meshes.Add(Haze(Paths::Sphere, FVector(140.f, 110.f, 110.f), 0.5f));
				D.Lights.Add(Glow(WaterFoam, 45.f, 900.f));
				Out.Add(TEXT("WaterDragon.Travel"), D);
			}
			{
				// Impact (VR 780): an enormous water explosion: a dome and a crown of water, a surge wave sweeping out along
				// the ground, foam rings, droplets and heavy blobs raining back, a mist cloud, wet ground; ultimate tier.
				FMTVFXDesc D = ImpactDesc(2.4f);
				D.Meshes.Add(ShockwaveRing(80.f, 820.f, 0.55f, WaterFoam, 1.8f));
				D.Meshes.Add(ShockwaveRing(60.f, 600.f, 0.45f, WaterFoam, 1.2f, 0.08f, 0.06f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 100.f, 650.f, 0.45f, 0.9f));
				FMTVFXMeshLayer Dome = WaterBody(Paths::Sphere, FVector(320.f, 320.f, 220.f));
				Dome.Duration = 0.6f;
				Dome.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
				Dome.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Dome);
				FMTVFXMeshLayer Crown = WaterBody(Paths::Beam, FVector(300.f, 300.f, 200.f));
				Crown.Duration = 0.8f;
				Crown.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.3f }, { 1.f, 1.1f } });
				Crown.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.6f, 0.7f }, { 1.f, 0.f } });
				D.Meshes.Add(Crown);
				FMTVFXMeshLayer Surge = WaterBody(Paths::Beam, FVector(780.f, 780.f, 30.f));
				Surge.Duration = 1.f;
				Surge.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.15f }, { 1.f, 1.f } });
				Surge.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.9f }, { 1.f, 0.f } });
				D.Meshes.Add(Surge);
				FMTVFXEmitter Splash = Droplets(170, 0.f, 400.f, 1300.f);
				Splash.Shape = EMTVFXShape::Sphere;
				Splash.Radius = 80.f;
				Splash.bBounce = true;
				Splash.bHero = true;
				Splash.MaxParticles = 180;
				D.Emitters.Add(Splash);
				FMTVFXEmitter Blobs = WaterBlobs(30, 0.f, 400.f, 1100.f, 10.f, 26.f);
				Blobs.Shape = EMTVFXShape::Sphere;
				Blobs.Radius = 80.f;
				D.Emitters.Add(Blobs);
				FMTVFXEmitter Froth = Foam(30, 0.f, 90.f, 200.f, 0.55f, 1.4f);
				Froth.bRadial = true;
				Froth.Shape = EMTVFXShape::Ring;
				Froth.Radius = 120.f;
				Froth.SpeedMin = 500.f;
				Froth.SpeedMax = 1000.f;
				Froth.Drag = 2.5f;
				Froth.bFollowGround = true;
				Froth.Offset = FVector(0.f, 0.f, 20.f);
				D.Emitters.Add(Froth);
				FMTVFXEmitter Cloud = Mist(26, 0.f, 150.f, 350.f, 0.45f, 2.4f);
				Cloud.Shape = EMTVFXShape::Sphere;
				Cloud.Radius = 200.f;
				D.Emitters.Add(Cloud);
				AddWet(D, 800.f, 16.f);
				D.Lights.Add(FlashLight(WaterFoam, 300.f, 1600.f, 0.6f));
				D.Shakes.Add(UltimateShake(0.65f, 5.f));
				Out.Add(TEXT("WaterDragon.Impact"), D);
			}
			{
				// Dissipation: the serpent slumps and collapses into rain, blobs and mist.
				FMTVFXDesc D = Desc(1.4f);
				FMTVFXMeshLayer Slump = WaterBody(Paths::Sphere, FVector(120.f, 120.f, 100.f));
				Slump.Duration = 0.5f;
				Slump.ScaleOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.3f } });
				Slump.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.9f }, { 1.f, 0.f } });
				D.Meshes.Add(Slump);
				FMTVFXEmitter Rain = Droplets(120, 0.f, 50.f, 300.f);
				Rain.Shape = EMTVFXShape::Sphere;
				Rain.Radius = 250.f;
				Rain.Direction = -FVector::UpVector;
				Rain.ConeDeg = 60.f;
				Rain.LifeMin = 0.6f;
				Rain.LifeMax = 1.2f;
				Rain.MaxParticles = 130;
				D.Emitters.Add(Rain);
				FMTVFXEmitter Blobs = WaterBlobs(24, 0.f, 50.f, 250.f, 8.f, 20.f);
				Blobs.Shape = EMTVFXShape::Sphere;
				Blobs.Radius = 220.f;
				Blobs.Direction = -FVector::UpVector;
				Blobs.ConeDeg = 70.f;
				D.Emitters.Add(Blobs);
				FMTVFXEmitter Vapour = Mist(16, 0.f, 120.f, 260.f, 0.35f, 1.8f);
				Vapour.Shape = EMTVFXShape::Sphere;
				Vapour.Radius = 200.f;
				D.Emitters.Add(Vapour);
				Out.Add(TEXT("WaterDragon.Dissipation"), D);
			}
			{
				// Flood formation (behind the caster): a wall of water swells up behind him, water rising from the ground,
				// foam and vapour, streams drawn into the gathering arms.
				FMTVFXDesc D = Desc(0.7f, true, 0.3f);
				FMTVFXMeshLayer Swell = WaterBody(Paths::WaveSheet, FVector(60.f, 260.f, 110.f));
				Swell.Offset = FVector(-170.f, 0.f, -90.f);
				Swell.ScaleOverLife = FMTVFXCurve::Grow(0.05f, 1.f);
				D.Meshes.Add(Swell);
				FMTVFXEmitter Rise = Droplets(0, 130.f, 250.f, 600.f);
				Rise.Shape = EMTVFXShape::Line;
				Rise.Extent = FVector(0.f, 240.f, 0.f);
				Rise.Offset = FVector(-150.f, 0.f, -80.f);
				Rise.ConeDeg = 20.f;
				Rise.bHero = true;
				D.Emitters.Add(Rise);
				FMTVFXEmitter Froth = Foam(0, 30.f, 50.f, 110.f, 0.5f, 0.8f);
				Froth.Shape = EMTVFXShape::Line;
				Froth.Extent = FVector(0.f, 240.f, 0.f);
				Froth.Offset = FVector(-150.f, 0.f, 0.f);
				Froth.Direction = FVector(0.4f, 0.f, 1.f);
				Froth.SpeedMin = 100.f;
				Froth.SpeedMax = 250.f;
				D.Emitters.Add(Froth);
				FMTVFXEmitter Vapour = Mist(0, 20.f, 60.f, 140.f, 0.3f, 1.2f);
				Vapour.Shape = EMTVFXShape::Line;
				Vapour.Extent = FVector(0.f, 240.f, 0.f);
				Vapour.Offset = FVector(-150.f, 0.f, 60.f);
				D.Emitters.Add(Vapour);
				D.Emitters.Add(Gather(40.f, 90.f, WaterFoam, EMTVFXRender::Stretched, 1.4f));
				Out.Add(TEXT("Flood.Formation"), D);
			}
			{
				// Zone (loop on the wave front, X = travel; 2400 wide, 380 high): six overlapping curling sheets of water,
				// a foaming crest, spray thrown ahead, heavy blobs, churning foam at the foot, a veil of mist; ultimate tier.
				FMTVFXDesc D = Desc(0.4f, true, 0.7f);
				D.bUpright = true;
				for (int32 i = 0; i < 6; ++i)
				{
					FMTVFXMeshLayer Wave = WaterBody(Paths::WaveSheet, FVector(115.f, 230.f, (i % 2 == 0) ? 190.f : 177.f));
					Wave.Offset = FVector((i % 3) * 18.f - 18.f, -1000.f + i * 400.f, 0.f);
					Wave.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 0.9f);
					D.Meshes.Add(Wave);
				}
				FMTVFXEmitter Crest = Foam(0, 200.f, 60.f, 130.f, 0.6f, 0.8f);
				Crest.Shape = EMTVFXShape::Line;
				Crest.Extent = FVector(0.f, 1200.f, 0.f);
				Crest.Offset = FVector(60.f, 0.f, 360.f);
				Crest.Direction = FVector(1.f, 0.f, 0.4f);
				Crest.ConeDeg = 25.f;
				Crest.SpeedMin = 150.f;
				Crest.SpeedMax = 350.f;
				Crest.bHero = true;
				Crest.MaxParticles = 180;
				D.Emitters.Add(Crest);
				FMTVFXEmitter Spray = Droplets(0, 220.f, 300.f, 800.f);
				Spray.Shape = EMTVFXShape::Line;
				Spray.Extent = FVector(0.f, 1200.f, 0.f);
				Spray.Offset = FVector(80.f, 0.f, 340.f);
				Spray.Direction = FVector(1.f, 0.f, 0.8f);
				Spray.MaxParticles = 220;
				D.Emitters.Add(Spray);
				FMTVFXEmitter Vapour = Mist(0, 40.f, 120.f, 260.f, 0.3f, 1.6f);
				Vapour.Shape = EMTVFXShape::Line;
				Vapour.Extent = FVector(0.f, 1200.f, 0.f);
				Vapour.Offset = FVector(-80.f, 0.f, 250.f);
				D.Emitters.Add(Vapour);
				FMTVFXEmitter Churn = Foam(0, 80.f, 80.f, 160.f, 0.45f, 1.f);
				Churn.Shape = EMTVFXShape::Line;
				Churn.Extent = FVector(0.f, 1200.f, 0.f);
				Churn.Offset = FVector(120.f, 0.f, 20.f);
				Churn.Direction = FVector(1.f, 0.f, 0.2f);
				Churn.SpeedMin = 200.f;
				Churn.SpeedMax = 450.f;
				D.Emitters.Add(Churn);
				FMTVFXEmitter Blobs = WaterBlobs(0, 30.f, 300.f, 700.f, 10.f, 22.f);
				Blobs.Shape = EMTVFXShape::Line;
				Blobs.Extent = FVector(0.f, 1200.f, 0.f);
				Blobs.Offset = FVector(60.f, 0.f, 300.f);
				Blobs.Direction = FVector(1.f, 0.f, 0.6f);
				D.Emitters.Add(Blobs);
				D.Shakes.Add(UltimateShake(0.6f, 4.f));
				Out.Add(TEXT("Flood.Zone"), D);
			}
			{
				// Trail (pooled; every 300 cm at the front's centre): a strip of wet ground across the full 2400 width, puddle
				// splashes and low mist.
				FMTVFXDesc D = ImpactDesc(0.6f);
				FMTVFXDecal Soaked = Decal(Paths::DecalWet, 1250.f, 14.f);
				Soaked.Length = 190.f;
				Soaked.bRandomYaw = false;
				Soaked.Depth = 300.f;
				Soaked.FadeOut = 4.f;
				D.Decals.Add(Soaked);
				FMTVFXEmitter Puddles = Droplets(18, 0.f, 80.f, 250.f);
				Puddles.Shape = EMTVFXShape::Line;
				Puddles.Extent = FVector(0.f, 1100.f, 0.f);
				Puddles.bFollowGround = true;
				Puddles.ConeDeg = 35.f;
				D.Emitters.Add(Puddles);
				FMTVFXEmitter Low = Mist(5, 0.f, 120.f, 240.f, 0.2f, 1.6f);
				Low.Shape = EMTVFXShape::Line;
				Low.Extent = FVector(0.f, 1100.f, 0.f);
				Low.bFollowGround = true;
				Low.Offset = FVector(0.f, 0.f, 30.f);
				D.Emitters.Add(Low);
				Out.Add(TEXT("Flood.Trail"), D);
			}
			{
				// Splash (at an enemy's feet with X = travel, scale 1; against a wall when the wave breaks, X pointing back off
				// the wall, scale 2): a sheet of water thrown up and along X, droplets, blobs, foam and mist.
				FMTVFXDesc D = ImpactDesc(1.4f);
				FMTVFXMeshLayer Sheet = WaterBody(Paths::WaveSheet, FVector(60.f, 220.f, 180.f));
				Sheet.Duration = 0.7f;
				Sheet.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 0.4f, 1.f }, { 1.f, 0.8f } });
				Sheet.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.9f }, { 0.6f, 0.8f }, { 1.f, 0.f } });
				D.Meshes.Add(Sheet);
				FMTVFXEmitter Thrown = Droplets(100, 0.f, 500.f, 1300.f);
				Thrown.Shape = EMTVFXShape::Line;
				Thrown.Extent = FVector(0.f, 200.f, 0.f);
				Thrown.Direction = FVector(0.6f, 0.f, 1.f);
				Thrown.ConeDeg = 35.f;
				Thrown.bHero = true;
				Thrown.MaxParticles = 110;
				D.Emitters.Add(Thrown);
				FMTVFXEmitter Blobs = WaterBlobs(14, 0.f, 400.f, 900.f, 10.f, 22.f);
				Blobs.Shape = EMTVFXShape::Line;
				Blobs.Extent = FVector(0.f, 200.f, 0.f);
				Blobs.Direction = FVector(0.5f, 0.f, 1.f);
				D.Emitters.Add(Blobs);
				FMTVFXEmitter Froth = Foam(18, 0.f, 80.f, 160.f, 0.55f, 1.2f);
				Froth.Shape = EMTVFXShape::Line;
				Froth.Extent = FVector(0.f, 200.f, 0.f);
				Froth.Offset = FVector(0.f, 0.f, 120.f);
				Froth.Direction = FVector(1.f, 0.f, 0.6f);
				Froth.SpeedMin = 200.f;
				Froth.SpeedMax = 500.f;
				D.Emitters.Add(Froth);
				D.Emitters.Add(Mist(14, 0.f, 120.f, 260.f, 0.4f, 1.8f));
				D.Shakes.Add(MinorShake(0.12f));
				Out.Add(TEXT("Flood.Splash"), D);
			}
			{
				// Dissipation: the wave breaks and collapses: foam bursting forward, water falling, mist, a band of wet ground.
				FMTVFXDesc D = ImpactDesc(1.6f);
				FMTVFXMeshLayer Collapse = WaterBody(Paths::WaveSheet, FVector(115.f, 1200.f, 190.f));
				Collapse.Duration = 0.6f;
				Collapse.ScaleOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.25f } });
				Collapse.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.9f }, { 1.f, 0.f } });
				D.Meshes.Add(Collapse);
				FMTVFXEmitter Break = Foam(40, 0.f, 100.f, 220.f, 0.6f, 1.4f);
				Break.Shape = EMTVFXShape::Line;
				Break.Extent = FVector(0.f, 1200.f, 0.f);
				Break.Offset = FVector(40.f, 0.f, 120.f);
				Break.Direction = FVector(1.f, 0.f, 0.2f);
				Break.SpeedMin = 250.f;
				Break.SpeedMax = 600.f;
				Break.bHero = true;
				D.Emitters.Add(Break);
				FMTVFXEmitter Fall = Droplets(120, 0.f, 200.f, 600.f);
				Fall.Shape = EMTVFXShape::Line;
				Fall.Extent = FVector(0.f, 1200.f, 0.f);
				Fall.Offset = FVector(40.f, 0.f, 250.f);
				Fall.Direction = FVector(1.f, 0.f, 0.5f);
				Fall.MaxParticles = 130;
				D.Emitters.Add(Fall);
				FMTVFXEmitter Vapour = Mist(20, 0.f, 150.f, 300.f, 0.35f, 2.f);
				Vapour.Shape = EMTVFXShape::Line;
				Vapour.Extent = FVector(0.f, 1200.f, 0.f);
				Vapour.Offset = FVector(0.f, 0.f, 150.f);
				D.Emitters.Add(Vapour);
				FMTVFXDecal Soaked = Decal(Paths::DecalWet, 1250.f, 14.f);
				Soaked.Length = 300.f;
				Soaked.bRandomYaw = false;
				Soaked.Offset = FVector(150.f, 0.f, 0.f);
				Soaked.FadeOut = 4.f;
				D.Decals.Add(Soaked);
				Out.Add(TEXT("Flood.Dissipation"), D);
			}
		}

		// =====================================================================================
		// Wind: visible as pale cyan / white distortion, condensation and streaks, moving dust and leaves.
		// =====================================================================================
		void AddWind(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Wind Blade formation: air compressed into a crescent in the palm; condensation streaks and leaves pulled in.
				FMTVFXDesc D = Desc(0.35f);
				D.FadeOut = 0.12f;
				FMTVFXMeshLayer Edge = Layer(Paths::Crescent, Paths::MatGlow, FVector(18.f, 55.f, 4.f), WindWhite, 2.2f, FMTVFXCurve::FadeInOut(0.3f, 0.1f),
					FMTVFXCurve::Grow(0.3f, 0.9f));
				Edge.Scalars.Add(TEXT("FresnelMix"), 0.8f);
				Edge.Scalars.Add(TEXT("NoiseAmount"), 0.7f);
				Flow(Edge, 1.f, 3.f, 3.f, 0.f, 1.6f);
				Edge.SpinSpeed = -240.f;
				D.Meshes.Add(Edge);
				FMTVFXMeshLayer Warp = Layer(Paths::Crescent, Paths::MatAir, FVector(22.f, 60.f, 8.f), FC::White, 1.f, FMTVFXCurve::FadeInOut(0.3f, 0.1f),
					FMTVFXCurve::Grow(0.3f, 0.9f));
				Warp.Scalars.Add(TEXT("Distortion"), 1.2f);
				Warp.SpinSpeed = -240.f;
				D.Meshes.Add(Warp);
				D.Meshes.Add(AirPulse(Paths::Sphere, 70.f, 18.f, 0.35f, 0.9f));
				FMTVFXEmitter Rush = Condensation(0, 60.f, 5.f, 10.f, 0.35f, 0.25f);
				Converge(Rush, 70.f, 3200.f);
				Rush.Orbit = 500.f;
				Rush.bHero = true;
				D.Emitters.Add(Rush);
				FMTVFXEmitter Glints = WindStreaks(0, 30.f, 0.f, 20.f, FVector::ForwardVector);
				Glints.Shape = EMTVFXShape::SphereShell;
				Glints.Radius = 60.f;
				Glints.Attract = 2600.f;
				Glints.Orbit = 600.f;
				Glints.bWorldSpace = false;
				D.Emitters.Add(Glints);
				FMTVFXEmitter Drawn = Leaves(0, 6.f);
				Drawn.Shape = EMTVFXShape::SphereShell;
				Drawn.Radius = 90.f;
				Drawn.Attract = 900.f;
				Drawn.Orbit = 300.f;
				Drawn.Gravity = 0.f;
				Drawn.bWorldSpace = false;
				D.Emitters.Add(Drawn);
				Out.Add(TEXT("WindBlade.Formation"), D);
			}
			{
				// Release: the slash. A bright crescent sweeps out of the hand, a pressure ring of warped air, condensation
				// and glints blasted forward, dust kicked at the feet.
				FMTVFXDesc D = Desc(0.5f);
				FMTVFXMeshLayer Slash = Layer(Paths::Crescent, Paths::MatGlow, FVector(35.f, 150.f, 6.f), WindWhite, 3.5f,
					FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 0.3f }, { 1.f, 1.1f } }));
				Slash.Duration = 0.14f;
				Slash.Scalars.Add(TEXT("FresnelMix"), 0.8f);
				Slash.Scalars.Add(TEXT("NoiseAmount"), 0.6f);
				Flow(Slash, 1.f, 3.f, 4.f, 0.f, 1.4f);
				Slash.Offset = FVector(40.f, 0.f, 0.f);
				D.Meshes.Add(Slash);
				D.Meshes.Add(FacingAir(20.f, 220.f, 0.25f, 1.4f));
				D.Meshes.Add(FacingRing(20.f, 160.f, 0.2f, WindWhite, 1.4f, 0.f, 0.05f));
				FMTVFXEmitter Blast = Condensation(20, 0.f, 6.f, 12.f, 0.4f, 0.3f);
				Blast.Direction = FVector::ForwardVector;
				Blast.ConeDeg = 30.f;
				Blast.SpeedMin = 800.f;
				Blast.SpeedMax = 1600.f;
				Blast.Shape = EMTVFXShape::Line;
				Blast.Extent = FVector(0.f, 80.f, 0.f);
				D.Emitters.Add(Blast);
				FMTVFXEmitter Glints = WindStreaks(14, 0.f, 1000.f, 2000.f, FVector::ForwardVector);
				Glints.Shape = EMTVFXShape::Line;
				Glints.Extent = FVector(0.f, 90.f, 0.f);
				D.Emitters.Add(Glints);
				FMTVFXEmitter Kicked = DustCloud(6, 0.f, 50.f, 100.f, 0.35f, 0.8f);
				Kicked.bFollowGround = true;
				Kicked.Offset = FVector(40.f, 0.f, 10.f);
				Kicked.Direction = FVector(1.f, 0.f, 0.2f);
				Kicked.ConeDeg = 40.f;
				Kicked.SpeedMin = 300.f;
				Kicked.SpeedMax = 600.f;
				D.Emitters.Add(Kicked);
				D.Shakes.Add(MinorShake(0.1f));
				Out.Add(TEXT("WindBlade.Release"), D);
			}
			{
				// Travel (crescent 520 wide): a flat crescent of warped air with a bright streaming edge and a cyan core,
				// condensation and glints peeling off behind, dust torn up from the ground below, leaves.
				FMTVFXDesc D = Desc(0.2f, true, 0.2f);
				FMTVFXMeshLayer Warp = Layer(Paths::Crescent, Paths::MatAir, FVector(60.f, 262.f, 14.f), FC::White, 1.f, FMTVFXCurve::Grow(0.f, 0.3f));
				Warp.Scalars.Add(TEXT("Distortion"), 1.4f);
				D.Meshes.Add(Warp);
				FMTVFXMeshLayer Edge = Layer(Paths::Crescent, Paths::MatGlow, FVector(56.f, 255.f, 10.f), WindWhite, 2.4f, FMTVFXCurve::Grow(0.f, 0.3f));
				Edge.Scalars.Add(TEXT("FresnelMix"), 0.8f);
				Edge.Scalars.Add(TEXT("NoiseAmount"), 0.7f);
				Flow(Edge, 1.f, 4.f, 3.f, 0.f, 1.5f);
				D.Meshes.Add(Edge);
				FMTVFXMeshLayer Core = Layer(Paths::Crescent, Paths::MatGlow, FVector(40.f, 245.f, 6.f), WindCyan, 3.2f, FMTVFXCurve::Grow(0.f, 0.3f));
				Core.Offset = FVector(6.f, 0.f, 0.f);
				Core.Scalars.Add(TEXT("FresnelMix"), 0.4f);
				Core.Scalars.Add(TEXT("NoiseAmount"), 0.5f);
				Flow(Core, 1.f, 5.f, 4.f, 0.f, 1.2f);
				D.Meshes.Add(Core);
				FMTVFXEmitter Vapour = Condensation(0, 120.f, 6.f, 12.f, 0.35f, 0.3f);
				Vapour.Shape = EMTVFXShape::Line;
				Vapour.Extent = FVector(0.f, 240.f, 0.f);
				Vapour.Direction = -FVector::ForwardVector;
				Vapour.ConeDeg = 6.f;
				Vapour.SpeedMin = 400.f;
				Vapour.SpeedMax = 900.f;
				Vapour.bHero = true;
				Vapour.MaxParticles = 60;
				D.Emitters.Add(Vapour);
				FMTVFXEmitter Glints = WindStreaks(0, 60.f, 300.f, 600.f, -FVector::ForwardVector);
				Glints.Shape = EMTVFXShape::Line;
				Glints.Extent = FVector(0.f, 250.f, 0.f);
				D.Emitters.Add(Glints);
				FMTVFXEmitter Torn = DustCloud(0, 40.f, 30.f, 70.f, 0.3f, 0.7f);
				Torn.Shape = EMTVFXShape::Line;
				Torn.Extent = FVector(0.f, 200.f, 0.f);
				Torn.bFollowGround = true;
				Torn.Offset = FVector(0.f, 0.f, 10.f);
				Torn.Direction = FVector(0.4f, 0.f, 1.f);
				Torn.SpeedMin = 150.f;
				Torn.SpeedMax = 400.f;
				D.Emitters.Add(Torn);
				D.Emitters.Add(Leaves(0, 12.f));
				Out.Add(TEXT("WindBlade.Travel"), D);
			}
			{
				// Impact: the crescent splits against what it hit: a flash of its edge, a ring and a pulse of air,
				// condensation, dust and leaves thrown out.
				FMTVFXDesc D = ImpactDesc(0.9f);
				FMTVFXMeshLayer Split = Layer(Paths::Crescent, Paths::MatGlow, FVector(40.f, 200.f, 8.f), WindWhite, 3.f,
					FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 1.4f } }));
				Split.Duration = 0.18f;
				Split.Scalars.Add(TEXT("FresnelMix"), 0.8f);
				Split.Scalars.Add(TEXT("NoiseAmount"), 0.7f);
				D.Meshes.Add(Split);
				D.Meshes.Add(ShockwaveRing(20.f, 240.f, 0.25f, WindWhite, 2.4f, 0.f, 0.05f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 30.f, 220.f, 0.3f, 1.1f));
				FMTVFXEmitter Burst = Condensation(18, 0.f, 6.f, 12.f, 0.4f, 0.35f);
				Burst.bRadial = true;
				Burst.Shape = EMTVFXShape::Sphere;
				Burst.Radius = 30.f;
				Burst.SpeedMin = 600.f;
				Burst.SpeedMax = 1200.f;
				Burst.Drag = 3.f;
				D.Emitters.Add(Burst);
				D.Emitters.Add(DustCloud(12, 0.f, 40.f, 100.f, 0.4f, 0.9f));
				FMTVFXEmitter Foliage = Leaves(14, 0.f);
				Foliage.bRadial = true;
				Foliage.Shape = EMTVFXShape::Sphere;
				Foliage.Radius = 30.f;
				Foliage.SpeedMin = 200.f;
				Foliage.SpeedMax = 500.f;
				D.Emitters.Add(Foliage);
				D.Shakes.Add(MinorShake(0.1f));
				Out.Add(TEXT("WindBlade.Impact"), D);
			}
			{
				// Dissipation: the crescent unravels into condensation wisps and settling dust.
				FMTVFXDesc D = Desc(0.6f);
				FMTVFXMeshLayer Unravel = Layer(Paths::Crescent, Paths::MatAir, FVector(60.f, 262.f, 14.f), FC::White, 1.f,
					FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 1.3f } }));
				Unravel.Duration = 0.3f;
				Unravel.Scalars.Add(TEXT("Distortion"), 1.f);
				D.Meshes.Add(Unravel);
				FMTVFXEmitter Wisps = Condensation(24, 0.f, 8.f, 16.f, 0.3f, 0.5f);
				Wisps.Shape = EMTVFXShape::Line;
				Wisps.Extent = FVector(0.f, 250.f, 0.f);
				Wisps.Direction = FVector::ForwardVector;
				Wisps.ConeDeg = 60.f;
				Wisps.SpeedMin = 100.f;
				Wisps.SpeedMax = 400.f;
				D.Emitters.Add(Wisps);
				FMTVFXEmitter Settle = DustCloud(8, 0.f, 40.f, 90.f, 0.3f, 1.f);
				Settle.Shape = EMTVFXShape::Line;
				Settle.Extent = FVector(0.f, 200.f, 0.f);
				D.Emitters.Add(Settle);
				Out.Add(TEXT("WindBlade.Dissipation"), D);
			}
			{
				// Tornado formation: a small whirl of air stirred in the hand, condensation, dust and a leaf or two circling.
				FMTVFXDesc D = Desc(0.55f, true, 0.2f);
				FMTVFXMeshLayer Whirl = Layer(Paths::Funnel, Paths::MatAir, FVector(22.f, 22.f, 40.f), FC::White, 1.f, FMTVFXCurve::Grow(0.f, 0.4f),
					FMTVFXCurve::Grow(0.2f, 0.9f));
				Whirl.Scalars.Add(TEXT("Distortion"), 1.2f);
				Whirl.SpinSpeed = 600.f;
				Whirl.Offset = FVector(0.f, 0.f, -30.f);
				D.Meshes.Add(Whirl);
				FMTVFXEmitter Swirl = Condensation(0, 50.f, 5.f, 10.f, 0.35f, 0.4f);
				Swirl.Shape = EMTVFXShape::Ring;
				Swirl.Radius = 45.f;
				Swirl.Orbit = 700.f;
				Swirl.ConeDeg = 10.f;
				Swirl.SpeedMin = 80.f;
				Swirl.SpeedMax = 200.f;
				Swirl.bWorldSpace = false;
				Swirl.bHero = true;
				D.Emitters.Add(Swirl);
				FMTVFXEmitter Dusty = DustCloud(0, 40.f, 20.f, 50.f, 0.3f, 0.6f);
				Dusty.Shape = EMTVFXShape::Ring;
				Dusty.Radius = 55.f;
				Dusty.Orbit = 500.f;
				Dusty.SpeedMin = 60.f;
				Dusty.SpeedMax = 160.f;
				Dusty.bWorldSpace = false;
				D.Emitters.Add(Dusty);
				FMTVFXEmitter Drawn = Leaves(0, 6.f);
				Drawn.Shape = EMTVFXShape::Ring;
				Drawn.Radius = 70.f;
				Drawn.Orbit = 400.f;
				Drawn.Gravity = -60.f;
				Drawn.bWorldSpace = false;
				D.Emitters.Add(Drawn);
				Out.Add(TEXT("Tornado.Formation"), D);
			}
			{
				// Zone (VR 360, 950 high, grown live to x1.56): a spinning funnel of warped air streaked with pale wind and
				// veiled in dust; dust, vapour, rocks and leaves drawn in at the base and flung up and out along it; a dust
				// skirt at the ground. Heavy shake as it forms.
				FMTVFXDesc D = Desc(0.6f, true, 0.8f);
				D.bUpright = true;
				FMTVFXMeshLayer Funnel = Layer(Paths::Funnel, Paths::MatAir, FVector(360.f, 360.f, 475.f), FC::White, 1.f, FMTVFXCurve::Grow(0.f, 0.6f),
					FMTVFXCurve::Grow(0.2f, 0.8f));
				Funnel.SpinSpeed = 480.f;
				Funnel.Scalars.Add(TEXT("Distortion"), 1.4f);
				D.Meshes.Add(Funnel);
				FMTVFXMeshLayer Streaks = Layer(Paths::Funnel, Paths::MatGlow, FVector(345.f, 345.f, 465.f), WindWhite, 0.32f, FMTVFXCurve::Grow(0.f, 0.8f),
					FMTVFXCurve::Grow(0.f, 0.8f));
				Streaks.SpinSpeed = 620.f;
				Streaks.Scalars.Add(TEXT("FresnelMix"), 0.5f);
				Streaks.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Flow(Streaks, 3.f, 0.6f, 1.4f, 0.3f, 3.f);
				D.Meshes.Add(Streaks);
				FMTVFXMeshLayer DustVeil = Layer(Paths::Funnel, Paths::MatSmoke, FVector(300.f, 300.f, 300.f), DustLight, 1.f,
					FMTVFXCurve({ { 0.f, 0.f }, { 0.8f, 0.35f }, { 1.f, 0.35f } }), FMTVFXCurve::Grow(0.2f, 0.8f));
				DustVeil.SpinSpeed = 380.f;
				D.Meshes.Add(DustVeil);
				FMTVFXEmitter Swirl = DustCloud(0, 80.f, 60.f, 140.f, 0.45f, 1.8f);
				Swirl.Shape = EMTVFXShape::Ring;
				Swirl.Radius = 110.f;
				Swirl.Orbit = 300.f;
				Swirl.Attract = -160.f; // flung outward as it climbs: the funnel flares
				Swirl.ConeDeg = 12.f;
				Swirl.SpeedMin = 300.f;
				Swirl.SpeedMax = 550.f;
				Swirl.bWorldSpace = false;
				Swirl.bHero = true;
				Swirl.MaxParticles = 150;
				D.Emitters.Add(Swirl);
				FMTVFXEmitter Vapour = Condensation(0, 70.f, 10.f, 20.f, 0.3f, 0.9f);
				Vapour.Shape = EMTVFXShape::Ring;
				Vapour.Radius = 160.f;
				Vapour.Orbit = 420.f;
				Vapour.Attract = -120.f;
				Vapour.SpeedMin = 250.f;
				Vapour.SpeedMax = 500.f;
				Vapour.Stretch = 6.f;
				Vapour.bWorldSpace = false;
				D.Emitters.Add(Vapour);
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
				FMTVFXEmitter Scour = DustCloud(0, 40.f, 80.f, 160.f, 0.35f, 1.f);
				Scour.Shape = EMTVFXShape::Ring;
				Scour.Radius = 120.f;
				Scour.bRadial = true;
				Scour.SpeedMin = 300.f;
				Scour.SpeedMax = 600.f;
				Scour.bFollowGround = true;
				D.Emitters.Add(Scour);
				FMTVFXEmitter Skirt = DustCloud(0, 30.f, 90.f, 180.f, 0.3f, 1.2f);
				Skirt.Shape = EMTVFXShape::Ring;
				Skirt.Radius = 260.f;
				Skirt.Orbit = 240.f;
				Skirt.SpeedMin = 20.f;
				Skirt.SpeedMax = 60.f;
				Skirt.bFollowGround = true;
				Skirt.Offset = FVector(0.f, 0.f, 20.f);
				D.Emitters.Add(Skirt);
				D.Shakes.Add(HeavyShake(0.3f));
				Out.Add(TEXT("Tornado.Zone"), D);
			}
			{
				// Trail (pooled): the ground the tornado crosses: dust scoured into a swirl, leaves and pebbles tossed up.
				FMTVFXDesc D = ImpactDesc(0.6f);
				FMTVFXEmitter Scour = DustCloud(6, 0.f, 70.f, 140.f, 0.35f, 1.2f);
				Scour.Shape = EMTVFXShape::Disc;
				Scour.Radius = 180.f;
				Scour.bFollowGround = true;
				Scour.Offset = FVector(0.f, 0.f, 15.f);
				Scour.Orbit = 200.f;
				D.Emitters.Add(Scour);
				FMTVFXEmitter Tossed = Leaves(5, 0.f);
				Tossed.Shape = EMTVFXShape::Disc;
				Tossed.Radius = 150.f;
				Tossed.bFollowGround = true;
				Tossed.SpeedMin = 150.f;
				Tossed.SpeedMax = 350.f;
				D.Emitters.Add(Tossed);
				FMTVFXEmitter Pebbles = Debris(3, 150.f, 350.f, 2.f, 5.f);
				Pebbles.Shape = EMTVFXShape::Disc;
				Pebbles.Radius = 150.f;
				Pebbles.bFollowGround = true;
				D.Emitters.Add(Pebbles);
				Out.Add(TEXT("Tornado.Trail"), D);
			}
			{
				// Lift (loop on a lifted enemy, capsule centre): a tight whirl of warped air around them, condensation and
				// dust spinning.
				FMTVFXDesc D = Desc(0.3f, true, 0.3f);
				FMTVFXMeshLayer Wrap = Layer(Paths::Funnel, Paths::MatAir, FVector(80.f, 80.f, 110.f), FC::White, 1.f, FMTVFXCurve::Grow(0.f, 0.4f),
					FMTVFXCurve::Grow(0.3f, 0.9f));
				Wrap.Offset = FVector(0.f, 0.f, -90.f);
				Wrap.SpinSpeed = 720.f;
				Wrap.Scalars.Add(TEXT("Distortion"), 1.f);
				D.Meshes.Add(Wrap);
				FMTVFXEmitter Swirl = Condensation(0, 50.f, 6.f, 12.f, 0.35f, 0.5f);
				Swirl.Shape = EMTVFXShape::Ring;
				Swirl.Radius = 70.f;
				Swirl.Orbit = 800.f;
				Swirl.SpeedMin = 50.f;
				Swirl.SpeedMax = 150.f;
				Swirl.Stretch = 6.f;
				Swirl.bWorldSpace = false;
				Swirl.bHero = true;
				D.Emitters.Add(Swirl);
				FMTVFXEmitter Specks = DustCloud(0, 20.f, 20.f, 45.f, 0.3f, 0.7f);
				Specks.Shape = EMTVFXShape::Ring;
				Specks.Radius = 80.f;
				Specks.Orbit = 600.f;
				Specks.bWorldSpace = false;
				D.Emitters.Add(Specks);
				Out.Add(TEXT("Tornado.Lift"), D);
			}
			{
				// Dissipation: the funnel unravels: a pulse of air, dust spreading and settling, leaves raining down,
				// condensation dispersing.
				FMTVFXDesc D = ImpactDesc(1.4f);
				FMTVFXMeshLayer Unravel = Layer(Paths::Funnel, Paths::MatAir, FVector(360.f, 360.f, 475.f), FC::White, 1.f,
					FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 1.3f } }));
				Unravel.Duration = 0.6f;
				Unravel.SpinSpeed = 300.f;
				Unravel.Scalars.Add(TEXT("Distortion"), 1.2f);
				D.Meshes.Add(Unravel);
				D.Meshes.Add(AirPulse(Paths::Sphere, 100.f, 450.f, 0.4f, 1.f));
				FMTVFXEmitter Cloud = DustCloud(30, 0.f, 120.f, 240.f, 0.45f, 2.f);
				Cloud.Shape = EMTVFXShape::Ring;
				Cloud.Radius = 150.f;
				Cloud.Offset = FVector(0.f, 0.f, 300.f);
				Cloud.bRadial = true;
				Cloud.SpeedMin = 200.f;
				Cloud.SpeedMax = 500.f;
				Cloud.Drag = 2.f;
				Cloud.Gravity = 60.f;
				Cloud.Buoyancy = 0.f;
				Cloud.bHero = true;
				D.Emitters.Add(Cloud);
				FMTVFXEmitter Falling = Leaves(24, 0.f);
				Falling.Shape = EMTVFXShape::Sphere;
				Falling.Radius = 250.f;
				Falling.Offset = FVector(0.f, 0.f, 400.f);
				Falling.bRadial = true;
				Falling.SpeedMin = 100.f;
				Falling.SpeedMax = 300.f;
				Falling.LifeMin = 1.5f;
				Falling.LifeMax = 2.5f;
				D.Emitters.Add(Falling);
				FMTVFXEmitter Disperse = Condensation(20, 0.f, 10.f, 20.f, 0.3f, 0.5f);
				Disperse.bRadial = true;
				Disperse.Shape = EMTVFXShape::Ring;
				Disperse.Radius = 200.f;
				Disperse.Offset = FVector(0.f, 0.f, 250.f);
				Disperse.SpeedMin = 400.f;
				Disperse.SpeedMax = 800.f;
				D.Emitters.Add(Disperse);
				Out.Add(TEXT("Tornado.Dissipation"), D);
			}
			{
				// Wind Burst formation (compress, 0.12 s): the air visibly pulled in: a sphere of warped air and two rings
				// contracting onto the caster, condensation rushing inward, dust and leaves dragged along the ground.
				FMTVFXDesc D = Desc(0.16f);
				D.FadeOut = 0.05f;
				FMTVFXMeshLayer Squeeze = AirPulse(Paths::Sphere, 320.f, 50.f, 0.16f, 1.4f);
				Squeeze.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.3f }, { 1.f, 1.f } });
				D.Meshes.Add(Squeeze);
				for (int32 i = 0; i < 2; ++i)
				{
					FMTVFXMeshLayer Band = ShockwaveRing(420.f - i * 90.f, 70.f, 0.16f, WindWhite, 2.2f, 0.f, 0.08f);
					Band.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
					Band.Rotation = FRotator(0.f, 0.f, i * 90.f);
					D.Meshes.Add(Band);
				}
				FMTVFXEmitter Rush = Condensation(30, 0.f, 8.f, 16.f, 0.4f, 0.16f);
				Rush.Shape = EMTVFXShape::SphereShell;
				Rush.Radius = 360.f;
				Rush.bRadial = true;
				Rush.SpeedMin = -2400.f;
				Rush.SpeedMax = -1600.f;
				Rush.Drag = 0.f;
				Rush.bHero = true;
				D.Emitters.Add(Rush);
				FMTVFXEmitter Dragged = DustCloud(14, 0.f, 50.f, 110.f, 0.35f, 0.25f);
				Dragged.Shape = EMTVFXShape::Ring;
				Dragged.Radius = 300.f;
				Dragged.bRadial = true;
				Dragged.SpeedMin = -1400.f;
				Dragged.SpeedMax = -900.f;
				Dragged.Drag = 0.f;
				Dragged.bFollowGround = true;
				D.Emitters.Add(Dragged);
				FMTVFXEmitter Drawn = Leaves(8, 0.f);
				Drawn.Shape = EMTVFXShape::Ring;
				Drawn.Radius = 280.f;
				Drawn.bRadial = true;
				Drawn.SpeedMin = -1300.f;
				Drawn.SpeedMax = -800.f;
				Drawn.Gravity = 0.f;
				Drawn.LifeMin = 0.2f;
				Drawn.LifeMax = 0.3f;
				D.Emitters.Add(Drawn);
				Out.Add(TEXT("WindBurst.Formation"), D);
			}
			{
				// Impact (VR 620): a huge sphere of pressure: its bright edge racing out, warped air, three ground rings, a
				// front of condensation, dust and pebbles blown out along the ground, leaves. Heavy tier.
				FMTVFXDesc D = ImpactDesc(1.2f);
				FMTVFXMeshLayer Front = Orb(620.f, WindWhite, 1.f, 1.f, 0.7f);
				Front.Duration = 0.35f;
				Front.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.1f }, { 1.f, 1.f } });
				Front.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				Flow(Front, 3.f, 3.f, 2.f, 0.5f, 2.f);
				D.Meshes.Add(Front);
				D.Meshes.Add(AirPulse(Paths::Sphere, 60.f, 640.f, 0.35f, 1.6f));
				for (int32 i = 0; i < 3; ++i)
				{
					D.Meshes.Add(ShockwaveRing(40.f, 700.f - i * 130.f, 0.5f, WindWhite, 4.f - i * 1.f, i * 0.07f, 0.09f));
				}
				D.Meshes.Add(AirPulse(Paths::ShockRing, 60.f, 720.f, 0.45f, 1.5f, 0.02f, 0.05f));
				FMTVFXEmitter Vapour = Condensation(60, 0.f, 10.f, 20.f, 0.4f, 0.45f);
				Vapour.bRadial = true;
				Vapour.Shape = EMTVFXShape::Sphere;
				Vapour.Radius = 60.f;
				Vapour.SpeedMin = 1300.f;
				Vapour.SpeedMax = 2200.f;
				Vapour.Drag = 3.5f;
				Vapour.bHero = true;
				D.Emitters.Add(Vapour);
				FMTVFXEmitter Blown = DustCloud(44, 0.f, 90.f, 190.f, 0.6f, 1.3f);
				Blown.Shape = EMTVFXShape::Ring;
				Blown.Radius = 60.f;
				Blown.bRadial = true;
				Blown.SpeedMin = 700.f;
				Blown.SpeedMax = 1200.f;
				Blown.Drag = 3.f;
				Blown.bFollowGround = true;
				D.Emitters.Add(Blown);
				FMTVFXEmitter Foliage = Leaves(24, 0.f);
				Foliage.Shape = EMTVFXShape::Ring;
				Foliage.Radius = 80.f;
				Foliage.bRadial = true;
				Foliage.SpeedMin = 500.f;
				Foliage.SpeedMax = 900.f;
				D.Emitters.Add(Foliage);
				FMTVFXEmitter Pebbles = Debris(10, 400.f, 800.f, 2.f, 5.f);
				Pebbles.Shape = EMTVFXShape::Ring;
				Pebbles.Radius = 80.f;
				Pebbles.bRadial = true;
				Pebbles.bFollowGround = true;
				Pebbles.Offset = FVector(0.f, 0.f, 10.f);
				D.Emitters.Add(Pebbles);
				D.Shakes.Add(HeavyShake(0.35f));
				Out.Add(TEXT("WindBurst.Impact"), D);
			}
		}

		// =====================================================================================
		// Earth: debris, cracks, dust and craters. Stone Cannon (shared) and Rudeus's signature cannon share one builder.
		// =====================================================================================

		/**
		 * Cannon formation (loop at the hand, charge-driven): fragments rip out of the ground under the hand and converge
		 * 25 cm above the palm, the rough core is compressed into a smooth drill slug that spins up inside pressure rings
		 * while dust is pulled in. Rudeus: x1.7 size, far faster spin, three rings with mana edges, crackling mana and a
		 * glow, violent vibration and warped air at full charge. Shared: plainer, one ring.
		 */
		FMTVFXDesc CannonFormation(bool bRudeus)
		{
			FMTVFXDesc D = Desc(1.2f, true, 0.15f);
			D.Charge.SizeScale = bRudeus ? 1.7f : 1.35f;
			D.Charge.SpinScale = bRudeus ? 5.f : 3.f;
			D.Charge.RateScale = bRudeus ? 2.2f : 1.6f;
			D.Charge.IntensityScale = bRudeus ? 2.4f : 1.4f;
			D.Charge.Vibration = bRudeus ? 1.8f : 0.6f;
			const FVector Above(0.f, 0.f, 25.f);
			const float SeamGlow = bRudeus ? 3.f : 0.f;
			FMTVFXMeshLayer Rough = RockBody(Paths::RockChunkD, FVector(13.f), SeamGlow);
			Rough.Offset = Above;
			Rough.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 0.2f, 1.f }, { 0.55f, 0.8f }, { 0.75f, 0.01f }, { 1.f, 0.01f } });
			Rough.SpinSpeed = 160.f;
			Rough.SpinAxis = FVector(0.4f, 0.3f, 1.f);
			D.Meshes.Add(Rough);
			FMTVFXMeshLayer Slug = RockBody(Paths::Slug, bRudeus ? FVector(26.f, 12.f, 12.f) : FVector(20.f, 9.f, 9.f), SeamGlow);
			Slug.Offset = Above;
			Slug.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.01f }, { 0.25f, 0.05f }, { 0.7f, 0.9f }, { 1.f, 1.f } });
			Slug.SpinSpeed = 720.f;
			Slug.SpinAxis = FVector::ForwardVector;
			D.Meshes.Add(Slug);
			const int32 RingCount = bRudeus ? 3 : 1;
			for (int32 i = 0; i < RingCount; ++i)
			{
				const float BandRadius = 34.f + i * 12.f;
				FMTVFXMeshLayer Band = Haze(Paths::ShockRing, FVector(BandRadius, BandRadius, 1.f), 1.2f);
				Band.Rotation = FRotator(90.f, 0.f, 0.f);
				Band.Offset = Above + FVector(-10.f - i * 16.f, 0.f, 0.f);
				Band.ChargeThreshold = bRudeus ? 0.25f + i * 0.3f : 0.5f;
				D.Meshes.Add(Band);
				if (bRudeus)
				{
					FMTVFXMeshLayer Edge = ThinRing(BandRadius - 4.f, Mana, 1.6f);
					Edge.Rotation = FRotator(90.f, 0.f, 0.f);
					Edge.Offset = Band.Offset;
					Edge.ChargeThreshold = Band.ChargeThreshold;
					Edge.SpinSpeed = 540.f;
					Edge.SpinAxis = FVector::ForwardVector;
					Flow(Edge, 4.f, 1.f, 3.f, 0.f, 1.8f);
					D.Meshes.Add(Edge);
				}
			}
			// At full spin the air around the slug warps.
			FMTVFXMeshLayer Warp = Haze(Paths::Sphere, bRudeus ? FVector(44.f, 30.f, 30.f) : FVector(34.f, 22.f, 22.f), bRudeus ? 1.4f : 0.9f);
			Warp.Offset = Above;
			Warp.ChargeThreshold = 0.7f;
			Warp.bWobble = true;
			D.Meshes.Add(Warp);
			// Fragments ripped out of the ground under the hand fly up and converge.
			FMTVFXEmitter Ripped = Debris(bRudeus ? 6 : 3, 150.f, 350.f, 3.f, bRudeus ? 9.f : 7.f);
			Ripped.Rate = bRudeus ? 14.f : 8.f;
			Ripped.Shape = EMTVFXShape::Disc;
			Ripped.Radius = 140.f;
			Ripped.bFollowGround = true;
			Ripped.Attract = 2600.f;
			Ripped.Gravity = 0.f;
			Ripped.Drag = 1.2f;
			Ripped.bBounce = false;
			Ripped.LifeMin = 0.35f;
			Ripped.LifeMax = 0.5f;
			Ripped.ConeDeg = 20.f;
			Ripped.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.8f, 0.7f }, { 1.f, 0.05f } });
			Ripped.bHero = true;
			if (bRudeus)
			{
				Ripped.Scalars.Add(TEXT("Glow"), 2.f);
				Ripped.ParamColor = Mana;
			}
			D.Emitters.Add(Ripped);
			FMTVFXEmitter Orbiting = Debris(0, 0.f, 20.f, 2.f, 5.f);
			Orbiting.Rate = bRudeus ? 26.f : 16.f;
			Orbiting.Shape = EMTVFXShape::SphereShell;
			Orbiting.Radius = 70.f;
			Orbiting.Offset = Above;
			Orbiting.Attract = 1800.f;
			Orbiting.Orbit = 520.f;
			Orbiting.Gravity = 0.f;
			Orbiting.bBounce = false;
			Orbiting.bWorldSpace = false;
			Orbiting.LifeMin = 0.35f;
			Orbiting.LifeMax = 0.5f;
			Orbiting.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.2f } });
			D.Emitters.Add(Orbiting);
			FMTVFXEmitter Pulled = Gather(40.f, 90.f, DustLight, EMTVFXRender::SpriteSmoke, 1.f);
			Pulled.Offset = Above;
			Pulled.SizeMin = 10.f;
			Pulled.SizeMax = 22.f;
			Pulled.AlphaOverLife = FMTVFXCurve::FadeInOut(0.2f, 0.4f);
			D.Emitters.Add(Pulled);
			FMTVFXEmitter Grit = Gather(30.f, 60.f, Dust, EMTVFXRender::Stretched, 1.f);
			Grit.MaterialPath = Paths::MatSmoke;
			Grit.Texture = Paths::TexDot;
			Grit.Offset = Above;
			Grit.SizeMin = 2.f;
			Grit.SizeMax = 4.f;
			D.Emitters.Add(Grit);
			if (bRudeus)
			{
				FMTVFXMeshLayer Sheath = Orb(20.f, Mana, 1.4f, 1.f, 0.6f);
				Sheath.Size = FVector(36.f, 18.f, 18.f);
				Sheath.Offset = Above;
				Sheath.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.5f);
				Sheath.ScaleOverLife = FMTVFXCurve::Grow(0.3f, 0.8f);
				D.Meshes.Add(Sheath);
				FMTVFXEmitter ManaIn = Gather(50.f, 55.f, Mana, EMTVFXRender::SpriteAdd, 6.f);
				ManaIn.Offset = Above;
				ManaIn.SizeMin = 2.5f;
				ManaIn.SizeMax = 5.f;
				D.Emitters.Add(ManaIn);
				FMTVFXEmitter Arcs = Sprites(EMTVFXRender::Stretched, 0, 30.f, 0.08f, 0.14f, 2.f, 3.5f, ManaWhite, Mana, 8.f);
				Arcs.Shape = EMTVFXShape::SphereShell;
				Arcs.Radius = 26.f;
				Arcs.Offset = Above;
				Arcs.Orbit = 1400.f;
				Arcs.SpeedMin = 0.f;
				Arcs.SpeedMax = 30.f;
				Arcs.Stretch = 6.f;
				Arcs.bWorldSpace = false;
				Arcs.ChargeThreshold = 0.6f;
				D.Emitters.Add(Arcs);
				D.Lights.Add(Glow(Mana, 60.f, 450.f, 0.2f, FMTVFXCurve::Grow(0.f, 0.45f)));
			}
			return D;
		}

		/**
		 * Cannon release (at the hand, aimed; scale 1 + 0.5 x charge). Rudeus: a flash, a huge cone of pressure, a circular
		 * shockwave at the hand, two sonic-boom rings racing ahead, a trailing spiral, dust blasted out behind him and
		 * ground pressure at his feet, mana sparks, a camera impulse that reaches the heavy tier when charged.
		 * Shared: a small flash, one ring, grit and dust.
		 */
		FMTVFXDesc CannonRelease(bool bRudeus)
		{
			FMTVFXDesc D = Desc(bRudeus ? 0.9f : 0.6f);
			D.Meshes.Add(Flash(bRudeus ? 45.f : 28.f, bRudeus ? ManaWhite : FireWhite, bRudeus ? 10.f : 5.f, 0.1f));
			D.Meshes.Add(FacingRing(20.f, bRudeus ? 260.f : 120.f, 0.25f, bRudeus ? ManaWhite : DustLight, bRudeus ? 3.f : 1.4f, 0.f, 0.08f));
			D.Meshes.Add(FacingAir(20.f, bRudeus ? 300.f : 140.f, 0.3f, 1.4f));
			if (bRudeus)
			{
				FMTVFXMeshLayer Cone = Layer(Paths::ConeShell, Paths::MatAir, FVector(130.f, 130.f, 170.f), FC::White, 1.f,
					FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 0.25f }, { 0.4f, 1.f }, { 1.f, 1.15f } }));
				Cone.Rotation = FRotator(-90.f, 0.f, 0.f); // apex at the hand, opening forward
				Cone.Duration = 0.3f;
				Cone.Scalars.Add(TEXT("Distortion"), 1.6f);
				D.Meshes.Add(Cone);
				for (int32 i = 0; i < 2; ++i)
				{
					FMTVFXMeshLayer Boom = FacingAir(40.f, 170.f + i * 40.f, 0.22f, 1.6f, 0.03f + i * 0.05f);
					Boom.Velocity = FVector(2400.f, 0.f, 0.f);
					D.Meshes.Add(Boom);
					FMTVFXMeshLayer BoomEdge = FacingRing(30.f, 150.f + i * 40.f, 0.2f, ManaWhite, 2.f, 0.03f + i * 0.05f, 0.05f);
					BoomEdge.Velocity = FVector(2400.f, 0.f, 0.f);
					D.Meshes.Add(BoomEdge);
				}
				FMTVFXMeshLayer Wake = Layer(Paths::Spiral, Paths::MatGlow, FVector(190.f, 34.f, 34.f), DustLight, 1.2f,
					FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 0.3f }, { 0.25f, 1.f }, { 1.f, 1.1f } }));
				Wake.Offset = FVector(190.f, 0.f, 0.f);
				Wake.Duration = 0.35f;
				Wake.SpinSpeed = 1400.f;
				Wake.SpinAxis = FVector::ForwardVector;
				Flow(Wake, 1.f, 4.f, 0.f, -3.f, 1.6f);
				D.Meshes.Add(Wake);
				FMTVFXMeshLayer WakeMana = Wake;
				WakeMana.Color = Mana;
				WakeMana.Intensity = 1.8f;
				WakeMana.Rotation = FRotator(0.f, 0.f, 120.f);
				WakeMana.Size = FVector(170.f, 26.f, 26.f);
				WakeMana.Offset = FVector(170.f, 0.f, 0.f);
				D.Meshes.Add(WakeMana);
			}
			FMTVFXEmitter Behind = DustCloud(bRudeus ? 20 : 8, 0.f, 70.f, bRudeus ? 150.f : 100.f, 0.5f, 1.2f);
			Behind.bFollowGround = true;
			Behind.Offset = FVector(-90.f, 0.f, 15.f);
			Behind.Direction = FVector(-1.f, 0.f, 0.35f);
			Behind.ConeDeg = 35.f;
			Behind.SpeedMin = 500.f;
			Behind.SpeedMax = bRudeus ? 1100.f : 700.f;
			Behind.Drag = 3.2f;
			Behind.bHero = bRudeus;
			D.Emitters.Add(Behind);
			FMTVFXEmitter Grit = DirtClods(bRudeus ? 22 : 10, 900.f, bRudeus ? 2200.f : 1400.f, 2.f, 4.f);
			Grit.Direction = FVector::ForwardVector;
			Grit.ConeDeg = 10.f;
			Grit.Gravity = 300.f;
			D.Emitters.Add(Grit);
			if (bRudeus)
			{
				FMTVFXEmitter Spark = Sprites(EMTVFXRender::Stretched, 20, 0.f, 0.15f, 0.3f, 2.f, 4.f, ManaWhite, Mana, 8.f);
				Spark.Direction = FVector::ForwardVector;
				Spark.ConeDeg = 14.f;
				Spark.SpeedMin = 1200.f;
				Spark.SpeedMax = 2600.f;
				Spark.Stretch = 8.f;
				D.Emitters.Add(Spark);
				FMTVFXEmitter Feet = DustCloud(14, 0.f, 60.f, 120.f, 0.4f, 0.9f);
				Feet.Shape = EMTVFXShape::Ring;
				Feet.Radius = 40.f;
				Feet.bRadial = true;
				Feet.SpeedMin = 500.f;
				Feet.SpeedMax = 900.f;
				Feet.Drag = 3.f;
				Feet.bFollowGround = true;
				Feet.Offset = FVector(-40.f, 0.f, 10.f);
				D.Emitters.Add(Feet);
				D.Lights.Add(FlashLight(Mana, 400.f, 900.f, 0.3f));
			}
			FMTVFXShake Kick = MinorShake(bRudeus ? 0.14f : 0.1f);
			Kick.ScaleStrength = bRudeus ? 3.5f : 1.f; // Rudeus fully charged (scale 1.5): ~0.39, the heavy tier
			Kick.Duration = bRudeus ? 0.3f : 0.2f;
			D.Shakes.Add(Kick);
			return D;
		}

		/** Cannon travel: the drill slug spinning inside its spiral shroud (visual radius 36 for Rudeus, 22 shared). */
		FMTVFXDesc CannonTravel(bool bRudeus)
		{
			FMTVFXDesc D = Desc(0.2f, true, 0.15f);
			FMTVFXMeshLayer Slug = RockBody(Paths::Slug, bRudeus ? FVector(36.f, 17.f, 17.f) : FVector(26.f, 12.f, 12.f), bRudeus ? 3.5f : 0.f);
			Slug.SpinSpeed = 2400.f;
			Slug.SpinAxis = FVector::ForwardVector;
			D.Meshes.Add(Slug);
			FMTVFXMeshLayer Shroud = Layer(Paths::Spiral, Paths::MatGlow, bRudeus ? FVector(70.f, 36.f, 36.f) : FVector(50.f, 22.f, 22.f),
				bRudeus ? Mana : DustLight, bRudeus ? 1.3f : 0.7f, FMTVFXCurve::Grow(0.f, 0.3f));
			Shroud.Offset = FVector(-40.f, 0.f, 0.f);
			Shroud.SpinSpeed = 1500.f;
			Shroud.SpinAxis = FVector::ForwardVector;
			Shroud.Scalars.Add(TEXT("FresnelMix"), 0.3f);
			Shroud.Scalars.Add(TEXT("NoiseAmount"), 0.8f);
			Flow(Shroud, 1.f, 3.f, 0.f, -2.5f, 1.8f);
			D.Meshes.Add(Shroud);
			D.Meshes.Add(Haze(Paths::Sphere, bRudeus ? FVector(70.f, 38.f, 38.f) : FVector(46.f, 24.f, 24.f), bRudeus ? 1.3f : 0.9f));
			const int32 RingCount = bRudeus ? 2 : 1;
			for (int32 i = 0; i < RingCount; ++i)
			{
				const float BandRadius = (bRudeus ? 44.f : 30.f) + i * 14.f;
				FMTVFXMeshLayer Band = Haze(Paths::ShockRing, FVector(BandRadius, BandRadius, 1.f), 1.2f);
				Band.Rotation = FRotator(90.f, 0.f, 0.f);
				Band.Offset = FVector(-55.f - i * 50.f, 0.f, 0.f);
				D.Meshes.Add(Band);
			}
			D.Emitters.Add(DustCloud(0, bRudeus ? 70.f : 45.f, 20.f, 50.f, 0.4f, 0.7f));
			FMTVFXEmitter Grit = DirtClods(0, 300.f, 700.f, 2.f, 4.f);
			Grit.Rate = bRudeus ? 70.f : 45.f;
			Grit.Direction = -FVector::ForwardVector;
			Grit.ConeDeg = 12.f;
			Grit.Gravity = 300.f;
			Grit.LifeMin = 0.2f;
			Grit.LifeMax = 0.35f;
			D.Emitters.Add(Grit);
			if (bRudeus)
			{
				FMTVFXEmitter Spark = Sprites(EMTVFXRender::Stretched, 0, 60.f, 0.15f, 0.3f, 2.f, 4.f, ManaWhite, Mana, 8.f);
				Spark.Direction = -FVector::ForwardVector;
				Spark.ConeDeg = 20.f;
				Spark.SpeedMin = 200.f;
				Spark.SpeedMax = 600.f;
				Spark.Shape = EMTVFXShape::SphereShell;
				Spark.Radius = 30.f;
				D.Emitters.Add(Spark);
				D.Lights.Add(Glow(Mana, 60.f, 500.f));
			}
			return D;
		}

		/** Cannon pierce (X = velocity): the slug bursting out of a body, soil and chips thrown through, a ring, a puff. */
		FMTVFXDesc CannonPierce(bool bRudeus)
		{
			FMTVFXDesc D = Desc(0.7f);
			D.Meshes.Add(Flash(bRudeus ? 36.f : 22.f, bRudeus ? ManaWhite : FireWhite, bRudeus ? 7.f : 4.f, 0.08f));
			D.Meshes.Add(FacingRing(10.f, bRudeus ? 130.f : 80.f, 0.2f, bRudeus ? Mana : DustLight, bRudeus ? 2.4f : 1.2f));
			FMTVFXEmitter Through = DirtClods(bRudeus ? 16 : 10, 700.f, 1500.f, 2.f, 5.f);
			Through.Direction = FVector::ForwardVector;
			Through.ConeDeg = 25.f;
			Through.Gravity = 500.f;
			Through.bHero = true;
			D.Emitters.Add(Through);
			FMTVFXEmitter Chips = Debris(bRudeus ? 6 : 4, 400.f, 900.f, 3.f, 7.f);
			Chips.Direction = FVector::ForwardVector;
			Chips.ConeDeg = 30.f;
			D.Emitters.Add(Chips);
			FMTVFXEmitter Puff = DustCloud(5, 0.f, 30.f, 70.f, 0.45f, 0.8f);
			Puff.Direction = FVector::ForwardVector;
			Puff.ConeDeg = 40.f;
			Puff.SpeedMin = 200.f;
			Puff.SpeedMax = 450.f;
			D.Emitters.Add(Puff);
			D.Shakes.Add(MinorShake(bRudeus ? 0.12f : 0.08f));
			if (bRudeus)
			{
				D.Lights.Add(FlashLight(Mana, 150.f, 500.f, 0.25f));
			}
			return D;
		}

		/**
		 * Cannon impact (Rudeus VR 260, shared VR 170): a flash, a pressure sphere and ring, rock chunks, slabs and soil
		 * flung out, a dust cloud and skirt, a crater with cracks. Rudeus adds a mana shockwave, heaved slabs around the
		 * crater, mana sparks, lingering dust and a strong light; charged, it shakes in the heavy tier.
		 */
		FMTVFXDesc CannonImpact(bool bRudeus)
		{
			const float R = bRudeus ? 260.f : 170.f;
			FMTVFXDesc D = ImpactDesc(bRudeus ? 2.8f : 2.2f);
			D.Meshes.Add(Flash(R * 0.5f, bRudeus ? ManaWhite : FireWhite, bRudeus ? 11.f : 6.f, 0.12f));
			D.Meshes.Add(AirPulse(Paths::Sphere, 30.f, R * 1.5f, 0.32f, 1.3f));
			D.Meshes.Add(AirPulse(Paths::ShockRing, 40.f, R * 1.8f, 0.4f, 1.4f, 0.02f, 0.05f));
			AddRockBurst(D, R, bRudeus ? 34 : 22);
			if (bRudeus)
			{
				D.Meshes.Add(ShockwaveRing(30.f, R * 1.4f, 0.3f, Mana, 3.f, 0.f, 0.06f));
				AddRaisedSlabs(D, 7, R * 0.55f, 38.f, 28.f, 2.4f, 17);
				FMTVFXEmitter Spark = Sprites(EMTVFXRender::Stretched, 26, 0.f, 0.2f, 0.45f, 2.f, 4.f, ManaWhite, Mana, 8.f);
				Spark.bRadial = true;
				Spark.Shape = EMTVFXShape::Sphere;
				Spark.Radius = 20.f;
				Spark.SpeedMin = 800.f;
				Spark.SpeedMax = 1800.f;
				Spark.Drag = 2.f;
				Spark.Stretch = 6.f;
				D.Emitters.Add(Spark);
				FMTVFXEmitter Lingering = DustCloud(10, 4.f, R * 0.6f, R * 1.1f, 0.3f, 3.2f);
				Lingering.Delay = 0.4f;
				Lingering.EmitDuration = 1.2f;
				Lingering.Shape = EMTVFXShape::Disc;
				Lingering.Radius = R * 0.8f;
				Lingering.bFollowGround = true;
				Lingering.Offset = FVector(0.f, 0.f, 40.f);
				Lingering.Buoyancy = 25.f;
				Lingering.SpeedMin = 10.f;
				Lingering.SpeedMax = 40.f;
				D.Emitters.Add(Lingering);
				D.Lights.Add(Glow(Mana, 700.f, 1400.f, 0.f, FMTVFXCurve({ { 0.f, 1.f }, { 0.25f, 0.3f }, { 1.f, 0.f } })));
			}
			else
			{
				D.Lights.Add(FlashLight(FireWhite, 180.f, 800.f, 0.4f));
			}
			AddCrater(D, R * (bRudeus ? 0.8f : 0.7f), 14.f);
			AddCracks(D, R * (bRudeus ? 1.35f : 1.1f), 14.f);
			FMTVFXShake Kick = MinorShake(bRudeus ? 0.16f : 0.12f);
			Kick.ScaleStrength = bRudeus ? 2.6f : 1.2f; // Rudeus charged (VR 416, x1.6): ~0.41, the heavy tier
			Kick.Duration = bRudeus ? 0.35f : 0.22f;
			Kick.Radius = bRudeus ? 4000.f : 2500.f;
			D.Shakes.Add(Kick);
			return D;
		}

		/** Cannon dissipation: the slug crumbles into chips and dust (Rudeus: its mana motes scatter). */
		FMTVFXDesc CannonDissipation(bool bRudeus)
		{
			FMTVFXDesc D = Desc(0.8f);
			FMTVFXEmitter Crumbs = Debris(bRudeus ? 10 : 7, 60.f, 250.f, 2.5f, 6.f);
			Crumbs.Shape = EMTVFXShape::Sphere;
			Crumbs.Radius = 15.f;
			Crumbs.ConeDeg = 90.f;
			D.Emitters.Add(Crumbs);
			D.Emitters.Add(DustCloud(8, 0.f, 30.f, 70.f, 0.4f, 1.f));
			D.Emitters.Add(DirtClods(10, 100.f, 300.f));
			if (bRudeus)
			{
				FMTVFXEmitter Scatter = Motes(0.f, 30.f, Mana, 6.f);
				Scatter.Burst = 14;
				Scatter.bRadial = true;
				Scatter.SpeedMin = 100.f;
				Scatter.SpeedMax = 260.f;
				D.Emitters.Add(Scatter);
			}
			return D;
		}

		void AddCannons(TMap<FName, FMTVFXDesc>& Out)
		{
			Out.Add(TEXT("RudeusCannon.Formation"), CannonFormation(true));
			Out.Add(TEXT("RudeusCannon.Release"), CannonRelease(true));
			Out.Add(TEXT("RudeusCannon.Travel"), CannonTravel(true));
			Out.Add(TEXT("RudeusCannon.Pierce"), CannonPierce(true));
			Out.Add(TEXT("RudeusCannon.Impact"), CannonImpact(true));
			Out.Add(TEXT("RudeusCannon.Dissipation"), CannonDissipation(true));
			Out.Add(TEXT("StoneCannon.Formation"), CannonFormation(false));
			Out.Add(TEXT("StoneCannon.Release"), CannonRelease(false));
			Out.Add(TEXT("StoneCannon.Travel"), CannonTravel(false));
			Out.Add(TEXT("StoneCannon.Pierce"), CannonPierce(false));
			Out.Add(TEXT("StoneCannon.Impact"), CannonImpact(false));
			Out.Add(TEXT("StoneCannon.Dissipation"), CannonDissipation(false));
		}

		void AddEarth(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Stone Bullet formation (basic attack): a pebble compressed in the palm from dust and chips.
				FMTVFXDesc D = Desc(0.35f);
				D.FadeOut = 0.1f;
				FMTVFXMeshLayer Seed = RockBody(Paths::Slug, FVector(9.f, 5.f, 5.f));
				Seed.ScaleOverLife = FMTVFXCurve::Grow(0.1f, 0.9f);
				Seed.SpinSpeed = 600.f;
				Seed.SpinAxis = FVector::ForwardVector;
				D.Meshes.Add(Seed);
				D.Emitters.Add(Gather(50.f, 45.f, DustLight, EMTVFXRender::SpriteSmoke, 1.f));
				FMTVFXEmitter Chips = Debris(0, 0.f, 20.f, 1.5f, 3.f);
				Chips.Rate = 16.f;
				Chips.Shape = EMTVFXShape::SphereShell;
				Chips.Radius = 45.f;
				Chips.Attract = 1600.f;
				Chips.Orbit = 400.f;
				Chips.Gravity = 0.f;
				Chips.bBounce = false;
				Chips.bWorldSpace = false;
				Chips.LifeMin = 0.3f;
				Chips.LifeMax = 0.45f;
				D.Emitters.Add(Chips);
				Out.Add(TEXT("StoneBullet.Formation"), D);
			}
			{
				// Travel: a spinning stone slug trailing grit and dust.
				FMTVFXDesc D = Desc(0.2f, true, 0.1f);
				FMTVFXMeshLayer Slug = RockBody(Paths::Slug, FVector(16.f, 8.f, 8.f));
				Slug.SpinSpeed = 1440.f;
				Slug.SpinAxis = FVector::ForwardVector;
				D.Meshes.Add(Slug);
				D.Emitters.Add(DustCloud(0, 40.f, 12.f, 30.f, 0.35f, 0.5f));
				FMTVFXEmitter Grit = DirtClods(0, 200.f, 500.f, 1.5f, 3.f);
				Grit.Rate = 40.f;
				Grit.Direction = -FVector::ForwardVector;
				Grit.ConeDeg = 10.f;
				Grit.Gravity = 400.f;
				Grit.LifeMin = 0.15f;
				Grit.LifeMax = 0.3f;
				D.Emitters.Add(Grit);
				Out.Add(TEXT("StoneBullet.Travel"), D);
			}
			{
				// Impact: chips, soil and a puff of dust, a small ring, a tiny camera tick.
				FMTVFXDesc D = ImpactDesc(0.9f);
				D.Meshes.Add(Flash(14.f, FireWhite, 3.f, 0.05f));
				D.Emitters.Add(Debris(10, 200.f, 500.f, 3.f, 8.f));
				D.Emitters.Add(DirtClods(10, 250.f, 600.f));
				D.Emitters.Add(DustCloud(8, 0.f, 30.f, 70.f, 0.5f, 0.9f));
				D.Meshes.Add(ShockwaveRing(10.f, 90.f, 0.2f, DustLight, 1.f));
				D.Shakes.Add(MinorShake(0.05f));
				Out.Add(TEXT("StoneBullet.Impact"), D);
			}
			{
				// Earth Wall formation: dust drawn to the rising hand, stones circling up around it, the ground trembling.
				FMTVFXDesc D = Desc(0.4f, true, 0.15f);
				FMTVFXEmitter Pulled = Gather(50.f, 80.f, DustLight, EMTVFXRender::SpriteSmoke, 1.f);
				Pulled.SizeMin = 12.f;
				Pulled.SizeMax = 24.f;
				D.Emitters.Add(Pulled);
				FMTVFXEmitter Stones = Debris(0, 80.f, 200.f, 2.f, 5.f);
				Stones.Rate = 20.f;
				Stones.Shape = EMTVFXShape::Ring;
				Stones.Radius = 50.f;
				Stones.Orbit = 300.f;
				Stones.Gravity = -100.f;
				Stones.bBounce = false;
				Stones.bWorldSpace = false;
				Stones.LifeMin = 0.4f;
				Stones.LifeMax = 0.6f;
				Stones.ConeDeg = 15.f;
				Stones.bHero = true;
				D.Emitters.Add(Stones);
				FMTVFXMeshLayer Core = RockBody(Paths::RockChunkD, FVector(9.f));
				Core.ScaleOverLife = FMTVFXCurve::Grow(0.1f, 0.9f);
				Core.SpinSpeed = 120.f;
				Core.SpinAxis = FVector(0.3f, 0.4f, 1.f);
				D.Meshes.Add(Core);
				FMTVFXEmitter Tremor = DustCloud(0, 12.f, 40.f, 80.f, 0.35f, 0.9f);
				Tremor.Shape = EMTVFXShape::Disc;
				Tremor.Radius = 120.f;
				Tremor.bFollowGround = true;
				D.Emitters.Add(Tremor);
				Out.Add(TEXT("EarthWall.Formation"), D);
			}
			{
				// Rise (per segment, at its ground point, X = facing; the segment is 170 wide, 90 thick, 300 high): the ground
				// bursts along its foot, chunks and soil thrown out, a dust column, pebbles raining off the rising top, cracks.
				FMTVFXDesc D = ImpactDesc(1.6f);
				FMTVFXEmitter Foot = DustCloud(20, 0.f, 70.f, 150.f, 0.55f, 1.6f);
				Foot.Shape = EMTVFXShape::Line;
				Foot.Extent = FVector(0.f, 95.f, 0.f);
				Foot.bRadial = true;
				Foot.SpeedMin = 180.f;
				Foot.SpeedMax = 420.f;
				Foot.bFollowGround = true;
				Foot.Offset = FVector(0.f, 0.f, 20.f);
				Foot.bHero = true;
				D.Emitters.Add(Foot);
				FMTVFXEmitter Thrown = Debris(12, 250.f, 650.f, 5.f, 14.f);
				Thrown.Shape = EMTVFXShape::Line;
				Thrown.Extent = FVector(0.f, 90.f, 0.f);
				Thrown.ConeDeg = 40.f;
				D.Emitters.Add(Thrown);
				FMTVFXEmitter Clods = DirtClods(16, 300.f, 800.f);
				Clods.Shape = EMTVFXShape::Line;
				Clods.Extent = FVector(0.f, 90.f, 0.f);
				D.Emitters.Add(Clods);
				FMTVFXEmitter Pebbles = Debris(0, 0.f, 50.f, 3.f, 8.f);
				Pebbles.Rate = 24.f;
				Pebbles.EmitDuration = 1.f;
				Pebbles.Delay = 0.2f;
				Pebbles.Shape = EMTVFXShape::Line;
				Pebbles.Extent = FVector(0.f, 80.f, 0.f);
				Pebbles.Offset = FVector(0.f, 0.f, 290.f);
				Pebbles.Direction = -FVector::UpVector;
				D.Emitters.Add(Pebbles);
				FMTVFXEmitter Column = DustCloud(8, 0.f, 90.f, 180.f, 0.4f, 2.f);
				Column.Shape = EMTVFXShape::Line;
				Column.Extent = FVector(0.f, 90.f, 0.f);
				Column.ConeDeg = 15.f;
				Column.SpeedMin = 150.f;
				Column.SpeedMax = 350.f;
				D.Emitters.Add(Column);
				AddCracks(D, 190.f, 14.f);
				D.Meshes.Add(ShockwaveRing(20.f, 190.f, 0.3f, DustLight, 1.f));
				D.Shakes.Add(HeavyShake(0.3f));
				Out.Add(TEXT("EarthWall.Rise"), D);
			}
			{
				// Crack (at the segment centre; scale 1 then 1.25): chips and grit blown off both faces, a dust puff.
				FMTVFXDesc D = Desc(0.8f);
				FMTVFXEmitter Chips = Debris(10, 200.f, 500.f, 3.f, 8.f);
				Chips.Shape = EMTVFXShape::Box;
				Chips.Extent = FVector(45.f, 85.f, 130.f);
				Chips.bRadial = true;
				D.Emitters.Add(Chips);
				FMTVFXEmitter Puff = DustCloud(8, 0.f, 50.f, 110.f, 0.45f, 1.2f);
				Puff.Shape = EMTVFXShape::Box;
				Puff.Extent = FVector(45.f, 85.f, 130.f);
				D.Emitters.Add(Puff);
				FMTVFXEmitter Grit = DirtClods(14, 200.f, 500.f);
				Grit.Shape = EMTVFXShape::Box;
				Grit.Extent = FVector(45.f, 85.f, 130.f);
				Grit.bRadial = true;
				D.Emitters.Add(Grit);
				D.Shakes.Add(MinorShake(0.08f));
				Out.Add(TEXT("EarthWall.Crack"), D);
			}
			{
				// Crumble (at the segment's ground point): big cracked chunks and rubble falling off the collapsing slab, a
				// billowing dust cloud at its foot, dust sifting down, cracks.
				FMTVFXDesc D = ImpactDesc(2.f);
				FMTVFXEmitter Chunks = Debris(14, 50.f, 250.f, 14.f, 34.f, Paths::RockChunkD);
				Chunks.Shape = EMTVFXShape::Box;
				Chunks.Extent = FVector(45.f, 85.f, 125.f);
				Chunks.Offset = FVector(0.f, 0.f, 150.f);
				Chunks.ConeDeg = 80.f;
				Chunks.bHero = true;
				Chunks.Scalars.Add(TEXT("Crack"), 1.f);
				D.Emitters.Add(Chunks);
				FMTVFXEmitter Rubble = Debris(16, 80.f, 300.f, 4.f, 10.f);
				Rubble.Shape = EMTVFXShape::Box;
				Rubble.Extent = FVector(45.f, 85.f, 125.f);
				Rubble.Offset = FVector(0.f, 0.f, 150.f);
				Rubble.ConeDeg = 80.f;
				D.Emitters.Add(Rubble);
				FMTVFXEmitter Cloud = DustCloud(24, 0.f, 120.f, 250.f, 0.6f, 2.4f);
				Cloud.Shape = EMTVFXShape::Line;
				Cloud.Extent = FVector(0.f, 110.f, 0.f);
				Cloud.bFollowGround = true;
				Cloud.Offset = FVector(0.f, 0.f, 40.f);
				Cloud.bRadial = true;
				Cloud.SpeedMin = 200.f;
				Cloud.SpeedMax = 500.f;
				Cloud.Drag = 2.5f;
				D.Emitters.Add(Cloud);
				FMTVFXEmitter Sifting = DustCloud(10, 0.f, 80.f, 160.f, 0.4f, 1.6f);
				Sifting.Shape = EMTVFXShape::Box;
				Sifting.Extent = FVector(45.f, 85.f, 110.f);
				Sifting.Offset = FVector(0.f, 0.f, 180.f);
				Sifting.Gravity = 80.f;
				Sifting.Buoyancy = 0.f;
				D.Emitters.Add(Sifting);
				AddCracks(D, 220.f, 12.f);
				D.Shakes.Add(MinorShake(0.15f));
				Out.Add(TEXT("EarthWall.Crumble"), D);
			}
			{
				// Earth Spikes formation: dust drawn to the hand, a stone point forming beneath it, shards circling in, the
				// ground trembling.
				FMTVFXDesc D = Desc(0.4f, true, 0.15f);
				FMTVFXEmitter Pulled = Gather(50.f, 70.f, DustLight, EMTVFXRender::SpriteSmoke, 1.f);
				Pulled.SizeMin = 10.f;
				Pulled.SizeMax = 20.f;
				D.Emitters.Add(Pulled);
				FMTVFXMeshLayer Point = RockBody(Paths::Spike, FVector(5.f, 5.f, 14.f));
				Point.Rotation = FRotator(180.f, 0.f, 0.f); // pointing down at the ground
				Point.ScaleOverLife = FMTVFXCurve::Grow(0.1f, 0.8f);
				Point.SpinSpeed = 200.f;
				D.Meshes.Add(Point);
				FMTVFXEmitter Shards = Debris(0, 0.f, 20.f, 2.f, 4.f);
				Shards.Rate = 18.f;
				Shards.Shape = EMTVFXShape::SphereShell;
				Shards.Radius = 50.f;
				Shards.Attract = 1600.f;
				Shards.Orbit = 300.f;
				Shards.Gravity = 0.f;
				Shards.bBounce = false;
				Shards.bWorldSpace = false;
				Shards.LifeMin = 0.35f;
				Shards.LifeMax = 0.5f;
				D.Emitters.Add(Shards);
				FMTVFXEmitter Tremor = DustCloud(0, 14.f, 40.f, 80.f, 0.35f, 0.9f);
				Tremor.Shape = EMTVFXShape::Disc;
				Tremor.Radius = 100.f;
				Tremor.bFollowGround = true;
				D.Emitters.Add(Tremor);
				Out.Add(TEXT("EarthSpikes.Formation"), D);
			}
			{
				// Aim (loop; origin at the caster's feet, X = aim, moved every frame): a glowing fault line on the ground from
				// the feet to 1300 cm (M_Decal_AimLine), the ground trembling along it, dust stirring where each spike will rise.
				FMTVFXDesc D = Desc(0.2f, true, 0.25f);
				FMTVFXDecal Guide = Decal(Paths::DecalAimLine, 45.f, 0.f, Amber, 2.5f);
				Guide.bUntilStop = true;
				Guide.Length = 650.f;
				Guide.Offset = FVector(650.f, 0.f, 0.f);
				Guide.FadeIn = 0.12f;
				Guide.FadeOut = 0.25f;
				Guide.bRandomYaw = false;
				Guide.Depth = 300.f;
				D.Decals.Add(Guide);
				FMTVFXEmitter Tremor = DustCloud(0, 30.f, 30.f, 60.f, 0.3f, 0.7f);
				Tremor.Shape = EMTVFXShape::Box;
				Tremor.Extent = FVector(650.f, 25.f, 0.f);
				Tremor.Offset = FVector(650.f, 0.f, 5.f);
				Tremor.bFollowGround = true;
				Tremor.SpeedMin = 20.f;
				Tremor.SpeedMax = 80.f;
				D.Emitters.Add(Tremor);
				const float Marks[4] = { 320.f, 640.f, 960.f, 1300.f };
				for (int32 i = 0; i < 4; ++i)
				{
					FMTVFXEmitter Mark = DustCloud(0, i == 3 ? 10.f : 5.f, 25.f, 55.f, 0.35f, 0.8f);
					Mark.Shape = EMTVFXShape::Ring;
					Mark.Radius = i == 3 ? 110.f : 55.f;
					Mark.Offset = FVector(Marks[i], 0.f, 5.f);
					Mark.bFollowGround = true;
					Mark.SpeedMin = 30.f;
					Mark.SpeedMax = 90.f;
					D.Emitters.Add(Mark);
				}
				Out.Add(TEXT("EarthSpikes.Aim"), D);
			}
			{
				// Eruption (VR 150, 280 high): a stone lance punches up with two shards, the ground bursts (chunks, soil, dust
				// skirt and ring) and cracks. The lance withdraws at 1.2 s when EarthSpikes.Crumble plays.
				FMTVFXDesc D = ImpactDesc(1.3f);
				FMTVFXMeshLayer Lance = RockBody(Paths::Spike, FVector(60.f, 60.f, 140.f));
				Lance.Offset = FVector(0.f, 0.f, -20.f);
				Lance.ScaleOverLife = RiseHoldSink(0.05f, 0.9f);
				Lance.Rotation = FRotator(4.f, 0.f, -3.f);
				D.Meshes.Add(Lance);
				for (int32 i = 0; i < 2; ++i)
				{
					FMTVFXMeshLayer Shard = RockBody(Paths::SpikeB, FVector(28.f, 28.f, 70.f));
					Shard.Offset = FVector(i == 0 ? 35.f : -30.f, i == 0 ? -30.f : 35.f, -15.f);
					Shard.Rotation = FRotator(i == 0 ? -22.f : -18.f, i * 140.f + 30.f, 0.f);
					Shard.Delay = 0.03f + i * 0.03f;
					Shard.Duration = 1.15f;
					Shard.ScaleOverLife = RiseHoldSink(0.06f, 0.9f);
					D.Meshes.Add(Shard);
				}
				AddRockBurst(D, 150.f, 12);
				AddCracks(D, 160.f, 12.f);
				D.Shakes.Add(MinorShake(0.12f));
				Out.Add(TEXT("EarthSpikes.Eruption"), D);
			}
			{
				// Final (VR 300, 700 high): an enormous spike with a crown of five shards leaning out, a pressure pulse, a big
				// ground burst, a dust column, a crater and wide cracks. Heavy tier.
				FMTVFXDesc D = ImpactDesc(1.6f);
				FMTVFXMeshLayer Lance = RockBody(Paths::SpikeB, FVector(125.f, 125.f, 350.f));
				Lance.Offset = FVector(0.f, 0.f, -40.f);
				Lance.ScaleOverLife = RiseHoldSink(0.04f, 0.92f);
				D.Meshes.Add(Lance);
				for (int32 i = 0; i < 5; ++i)
				{
					const float Angle = i * 72.f + 20.f;
					const float Rad = FMath::DegreesToRadians(Angle);
					FMTVFXMeshLayer Shard = RockBody(i % 2 == 0 ? Paths::Spike : Paths::SpikeB, FVector(45.f, 45.f, 120.f + (i % 3) * 30.f));
					Shard.Offset = FVector(FMath::Cos(Rad) * 170.f, FMath::Sin(Rad) * 170.f, -25.f);
					Shard.Rotation = FRotator(-26.f - (i % 2) * 8.f, Angle, 0.f);
					Shard.Delay = 0.02f + i * 0.015f;
					Shard.Duration = 1.45f;
					Shard.ScaleOverLife = RiseHoldSink(0.05f, 0.92f);
					D.Meshes.Add(Shard);
				}
				D.Meshes.Add(AirPulse(Paths::Sphere, 50.f, 500.f, 0.35f, 1.3f));
				AddRockBurst(D, 300.f, 30, 1.3f);
				FMTVFXEmitter Column = DustCloud(12, 0.f, 150.f, 280.f, 0.5f, 2.4f);
				Column.ConeDeg = 20.f;
				Column.SpeedMin = 300.f;
				Column.SpeedMax = 700.f;
				Column.Shape = EMTVFXShape::Disc;
				Column.Radius = 120.f;
				D.Emitters.Add(Column);
				AddCrater(D, 260.f, 14.f);
				AddCracks(D, 420.f, 14.f);
				D.Shakes.Add(HeavyShake(0.42f));
				Out.Add(TEXT("EarthSpikes.Final"), D);
			}
			{
				// Crumble (at a spike's ground point 1.2 s after it rose; scale 1, or 2 for the final): cracked chunks falling
				// off the withdrawing spike, a dust puff and soil.
				FMTVFXDesc D = ImpactDesc(1.4f);
				FMTVFXEmitter Chunks = Debris(10, 40.f, 180.f, 8.f, 20.f);
				Chunks.Shape = EMTVFXShape::Box;
				Chunks.Extent = FVector(40.f, 40.f, 110.f);
				Chunks.Offset = FVector(0.f, 0.f, 130.f);
				Chunks.ConeDeg = 85.f;
				Chunks.Scalars.Add(TEXT("Crack"), 1.f);
				Chunks.bHero = true;
				D.Emitters.Add(Chunks);
				FMTVFXEmitter Puff = DustCloud(10, 0.f, 60.f, 130.f, 0.45f, 1.6f);
				Puff.Shape = EMTVFXShape::Box;
				Puff.Extent = FVector(40.f, 40.f, 100.f);
				Puff.Offset = FVector(0.f, 0.f, 110.f);
				D.Emitters.Add(Puff);
				D.Emitters.Add(DirtClods(12, 100.f, 300.f));
				Out.Add(TEXT("EarthSpikes.Crumble"), D);
			}
		}

		// =====================================================================================
		// Quagmire (Rudeus): ground turned to a moving mud swamp. Earth + water, never a painted circle.
		// =====================================================================================
		void AddQuagmire(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Formation (loop at the palm lowered to the ground, charge): a swirling ball of muddy water with a teal mana
				// ring, mud flecks orbiting and drawn in, drips falling toward the ground.
				FMTVFXDesc D = Desc(0.45f, true, 0.15f);
				D.Charge.SizeScale = 1.45f;
				D.Charge.IntensityScale = 1.6f;
				D.Charge.RateScale = 2.f;
				D.Charge.SpinScale = 1.8f;
				FMTVFXMeshLayer Ball = WaterBody(Paths::Sphere, FVector(12.f), MudWet);
				Ball.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 0.7f);
				D.Meshes.Add(Ball);
				FMTVFXMeshLayer Band = ThinRing(22.f, Teal, 2.2f);
				Band.SpinSpeed = 300.f;
				Band.Rotation = FRotator(0.f, 0.f, 15.f);
				D.Meshes.Add(Band);
				FMTVFXMeshLayer Band2 = ThinRing(28.f, Teal, 1.5f);
				Band2.SpinSpeed = -220.f;
				Band2.Rotation = FRotator(0.f, 60.f, -20.f);
				Band2.ChargeThreshold = 0.5f;
				D.Meshes.Add(Band2);
				FMTVFXEmitter Flecks = Gather(40.f, 70.f, Mud, EMTVFXRender::Stretched, 1.f);
				Flecks.MaterialPath = Paths::MatSmoke;
				Flecks.Texture = Paths::TexDot;
				Flecks.SizeMin = 3.f;
				Flecks.SizeMax = 6.f;
				Flecks.bHero = true;
				D.Emitters.Add(Flecks);
				FMTVFXEmitter Drips = Sprites(EMTVFXRender::Stretched, 0, 18.f, 0.4f, 0.7f, 2.5f, 4.f, MudWet, Mud, 1.f);
				Drips.MaterialPath = Paths::MatSmoke;
				Drips.Texture = Paths::TexDot;
				Drips.Direction = -FVector::UpVector;
				Drips.ConeDeg = 10.f;
				Drips.SpeedMin = 50.f;
				Drips.SpeedMax = 150.f;
				Drips.Gravity = 980.f;
				Drips.Shape = EMTVFXShape::Sphere;
				Drips.Radius = 10.f;
				D.Emitters.Add(Drips);
				D.Emitters.Add(Gather(20.f, 60.f, Teal, EMTVFXRender::SpriteAdd, 5.f));
				D.Lights.Add(Glow(Teal, 25.f, 300.f, 0.1f, FMTVFXCurve::Grow(0.f, 0.45f)));
				Out.Add(TEXT("Quagmire.Formation"), D);
			}
			{
				// Target (loop on the ground target; VR 900, resized live): a faint teal boundary, rings contracting toward
				// the centre, dust stirring along the edge and the ground trembling inside, a low shimmer.
				FMTVFXDesc D = Desc(0.3f, true, 0.3f);
				D.bUpright = true;
				D.Charge.IntensityScale = 1.5f;
				FMTVFXMeshLayer Edge = Layer(Paths::ShockRing, Paths::MatGlow, FVector(900.f, 900.f, 2.f), Teal, 0.35f, FMTVFXCurve::Grow(0.f, 0.5f));
				Edge.Offset = FVector(0.f, 0.f, 6.f);
				Edge.SpinSpeed = 8.f;
				Edge.Scalars.Add(TEXT("NoiseAmount"), 0.9f);
				Flow(Edge, 6.f, 1.f, 0.4f, 0.f, 2.f);
				D.Meshes.Add(Edge);
				FMTVFXEmitter Contract = Sprites(EMTVFXRender::Mesh, 0, 1.6f, 1.2f, 1.4f, 880.f, 900.f, Teal, Teal, 0.6f);
				Contract.MeshPath = Paths::ShockRing;
				Contract.MaterialPath = Paths::MatGlow;
				Contract.bFlat = true;
				Contract.SpeedMin = 0.f;
				Contract.SpeedMax = 0.f;
				Contract.bWorldSpace = false;
				Contract.Offset = FVector(0.f, 0.f, 8.f);
				Contract.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.25f } });
				Contract.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.2f, 0.5f }, { 1.f, 0.f } });
				Contract.MaxParticles = 4;
				D.Emitters.Add(Contract);
				FMTVFXEmitter Stir = DustCloud(0, 25.f, 30.f, 60.f, 0.3f, 1.f);
				Stir.Shape = EMTVFXShape::Ring;
				Stir.Radius = 900.f;
				Stir.bFollowGround = true;
				Stir.Offset = FVector(0.f, 0.f, 10.f);
				Stir.SpeedMin = 20.f;
				Stir.SpeedMax = 60.f;
				D.Emitters.Add(Stir);
				FMTVFXEmitter Tremble = Debris(0, 100.f, 200.f, 1.5f, 3.5f);
				Tremble.Rate = 8.f;
				Tremble.Shape = EMTVFXShape::Disc;
				Tremble.Radius = 850.f;
				Tremble.bFollowGround = true;
				Tremble.ConeDeg = 15.f;
				Tremble.LifeMin = 0.4f;
				Tremble.LifeMax = 0.6f;
				D.Emitters.Add(Tremble);
				FMTVFXMeshLayer Shimmer = Haze(Paths::Disc, FVector(900.f, 900.f, 2.f), 0.25f);
				Shimmer.Offset = FVector(0.f, 0.f, 20.f);
				D.Meshes.Add(Shimmer);
				Out.Add(TEXT("Quagmire.Target"), D);
			}
			{
				// Zone (loop on the zone; VR 900). The ground transforms over 1.2 s: cracks (0-0.35 s: dust spurts, chips),
				// water pushed up through them (0.35-0.8 s: muddy spurts, water blobs), then liquefied mud (0.8 s on:
				// bubbles swelling and popping, ripples, splashes, low vapour, a few teal glints). The mud decal
				// (M_Decal_Mud, driven by Age) and a saturated wet ring stay until the zone stops; earth fragments heave up
				// around the boundary.
				FMTVFXDesc D = Desc(1.2f, true, 1.f);
				D.bUpright = true;
				FMTVFXDecal Swamp = Decal(Paths::DecalMud, 900.f, 0.f, MudWet, 1.f);
				Swamp.bUntilStop = true;
				Swamp.Depth = 400.f;
				Swamp.FadeIn = 0.2f;
				Swamp.FadeOut = 1.2f;
				Swamp.SortOrder = 2;
				Swamp.Scalars.Add(TEXT("Flow"), 1.f);
				D.Decals.Add(Swamp);
				FMTVFXDecal Saturated = Decal(Paths::DecalWet, 1050.f, 0.f, FC::White, 1.f, 0.35f);
				Saturated.bUntilStop = true;
				Saturated.Depth = 400.f;
				Saturated.FadeIn = 0.6f;
				Saturated.FadeOut = 1.4f;
				Saturated.SortOrder = 1;
				D.Decals.Add(Saturated);
				FMTVFXEmitter Spurts = DustCloud(16, 0.f, 60.f, 120.f, 0.4f, 0.8f);
				Spurts.Shape = EMTVFXShape::Disc;
				Spurts.Radius = 800.f;
				Spurts.bFollowGround = true;
				Spurts.ConeDeg = 20.f;
				Spurts.SpeedMin = 100.f;
				Spurts.SpeedMax = 250.f;
				D.Emitters.Add(Spurts);
				FMTVFXEmitter Chips = Debris(10, 150.f, 300.f, 2.f, 5.f);
				Chips.Shape = EMTVFXShape::Disc;
				Chips.Radius = 800.f;
				Chips.bFollowGround = true;
				Chips.ConeDeg = 25.f;
				Chips.LifeMin = 0.6f;
				Chips.LifeMax = 1.f;
				D.Emitters.Add(Chips);
				FMTVFXEmitter Upwell = Sprites(EMTVFXRender::Stretched, 0, 70.f, 0.5f, 0.8f, 3.f, 6.f, MudWet, Mud, 1.f);
				Upwell.MaterialPath = Paths::MatSmoke;
				Upwell.Texture = Paths::TexDot;
				Upwell.Delay = 0.35f;
				Upwell.EmitDuration = 0.5f;
				Upwell.Shape = EMTVFXShape::Disc;
				Upwell.Radius = 780.f;
				Upwell.bFollowGround = true;
				Upwell.ConeDeg = 15.f;
				Upwell.SpeedMin = 180.f;
				Upwell.SpeedMax = 420.f;
				Upwell.Gravity = 980.f;
				Upwell.bHero = true;
				D.Emitters.Add(Upwell);
				FMTVFXEmitter Seep = WaterBlobs(0, 20.f, 100.f, 250.f, 6.f, 12.f);
				Seep.ColorStart = MudWet;
				Seep.ColorEnd = Mud;
				Seep.Delay = 0.35f;
				Seep.EmitDuration = 0.6f;
				Seep.Shape = EMTVFXShape::Disc;
				Seep.Radius = 760.f;
				Seep.bFollowGround = true;
				Seep.ConeDeg = 20.f;
				D.Emitters.Add(Seep);
				FMTVFXEmitter Bubbles = Sprites(EMTVFXRender::Mesh, 0, 10.f, 0.9f, 1.6f, 10.f, 26.f, MudWet, MudWet, 1.f);
				Bubbles.MeshPath = Paths::Sphere;
				Bubbles.MaterialPath = Paths::MatWater;
				Bubbles.Delay = 0.8f;
				Bubbles.Shape = EMTVFXShape::Disc;
				Bubbles.Radius = 820.f;
				Bubbles.bFollowGround = true;
				Bubbles.Offset = FVector(0.f, 0.f, -4.f);
				Bubbles.SpeedMin = 0.f;
				Bubbles.SpeedMax = 0.f;
				Bubbles.SizeOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.7f, 1.f }, { 0.85f, 1.1f }, { 1.f, 0.f } });
				Bubbles.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 1.f } });
				Bubbles.bHero = true;
				D.Emitters.Add(Bubbles);
				FMTVFXEmitter Pops = Sprites(EMTVFXRender::Stretched, 0, 12.f, 0.4f, 0.6f, 3.f, 6.f, MudWet, Mud, 1.f);
				Pops.MaterialPath = Paths::MatSmoke;
				Pops.Texture = Paths::TexDot;
				Pops.Delay = 0.9f;
				Pops.Shape = EMTVFXShape::Disc;
				Pops.Radius = 820.f;
				Pops.bFollowGround = true;
				Pops.ConeDeg = 35.f;
				Pops.SpeedMin = 120.f;
				Pops.SpeedMax = 260.f;
				Pops.Gravity = 980.f;
				D.Emitters.Add(Pops);
				FMTVFXEmitter Ripples = Sprites(EMTVFXRender::Mesh, 0, 5.f, 1.4f, 2.f, 60.f, 160.f, WaterFoam, WaterFoam, 0.35f);
				Ripples.MeshPath = Paths::ShockRing;
				Ripples.MaterialPath = Paths::MatGlow;
				Ripples.bFlat = true;
				Ripples.Delay = 0.8f;
				Ripples.Shape = EMTVFXShape::Disc;
				Ripples.Radius = 750.f;
				Ripples.bFollowGround = true;
				Ripples.Offset = FVector(0.f, 0.f, 4.f);
				Ripples.SpeedMin = 0.f;
				Ripples.SpeedMax = 0.f;
				Ripples.SizeOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
				Ripples.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.2f, 0.5f }, { 1.f, 0.f } });
				D.Emitters.Add(Ripples);
				FMTVFXEmitter Vapour = SmokePuffs(0, 5.f, 150.f, 300.f, MistWhite, 0.2f, 2.5f);
				Vapour.Shape = EMTVFXShape::Disc;
				Vapour.Radius = 800.f;
				Vapour.bFollowGround = true;
				Vapour.Buoyancy = 15.f;
				D.Emitters.Add(Vapour);
				FMTVFXEmitter Heave = Debris(14, 150.f, 320.f, 10.f, 24.f, Paths::RockChunkB);
				Heave.Delay = 0.15f;
				Heave.Shape = EMTVFXShape::Ring;
				Heave.Radius = 900.f;
				Heave.bFollowGround = true;
				Heave.ConeDeg = 30.f;
				Heave.LifeMin = 7.f;
				Heave.LifeMax = 9.f;
				Heave.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.85f, 1.f }, { 1.f, 0.1f } });
				Heave.MaxParticles = 16;
				D.Emitters.Add(Heave);
				FMTVFXEmitter Glints = Motes(6.f, 800.f, Teal, 3.f);
				Glints.Shape = EMTVFXShape::Disc;
				Glints.bFollowGround = true;
				Glints.Offset = FVector(0.f, 0.f, 10.f);
				D.Emitters.Add(Glints);
				D.Shakes.Add(MinorShake(0.1f, 0.35f));
				Out.Add(TEXT("Quagmire.Zone"), D);
			}
			{
				// Ripple (pooled; at a moving target's feet): two sheen ripples spreading, a dark glossy sinking footprint,
				// mud flecks and a bubble.
				FMTVFXDesc D = ImpactDesc(1.f);
				D.Meshes.Add(ShockwaveRing(10.f, 90.f, 0.6f, WaterFoam, 0.6f, 0.f, 0.05f));
				D.Meshes.Add(ShockwaveRing(10.f, 60.f, 0.5f, WaterFoam, 0.45f, 0.12f, 0.05f));
				FMTVFXMeshLayer Print = WaterBody(Paths::Disc, FVector(35.f, 35.f, 1.f), MudWet);
				Print.Offset = FVector(0.f, 0.f, 2.f);
				Print.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.8f }, { 1.f, 0.f } });
				Print.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.6f }, { 0.3f, 1.f }, { 1.f, 1.1f } });
				Print.bWobble = false;
				D.Meshes.Add(Print);
				FMTVFXEmitter Flecks = Sprites(EMTVFXRender::Stretched, 6, 0.f, 0.3f, 0.5f, 2.5f, 4.5f, MudWet, Mud, 1.f);
				Flecks.MaterialPath = Paths::MatSmoke;
				Flecks.Texture = Paths::TexDot;
				Flecks.ConeDeg = 40.f;
				Flecks.SpeedMin = 100.f;
				Flecks.SpeedMax = 220.f;
				Flecks.Gravity = 980.f;
				D.Emitters.Add(Flecks);
				FMTVFXEmitter Bubble = Sprites(EMTVFXRender::Mesh, 2, 0.f, 0.6f, 0.9f, 5.f, 9.f, MudWet, MudWet, 1.f);
				Bubble.MeshPath = Paths::Sphere;
				Bubble.MaterialPath = Paths::MatWater;
				Bubble.Shape = EMTVFXShape::Disc;
				Bubble.Radius = 30.f;
				Bubble.SpeedMin = 0.f;
				Bubble.SpeedMax = 0.f;
				Bubble.SizeOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.7f, 1.f }, { 1.f, 0.f } });
				Bubble.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 1.f } });
				D.Emitters.Add(Bubble);
				Out.Add(TEXT("Quagmire.Ripple"), D);
			}
			{
				// Splash (a fast entry): a crown of mud thrown up, mud flecks, muddy blobs, brown spray and a ripple ring.
				FMTVFXDesc D = ImpactDesc(1.2f);
				FMTVFXMeshLayer Crown = WaterBody(Paths::Beam, FVector(70.f, 70.f, 45.f), MudWet);
				Crown.Duration = 0.5f;
				Crown.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.4f }, { 1.f, 1.f } });
				Crown.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Crown);
				D.Meshes.Add(ShockwaveRing(20.f, 140.f, 0.5f, WaterFoam, 0.8f, 0.f, 0.06f));
				FMTVFXEmitter Flecks = Sprites(EMTVFXRender::Stretched, 30, 0.f, 0.5f, 0.9f, 3.f, 6.f, MudWet, Mud, 1.f);
				Flecks.MaterialPath = Paths::MatSmoke;
				Flecks.Texture = Paths::TexDot;
				Flecks.bRadial = true;
				Flecks.Shape = EMTVFXShape::Sphere;
				Flecks.Radius = 20.f;
				Flecks.SpeedMin = 300.f;
				Flecks.SpeedMax = 700.f;
				Flecks.Gravity = 980.f;
				Flecks.bHero = true;
				D.Emitters.Add(Flecks);
				FMTVFXEmitter Blobs = WaterBlobs(8, 0.f, 250.f, 550.f, 6.f, 12.f);
				Blobs.ColorStart = MudWet;
				Blobs.ColorEnd = Mud;
				D.Emitters.Add(Blobs);
				D.Emitters.Add(SmokePuffs(6, 0.f, 40.f, 80.f, MudWet, 0.45f, 0.9f));
				D.Emitters.Add(Mist(4, 0.f, 40.f, 80.f, 0.25f, 1.f));
				D.Shakes.Add(MinorShake(0.06f));
				Out.Add(TEXT("Quagmire.Splash"), D);
			}
			{
				// Dissipation (drying, VR 900): the mud dries into pale cracked earth; vapour and dust rise off it, crumbs.
				FMTVFXDesc D = ImpactDesc(1.2f);
				FMTVFXDecal Dried = Decal(Paths::DecalCracks, 850.f, 10.f, FC(0.18f, 0.15f, 0.12f));
				Dried.FadeIn = 0.6f;
				Dried.FadeOut = 3.f;
				D.Decals.Add(Dried);
				FMTVFXEmitter Steam = SmokePuffs(14, 10.f, 200.f, 350.f, MistWhite, 0.3f, 2.4f);
				Steam.EmitDuration = 1.f;
				Steam.Shape = EMTVFXShape::Disc;
				Steam.Radius = 850.f;
				Steam.bFollowGround = true;
				Steam.Buoyancy = 60.f;
				D.Emitters.Add(Steam);
				FMTVFXEmitter Dust2 = DustCloud(12, 0.f, 80.f, 160.f, 0.3f, 1.6f);
				Dust2.Shape = EMTVFXShape::Disc;
				Dust2.Radius = 850.f;
				Dust2.bFollowGround = true;
				D.Emitters.Add(Dust2);
				FMTVFXEmitter Crumbs = Debris(10, 100.f, 200.f, 1.5f, 3.5f);
				Crumbs.Shape = EMTVFXShape::Disc;
				Crumbs.Radius = 800.f;
				Crumbs.bFollowGround = true;
				D.Emitters.Add(Crumbs);
				Out.Add(TEXT("Quagmire.Dissipation"), D);
			}
		}

		// =====================================================================================
		// Elemental Barrage (Rudeus): four elemental formations fire in turn, then collapse into one multi-element blast.
		// =====================================================================================
		void AddBarrage(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Formation (the raised hands): the four elements drawn in (embers, water, dust, streaks) around a mana core.
				FMTVFXDesc D = Desc(0.6f, true, 0.2f);
				FMTVFXEmitter FireIn = Gather(22.f, 90.f, FireYellow, EMTVFXRender::SpriteAdd, 8.f);
				FireIn.SizeMin = 2.5f;
				FireIn.SizeMax = 5.f;
				D.Emitters.Add(FireIn);
				D.Emitters.Add(Gather(22.f, 90.f, WaterFoam, EMTVFXRender::Stretched, 1.6f));
				D.Emitters.Add(Gather(16.f, 90.f, DustLight, EMTVFXRender::SpriteSmoke, 1.f));
				FMTVFXEmitter WindIn = Gather(18.f, 90.f, WindWhite, EMTVFXRender::Stretched, 1.f);
				WindIn.MaterialPath = Paths::MatSmoke;
				WindIn.Texture = Paths::TexStreak;
				D.Emitters.Add(WindIn);
				FMTVFXMeshLayer Core = Orb(9.f, Mana, 8.f, 0.3f, 0.5f);
				Core.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 0.8f);
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Core);
				FMTVFXMeshLayer Band = ThinRing(22.f, Mana, 2.f);
				Band.SpinSpeed = 300.f;
				Band.Rotation = FRotator(20.f, 0.f, 0.f);
				D.Meshes.Add(Band);
				D.Lights.Add(Glow(Mana, 40.f, 400.f, 0.1f, FMTVFXCurve::Grow(0.f, 0.45f)));
				Out.Add(TEXT("Barrage.Formation"), D);
			}
			{
				// OrbFire (behind upper left; attached to the caster, moved every frame): embers spiralling into a turning
				// flame core with a crown of flame tongues, a little heat haze and light.
				FMTVFXDesc D = Desc(0.35f, true, 0.3f);
				FMTVFXMeshLayer Core = Orb(13.f, FireCore, 12.f, 0.f, 0.4f);
				Core.bWobble = true;
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.4f);
				Core.ScaleOverLife = FMTVFXCurve::Grow(0.1f, 0.8f);
				D.Meshes.Add(Core);
				FMTVFXMeshLayer Shell = Orb(26.f, FireOrange, 2.6f, 0.8f, 1.f);
				Flow(Shell, 2.6f, 2.6f, 1.4f, -0.8f, 2.4f);
				Shell.SpinSpeed = 300.f;
				Shell.SpinAxis = FVector(0.2f, 0.3f, 1.f);
				Shell.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.5f);
				Shell.ScaleOverLife = FMTVFXCurve::Grow(0.1f, 0.8f);
				D.Meshes.Add(Shell);
				for (int32 i = 0; i < 3; ++i)
				{
					FMTVFXMeshLayer Tongue = Layer(Paths::Flame, Paths::MatGlow, FVector(9.f, 9.f, 16.f), FireOrange, 2.4f, FMTVFXCurve::Grow(0.f, 0.5f),
						FMTVFXCurve::Grow(0.1f, 0.8f));
					Tongue.Rotation = FRotator(-35.f, i * 120.f, 0.f);
					Tongue.SpinSpeed = 240.f;
					Tongue.Scalars.Add(TEXT("FresnelMix"), 0.5f);
					Tongue.Scalars.Add(TEXT("NoiseAmount"), 1.f);
					Flow(Tongue, 1.f, 2.f, 0.2f, -1.6f, 2.f);
					D.Meshes.Add(Tongue);
				}
				FMTVFXEmitter Spiral = Gather(30.f, 60.f, FireYellow, EMTVFXRender::SpriteAdd, 9.f);
				Spiral.SizeMin = 2.f;
				Spiral.SizeMax = 4.f;
				Spiral.Orbit = 600.f;
				Spiral.ColorEnd = FireWhite;
				D.Emitters.Add(Spiral);
				FMTVFXEmitter Lick = FlameWisps(0, 25.f, 8.f, 14.f, 0.3f);
				Lick.Shape = EMTVFXShape::Sphere;
				Lick.Radius = 14.f;
				D.Emitters.Add(Lick);
				D.Meshes.Add(Haze(Paths::Sphere, FVector(30.f), 0.5f));
				D.Lights.Add(Glow(FireOrange, 30.f, 320.f, 0.3f, FMTVFXCurve::Grow(0.f, 0.45f)));
				Out.Add(TEXT("Barrage.OrbFire"), D);
			}
			{
				// OrbWater (behind upper right): a spinning ring of water beads around a water core, droplets flung off it,
				// a foam edge, mist.
				FMTVFXDesc D = Desc(0.35f, true, 0.3f);
				FMTVFXMeshLayer Heart = WaterBody(Paths::Sphere, FVector(13.f));
				Heart.ScaleOverLife = FMTVFXCurve::Grow(0.1f, 0.8f);
				D.Meshes.Add(Heart);
				FMTVFXMeshLayer Band = ThinRing(30.f, WaterFoam, 1.f);
				Band.Rotation = FRotator(0.f, 0.f, 25.f);
				Band.SpinSpeed = 420.f;
				D.Meshes.Add(Band);
				FMTVFXEmitter Beads = WaterBlobs(0, 40.f, 0.f, 10.f, 3.f, 6.f);
				Beads.Shape = EMTVFXShape::Ring;
				Beads.Radius = 30.f;
				Beads.Orbit = 620.f;
				Beads.Gravity = 0.f;
				Beads.Drag = 0.f;
				Beads.LifeMin = 0.5f;
				Beads.LifeMax = 0.8f;
				Beads.bWorldSpace = false;
				Beads.bHero = true;
				D.Emitters.Add(Beads);
				FMTVFXEmitter Flung = Droplets(0, 30.f, 60.f, 180.f);
				Flung.Shape = EMTVFXShape::Ring;
				Flung.Radius = 30.f;
				Flung.bRadial = true;
				D.Emitters.Add(Flung);
				D.Emitters.Add(Mist(0, 8.f, 15.f, 30.f, 0.2f, 0.8f));
				Out.Add(TEXT("Barrage.OrbWater"), D);
			}
			{
				// OrbEarth (lower left): a dense stone with faint mana seams, rock fragments orbiting it, dust.
				FMTVFXDesc D = Desc(0.35f, true, 0.3f);
				FMTVFXMeshLayer Stone = RockBody(Paths::RockChunkD, FVector(14.f), 1.5f, Mana);
				Stone.SpinSpeed = 90.f;
				Stone.SpinAxis = FVector(0.3f, 0.5f, 1.f);
				Stone.ScaleOverLife = FMTVFXCurve::Grow(0.1f, 0.8f);
				D.Meshes.Add(Stone);
				FMTVFXEmitter Orbiting = Debris(4, 0.f, 10.f, 2.5f, 5.f);
				Orbiting.Rate = 6.f;
				Orbiting.Shape = EMTVFXShape::Ring;
				Orbiting.Radius = 32.f;
				Orbiting.Orbit = 260.f;
				Orbiting.Gravity = 0.f;
				Orbiting.bBounce = false;
				Orbiting.bWorldSpace = false;
				Orbiting.LifeMin = 1.2f;
				Orbiting.LifeMax = 1.8f;
				Orbiting.MaxParticles = 12;
				Orbiting.bHero = true;
				Orbiting.SizeOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 0.15f, 1.f }, { 0.85f, 1.f }, { 1.f, 0.2f } });
				D.Emitters.Add(Orbiting);
				FMTVFXEmitter Specks = DustCloud(0, 10.f, 8.f, 16.f, 0.25f, 0.8f);
				Specks.Shape = EMTVFXShape::Ring;
				Specks.Radius = 30.f;
				Specks.Orbit = 300.f;
				Specks.bWorldSpace = false;
				D.Emitters.Add(Specks);
				Out.Add(TEXT("Barrage.OrbEarth"), D);
			}
			{
				// OrbWind (lower right): streak rings spinning on crossed axes around a core of warped air, condensation and
				// glints orbiting.
				FMTVFXDesc D = Desc(0.35f, true, 0.3f);
				FMTVFXMeshLayer Core = Haze(Paths::Sphere, FVector(18.f), 1.4f);
				Core.bWobble = true;
				D.Meshes.Add(Core);
				for (int32 i = 0; i < 2; ++i)
				{
					FMTVFXMeshLayer Band = ThinRing(30.f - i * 4.f, WindWhite, 1.3f);
					Band.Rotation = FRotator(i == 0 ? 15.f : 75.f, i * 60.f, 0.f);
					Band.SpinSpeed = i == 0 ? 720.f : -600.f;
					Band.SpinAxis = i == 0 ? FVector::UpVector : FVector::ForwardVector;
					Flow(Band, 4.f, 1.f, 3.f, 0.f, 1.6f);
					D.Meshes.Add(Band);
				}
				FMTVFXEmitter Swirl = Condensation(0, 40.f, 3.f, 6.f, 0.35f, 0.35f);
				Swirl.Shape = EMTVFXShape::Ring;
				Swirl.Radius = 28.f;
				Swirl.Orbit = 720.f;
				Swirl.SpeedMin = 0.f;
				Swirl.SpeedMax = 10.f;
				Swirl.Stretch = 5.f;
				Swirl.bWorldSpace = false;
				Swirl.bHero = true;
				D.Emitters.Add(Swirl);
				FMTVFXEmitter Glints = WindStreaks(0, 20.f, 0.f, 10.f, FVector::ForwardVector);
				Glints.Shape = EMTVFXShape::Ring;
				Glints.Radius = 26.f;
				Glints.Orbit = 800.f;
				Glints.bWorldSpace = false;
				D.Emitters.Add(Glints);
				Out.Add(TEXT("Barrage.OrbWind"), D);
			}
			{
				// MuzzleFire (pooled; at the orb, aimed): a flash, a ring, flame tongues and sparks spat forward.
				FMTVFXDesc D = Desc(0.35f);
				D.Meshes.Add(Flash(18.f, FireCore, 9.f, 0.08f));
				D.Meshes.Add(FacingRing(5.f, 42.f, 0.12f, FireYellow, 3.f));
				FMTVFXEmitter Tongues = FlameWisps(8, 0.f, 8.f, 16.f, 0.25f);
				Tongues.Direction = FVector::ForwardVector;
				Tongues.ConeDeg = 20.f;
				Tongues.SpeedMin = 400.f;
				Tongues.SpeedMax = 900.f;
				Tongues.Drag = 4.f;
				Tongues.Buoyancy = 100.f;
				D.Emitters.Add(Tongues);
				FMTVFXEmitter Spk = Sparks(8, 0.f, 500.f, 1100.f, FireYellow, 300.f);
				Spk.bRadial = false;
				Spk.Direction = FVector::ForwardVector;
				Spk.ConeDeg = 25.f;
				D.Emitters.Add(Spk);
				D.Lights.Add(FlashLight(FireOrange, 80.f, 350.f, 0.2f));
				D.Shakes.Add(MinorShake(0.05f));
				Out.Add(TEXT("Barrage.MuzzleFire"), D);
			}
			{
				// MuzzleWater: a pressure ring, a jet of spray, a puff of vapour.
				FMTVFXDesc D = Desc(0.35f);
				D.Meshes.Add(FacingRing(5.f, 45.f, 0.14f, WaterFoam, 1.5f));
				D.Meshes.Add(FacingAir(5.f, 55.f, 0.16f, 1.1f));
				FMTVFXEmitter Spray = Droplets(14, 0.f, 600.f, 1200.f);
				Spray.Direction = FVector::ForwardVector;
				Spray.ConeDeg = 18.f;
				Spray.Gravity = 600.f;
				D.Emitters.Add(Spray);
				FMTVFXEmitter Vapour = Mist(3, 0.f, 20.f, 40.f, 0.3f, 0.6f);
				Vapour.Direction = FVector::ForwardVector;
				Vapour.SpeedMin = 200.f;
				Vapour.SpeedMax = 400.f;
				D.Emitters.Add(Vapour);
				D.Shakes.Add(MinorShake(0.05f));
				Out.Add(TEXT("Barrage.MuzzleWater"), D);
			}
			{
				// MuzzleEarth: a dusty ring, a puff of dust and grit blasted forward.
				FMTVFXDesc D = Desc(0.35f);
				D.Meshes.Add(FacingRing(5.f, 40.f, 0.12f, DustLight, 1.2f));
				FMTVFXEmitter Puff = DustCloud(5, 0.f, 20.f, 45.f, 0.45f, 0.6f);
				Puff.Direction = FVector::ForwardVector;
				Puff.ConeDeg = 35.f;
				Puff.SpeedMin = 200.f;
				Puff.SpeedMax = 450.f;
				D.Emitters.Add(Puff);
				FMTVFXEmitter Grit = DirtClods(8, 600.f, 1300.f, 1.5f, 3.f);
				Grit.Direction = FVector::ForwardVector;
				Grit.ConeDeg = 15.f;
				Grit.Gravity = 400.f;
				D.Emitters.Add(Grit);
				D.Shakes.Add(MinorShake(0.06f));
				Out.Add(TEXT("Barrage.MuzzleEarth"), D);
			}
			{
				// MuzzleWind: a ring of warped air, a pale ring, condensation and glints shot forward.
				FMTVFXDesc D = Desc(0.35f);
				D.Meshes.Add(FacingAir(5.f, 70.f, 0.16f, 1.4f));
				D.Meshes.Add(FacingRing(5.f, 55.f, 0.14f, WindWhite, 1.5f, 0.f, 0.05f));
				FMTVFXEmitter Rush = Condensation(10, 0.f, 4.f, 8.f, 0.35f, 0.25f);
				Rush.Direction = FVector::ForwardVector;
				Rush.ConeDeg = 20.f;
				Rush.SpeedMin = 900.f;
				Rush.SpeedMax = 1600.f;
				D.Emitters.Add(Rush);
				D.Emitters.Add(WindStreaks(8, 0.f, 1000.f, 1800.f, FVector::ForwardVector));
				Out.Add(TEXT("Barrage.MuzzleWind"), D);
			}
			{
				// Collapse (loop at (150, 0, 40) in the caster frame, 0.35 s): the four elements stream inward from all sides
				// into one unstable core wrapped in fire and water shells, rings contracting, a rising light.
				FMTVFXDesc D = Desc(0.35f, true, 0.1f);
				FMTVFXMeshLayer Core = Orb(18.f, ManaWhite, 10.f, 0.3f, 0.5f);
				Core.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 1.f);
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				Core.bWobble = true;
				D.Meshes.Add(Core);
				FMTVFXMeshLayer FireShell = Orb(30.f, FireOrange, 2.f, 0.85f, 1.f);
				Flow(FireShell, 2.6f, 2.6f, 1.8f, -1.f, 2.4f);
				FireShell.SpinSpeed = 500.f;
				FireShell.ScaleOverLife = FMTVFXCurve({ { 0.f, 2.2f }, { 1.f, 1.f } });
				FireShell.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.4f);
				FireShell.bWobble = true;
				D.Meshes.Add(FireShell);
				FMTVFXMeshLayer WaterShell = WaterBody(Paths::Sphere, FVector(36.f));
				WaterShell.ScaleOverLife = FMTVFXCurve({ { 0.f, 2.4f }, { 1.f, 1.f } });
				D.Meshes.Add(WaterShell);
				for (int32 i = 0; i < 2; ++i)
				{
					FMTVFXMeshLayer Band = ShockwaveRing(160.f, 40.f, 0.35f, i == 0 ? Mana : WindWhite, 2.f, 0.f, 0.06f);
					Band.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
					Band.Rotation = FRotator(0.f, 0.f, i * 90.f);
					D.Meshes.Add(Band);
				}
				FMTVFXEmitter FireIn = FlameWisps(0, 50.f, 10.f, 20.f, 0.25f);
				Converge(FireIn, 150.f, 3000.f);
				D.Emitters.Add(FireIn);
				FMTVFXEmitter WaterIn = Droplets(0, 60.f, 0.f, 0.f);
				Converge(WaterIn, 150.f, 3000.f);
				D.Emitters.Add(WaterIn);
				FMTVFXEmitter EarthIn = Debris(0, 0.f, 0.f, 2.f, 5.f);
				EarthIn.Rate = 20.f;
				EarthIn.LifeMin = 0.25f;
				EarthIn.LifeMax = 0.35f;
				Converge(EarthIn, 150.f, 3000.f);
				D.Emitters.Add(EarthIn);
				FMTVFXEmitter WindIn = Condensation(0, 40.f, 5.f, 10.f, 0.35f, 0.3f);
				Converge(WindIn, 150.f, 3000.f);
				D.Emitters.Add(WindIn);
				D.Meshes.Add(Haze(Paths::Sphere, FVector(60.f), 1.2f));
				D.Lights.Add(Glow(ManaWhite, 400.f, 900.f, 0.4f, FMTVFXCurve::Grow(0.1f, 0.45f)));
				Out.Add(TEXT("Barrage.Collapse"), D);
			}
			{
				// Finale travel (the combined orb, radius 60): a white-hot mana core in a turning fire shell, a ring of water
				// and orbiting stones, warped air, a trail of flame, spray, dust and condensation.
				FMTVFXDesc D = Desc(0.2f, true, 0.2f);
				FMTVFXMeshLayer Core = Orb(26.f, ManaWhite, 10.f, 0.2f, 0.5f);
				Core.bWobble = true;
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Core);
				FMTVFXMeshLayer FireShell = Orb(44.f, FireOrange, 2.2f, 0.85f, 1.f);
				Flow(FireShell, 2.6f, 2.6f, 1.6f, -0.8f, 2.4f);
				FireShell.SpinSpeed = 400.f;
				FireShell.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(FireShell);
				FMTVFXMeshLayer Band = ThinRing(58.f, WaterFoam, 1.4f);
				Band.Rotation = FRotator(90.f, 0.f, 0.f);
				Band.SpinSpeed = 600.f;
				Band.SpinAxis = FVector::UpVector;
				D.Meshes.Add(Band);
				D.Meshes.Add(Haze(Paths::Sphere, FVector(70.f), 1.2f));
				FMTVFXEmitter Beads = WaterBlobs(0, 30.f, 0.f, 10.f, 4.f, 8.f);
				Beads.Shape = EMTVFXShape::Ring;
				Beads.Radius = 55.f;
				Beads.Orbit = 500.f;
				Beads.Gravity = 0.f;
				Beads.bWorldSpace = false;
				D.Emitters.Add(Beads);
				FMTVFXEmitter Stones = Debris(6, 0.f, 10.f, 3.f, 7.f);
				Stones.Rate = 4.f;
				Stones.Shape = EMTVFXShape::Ring;
				Stones.Radius = 50.f;
				Stones.Orbit = 360.f;
				Stones.Gravity = 0.f;
				Stones.bBounce = false;
				Stones.bWorldSpace = false;
				Stones.LifeMin = 1.f;
				Stones.LifeMax = 1.5f;
				D.Emitters.Add(Stones);
				FMTVFXEmitter Tongues = FlameWisps(0, 60.f, 20.f, 40.f, 0.35f);
				Tongues.Shape = EMTVFXShape::Sphere;
				Tongues.Radius = 30.f;
				Tongues.Direction = -FVector::ForwardVector;
				Tongues.ConeDeg = 25.f;
				Tongues.bHero = true;
				D.Emitters.Add(Tongues);
				FMTVFXEmitter Spray = Droplets(0, 60.f, 100.f, 300.f);
				Spray.Direction = -FVector::ForwardVector;
				Spray.ConeDeg = 35.f;
				D.Emitters.Add(Spray);
				D.Emitters.Add(DustCloud(0, 20.f, 30.f, 60.f, 0.35f, 0.7f));
				FMTVFXEmitter Vapour = Condensation(0, 30.f, 6.f, 12.f, 0.3f, 0.35f);
				Vapour.Direction = -FVector::ForwardVector;
				Vapour.ConeDeg = 15.f;
				Vapour.SpeedMin = 200.f;
				Vapour.SpeedMax = 500.f;
				D.Emitters.Add(Vapour);
				D.Emitters.Add(EmberDrift(0, 20.f, 30.f, 0.8f));
				D.Lights.Add(Glow(ManaWhite, 250.f, 1400.f, 0.3f));
				Out.Add(TEXT("BarrageFinale.Travel"), D);
			}
			{
				// Finale impact (VR 900): one blast, four elements, each keeping its identity: a flash; fire expanding through
				// the centre; stone spikes erupting outward with chunks and soil; water spiralling up in a column; the wind
				// driving the whole blast out as a pressure front of warped air, rings, condensation and a racing dust ring;
				// scorch, wet ground, cracks and a crater. Ultimate tier.
				FMTVFXDesc D = ImpactDesc(3.f);
				D.Meshes.Add(Flash(350.f, FireWhite, 10.f, 0.15f));
				FMTVFXMeshLayer Fireball = Orb(420.f, FireOrange, 2.4f, 1.f, 1.f);
				Flow(Fireball, 2.5f, 2.5f, 0.2f, 0.5f, 2.2f);
				Fireball.Duration = 0.9f;
				Fireball.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.25f }, { 0.35f, 0.9f }, { 1.f, 1.05f } });
				Fireball.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.35f, 0.9f }, { 1.f, 0.f } });
				D.Meshes.Add(Fireball);
				FMTVFXEmitter Body = FireBody(40, 0.f, 120.f, 240.f, 0.8f);
				Body.bRadial = true;
				Body.Shape = EMTVFXShape::Sphere;
				Body.Radius = 80.f;
				Body.SpeedMin = 400.f;
				Body.SpeedMax = 1100.f;
				Body.Drag = 3.f;
				Body.bHero = true;
				D.Emitters.Add(Body);
				FMTVFXEmitter Tongues = FlameWisps(30, 0.f, 40.f, 80.f, 0.5f);
				Tongues.bRadial = true;
				Tongues.Shape = EMTVFXShape::Sphere;
				Tongues.Radius = 60.f;
				Tongues.SpeedMin = 900.f;
				Tongues.SpeedMax = 1700.f;
				Tongues.Drag = 3.f;
				D.Emitters.Add(Tongues);
				for (int32 i = 0; i < 8; ++i)
				{
					const float Angle = i * 45.f + 10.f;
					const float Rad = FMath::DegreesToRadians(Angle);
					const float Reach = (i % 2 == 0) ? 520.f : 640.f;
					FMTVFXMeshLayer Spike = RockBody(i % 2 == 0 ? Paths::Spike : Paths::SpikeB, FVector(50.f, 50.f, 150.f + (i % 3) * 20.f));
					Spike.Offset = FVector(FMath::Cos(Rad) * Reach, FMath::Sin(Rad) * Reach, -30.f);
					Spike.Rotation = FRotator(-30.f - (i % 3) * 6.f, Angle, 0.f);
					Spike.Delay = 0.03f + (i % 4) * 0.02f;
					Spike.Duration = 2.4f;
					Spike.ScaleOverLife = RiseHoldSink(0.04f, 0.8f);
					D.Meshes.Add(Spike);
				}
				AddRockBurst(D, 600.f, 40, 1.2f);
				FMTVFXEmitter Fountain = Droplets(90, 0.f, 900.f, 1600.f);
				Fountain.Shape = EMTVFXShape::Ring;
				Fountain.Radius = 150.f;
				Fountain.Orbit = 260.f;
				Fountain.ConeDeg = 12.f;
				Fountain.Gravity = 700.f;
				Fountain.MaxParticles = 100;
				D.Emitters.Add(Fountain);
				FMTVFXEmitter Blobs = WaterBlobs(24, 0.f, 800.f, 1400.f, 10.f, 22.f);
				Blobs.Shape = EMTVFXShape::Ring;
				Blobs.Radius = 150.f;
				Blobs.Orbit = 200.f;
				Blobs.ConeDeg = 15.f;
				D.Emitters.Add(Blobs);
				FMTVFXMeshLayer Column = WaterBody(Paths::Spiral, FVector(450.f, 180.f, 180.f));
				Column.Rotation = FRotator(90.f, 0.f, 0.f);
				Column.Offset = FVector(0.f, 0.f, 380.f);
				Column.Duration = 1.f;
				Column.SpinSpeed = 400.f;
				Column.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 0.4f, 1.f }, { 1.f, 1.1f } });
				Column.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.6f, 0.8f }, { 1.f, 0.f } });
				D.Meshes.Add(Column);
				FMTVFXEmitter Cloud = Mist(20, 0.f, 200.f, 400.f, 0.4f, 2.4f);
				Cloud.Shape = EMTVFXShape::Sphere;
				Cloud.Radius = 300.f;
				D.Emitters.Add(Cloud);
				D.Meshes.Add(AirPulse(Paths::Sphere, 100.f, 950.f, 0.45f, 1.6f));
				D.Meshes.Add(AirPulse(Paths::ShockRing, 100.f, 1100.f, 0.5f, 1.5f, 0.03f, 0.05f));
				D.Meshes.Add(ShockwaveRing(80.f, 1000.f, 0.5f, WindWhite, 3.f, 0.f, 0.08f));
				D.Meshes.Add(ShockwaveRing(100.f, 1100.f, 0.6f, DustLight, 1.5f, 0.05f));
				FMTVFXEmitter Front = Condensation(30, 0.f, 12.f, 24.f, 0.4f, 0.5f);
				Front.bRadial = true;
				Front.Shape = EMTVFXShape::Sphere;
				Front.Radius = 100.f;
				Front.SpeedMin = 1600.f;
				Front.SpeedMax = 2600.f;
				Front.Drag = 3.f;
				D.Emitters.Add(Front);
				FMTVFXEmitter Racing = DustCloud(50, 0.f, 150.f, 300.f, 0.55f, 1.8f);
				Racing.Shape = EMTVFXShape::Ring;
				Racing.Radius = 120.f;
				Racing.bRadial = true;
				Racing.SpeedMin = 1200.f;
				Racing.SpeedMax = 2000.f;
				Racing.Drag = 3.f;
				Racing.bFollowGround = true;
				Racing.Offset = FVector(0.f, 0.f, 20.f);
				D.Emitters.Add(Racing);
				D.Lights.Add(FlashLight(FireWhite, 1500.f, 3000.f, 0.6f));
				AddScorch(D, 600.f, 14.f);
				AddWet(D, 800.f, 14.f);
				AddCrater(D, 600.f, 14.f);
				AddCracks(D, 1000.f, 14.f);
				D.Shakes.Add(UltimateShake(0.72f, 5.5f));
				Out.Add(TEXT("BarrageFinale.Impact"), D);
			}
		}

		// =====================================================================================
		// Rudeus: Demon Eye and the Quagmire Magician awakening (unchanged look, kept for their abilities).
		// =====================================================================================
		void AddRudeusMisc(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				FMTVFXDesc D = Desc(0.8f);
				FMTVFXMeshLayer Eye = Orb(18.f, Mana, 12.f, 0.4f, 0.3f);
				Eye.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 0.3f, 1.f }, { 1.f, 1.4f } });
				D.Meshes.Add(Eye);
				D.Meshes.Add(ShockwaveRing(10.f, 120.f, 0.45f, Mana, 5.f, 0.05f, 0.08f));
				D.Emitters.Add(Motes(40.f, 70.f, Mana));
				D.Lights.Add(Glow(Mana, 80.f, 500.f));
				Out.Add(TEXT("DemonEye.Cast"), D);
			}
			{
				FMTVFXDesc D = Desc(0.5f, true, 0.6f);
				FMTVFXEmitter Orbiting = Motes(14.f, 40.f, Mana, 5.f);
				Orbiting.Orbit = 180.f;
				Orbiting.bWorldSpace = false;
				D.Emitters.Add(Orbiting);
				Out.Add(TEXT("DemonEye.Aura"), D);
			}
			{
				// Quagmire Magician: a maelstrom of mud, stone and mana around Rudeus.
				FMTVFXDesc D = Desc(1.8f);
				FMTVFXMeshLayer Pillar = Layer(Paths::Beam, Paths::MatGlow, FVector(70.f, 70.f, 520.f), Mana, 1.f,
					FMTVFXCurve({ { 0.f, 0.f }, { 0.2f, 0.6f }, { 0.85f, 1.f }, { 1.f, 0.f } }), FMTVFXCurve({ { 0.f, 0.3f }, { 0.85f, 1.f }, { 1.f, 1.6f } }));
				Pillar.Scalars.Add(TEXT("FresnelMix"), 0.7f);
				Pillar.Scalars.Add(TEXT("NoiseAmount"), 1.f);
				Flow(Pillar, 2.f, 0.4f, 0.13f, 1.3f, 2.f);
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
				AddCracks(D, 380.f, 14.f, FC(0.06f, 0.05f, 0.04f), 1.5f);
				D.Shakes.Add(HeavyShake(0.45f, 1.5f));
				Out.Add(TEXT("QuagmireMagician.Cast"), D);
			}
			{
				FMTVFXDesc D = Desc(0.6f, true, 0.8f);
				FMTVFXEmitter Orbiting = Debris(0, 0.f, 10.f, 4.f, 8.f);
				Orbiting.Rate = 6.f;
				Orbiting.Shape = EMTVFXShape::Ring;
				Orbiting.Radius = 90.f;
				Orbiting.Orbit = 160.f;
				Orbiting.Gravity = 0.f;
				Orbiting.bBounce = false;
				Orbiting.bWorldSpace = false;
				Orbiting.LifeMin = 1.5f;
				Orbiting.LifeMax = 2.f;
				Orbiting.Offset = FVector(0.f, 0.f, 60.f);
				Orbiting.MaxParticles = 14;
				D.Emitters.Add(Orbiting);
				D.Emitters.Add(Motes(20.f, 80.f, Mana, 5.f));
				Out.Add(TEXT("QuagmireMagician.Aura"), D);
			}
		}

		// =====================================================================================
		// Disturb Magic (Orsted): pale blue-white energy, black distortion ripples, thin dragon-line glyphs, warped air.
		// Restrained and precise: never a laser, a damage projectile or an explosion.
		// =====================================================================================

		/** The collapse of a spell Disturb Magic cancelled: the glyph snaps shut on it; the element fails in its own way. */
		FMTVFXDesc DisturbCollapseBase()
		{
			FMTVFXDesc D = Desc(0.8f);
			FMTVFXMeshLayer Snap = GlyphLayer(26.f, ArcaneBlue, 4.f);
			Snap.Duration = 0.22f;
			Snap.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.6f }, { 1.f, 1.f } });
			Snap.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
			D.Meshes.Add(Snap);
			D.Meshes.Add(FacingRing(30.f, 6.f, 0.18f, ArcaneBlue, 2.5f, 0.f, 0.06f));
			D.Meshes.Add(VoidRing(8.f, 60.f, 0.3f));
			D.Lights.Add(FlashLight(ArcaneBlue, 60.f, 300.f, 0.2f));
			return D;
		}

		void AddDisturbMagic(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Formation (the calmly raised palm): a tiny pale core, a thin dragon-line glyph turning before the palm,
				// black ripples and warped air spreading from it, a few motes.
				FMTVFXDesc D = Desc(0.2f, true, 0.1f);
				FMTVFXMeshLayer Core = Orb(5.f, DragonWhite, 10.f, 0.3f, 0.3f);
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Core);
				FMTVFXMeshLayer Halo = Orb(9.f, ArcaneBlue, 2.5f, 1.f, 0.6f);
				Halo.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				D.Meshes.Add(Halo);
				FMTVFXMeshLayer Sigil = GlyphLayer(16.f, ArcaneBlue, 3.f);
				Sigil.ScaleOverLife = FMTVFXCurve::Grow(0.2f, 0.5f);
				Sigil.SpinSpeed = 90.f;
				Sigil.SpinAxis = FVector::ForwardVector;
				D.Meshes.Add(Sigil);
				D.Meshes.Add(Haze(Paths::Sphere, FVector(20.f), 0.8f));
				FMTVFXEmitter Ripples = Sprites(EMTVFXRender::Mesh, 0, 5.f, 0.45f, 0.55f, 36.f, 44.f, VoidBlack, VoidBlack, 1.f);
				Ripples.MeshPath = Paths::ShockRing;
				Ripples.MaterialPath = Paths::MatSmoke;
				Ripples.bFlat = true;
				Ripples.SpeedMin = 0.f;
				Ripples.SpeedMax = 0.f;
				Ripples.bWorldSpace = false;
				Ripples.SizeOverLife = FMTVFXCurve({ { 0.f, 0.3f }, { 1.f, 1.f } });
				Ripples.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.2f, 0.6f }, { 1.f, 0.f } });
				Ripples.MaxParticles = 4;
				D.Emitters.Add(Ripples);
				FMTVFXEmitter Warps = Ripples;
				Warps.MaterialPath = Paths::MatAir;
				Warps.Rate = 4.f;
				Warps.Scalars.Add(TEXT("Distortion"), 1.2f);
				D.Emitters.Add(Warps);
				FMTVFXEmitter Specks = Motes(6.f, 20.f, ArcaneBlue, 5.f);
				Specks.Orbit = 240.f;
				Specks.bWorldSpace = false;
				Specks.SizeMin = 2.f;
				Specks.SizeMax = 3.5f;
				D.Emitters.Add(Specks);
				D.Lights.Add(Glow(ArcaneBlue, 25.f, 250.f, 0.f, FMTVFXCurve::Grow(0.f, 0.45f)));
				Out.Add(TEXT("DisturbMagic.Formation"), D);
			}
			{
				// Cast (the finger flick, aimed): a glyph flash, a pale ring and a ring of warped air, a black ripple,
				// thin streaks sent forward.
				FMTVFXDesc D = Desc(0.35f);
				FMTVFXMeshLayer Sigil = GlyphLayer(30.f, ArcaneBlue, 5.f);
				Sigil.Duration = 0.2f;
				Sigil.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.65f }, { 1.f, 1.f } });
				Sigil.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Sigil);
				D.Meshes.Add(FacingRing(6.f, 60.f, 0.15f, ArcaneBlue, 3.f, 0.f, 0.05f));
				D.Meshes.Add(FacingAir(6.f, 90.f, 0.18f, 1.2f));
				D.Meshes.Add(VoidRing(10.f, 50.f, 0.25f));
				FMTVFXEmitter Streaks = Sprites(EMTVFXRender::Stretched, 8, 0.f, 0.12f, 0.2f, 2.f, 3.f, DragonWhite, ArcaneBlue, 5.f);
				Streaks.Direction = FVector::ForwardVector;
				Streaks.ConeDeg = 8.f;
				Streaks.SpeedMin = 900.f;
				Streaks.SpeedMax = 1500.f;
				Streaks.Stretch = 10.f;
				D.Emitters.Add(Streaks);
				D.Lights.Add(FlashLight(ArcaneBlue, 60.f, 300.f, 0.15f));
				Out.Add(TEXT("DisturbMagic.Cast"), D);
			}
			{
				// Pulse (loop, moved every frame at 6500 cm/s, X = travel): nearly invisible: a thin ripple of warped air
				// with a small turning glyph at its head, a faint trail of pale streaks and black wisps.
				FMTVFXDesc D = Desc(0.1f, true, 0.1f);
				FMTVFXMeshLayer Ripple = Haze(Paths::ShockRing, FVector(40.f, 40.f, 1.f), 1.2f);
				Ripple.Rotation = FRotator(90.f, 0.f, 0.f);
				D.Meshes.Add(Ripple);
				FMTVFXMeshLayer Echo = Haze(Paths::ShockRing, FVector(28.f, 28.f, 1.f), 0.9f);
				Echo.Rotation = FRotator(90.f, 0.f, 0.f);
				Echo.Offset = FVector(-30.f, 0.f, 0.f);
				D.Meshes.Add(Echo);
				FMTVFXMeshLayer Head = GlyphLayer(10.f, ArcaneBlue, 2.2f);
				Head.SpinSpeed = 360.f;
				Head.SpinAxis = FVector::ForwardVector;
				D.Meshes.Add(Head);
				FMTVFXEmitter Trail = Sprites(EMTVFXRender::Stretched, 0, 30.f, 0.12f, 0.2f, 1.5f, 3.f, DragonWhite, ArcaneBlue, 1.2f);
				Trail.Shape = EMTVFXShape::Ring;
				Trail.Radius = 20.f;
				Trail.Direction = -FVector::ForwardVector;
				Trail.ConeDeg = 5.f;
				Trail.SpeedMin = 50.f;
				Trail.SpeedMax = 150.f;
				Trail.Stretch = 8.f;
				D.Emitters.Add(Trail);
				FMTVFXEmitter Wisps = SmokePuffs(0, 10.f, 10.f, 18.f, VoidBlack, 0.25f, 0.3f);
				Wisps.Buoyancy = 0.f;
				D.Emitters.Add(Wisps);
				Out.Add(TEXT("DisturbMagic.Pulse"), D);
			}
			{
				// Hit (at the target): the glyph snaps open on them with a pale ring, warped air and a black ripple; a few
				// pale sparks disperse. No explosion: counter-magic, not damage.
				FMTVFXDesc D = Desc(0.6f);
				FMTVFXMeshLayer Sigil = GlyphLayer(55.f, ArcaneBlue, 4.f);
				Sigil.Duration = 0.3f;
				Sigil.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.55f }, { 1.f, 1.f } });
				Sigil.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Sigil);
				D.Meshes.Add(FacingRing(10.f, 120.f, 0.25f, ArcaneBlue, 3.f, 0.f, 0.05f));
				D.Meshes.Add(FacingAir(10.f, 150.f, 0.28f, 1.3f));
				D.Meshes.Add(VoidRing(10.f, 90.f, 0.35f, 0.03f));
				FMTVFXEmitter Specks = Sprites(EMTVFXRender::Stretched, 14, 0.f, 0.2f, 0.4f, 2.f, 3.5f, DragonWhite, ArcaneBlue, 6.f);
				Specks.bRadial = true;
				Specks.Shape = EMTVFXShape::Sphere;
				Specks.Radius = 20.f;
				Specks.SpeedMin = 200.f;
				Specks.SpeedMax = 600.f;
				Specks.Drag = 3.f;
				Specks.Stretch = 5.f;
				D.Emitters.Add(Specks);
				D.Lights.Add(FlashLight(ArcaneBlue, 120.f, 500.f, 0.2f));
				Out.Add(TEXT("DisturbMagic.Hit"), D);
			}
			{
				// CollapseFire: the fire gutters and collapses into sparks, embers falling, a puff of smoke.
				FMTVFXDesc D = DisturbCollapseBase();
				D.Meshes.Add(Flash(20.f, FireYellow, 6.f, 0.06f));
				FMTVFXEmitter Gutter = FireBody(4, 0.f, 16.f, 30.f, 0.2f);
				Gutter.SpeedMin = 20.f;
				Gutter.SpeedMax = 60.f;
				Gutter.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.1f } });
				D.Emitters.Add(Gutter);
				D.Emitters.Add(Sparks(30, 0.f, 200.f, 600.f, FireYellow, 600.f));
				FMTVFXEmitter Falling = EmberDrift(12, 0.f, 15.f, 1.f);
				Falling.Buoyancy = -200.f;
				D.Emitters.Add(Falling);
				D.Emitters.Add(SmokePuffs(4, 0.f, 20.f, 45.f, Smoke, 0.45f, 1.f));
				Out.Add(TEXT("DisturbMagic.CollapseFire"), D);
			}
			{
				// CollapseWater: the water loses its shape and falls as drops.
				FMTVFXDesc D = DisturbCollapseBase();
				FMTVFXMeshLayer Slump = WaterBody(Paths::Sphere, FVector(16.f));
				Slump.Duration = 0.3f;
				Slump.ScaleOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.3f } });
				Slump.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Slump);
				FMTVFXEmitter Fall = Droplets(30, 0.f, 50.f, 200.f);
				Fall.Shape = EMTVFXShape::Sphere;
				Fall.Radius = 15.f;
				Fall.Direction = -FVector::UpVector;
				Fall.ConeDeg = 70.f;
				D.Emitters.Add(Fall);
				FMTVFXEmitter Blobs = WaterBlobs(8, 0.f, 40.f, 150.f, 4.f, 8.f);
				Blobs.Direction = -FVector::UpVector;
				Blobs.ConeDeg = 70.f;
				D.Emitters.Add(Blobs);
				D.Emitters.Add(Mist(4, 0.f, 20.f, 40.f, 0.3f, 0.8f));
				Out.Add(TEXT("DisturbMagic.CollapseWater"), D);
			}
			{
				// CollapseEarth: the construct crumbles: chips, soil and dust fall away.
				FMTVFXDesc D = DisturbCollapseBase();
				FMTVFXMeshLayer Husk = RockBody(Paths::RockChunkD, FVector(12.f));
				Husk.Duration = 0.25f;
				Husk.ScaleOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.02f } });
				Husk.Scalars.Add(TEXT("Crack"), 1.f);
				D.Meshes.Add(Husk);
				FMTVFXEmitter Crumbs = Debris(12, 50.f, 180.f, 4.f, 10.f);
				Crumbs.Shape = EMTVFXShape::Sphere;
				Crumbs.Radius = 12.f;
				Crumbs.ConeDeg = 90.f;
				D.Emitters.Add(Crumbs);
				D.Emitters.Add(DirtClods(12, 50.f, 200.f));
				D.Emitters.Add(DustCloud(6, 0.f, 25.f, 55.f, 0.4f, 1.f));
				Out.Add(TEXT("DisturbMagic.CollapseEarth"), D);
			}
			{
				// CollapseWind: the compressed air disperses in a pulse, streaks and condensation scattering.
				FMTVFXDesc D = DisturbCollapseBase();
				D.Meshes.Add(AirPulse(Paths::Sphere, 10.f, 90.f, 0.25f, 1.2f));
				FMTVFXEmitter Scatter = Condensation(16, 0.f, 5.f, 10.f, 0.35f, 0.35f);
				Scatter.bRadial = true;
				Scatter.Shape = EMTVFXShape::Sphere;
				Scatter.Radius = 15.f;
				Scatter.SpeedMin = 400.f;
				Scatter.SpeedMax = 900.f;
				D.Emitters.Add(Scatter);
				FMTVFXEmitter Glints = WindStreaks(10, 0.f, 500.f, 1000.f, FVector::UpVector);
				Glints.bRadial = true;
				Glints.Shape = EMTVFXShape::Sphere;
				Glints.Radius = 10.f;
				D.Emitters.Add(Glints);
				D.Emitters.Add(DustCloud(4, 0.f, 20.f, 40.f, 0.3f, 0.7f));
				Out.Add(TEXT("DisturbMagic.CollapseWind"), D);
			}
			{
				// CollapseArcane: the mana flickers out: a flickering core, motes dispersing, a ring contracting to nothing.
				FMTVFXDesc D = DisturbCollapseBase();
				FMTVFXMeshLayer Core = Orb(14.f, Mana, 6.f, 0.3f, 0.5f);
				Core.Duration = 0.3f;
				Core.ScaleOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.3f, 1.2f }, { 1.f, 0.1f } });
				Core.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 0.15f, 0.3f }, { 0.3f, 1.f }, { 0.5f, 0.2f }, { 0.7f, 0.7f }, { 1.f, 0.f } });
				D.Meshes.Add(Core);
				D.Meshes.Add(ShockwaveRing(40.f, 5.f, 0.25f, Mana, 3.f, 0.f, 0.06f));
				FMTVFXEmitter Scatter = Motes(0.f, 15.f, Mana, 6.f);
				Scatter.Burst = 20;
				Scatter.bRadial = true;
				Scatter.SpeedMin = 100.f;
				Scatter.SpeedMax = 300.f;
				D.Emitters.Add(Scatter);
				FMTVFXLight Flicker = Glow(Mana, 80.f, 350.f, 0.9f, FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } }));
				Flicker.Duration = 0.35f;
				D.Lights.Add(Flicker);
				Out.Add(TEXT("DisturbMagic.CollapseArcane"), D);
				// Older gameplay spawns "DisturbMagic.Impact" where a projectile was disrupted: the same arcane collapse.
				Out.Add(TEXT("DisturbMagic.Impact"), D);
			}
			{
				// Seal (attached to the sealed hand's socket for 3 s; one-shot): a binding glyph turning before the hand, two
				// crossed rings, black wisps circling, a faint flickering light.
				FMTVFXDesc D = Desc(3.f);
				D.FadeOut = 0.3f;
				FMTVFXMeshLayer Binding = GlyphLayer(14.f, ArcaneBlue, 2.5f);
				Binding.AlphaOverLife = FMTVFXCurve::FadeInOut(0.05f, 0.1f);
				Binding.SpinSpeed = 120.f;
				Binding.SpinAxis = FVector::ForwardVector;
				D.Meshes.Add(Binding);
				for (int32 i = 0; i < 2; ++i)
				{
					FMTVFXMeshLayer Band = ThinRing(12.f, i == 0 ? ArcaneBlue : PaleGold, 1.8f);
					Band.Rotation = FRotator(i == 0 ? 90.f : 0.f, 0.f, i == 0 ? 0.f : 90.f);
					Band.AlphaOverLife = FMTVFXCurve::FadeInOut(0.05f, 0.1f);
					Band.SpinSpeed = i == 0 ? 200.f : -160.f;
					Band.SpinAxis = i == 0 ? FVector::UpVector : FVector::ForwardVector;
					D.Meshes.Add(Band);
				}
				FMTVFXEmitter Wisps = SmokePuffs(0, 8.f, 6.f, 12.f, VoidBlack, 0.3f, 0.6f);
				Wisps.Shape = EMTVFXShape::Ring;
				Wisps.Radius = 14.f;
				Wisps.Orbit = 300.f;
				Wisps.Buoyancy = 0.f;
				Wisps.bWorldSpace = false;
				D.Emitters.Add(Wisps);
				D.Lights.Add(Glow(ArcaneBlue, 20.f, 200.f, 0.3f, FMTVFXCurve::FadeInOut(0.05f, 0.1f)));
				Out.Add(TEXT("DisturbMagic.Seal"), D);
			}
			{
				// Destabilize (a zone, wall, tornado or serpent; base radius 450, scaled by its size): a large glyph flares
				// flat across it, black ripples and a pulse of warped air spread, pale crackles and motes.
				FMTVFXDesc D = ImpactDesc(1.f);
				FMTVFXMeshLayer Sigil = GlyphLayer(180.f, ArcaneBlue, 2.f, true);
				Sigil.Offset = FVector(0.f, 0.f, 25.f);
				Sigil.Duration = 0.6f;
				Sigil.ScaleOverLife = FMTVFXCurve({ { 0.f, 0.6f }, { 1.f, 1.1f } });
				Sigil.AlphaOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.f } });
				D.Meshes.Add(Sigil);
				D.Meshes.Add(VoidRing(60.f, 450.f, 0.6f, 0.f, true));
				D.Meshes.Add(VoidRing(40.f, 320.f, 0.5f, 0.1f, true));
				D.Meshes.Add(AirPulse(Paths::Sphere, 60.f, 470.f, 0.45f, 1.1f));
				FMTVFXEmitter Crackle = Sprites(EMTVFXRender::Stretched, 30, 0.f, 0.2f, 0.4f, 2.5f, 4.f, DragonWhite, ArcaneBlue, 6.f);
				Crackle.bRadial = true;
				Crackle.Shape = EMTVFXShape::Sphere;
				Crackle.Radius = 220.f;
				Crackle.SpeedMin = 200.f;
				Crackle.SpeedMax = 500.f;
				Crackle.Drag = 3.f;
				Crackle.Stretch = 6.f;
				D.Emitters.Add(Crackle);
				FMTVFXEmitter Rising = Motes(0.f, 300.f, ArcaneBlue, 5.f);
				Rising.Burst = 20;
				Rising.Shape = EMTVFXShape::Disc;
				Rising.Buoyancy = 120.f;
				D.Emitters.Add(Rising);
				D.Lights.Add(FlashLight(ArcaneBlue, 150.f, 900.f, 0.4f));
				D.Shakes.Add(MinorShake(0.08f));
				Out.Add(TEXT("DisturbMagic.Destabilize"), D);
			}
		}

		// =====================================================================================
		// Dragon Step and Dragon Crush (Orsted): force and precision; pale gold, white and deep blue accents.
		// =====================================================================================
		void AddDragonStep(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Formation (at the feet): the dragon aura gathers: pale-gold and deep-blue rings closing in, motes rising,
				// dust stirring toward him.
				FMTVFXDesc D = Desc(0.12f, true, 0.1f);
				D.bUpright = true;
				FMTVFXMeshLayer Closing = ShockwaveRing(180.f, 40.f, 0.12f, PaleGold, 2.f, 0.f, 0.06f);
				Closing.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
				D.Meshes.Add(Closing);
				FMTVFXMeshLayer Deep = ShockwaveRing(140.f, 30.f, 0.12f, DeepBlue, 3.f, 0.02f, 0.05f);
				Deep.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.2f }, { 1.f, 1.f } });
				D.Meshes.Add(Deep);
				FMTVFXEmitter Rising = Motes(80.f, 60.f, PaleGold, 6.f);
				Rising.Shape = EMTVFXShape::Disc;
				Rising.bFollowGround = true;
				Rising.Buoyancy = 300.f;
				D.Emitters.Add(Rising);
				FMTVFXEmitter Stir = DustCloud(0, 30.f, 20.f, 40.f, 0.3f, 0.4f);
				Stir.Shape = EMTVFXShape::Ring;
				Stir.Radius = 60.f;
				Stir.Attract = 600.f;
				Stir.bFollowGround = true;
				D.Emitters.Add(Stir);
				D.Lights.Add(Glow(PaleGold, 50.f, 300.f, 0.f, FMTVFXCurve::Grow(0.f, 0.45f)));
				Out.Add(TEXT("DragonStep.Formation"), D);
			}
			{
				// Launch (VR 350 ground burst): a flash at the feet, dust and pale-gold rings with warped air racing out, a
				// dust ring, chips, cracks.
				FMTVFXDesc D = ImpactDesc(0.8f);
				D.Meshes.Add(Flash(40.f, DragonWhite, 6.f, 0.08f));
				D.Meshes.Add(ShockwaveRing(30.f, 380.f, 0.3f, DustLight, 1.3f));
				D.Meshes.Add(ShockwaveRing(20.f, 350.f, 0.25f, PaleGold, 3.f, 0.f, 0.05f));
				D.Meshes.Add(AirPulse(Paths::ShockRing, 30.f, 380.f, 0.3f, 1.4f, 0.f, 0.05f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 20.f, 200.f, 0.22f, 1.2f));
				FMTVFXEmitter Ring = DustCloud(24, 0.f, 50.f, 110.f, 0.5f, 0.9f);
				Ring.Shape = EMTVFXShape::Ring;
				Ring.Radius = 40.f;
				Ring.bRadial = true;
				Ring.SpeedMin = 700.f;
				Ring.SpeedMax = 1200.f;
				Ring.Drag = 3.f;
				Ring.bFollowGround = true;
				Ring.Offset = FVector(0.f, 0.f, 10.f);
				Ring.bHero = true;
				D.Emitters.Add(Ring);
				FMTVFXEmitter Chips = Debris(8, 250.f, 500.f, 3.f, 7.f);
				Chips.ConeDeg = 35.f;
				D.Emitters.Add(Chips);
				AddCracks(D, 170.f, 8.f);
				D.Shakes.Add(MinorShake(0.12f));
				Out.Add(TEXT("DragonStep.Launch"), D);
			}
			{
				// Travel (loop on Orsted): three streaked afterimages, a wake of warped air, pale-gold and deep-blue streaks,
				// dust kicked off the ground.
				FMTVFXDesc D = Desc(0.3f, true, 0.3f);
				D.Afterimages.Count = 3;
				D.Afterimages.Interval = 0.04f;
				D.Afterimages.Lifetime = 0.32f;
				D.Afterimages.Color = FC(0.95f, 0.9f, 0.78f);
				D.Afterimages.Intensity = 3.5f;
				FMTVFXMeshLayer Wake = Haze(Paths::Sphere, FVector(170.f, 55.f, 95.f), 1.2f);
				Wake.Offset = FVector(-140.f, 0.f, 90.f);
				D.Meshes.Add(Wake);
				FMTVFXEmitter Kick = DustCloud(0, 70.f, 40.f, 90.f, 0.45f, 0.9f);
				Kick.bFollowGround = true;
				Kick.Offset = FVector(-40.f, 0.f, 10.f);
				Kick.Direction = FVector(-1.f, 0.f, 0.3f);
				Kick.ConeDeg = 35.f;
				D.Emitters.Add(Kick);
				FMTVFXEmitter Streaks = Sprites(EMTVFXRender::Stretched, 0, 50.f, 0.15f, 0.3f, 5.f, 9.f, PaleGold, DragonWhite, 1.8f);
				Streaks.Shape = EMTVFXShape::Box;
				Streaks.Extent = FVector(20.f, 30.f, 80.f);
				Streaks.Offset = FVector(0.f, 0.f, 90.f);
				Streaks.Direction = -FVector::ForwardVector;
				Streaks.ConeDeg = 4.f;
				Streaks.SpeedMin = 800.f;
				Streaks.SpeedMax = 1400.f;
				Streaks.Stretch = 10.f;
				D.Emitters.Add(Streaks);
				FMTVFXEmitter Deep = Streaks;
				Deep.Rate = 20.f;
				Deep.ColorStart = DeepBlue;
				Deep.ColorEnd = DeepBlue;
				Deep.Intensity = 2.5f;
				D.Emitters.Add(Deep);
				Out.Add(TEXT("DragonStep.Travel"), D);
			}
			{
				// Arrive (VR 220, at his feet facing the target): a pressure ring in dust, pale gold and deep blue, warped air,
				// a dust ring, pale-gold sparks, a flash of light.
				FMTVFXDesc D = ImpactDesc(0.7f);
				D.Meshes.Add(ShockwaveRing(20.f, 240.f, 0.25f, DustLight, 1.2f));
				D.Meshes.Add(ShockwaveRing(20.f, 220.f, 0.2f, PaleGold, 3.f, 0.f, 0.05f));
				D.Meshes.Add(ShockwaveRing(15.f, 180.f, 0.22f, DeepBlue, 2.5f, 0.03f, 0.04f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 20.f, 170.f, 0.2f, 1.2f));
				FMTVFXEmitter Ring = DustCloud(16, 0.f, 40.f, 90.f, 0.45f, 0.8f);
				Ring.Shape = EMTVFXShape::Ring;
				Ring.Radius = 30.f;
				Ring.bRadial = true;
				Ring.SpeedMin = 500.f;
				Ring.SpeedMax = 900.f;
				Ring.Drag = 3.f;
				Ring.bFollowGround = true;
				Ring.Offset = FVector(0.f, 0.f, 10.f);
				D.Emitters.Add(Ring);
				FMTVFXEmitter Spk = Sparks(14, 0.f, 300.f, 700.f, PaleGold, 500.f);
				Spk.Offset = FVector(0.f, 0.f, 60.f);
				D.Emitters.Add(Spk);
				D.Lights.Add(FlashLight(PaleGold, 150.f, 500.f, 0.25f));
				D.Shakes.Add(MinorShake(0.12f));
				Out.Add(TEXT("DragonStep.Arrive"), D);
			}
		}

		void AddDragonCrush(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Formation (loop on hand_r during the wind-up): the Dragon God aura compressed tight around the fist: a white
				// core, a pale-gold shell and a deep-blue rim closing in, two thin rings spinning, motes drawn in, warped air.
				FMTVFXDesc D = Desc(0.4f, true, 0.1f);
				FMTVFXMeshLayer Core = Orb(8.f, DragonWhite, 10.f, 0.3f, 0.3f);
				Core.bWobble = true;
				Core.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.3f);
				Core.ScaleOverLife = FMTVFXCurve::Grow(0.3f, 0.8f);
				D.Meshes.Add(Core);
				FMTVFXMeshLayer Aura = Orb(15.f, PaleGold, 2.5f, 0.9f, 0.8f);
				Flow(Aura, 2.4f, 2.4f, 1.2f, -0.6f, 2.f);
				Aura.SpinSpeed = 300.f;
				Aura.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.4f);
				Aura.ScaleOverLife = FMTVFXCurve({ { 0.f, 1.8f }, { 1.f, 1.f } });
				D.Meshes.Add(Aura);
				FMTVFXMeshLayer Rim = Orb(19.f, DeepBlue, 3.f, 1.f, 0.5f);
				Rim.AlphaOverLife = FMTVFXCurve::Grow(0.f, 0.4f);
				Rim.ScaleOverLife = FMTVFXCurve({ { 0.f, 2.f }, { 1.f, 1.f } });
				D.Meshes.Add(Rim);
				for (int32 i = 0; i < 2; ++i)
				{
					FMTVFXMeshLayer Band = ThinRing(22.f + i * 4.f, PaleGold, 2.2f);
					Band.Rotation = FRotator(i == 0 ? 90.f : 0.f, 0.f, i == 0 ? 0.f : 90.f);
					Band.SpinSpeed = i == 0 ? 500.f : -400.f;
					Band.SpinAxis = i == 0 ? FVector::UpVector : FVector::ForwardVector;
					D.Meshes.Add(Band);
				}
				D.Emitters.Add(Gather(50.f, 70.f, PaleGold, EMTVFXRender::SpriteAdd, 6.f));
				D.Meshes.Add(Haze(Paths::Sphere, FVector(28.f), 0.9f));
				D.Lights.Add(Glow(PaleGold, 60.f, 400.f, 0.f, FMTVFXCurve::Grow(0.f, 0.45f)));
				Out.Add(TEXT("DragonCrush.Formation"), D);
			}
			{
				// Impact (on the ground 120 cm ahead, VR 650, crack decal 520): the stored energy released at once: a white
				// and pale-gold flash, a massive shockwave in dust, gold and deep blue with rings of warped air, slabs of
				// ground heaved up around the blow, chunks and soil, a dust ring racing out and a dust column, broken ground
				// and a crater, a bright light. Ultimate shake with a FOV kick.
				FMTVFXDesc D = ImpactDesc(2.6f);
				D.Meshes.Add(Flash(160.f, DragonWhite, 10.f, 0.1f));
				D.Meshes.Add(Flash(90.f, PaleGold, 6.f, 0.22f));
				D.Meshes.Add(ShockwaveRing(60.f, 700.f, 0.5f, DustLight, 1.6f));
				D.Meshes.Add(ShockwaveRing(50.f, 650.f, 0.4f, PaleGold, 3.f, 0.f, 0.05f));
				D.Meshes.Add(ShockwaveRing(40.f, 560.f, 0.45f, DeepBlue, 2.5f, 0.04f, 0.04f));
				for (int32 i = 0; i < 2; ++i)
				{
					D.Meshes.Add(AirPulse(Paths::ShockRing, 60.f, 900.f - i * 200.f, 0.5f, 1.4f, i * 0.08f, 0.06f));
				}
				D.Meshes.Add(AirPulse(Paths::Sphere, 50.f, 500.f, 0.35f, 1.5f));
				AddRaisedSlabs(D, 9, 260.f, 55.f, 32.f, 2.4f, 71);
				FMTVFXEmitter Chunks = Debris(40, 500.f, 1300.f, 10.f, 34.f);
				Chunks.ConeDeg = 40.f;
				Chunks.bHero = true;
				Chunks.MaxParticles = 44;
				D.Emitters.Add(Chunks);
				D.Emitters.Add(DirtClods(30, 600.f, 1400.f));
				FMTVFXEmitter Racing = DustCloud(40, 0.f, 120.f, 260.f, 0.65f, 2.2f);
				Racing.Shape = EMTVFXShape::Ring;
				Racing.Radius = 80.f;
				Racing.bRadial = true;
				Racing.SpeedMin = 900.f;
				Racing.SpeedMax = 1500.f;
				Racing.Drag = 2.8f;
				Racing.bFollowGround = true;
				Racing.bHero = true;
				D.Emitters.Add(Racing);
				FMTVFXEmitter Column = DustCloud(14, 0.f, 150.f, 300.f, 0.55f, 2.4f);
				Column.ConeDeg = 25.f;
				Column.SpeedMin = 300.f;
				Column.SpeedMax = 600.f;
				D.Emitters.Add(Column);
				AddCracks(D, 520.f, 16.f, FC(0.05f, 0.045f, 0.04f));
				AddCrater(D, 300.f, 16.f);
				D.Lights.Add(FlashLight(PaleGold, 900.f, 2000.f, 0.35f));
				D.Shakes.Add(UltimateShake(0.72f, 5.5f));
				Out.Add(TEXT("DragonCrush.Impact"), D);
			}
			{
				// TargetHit (at the primary target's centre, X pointing back toward Orsted): a white flash, a pale-gold ring
				// and warped air around the blow, sparks and dust driven straight through the target (along -X).
				FMTVFXDesc D = Desc(0.6f);
				D.Meshes.Add(Flash(60.f, DragonWhite, 8.f, 0.08f));
				D.Meshes.Add(FacingRing(20.f, 200.f, 0.22f, PaleGold, 3.f, 0.f, 0.06f));
				D.Meshes.Add(FacingAir(20.f, 260.f, 0.25f, 1.5f));
				FMTVFXEmitter Through = Sparks(30, 0.f, 800.f, 1800.f, PaleGold, 400.f);
				Through.bRadial = false;
				Through.Direction = -FVector::ForwardVector;
				Through.ConeDeg = 30.f;
				D.Emitters.Add(Through);
				FMTVFXEmitter Puff = DustCloud(8, 0.f, 40.f, 90.f, 0.4f, 0.8f);
				Puff.Direction = -FVector::ForwardVector;
				Puff.ConeDeg = 45.f;
				Puff.SpeedMin = 300.f;
				Puff.SpeedMax = 700.f;
				D.Emitters.Add(Puff);
				D.Lights.Add(FlashLight(PaleGold, 300.f, 700.f, 0.3f));
				Out.Add(TEXT("DragonCrush.TargetHit"), D);
			}
		}

		// =====================================================================================
		// Orsted: Palm Strike, Saint Dragon Aura, Dragon God awakening (kept for their abilities).
		// =====================================================================================
		void AddOrstedMisc(TMap<FName, FMTVFXDesc>& Out)
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
				D.Shakes.Add(MinorShake(0.12f));
				Out.Add(TEXT("PalmStrike.Impact"), D);
			}
			{
				// Saint Dragon Aura: pressure ripples and a faint pale-gold shimmer.
				FMTVFXDesc D = Desc(1.2f);
				D.Meshes.Add(ShockwaveRing(40.f, 600.f, 0.8f, Gold, 2.5f, 0.f, 0.05f));
				D.Meshes.Add(AirPulse(Paths::Sphere, 60.f, 500.f, 0.7f, 1.4f));
				D.Emitters.Add(Motes(60.f, 120.f, Gold, 5.f));
				D.Lights.Add(Glow(Gold, 120.f, 800.f));
				D.Shakes.Add(HeavyShake(0.2f));
				Out.Add(TEXT("DragonAura.Cast"), D);
			}
			{
				FMTVFXDesc D = Desc(0.8f, true, 0.8f);
				FMTVFXEmitter Ripples = Sprites(EMTVFXRender::Mesh, 0, 1.6f, 1.4f, 1.8f, 350.f, 500.f, Gold, Gold, 1.6f);
				Ripples.MeshPath = Paths::ShockRing;
				Ripples.MaterialPath = Paths::MatGlow;
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
				FMTVFXEmitter Condense = Sprites(EMTVFXRender::Mesh, 0, 5.f, 0.6f, 0.7f, 500.f, 700.f, Gold, Gold, 3.f);
				Condense.MeshPath = Paths::ShockRing;
				Condense.MaterialPath = Paths::MatGlow;
				Condense.bFlat = true;
				Condense.SpeedMin = 0.f;
				Condense.SpeedMax = 0.f;
				Condense.EmitDuration = 1.1f;
				Condense.Offset = FVector(0.f, 0.f, 20.f);
				Condense.SizeOverLife = FMTVFXCurve({ { 0.f, 1.f }, { 1.f, 0.05f } });
				Condense.AlphaOverLife = FMTVFXCurve({ { 0.f, 0.f }, { 0.3f, 0.8f }, { 1.f, 0.f } });
				Condense.MaxParticles = 10;
				D.Emitters.Add(Condense);
				FMTVFXEmitter Inward = Gather(60.f, 300.f, Gold, EMTVFXRender::SpriteAdd, 6.f);
				Inward.EmitDuration = 1.1f;
				Inward.Attract = 900.f;
				D.Emitters.Add(Inward);
				D.Meshes.Add(Flash(160.f, Gold, 12.f, 0.3f, 1.15f));
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
				FMTVFXLight Burst = FlashLight(Gold, 900.f, 1600.f, 0.45f);
				Burst.Delay = 1.15f;
				D.Lights.Add(Burst);
				AddCracks(D, 420.f, 16.f, FC(0.05f, 0.045f, 0.04f), 1.15f);
				D.Shakes.Add(UltimateShake(0.65f, 4.f, 1.15f));
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

		// =====================================================================================
		// Shared hit reactions and fallbacks (Hit.* and Dodge.* are pooled).
		// =====================================================================================
		void AddShared(TMap<FName, FMTVFXDesc>& Out)
		{
			{
				// Hit.Light (X points back at the attacker): a tiny flash, a ring facing the blow, sparks thrown back.
				FMTVFXDesc D = Desc(0.35f);
				D.Meshes.Add(Flash(8.f, Silver, 8.f, 0.06f));
				D.Meshes.Add(FacingRing(6.f, 55.f, 0.14f, Silver, 2.2f, 0.f, 0.06f));
				FMTVFXEmitter Spk = Sparks(12, 0.f, 200.f, 600.f, Silver, 600.f);
				Spk.bRadial = false;
				Spk.Direction = FVector::ForwardVector;
				Spk.ConeDeg = 50.f;
				D.Emitters.Add(Spk);
				Out.Add(TEXT("Hit.Light"), D);
			}
			{
				// Hit.Heavy: a bigger flash, a ring and a ring of warped air facing the blow, a spray of sparks, a dust puff,
				// a small camera tick.
				FMTVFXDesc D = Desc(0.6f);
				D.Meshes.Add(Flash(16.f, FC::White, 9.f, 0.08f));
				D.Meshes.Add(FacingRing(10.f, 110.f, 0.18f, Silver, 2.5f, 0.f, 0.06f));
				D.Meshes.Add(FacingAir(10.f, 140.f, 0.2f, 1.2f));
				FMTVFXEmitter Spk = Sparks(24, 0.f, 300.f, 900.f, Silver, 700.f);
				Spk.bRadial = false;
				Spk.Direction = FVector::ForwardVector;
				Spk.ConeDeg = 60.f;
				D.Emitters.Add(Spk);
				D.Emitters.Add(DustCloud(5, 0.f, 30.f, 70.f, 0.4f, 0.8f));
				D.Shakes.Add(MinorShake(0.12f));
				Out.Add(TEXT("Hit.Heavy"), D);
			}
			{
				FMTVFXDesc D = Desc(0.4f);
				FMTVFXMeshLayer Spark = Orb(10.f, Mana, 8.f, 0.3f, 0.3f);
				Spark.ScaleOverLife = FMTVFXCurve::Grow(0.3f, 0.5f);
				D.Meshes.Add(Spark);
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

		// =====================================================================================
		// Styles: "<Preset>.Formation@Orsted" for the twelve element spells (picked by SpawnPreset when Orsted casts).
		// =====================================================================================
		void AddOrstedStyles(TMap<FName, FMTVFXDesc>& Out)
		{
			Out.Add(TEXT("Fireball.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("Fireball.Formation")), 30.f));
			Out.Add(TEXT("FlameWave.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("FlameWave.Formation")), 40.f));
			Out.Add(TEXT("Inferno.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("Inferno.Formation")), 34.f));
			Out.Add(TEXT("WaterBullet.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("WaterBullet.Formation")), 28.f));
			Out.Add(TEXT("WaterDragon.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("WaterDragon.Formation")), 150.f));
			Out.Add(TEXT("Flood.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("Flood.Formation")), 60.f));
			Out.Add(TEXT("StoneCannon.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("StoneCannon.Formation")), 32.f));
			Out.Add(TEXT("EarthWall.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("EarthWall.Formation")), 36.f));
			Out.Add(TEXT("EarthSpikes.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("EarthSpikes.Formation")), 34.f));
			Out.Add(TEXT("WindBlade.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("WindBlade.Formation")), 34.f));
			Out.Add(TEXT("Tornado.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("Tornado.Formation")), 40.f));
			Out.Add(TEXT("WindBurst.Formation@Orsted"), OrstedStyle(Out.FindChecked(TEXT("WindBurst.Formation")), 60.f));
		}

		/** Emitters left at MaxParticles 0 get room for their burst plus their steady-state population. */
		void AutoCapacity(TMap<FName, FMTVFXDesc>& Out)
		{
			for (TPair<FName, FMTVFXDesc>& Pair : Out)
			{
				const float RateBoost = FMath::Max(1.f, Pair.Value.Charge.RateScale);
				for (FMTVFXEmitter& E : Pair.Value.Emitters)
				{
					if (E.MaxParticles <= 0)
					{
						const int32 Steady = FMath::CeilToInt(E.Rate * RateBoost * E.LifeMax * 1.1f);
						E.MaxParticles = FMath::Clamp(E.Burst + Steady + 2, 4, 256);
					}
				}
			}
		}

		TMap<FName, FMTVFXDesc> BuildLibrary()
		{
			TMap<FName, FMTVFXDesc> Out;
			AddFire(Out);
			AddWater(Out);
			AddWind(Out);
			AddEarth(Out);
			AddCannons(Out);
			AddQuagmire(Out);
			AddBarrage(Out);
			AddRudeusMisc(Out);
			AddDisturbMagic(Out);
			AddDragonStep(Out);
			AddDragonCrush(Out);
			AddOrstedMisc(Out);
			AddShared(Out);
			AddOrstedStyles(Out);
			AutoCapacity(Out);
			return Out;
		}

		const TMap<FName, FMTVFXDesc>& Library()
		{
			static const TMap<FName, FMTVFXDesc> Presets = BuildLibrary();
			return Presets;
		}

		/** Base names that have at least one "<Name>@<Style>" variant. */
		const TSet<FName>& StyledBases()
		{
			static const TSet<FName> Bases = []()
			{
				TSet<FName> Result;
				for (const TPair<FName, FMTVFXDesc>& Pair : Library())
				{
					FString Base;
					FString Style;
					if (Pair.Key.ToString().Split(TEXT("@"), &Base, &Style))
					{
						Result.Add(FName(*Base));
					}
				}
				return Result;
			}();
			return Bases;
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

	bool HasStyleVariants(FName Name)
	{
		return !Name.IsNone() && StyledBases().Contains(Name);
	}
}

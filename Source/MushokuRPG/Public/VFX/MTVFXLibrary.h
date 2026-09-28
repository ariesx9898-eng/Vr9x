// Spell effect presets ("Fireball.Travel", "DragonCrush.Impact", ...) and the asset paths they use.
#pragma once

#include "CoreMinimal.h"
#include "VFX/MTSpellVFX.h"

class UStaticMesh;
class UMaterialInterface;
class UTexture;

namespace MTVFX
{
	/** The preset of that name, or null. */
	MUSHOKURPG_API const FMTVFXDesc* FindPreset(FName Name);
	/** Every preset name (tests and the effects gallery iterate over them). */
	MUSHOKURPG_API TArray<FName> GetPresetNames();

	/** Cached asset loads with fallbacks: never null for meshes/materials (engine basics as last resort). */
	MUSHOKURPG_API UStaticMesh* LoadMesh(const FString& Path);
	MUSHOKURPG_API UMaterialInterface* LoadMaterial(const FString& Path);
	MUSHOKURPG_API UTexture* LoadTexture(const FString& Path);

	// Asset paths (imported by Content/Python/mt_setup_laplace_vfx.py).
	namespace Paths
	{
		extern MUSHOKURPG_API const TCHAR* Sphere;
		extern MUSHOKURPG_API const TCHAR* Cube;
		extern MUSHOKURPG_API const TCHAR* Cylinder;
		extern MUSHOKURPG_API const TCHAR* Cone;
		extern MUSHOKURPG_API const TCHAR* Plane;
		extern MUSHOKURPG_API const TCHAR* Ring;
		extern MUSHOKURPG_API const TCHAR* ShockRing;
		extern MUSHOKURPG_API const TCHAR* Disc;
		extern MUSHOKURPG_API const TCHAR* Crescent;
		extern MUSHOKURPG_API const TCHAR* Spike;
		extern MUSHOKURPG_API const TCHAR* RockChunk;
		extern MUSHOKURPG_API const TCHAR* RockChunkB;
		extern MUSHOKURPG_API const TCHAR* Funnel;
		extern MUSHOKURPG_API const TCHAR* Flame;
		extern MUSHOKURPG_API const TCHAR* DragonHead;
		extern MUSHOKURPG_API const TCHAR* DragonSegment;
		extern MUSHOKURPG_API const TCHAR* Beam;
		extern MUSHOKURPG_API const TCHAR* EarthWall;
		extern MUSHOKURPG_API const TCHAR* Crystal;

		extern MUSHOKURPG_API const TCHAR* MatGlow;
		extern MUSHOKURPG_API const TCHAR* MatSprite;
		extern MUSHOKURPG_API const TCHAR* MatSmoke;
		extern MUSHOKURPG_API const TCHAR* MatWater;
		extern MUSHOKURPG_API const TCHAR* MatAir;
		extern MUSHOKURPG_API const TCHAR* MatRock;
		extern MUSHOKURPG_API const TCHAR* MatGhost;
		extern MUSHOKURPG_API const TCHAR* DecalScorch;
		extern MUSHOKURPG_API const TCHAR* DecalCracks;
		extern MUSHOKURPG_API const TCHAR* DecalWet;
		extern MUSHOKURPG_API const TCHAR* DecalCircle;

		extern MUSHOKURPG_API const TCHAR* TexDot;
		extern MUSHOKURPG_API const TCHAR* TexPuff;
		extern MUSHOKURPG_API const TCHAR* TexStreak;
	}
}

// Spell effect presets ("Fireball.Travel", "DragonCrush.Impact", ...) and the asset paths they use.
// Docs/LaPlace/VFX.md lists every preset; Tools/vfx/check_presets.py checks them against Tools/vfx/phase_contract.json.
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
	/** True when at least one style variant "<Name>@<CharacterId>" exists for Name. */
	MUSHOKURPG_API bool HasStyleVariants(FName Name);

	/** Cached asset loads with fallbacks: never null for meshes/materials (engine basics as last resort). */
	MUSHOKURPG_API UStaticMesh* LoadMesh(const FString& Path);
	MUSHOKURPG_API UMaterialInterface* LoadMaterial(const FString& Path);
	/** A decal material, or the closest existing decal (mud -> wet, crater -> cracks, aim line -> circle); null if none. */
	MUSHOKURPG_API UMaterialInterface* LoadDecalMaterial(const FString& Path);
	MUSHOKURPG_API UTexture* LoadTexture(const FString& Path);

	// Asset paths (imported by Content/Python/mt_setup_laplace.py; meshes from Tools/kit/arch_vfx.py).
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
		extern MUSHOKURPG_API const TCHAR* SpikeB;
		extern MUSHOKURPG_API const TCHAR* RockChunk;
		extern MUSHOKURPG_API const TCHAR* RockChunkB;
		extern MUSHOKURPG_API const TCHAR* RockChunkLong;
		extern MUSHOKURPG_API const TCHAR* RockChunkD;
		extern MUSHOKURPG_API const TCHAR* Funnel;
		extern MUSHOKURPG_API const TCHAR* Flame;
		extern MUSHOKURPG_API const TCHAR* DragonHead;
		extern MUSHOKURPG_API const TCHAR* DragonSegment;
		extern MUSHOKURPG_API const TCHAR* Beam;
		extern MUSHOKURPG_API const TCHAR* EarthWall;
		extern MUSHOKURPG_API const TCHAR* EarthWallB;
		extern MUSHOKURPG_API const TCHAR* EarthWallC;
		extern MUSHOKURPG_API const TCHAR* Crystal;
		extern MUSHOKURPG_API const TCHAR* Slug;
		extern MUSHOKURPG_API const TCHAR* Spiral;
		extern MUSHOKURPG_API const TCHAR* WaveSheet;
		extern MUSHOKURPG_API const TCHAR* Slab;
		extern MUSHOKURPG_API const TCHAR* Glyph;
		extern MUSHOKURPG_API const TCHAR* ConeShell;

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
		extern MUSHOKURPG_API const TCHAR* DecalMud;
		extern MUSHOKURPG_API const TCHAR* DecalCrater;
		extern MUSHOKURPG_API const TCHAR* DecalAimLine;

		extern MUSHOKURPG_API const TCHAR* TexDot;
		extern MUSHOKURPG_API const TCHAR* TexPuff;
		extern MUSHOKURPG_API const TCHAR* TexStreak;
		extern MUSHOKURPG_API const TCHAR* TexFlame;
		extern MUSHOKURPG_API const TCHAR* TexFirePuff;
		extern MUSHOKURPG_API const TCHAR* TexLeaf;
	}
}

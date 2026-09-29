#include "VFX/MTVFXPreloadSubsystem.h"

#include "Combat/MTCombatStatics.h"
#include "Components/DecalComponent.h"
#include "Core/MTTypes.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "VFX/MTVFXLibrary.h"

void UMTVFXPreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	using namespace MTVFX;
	for (const TCHAR* Path : { Paths::Ring, Paths::ShockRing, Paths::Disc, Paths::Crescent, Paths::Spike, Paths::RockChunk,
			 Paths::RockChunkB, Paths::Funnel, Paths::Flame, Paths::DragonHead, Paths::DragonSegment, Paths::Beam, Paths::EarthWall,
			 Paths::Crystal, Paths::SpikeB, Paths::RockChunkLong, Paths::RockChunkD, Paths::EarthWallB, Paths::EarthWallC, Paths::Slug,
			 Paths::Spiral, Paths::WaveSheet, Paths::Slab, Paths::Glyph, Paths::ConeShell })
	{
		Assets.Add(LoadMesh(Path));
	}
	// Loading a material loads the textures it samples.
	for (const TCHAR* Path : { Paths::MatGlow, Paths::MatSprite, Paths::MatSmoke, Paths::MatWater, Paths::MatAir, Paths::MatRock,
			 Paths::MatGhost, Paths::DecalScorch, Paths::DecalCracks, Paths::DecalWet, Paths::DecalCircle, Paths::DecalMud,
			 Paths::DecalCrater, Paths::DecalAimLine, MTCombat::ZoneDecalMaterialPath })
	{
		Assets.Add(LoadMaterial(Path));
	}
	for (const TCHAR* Path : { Paths::TexDot, Paths::TexPuff, Paths::TexStreak, Paths::TexFlame, Paths::TexFirePuff, Paths::TexLeaf })
	{
		Assets.Add(LoadTexture(Path));
	}
	Assets.Remove(nullptr);
	UE_LOG(LogMushoku, Log, TEXT("VFX: %d effect assets kept loaded"), Assets.Num());
}

void UMTVFXPreloadSubsystem::WarmUpDecals(AActor* Around)
{
	UWorld* World = Around ? Around->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || !Around->GetRootComponent())
	{
		return;
	}
	using namespace MTVFX;
	for (const TCHAR* Path : { Paths::DecalScorch, Paths::DecalCracks, Paths::DecalWet, Paths::DecalCircle, Paths::DecalMud, Paths::DecalCrater,
			 Paths::DecalAimLine, MTCombat::ZoneDecalMaterialPath })
	{
		UMaterialInterface* Material = LoadMaterial(Path);
		if (!Material || !Material->GetMaterial() || !Material->GetMaterial()->IsDeferredDecal())
		{
			continue;
		}
		// Two metres across at the feet (large on screen, so it is drawn, not culled), projected straight down.
		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAttached(Material, FVector(200.f, 100.f, 100.f), Around->GetRootComponent(),
			NAME_None, FVector(0.f, 0.f, -100.f), FRotator(-90.f, 0.f, 0.f), EAttachLocation::KeepRelativeOffset, 2.f);
		if (Decal)
		{
			// The fade-in never starts within its two seconds: drawn every frame, invisible.
			Decal->SetFadeIn(60.f, 1.f);
			Decal->SetFadeScreenSize(0.f);
		}
	}
}

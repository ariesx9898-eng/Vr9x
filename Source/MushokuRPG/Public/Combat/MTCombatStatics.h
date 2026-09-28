// Small shared helpers for combat code (FX spawning, target queries, element interactions).
#pragma once

#include "CoreMinimal.h"
#include "Core/MTDataTypes.h"

class AMTCharacterBase;
class UNiagaraSystem;
class USoundBase;
class UMaterialInterface;
class UStaticMesh;
class UNiagaraComponent;

namespace MTCombat
{
	/** Placeholder assets created by Content/Python/mt_create_materials.py (fallbacks if missing). */
	static const TCHAR* ZoneDecalMaterialPath = TEXT("/Game/Materials/M_MT_ZoneDecal.M_MT_ZoneDecal");
	static const TCHAR* SpellBodyMaterialPath = TEXT("/Game/Materials/M_MT_SpellBody.M_MT_SpellBody");
	static const TCHAR* ForesightMaterialPath = TEXT("/Game/Materials/M_MT_ForesightGhost.M_MT_ForesightGhost");

	MUSHOKURPG_API UNiagaraComponent* SpawnFX(const UObject* WorldContext, const TSoftObjectPtr<UNiagaraSystem>& System, const FVector& Location, const FRotator& Rotation, float Scale = 1.f);
	MUSHOKURPG_API void PlaySound(const UObject* WorldContext, const TSoftObjectPtr<USoundBase>& Sound, const FVector& Location, float Volume = 1.f);

	/** Alive characters hostile to Source whose capsule intersects the sphere. */
	MUSHOKURPG_API TArray<AMTCharacterBase*> GetHostilesInRadius(const AMTCharacterBase* Source, const FVector& Center, float Radius);
	/** Alive characters on Source's team (including Source) inside the sphere. */
	MUSHOKURPG_API TArray<AMTCharacterBase*> GetAlliesInRadius(const AMTCharacterBase* Source, const FVector& Center, float Radius);

	/**
	 * Element interaction multipliers for damage / stagger based on the target's state tags.
	 * Quagmire + Earth = more stagger, Water vs Burning = extinguish bonus, Fire vs InStorm = weaker.
	 */
	MUSHOKURPG_API void ApplyElementInteractions(FMTDamageSpec& Spec, const AMTCharacterBase* Target);

	MUSHOKURPG_API UMaterialInterface* LoadMaterial(const TCHAR* Path);
	MUSHOKURPG_API UStaticMesh* LoadEngineShape(const TCHAR* ShapeName);
}

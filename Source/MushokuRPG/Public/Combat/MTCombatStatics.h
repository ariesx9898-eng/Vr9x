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
class USceneComponent;
class AMTSpellVFX;

namespace MTCombat
{
	/** Placeholder assets created by Content/Python/mt_create_materials.py (fallbacks if missing). */
	static const TCHAR* ZoneDecalMaterialPath = TEXT("/Game/Materials/M_MT_ZoneDecal.M_MT_ZoneDecal");
	static const TCHAR* SpellBodyMaterialPath = TEXT("/Game/Materials/M_MT_SpellBody.M_MT_SpellBody");
	static const TCHAR* ForesightMaterialPath = TEXT("/Game/Materials/M_MT_ForesightGhost.M_MT_ForesightGhost");

	/**
	 * Loads an optional presentation asset (VFX, sound, mesh, material) named in data. Data may name assets that are not
	 * authored yet: those are skipped quietly, with one log line per path, instead of a load warning and a package
	 * search on every cast. Game thread.
	 */
	MUSHOKURPG_API UObject* LoadOptionalAsset(const FSoftObjectPath& Path);
	template <typename T>
	T* LoadOptional(const TSoftObjectPtr<T>& Soft)
	{
		return Cast<T>(LoadOptionalAsset(Soft.ToSoftObjectPath()));
	}

	MUSHOKURPG_API UNiagaraComponent* SpawnFX(const UObject* WorldContext, const TSoftObjectPtr<UNiagaraSystem>& System, const FVector& Location, const FRotator& Rotation, float Scale = 1.f);
	MUSHOKURPG_API void PlaySound(const UObject* WorldContext, const TSoftObjectPtr<USoundBase>& Sound, const FVector& Location, float Volume = 1.f);

	/**
	 * Spell presentation for one phase: the authored Niagara system when it exists, otherwise the runtime preset
	 * "<FX.Preset>.<Phase>" (MTVFXLibrary.cpp). Returns the runtime effect (null for Niagara or when neither exists).
	 * AttachTo/Socket make the effect follow a component (hands, projectiles, zones).
	 */
	MUSHOKURPG_API AMTSpellVFX* SpawnSpellFX(const UObject* WorldContext, const FMTSpellFX& FX, const TSoftObjectPtr<UNiagaraSystem>& Authored,
		const TCHAR* Phase, const FTransform& Transform, float Scale = 1.f, USceneComponent* AttachTo = nullptr, FName Socket = NAME_None, AActor* Source = nullptr);
	/** Runtime preset "<Preset>.<Phase>" only (no Niagara); null when the preset has no such phase. */
	MUSHOKURPG_API AMTSpellVFX* SpawnPresetPhase(const UObject* WorldContext, FName Preset, const TCHAR* Phase, const FTransform& Transform, float Scale = 1.f,
		USceneComponent* AttachTo = nullptr, FName Socket = NAME_None, AActor* Source = nullptr);
	/** Stops a looping runtime effect and lets it fade where it is (detached from whatever it followed). */
	MUSHOKURPG_API void StopSpellFX(AMTSpellVFX* Effect);

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

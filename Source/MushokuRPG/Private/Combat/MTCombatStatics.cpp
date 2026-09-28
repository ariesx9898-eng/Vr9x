#include "Combat/MTCombatStatics.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Core/MTGameplayTags.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Core/MTTypes.h"
#include "Misc/PackageName.h"

namespace MTCombat
{
	UObject* LoadOptionalAsset(const FSoftObjectPath& Path)
	{
		if (Path.IsNull())
		{
			return nullptr;
		}
		if (UObject* Resolved = Path.ResolveObject())
		{
			return Resolved;
		}
		static TSet<FSoftObjectPath> Missing;
		if (Missing.Contains(Path))
		{
			return nullptr;
		}
		// Check the package first: loading a missing one warns and searches for it every time.
		UObject* Loaded = FPackageName::DoesPackageExist(Path.GetLongPackageName()) ? Path.TryLoad() : nullptr;
		if (!Loaded)
		{
			Missing.Add(Path);
			UE_LOG(LogMushoku, Log, TEXT("Presentation asset %s is not authored yet: skipped (logged once)."), *Path.ToString());
		}
		return Loaded;
	}

	UNiagaraComponent* SpawnFX(const UObject* WorldContext, const TSoftObjectPtr<UNiagaraSystem>& System, const FVector& Location, const FRotator& Rotation, float Scale)
	{
		if (System.IsNull() || !WorldContext)
		{
			return nullptr;
		}
		UNiagaraSystem* Loaded = LoadOptional(System);
		UWorld* World = WorldContext->GetWorld();
		if (!Loaded || !World)
		{
			return nullptr;
		}
		// Pooled: spell FX are spawned constantly in combat.
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Loaded, Location, Rotation, FVector(Scale), true, true, ENCPoolMethod::AutoRelease, true);
	}

	void PlaySound(const UObject* WorldContext, const TSoftObjectPtr<USoundBase>& Sound, const FVector& Location, float Volume)
	{
		if (Sound.IsNull() || !WorldContext)
		{
			return;
		}
		if (USoundBase* Loaded = LoadOptional(Sound))
		{
			UGameplayStatics::PlaySoundAtLocation(WorldContext, Loaded, Location, Volume);
		}
	}

	static TArray<AMTCharacterBase*> QueryCharacters(const AMTCharacterBase* Source, const FVector& Center, float Radius, bool bHostile)
	{
		TArray<AMTCharacterBase*> Result;
		UWorld* World = Source ? Source->GetWorld() : nullptr;
		if (!World || Radius <= 0.f)
		{
			return Result;
		}
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTQueryCharacters), false);
		World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AMTCharacterBase* Character = Cast<AMTCharacterBase>(Overlap.GetActor());
			if (!Character || !Character->IsAlive() || Result.Contains(Character))
			{
				continue;
			}
			if (Source->IsHostileTo(Character) == bHostile)
			{
				Result.Add(Character);
			}
		}
		return Result;
	}

	TArray<AMTCharacterBase*> GetHostilesInRadius(const AMTCharacterBase* Source, const FVector& Center, float Radius)
	{
		return QueryCharacters(Source, Center, Radius, true);
	}

	TArray<AMTCharacterBase*> GetAlliesInRadius(const AMTCharacterBase* Source, const FVector& Center, float Radius)
	{
		return QueryCharacters(Source, Center, Radius, false);
	}

	void ApplyElementInteractions(FMTDamageSpec& Spec, const AMTCharacterBase* Target)
	{
		if (!Target)
		{
			return;
		}
		const FGameplayTagContainer& Tags = Target->GetStateTags();
		switch (Spec.Element)
		{
		case EMTElement::Earth:
			// Quagmire synergy: stone against a mired target lands with far more stagger.
			if (Tags.HasTag(MTTags::State_InQuagmire))
			{
				Spec.Stagger *= 1.6f;
				Spec.Damage *= 1.1f;
			}
			break;
		case EMTElement::Water:
			if (Tags.HasTag(MTTags::State_Burning))
			{
				Spec.Stagger *= 1.25f; // steam burst
			}
			break;
		case EMTElement::Fire:
			if (Tags.HasTag(MTTags::State_InStorm))
			{
				Spec.Damage *= 0.8f; // rain dampens fire
			}
			if (Tags.HasTag(MTTags::State_Frozen))
			{
				Spec.Damage *= 1.2f; // thermal shock
			}
			break;
		case EMTElement::Wind:
			if (Tags.HasTag(MTTags::State_Frozen))
			{
				Spec.Stagger *= 1.3f;
			}
			break;
		default:
			break;
		}
	}

	UMaterialInterface* LoadMaterial(const TCHAR* Path)
	{
		return Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	}

	UStaticMesh* LoadEngineShape(const TCHAR* ShapeName)
	{
		const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), ShapeName, ShapeName);
		return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	}
}

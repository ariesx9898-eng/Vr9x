#include "Combat/MTCombatStatics.h"
#include "VFX/MTSpellVFX.h"
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
#include "TimerManager.h"
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

	AMTSpellVFX* SpawnPresetPhase(const UObject* WorldContext, FName Preset, const TCHAR* Phase, const FTransform& Transform, float Scale,
		USceneComponent* AttachTo, FName Socket, AActor* Source)
	{
		if (Preset.IsNone() || !Phase || !WorldContext)
		{
			return nullptr;
		}
		const FName Name(*FString::Printf(TEXT("%s.%s"), *Preset.ToString(), Phase));
		return AMTSpellVFX::SpawnPreset(const_cast<UObject*>(WorldContext), Name, Transform, Scale, AttachTo, Socket, Source);
	}

	AMTSpellVFX* SpawnSpellFX(const UObject* WorldContext, const FMTSpellFX& FX, const TSoftObjectPtr<UNiagaraSystem>& Authored,
		const TCHAR* Phase, const FTransform& Transform, float Scale, USceneComponent* AttachTo, FName Socket, AActor* Source)
	{
		if (!Authored.IsNull() && LoadOptional(Authored))
		{
			SpawnFX(WorldContext, Authored, Transform.GetLocation(), Transform.Rotator(), Scale);
			return nullptr;
		}
		return SpawnPresetPhase(WorldContext, FX.Preset, Phase, Transform, Scale * FMath::Max(0.05f, FX.PresetScale), AttachTo, Socket, Source);
	}

	void StopSpellFX(AMTSpellVFX* Effect)
	{
		if (IsValid(Effect))
		{
			Effect->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Effect->Stop();
		}
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

	void HitStop(const TArray<AActor*>& Actors, float Seconds)
	{
		if (Seconds <= 0.f)
		{
			return;
		}
		UWorld* StopWorld = nullptr;
		TArray<TWeakObjectPtr<AActor>> Frozen;
		for (AActor* Each : Actors)
		{
			if (IsValid(Each) && !Frozen.Contains(Each))
			{
				StopWorld = StopWorld ? StopWorld : Each->GetWorld();
				// Not a full stop: a sliver of motion keeps it from reading as a hitch.
				Each->CustomTimeDilation = 0.03f;
				Frozen.Add(Each);
			}
		}
		if (!StopWorld || Frozen.Num() == 0)
		{
			return;
		}
		// World timers run on world time, so the freeze lasts Seconds no matter how slow the actors are.
		FTimerHandle Handle;
		StopWorld->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Frozen]()
		{
			for (const TWeakObjectPtr<AActor>& Weak : Frozen)
			{
				if (AActor* Resumed = Weak.Get())
				{
					Resumed->CustomTimeDilation = 1.f;
				}
			}
		}), Seconds, false);
	}

	FVector GroundBelow(const UObject* WorldContext, const FVector& Point)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World)
		{
			return Point;
		}
		FHitResult Hit;
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTGroundBelow), false);
		if (World->LineTraceSingleByObjectType(Hit, Point + FVector(0.f, 0.f, 400.f), Point - FVector(0.f, 0.f, 3000.f), Objects, Params))
		{
			return Hit.ImpactPoint;
		}
		return Point;
	}

	FMTDamageSpec MakeAbilityHit(const FMTAbilityData& Row, AActor* Instigator, const FVector& HitLocation, const FVector& Direction, float DamageScale)
	{
		FMTDamageSpec Spec;
		Spec.Damage = Row.Damage * DamageScale;
		Spec.Stagger = Row.Stagger;
		Spec.Knockback = Row.Knockback;
		Spec.Launch = Row.Launch;
		Spec.Element = Row.Element;
		Spec.bIsMagic = Row.Element != EMTElement::None;
		Spec.SourceAbility = Row.AbilityID;
		Spec.Instigator = Instigator;
		Spec.HitLocation = HitLocation;
		Spec.HitDirection = Direction.IsNearlyZero() ? FVector::ForwardVector : Direction.GetSafeNormal();
		return Spec;
	}

	void ApplyBurn(const FMTAbilityData& Row, AMTCharacterBase* Target, AActor* Instigator, bool bDefaultBurn)
	{
		if (!Target || !Target->IsAlive() || !Target->GetAttributes() || Row.Element != EMTElement::Fire)
		{
			return;
		}
		const bool bFromRow = Row.BurnSeconds > 0.f;
		if (!bFromRow && !bDefaultBurn)
		{
			return;
		}
		FMTStatusEffect Burn;
		Burn.Id = TEXT("Burning");
		Burn.Duration = bFromRow ? Row.BurnSeconds : 3.f;
		Burn.HealthPerSecond = -(bFromRow ? FMath::Max(0.f, Row.BurnDamagePerSecond) : Row.Damage * 0.08f);
		Burn.GrantedTags.AddTag(MTTags::State_Burning);
		Burn.Instigator = Instigator;
		Target->GetAttributes()->AddStatusEffect(Burn);
	}
}

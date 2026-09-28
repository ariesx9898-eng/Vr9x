#include "Combat/MTZoneActor.h"
#include "Combat/MTCombatStatics.h"
#include "Combat/MTTelegraphSubsystem.h"
#include "Combat/MTProjectile.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbilityComponent.h"
#include "Core/MTGameplayTags.h"
#include "Components/DecalComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Engine/World.h"

static const float ZonePulseInterval = 0.25f;

AMTZoneActor::AMTZoneActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal->SetupAttachment(Root);
	// Project straight down onto whatever surface is there (terrain, floors, stairs).
	Decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Decal->DecalSize = FVector(200.f, 400.f, 400.f);
	Decal->SetFadeScreenSize(0.f);

	LoopFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LoopFX"));
	LoopFX->SetupAttachment(Root);
	LoopFX->SetAutoActivate(false);
}

void AMTZoneActor::InitZone(const FMTAbilityData& InData, AMTCharacterBase* InOwner, float AreaMultiplier, bool bAttachToOwner)
{
	Data = InData;
	OwnerCharacter = InOwner;
	SetOwner(InOwner);
	SetInstigator(InOwner);
	Radius = FMath::Max(50.f, Data.AOERadius * AreaMultiplier);
	InnerRadius = Data.InnerRadius * AreaMultiplier;
	NextEruptionTime = Data.ActivationDelay;

	if (bAttachToOwner && InOwner)
	{
		AttachToActor(InOwner, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	// Decal box: X = projection depth, Y/Z = radius.
	Decal->DecalSize = FVector(250.f, Radius, Radius);
	UMaterialInterface* DecalMaterial = Data.FX.DecalMaterial.IsNull() ? MTCombat::LoadMaterial(MTCombat::ZoneDecalMaterialPath) : Data.FX.DecalMaterial.LoadSynchronous();
	if (DecalMaterial)
	{
		DecalMID = UMaterialInstanceDynamic::Create(DecalMaterial, this);
		FLinearColor Tint = MTUtil::ElementColor(Data.Element);
		if (Data.ZoneKind == EMTZoneKind::Mire)
		{
			Tint = FLinearColor(0.12f, 0.08f, 0.05f); // dark wet mud
			DecalMID->SetScalarParameterValue(TEXT("Wetness"), 1.f);
		}
		else if (Data.ZoneKind == EMTZoneKind::Eruptions)
		{
			Tint = FLinearColor(1.f, 0.25f, 0.05f); // warning ring first
			DecalMID->SetScalarParameterValue(TEXT("RingOnly"), 1.f);
		}
		DecalMID->SetVectorParameterValue(TEXT("Color"), Tint);
		DecalMID->SetScalarParameterValue(TEXT("Opacity"), 0.f);
		Decal->SetDecalMaterial(DecalMID);
	}
	else
	{
		UE_LOG(LogMushoku, Warning, TEXT("Zone %s: no decal material (run Content/Python/mt_create_materials.py)"), *Data.AbilityID.ToString());
	}

	if (!Data.FX.Travel.IsNull())
	{
		if (UNiagaraSystem* Loop = Data.FX.Travel.LoadSynchronous())
		{
			LoopFX->SetAsset(Loop);
			LoopFX->SetFloatParameter(TEXT("Radius"), Radius);
			LoopFX->Activate(true);
		}
	}
	MTCombat::SpawnFX(this, Data.FX.Formation, GetActorLocation(), FRotator::ZeroRotator, Radius / 400.f);
	MTCombat::PlaySound(this, Data.FX.CastSound, GetActorLocation());

	// Flame Field etc. announce themselves so AI can step out and Demon Eye can show it.
	if (Data.ZoneKind == EMTZoneKind::Eruptions)
	{
		if (UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this))
		{
			FMTAttackTelegraph T;
			T.Attacker = InOwner;
			T.AttackId = Data.AbilityID;
			T.ImpactLocation = GetActorLocation();
			T.Radius = Radius;
			T.ImpactTime = GetWorld()->GetTimeSeconds() + Data.ActivationDelay;
			T.bIsMagic = true;
			T.PredictedAttackerLocation = InOwner ? InOwner->GetActorLocation() : GetActorLocation();
			T.PredictedAttackerRotation = InOwner ? InOwner->GetActorRotation() : FRotator::ZeroRotator;
			Telegraphs->PublishTelegraph(T);
		}
	}
}

void AMTZoneActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bExpired)
	{
		return;
	}
	Age += DeltaSeconds;
	UpdateDecalLook(DeltaSeconds);

	const float Duration = Data.Duration > 0.f ? Data.Duration : 3.f;

	if (Data.ZoneKind == EMTZoneKind::Eruptions)
	{
		// Wind + Fire: a Tempest Domain overlapping the field fans the flames wider.
		AMTZoneActor* Wind = nullptr;
		if (IsInsideZoneOfKind(this, GetActorLocation(), EMTZoneKind::WindField, &Wind) && Wind)
		{
			Radius = FMath::Max(Radius, Data.AOERadius * 1.35f);
			Decal->DecalSize = FVector(250.f, Radius, Radius);
		}
		if (Age >= NextEruptionTime && EruptionsDone < FMath::Max(1, Data.PulseCount))
		{
			Erupt();
			++EruptionsDone;
			NextEruptionTime = Age + Data.PulseInterval;
		}
		if (EruptionsDone >= FMath::Max(1, Data.PulseCount) && Age >= NextEruptionTime)
		{
			Expire();
		}
		return;
	}

	if (Data.ZoneKind == EMTZoneKind::WindField && OwnerCharacter.IsValid())
	{
		ApplyWindField(MTCombat::GetHostilesInRadius(OwnerCharacter.Get(), GetActorLocation(), Radius), DeltaSeconds);
	}

	PulseAccumulator += DeltaSeconds;
	if (PulseAccumulator >= ZonePulseInterval)
	{
		PulseAccumulator = 0.f;
		Pulse();
	}

	if (Age >= Duration)
	{
		Expire();
	}
}

void AMTZoneActor::Pulse()
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	if (!Owner)
	{
		Expire();
		return;
	}
	const TArray<AMTCharacterBase*> Hostiles = MTCombat::GetHostilesInRadius(Owner, GetActorLocation(), Radius);
	switch (Data.ZoneKind)
	{
	case EMTZoneKind::Mire: ApplyMire(Hostiles); break;
	case EMTZoneKind::Freeze: ApplyFreeze(Hostiles); break;
	case EMTZoneKind::Storm: ApplyStorm(); break;
	case EMTZoneKind::Aura: ApplyAura(Hostiles); break;
	default: break;
	}
}

void AMTZoneActor::ApplyMire(const TArray<AMTCharacterBase*>& Targets)
{
	for (AMTCharacterBase* Target : Targets)
	{
		FMTStatusEffect Effect;
		Effect.Id = TEXT("Quagmire");
		Effect.Duration = ZonePulseInterval * 2.5f; // lingers briefly after leaving the mud
		Effect.Movement = Data.ZoneMovement;
		Effect.Movement.bRooted = false; // Quagmire slows, never immobilises
		Effect.GrantedTags.AddTag(MTTags::State_InQuagmire);
		Target->GetAttributes()->AddStatusEffect(Effect);
	}
}

void AMTZoneActor::ApplyFreeze(const TArray<AMTCharacterBase*>& Targets)
{
	// Frost + Storm: a soaked area freezes harder (longer root).
	const bool bWet = IsInsideZoneOfKind(this, GetActorLocation(), EMTZoneKind::Storm);
	for (AMTCharacterBase* Target : Targets)
	{
		const float Dist = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());
		FMTStatusEffect Effect;
		Effect.GrantedTags.AddTag(MTTags::State_Frozen);
		if (Dist <= InnerRadius && !Rooted.Contains(Target))
		{
			// Centre: one application of the strong effect per cast (root for ordinary enemies).
			Rooted.Add(Target);
			Effect.Id = TEXT("FrostPrisonCore");
			Effect.Duration = bWet ? 2.2f : 1.5f;
			Effect.Movement = Data.ZoneInnerMovement;
			const FMTDamageSpec Chill = [&]()
			{
				FMTDamageSpec S;
				S.Damage = Data.Damage;
				S.Stagger = Data.Stagger;
				S.Element = EMTElement::Water;
				S.SourceAbility = Data.AbilityID;
				S.Instigator = OwnerCharacter.Get();
				S.HitLocation = Target->GetActorLocation();
				S.HitDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				return S;
			}();
			const FMTDamageResult Result = Target->ReceiveHit(Chill);
			if (OwnerCharacter.IsValid() && OwnerCharacter->GetAbilities())
			{
				OwnerCharacter->GetAbilities()->NotifyAbilityHit(Data.AbilityID, Result.DamageDealt);
			}
		}
		else
		{
			Effect.Id = TEXT("FrostPrisonChill");
			Effect.Duration = ZonePulseInterval * 2.5f;
			Effect.Movement = Data.ZoneMovement;
		}
		Target->GetAttributes()->AddStatusEffect(Effect);
	}
}

void AMTZoneActor::ApplyStorm()
{
	// Allies inside the storm get the water-efficiency blessing; enemies get soaked.
	for (AMTCharacterBase* Ally : MTCombat::GetAlliesInRadius(OwnerCharacter.Get(), GetActorLocation(), Radius))
	{
		FMTStatusEffect Blessing;
		Blessing.Id = TEXT("StormBlessing");
		Blessing.Duration = ZonePulseInterval * 2.5f;
		Blessing.GrantedTags.AddTag(MTTags::State_InStorm);
		Ally->GetAttributes()->AddStatusEffect(Blessing);
	}
	for (AMTCharacterBase* Enemy : MTCombat::GetHostilesInRadius(OwnerCharacter.Get(), GetActorLocation(), Radius))
	{
		FMTStatusEffect Soaked;
		Soaked.Id = TEXT("Soaked");
		Soaked.Duration = 3.f;
		Soaked.Movement.SpeedMultiplier = 0.9f;
		Soaked.GrantedTags.AddTag(MTTags::State_InStorm);
		Enemy->GetAttributes()->RemoveStatusEffect(TEXT("Burning"));
		Enemy->GetAttributes()->AddStatusEffect(Soaked);
	}
}

void AMTZoneActor::ApplyWindField(const TArray<AMTCharacterBase*>& Targets, float DeltaSeconds)
{
	const FVector Center = GetActorLocation();
	for (AMTCharacterBase* Target : Targets)
	{
		UMTAttributeComponent* Attr = Target->GetAttributes();
		if (!Attr || !Attr->bIsWeak)
		{
			continue; // only weak enemies are shoved; strong ones lean into it
		}
		const FVector Out = (Target->GetActorLocation() - Center).GetSafeNormal2D();
		const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Out);
		Target->AddMovementInput(Out * 0.6f + Tangent * 0.4f, 1.f, true);
		Target->LaunchCharacter((Out * 350.f + Tangent * 250.f) * DeltaSeconds * 4.f, false, false);
	}

	// Partially redirect ordinary hostile projectiles entering the domain.
	if (UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this))
	{
		for (AMTProjectile* Spell : Telegraphs->GetSpellsNear(Center, Radius, OwnerCharacter.Get()))
		{
			if (Spell && Spell->IsDeflectable())
			{
				const FVector Vel = Spell->GetProjectileVelocity().GetSafeNormal();
				const FVector Out = (Spell->GetActorLocation() - Center).GetSafeNormal2D();
				Spell->Deflect((Vel + FVector::CrossProduct(FVector::UpVector, Out) * 0.8f).GetSafeNormal());
			}
		}
	}

	for (AMTCharacterBase* Target : Targets)
	{
		FMTStatusEffect Gust;
		Gust.Id = TEXT("TempestDrag");
		Gust.Duration = 0.3f;
		Gust.Movement.SpeedMultiplier = 0.85f;
		Gust.GrantedTags.AddTag(MTTags::State_InWindField);
		Target->GetAttributes()->AddStatusEffect(Gust);
	}
}

void AMTZoneActor::ApplyAura(const TArray<AMTCharacterBase*>& Targets)
{
	// Saint Dragon Aura pressure: weak NPCs falter and back away; strong foes and bosses resist.
	const FVector Center = GetActorLocation();
	for (AMTCharacterBase* Target : Targets)
	{
		UMTAttributeComponent* Attr = Target->GetAttributes();
		if (!Attr || !Attr->bIsWeak || Attr->bCrowdControlImmune)
		{
			continue;
		}
		FMTStatusEffect Pressure;
		Pressure.Id = TEXT("DragonPressure");
		Pressure.Duration = 0.6f;
		Pressure.Movement.SpeedMultiplier = 0.7f;
		Pressure.Stats.DamageMultiplier = 0.85f;
		Attr->AddStatusEffect(Pressure);
		const FVector Away = (Target->GetActorLocation() - Center).GetSafeNormal2D();
		Target->AddMovementInput(Away, 0.5f, true);
	}
}

void AMTZoneActor::Erupt()
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	if (!Owner)
	{
		return;
	}
	// Several eruptions scattered inside the circle, each with its own small blast radius.
	const int32 Bursts = 4;
	const float BurstRadius = FMath::Max(90.f, Radius * 0.38f);
	FRandomStream Stream(GetUniqueID() + EruptionsDone * 7919);
	TSet<AMTCharacterBase*> HitThisPulse;
	for (int32 i = 0; i < Bursts; ++i)
	{
		const float Angle = Stream.FRandRange(0.f, 2.f * PI);
		const float Dist = Radius * FMath::Sqrt(Stream.FRand()) * 0.75f;
		const FVector Point = GetActorLocation() + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		MTCombat::SpawnFX(this, Data.FX.Impact, Point, FRotator::ZeroRotator, BurstRadius / 150.f);
		for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, Point, BurstRadius))
		{
			if (HitThisPulse.Contains(Target))
			{
				continue;
			}
			HitThisPulse.Add(Target);
			FMTDamageSpec Spec;
			Spec.Damage = Data.Damage;
			Spec.Stagger = Data.Stagger;
			Spec.Knockback = Data.Knockback;
			Spec.Element = Data.Element;
			Spec.SourceAbility = Data.AbilityID;
			Spec.Instigator = Owner;
			Spec.HitLocation = Point;
			Spec.HitDirection = (Target->GetActorLocation() - Point).GetSafeNormal2D();
			MTCombat::ApplyElementInteractions(Spec, Target);
			const FMTDamageResult Result = Target->ReceiveHit(Spec);
			if (Data.Element == EMTElement::Fire)
			{
				FMTStatusEffect Burn;
				Burn.Id = TEXT("Burning");
				Burn.Duration = 3.f;
				Burn.HealthPerSecond = -Data.Damage * 0.08f;
				Burn.GrantedTags.AddTag(MTTags::State_Burning);
				Burn.Instigator = Owner;
				Target->GetAttributes()->AddStatusEffect(Burn);
			}
			if (Owner->GetAbilities())
			{
				Owner->GetAbilities()->NotifyAbilityHit(Data.AbilityID, Result.DamageDealt);
			}
		}
	}
	MTCombat::PlaySound(this, Data.FX.ImpactSound, GetActorLocation());
	if (DecalMID)
	{
		DecalMID->SetScalarParameterValue(TEXT("RingOnly"), 0.f);
	}
}

void AMTZoneActor::UpdateDecalLook(float DeltaSeconds)
{
	if (!DecalMID)
	{
		return;
	}
	const float Duration = Data.Duration > 0.f ? Data.Duration : 3.f;
	// Fade in over 0.3 s (the ground visibly turning to mud), fade out over the last 0.6 s.
	const float In = FMath::Clamp(Age / 0.3f, 0.f, 1.f);
	const float Out = Data.ZoneKind == EMTZoneKind::Eruptions ? 1.f : FMath::Clamp((Duration - Age) / 0.6f, 0.f, 1.f);
	DecalMID->SetScalarParameterValue(TEXT("Opacity"), In * Out);
	DecalMID->SetScalarParameterValue(TEXT("Age"), Age);
}

void AMTZoneActor::Expire()
{
	if (bExpired)
	{
		return;
	}
	bExpired = true;
	if (LoopFX)
	{
		LoopFX->Deactivate();
	}
	MTCombat::SpawnFX(this, Data.FX.Dissipation, GetActorLocation(), FRotator::ZeroRotator, Radius / 400.f);
	if (DecalMID)
	{
		DecalMID->SetScalarParameterValue(TEXT("Opacity"), 0.f);
	}
	SetLifeSpan(0.8f);
}

bool AMTZoneActor::IsInsideZoneOfKind(const UObject* WorldContext, const FVector& Location, EMTZoneKind Kind, AMTZoneActor** OutZone)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AMTZoneActor> It(World); It; ++It)
	{
		AMTZoneActor* Zone = *It;
		if (Zone && Zone != WorldContext && Zone->IsActiveZone() && Zone->GetKind() == Kind &&
			FVector::DistSquared2D(Zone->GetActorLocation(), Location) <= FMath::Square(Zone->GetRadius()))
		{
			if (OutZone)
			{
				*OutZone = Zone;
			}
			return true;
		}
	}
	return false;
}

#include "Combat/MTZoneActor.h"
#include "CollisionQueryParams.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VFX/MTSpellVFX.h"
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

namespace
{
	/** Ground under P (landscape / static geometry only: pawns never count as floor). */
	FVector GroundAt(const UWorld* World, const FVector& P)
	{
		if (!World)
		{
			return P;
		}
		FHitResult Hit;
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTZoneGround), false);
		if (World->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, 400.f), P - FVector(0.f, 0.f, 1500.f), Objects, Params))
		{
			return Hit.ImpactPoint;
		}
		return P;
	}

	bool IsNewZoneKind(EMTZoneKind Kind)
	{
		return Kind == EMTZoneKind::DamageField || Kind == EMTZoneKind::Burst || Kind == EMTZoneKind::LineEruptions
			|| Kind == EMTZoneKind::Vortex || Kind == EMTZoneKind::Wave;
	}
}

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
	else if (InOwner)
	{
		// Direction of the cast (caster to aim point, horizontal). Moving zones start just in front of the caster and
		// roll that way; line eruptions run from the caster to the aim point; a burst goes off around the caster.
		FVector ToAim = GetActorLocation() - InOwner->GetActorLocation();
		ToAim.Z = 0.f;
		MoveDirection = ToAim.IsNearlyZero() ? InOwner->GetActorForwardVector().GetSafeNormal2D() : ToAim.GetSafeNormal();
		if (Data.ZoneMoveSpeed > 0.f)
		{
			SetActorLocation(GroundAt(GetWorld(), InOwner->GetActorLocation() + MoveDirection * (InOwner->GetSimpleCollisionRadius() + 130.f)));
		}
		else if (Data.ZoneKind == EMTZoneKind::Burst)
		{
			SetActorLocation(GroundAt(GetWorld(), InOwner->GetActorLocation()));
		}
		if (Data.ZoneKind == EMTZoneKind::LineEruptions)
		{
			LineStart = GroundAt(GetWorld(), InOwner->GetActorLocation() + MoveDirection * (InOwner->GetSimpleCollisionRadius() + 100.f));
		}
		SetActorRotation(MoveDirection.Rotation());
	}

	// Decal box: X = projection depth, Y/Z = radius.
	Decal->DecalSize = FVector(250.f, Radius, Radius);
	// The zone's own decal material when it exists, else the shared zone decal (mt_create_materials.py).
	UMaterialInterface* DecalMaterial = MTCombat::LoadOptional(Data.FX.DecalMaterial);
	if (!DecalMaterial)
	{
		DecalMaterial = MTCombat::LoadMaterial(MTCombat::ZoneDecalMaterialPath);
	}
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

	if (UNiagaraSystem* Loop = MTCombat::LoadOptional(Data.FX.Travel))
	{
		LoopFX->SetAsset(Loop);
		LoopFX->SetFloatParameter(TEXT("Radius"), Radius);
		LoopFX->Activate(true);
	}
	else
	{
		// Runtime zone effect sized with the zone (awakening makes Quagmire larger).
		const float AreaScale = Radius / FMath::Max(50.f, Data.AOERadius);
		ZoneVFX = MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Zone"), GetActorTransform(), FMath::Max(0.05f, Data.FX.PresetScale) * AreaScale,
			Root, NAME_None, InOwner);
	}
	if (IsNewZoneKind(Data.ZoneKind))
	{
		// These carry their own effects; a ground circle would look wrong on a moving wall of fire or a wave.
		Decal->SetVisibility(false);
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

	if (Data.ZoneMoveSpeed > 0.f)
	{
		MoveZone(DeltaSeconds);
	}

	if (Data.ZoneKind == EMTZoneKind::Burst)
	{
		if (!bBurstDone)
		{
			ApplyBurst();
		}
		if (Age >= Duration)
		{
			Expire();
		}
		return;
	}

	if (Data.ZoneKind == EMTZoneKind::LineEruptions)
	{
		if (Age >= NextEruptionTime && EruptionsDone < FMath::Max(1, Data.PulseCount))
		{
			EruptLine();
			++EruptionsDone;
			NextEruptionTime = Age + Data.PulseInterval;
		}
		if (EruptionsDone >= FMath::Max(1, Data.PulseCount) && Age >= NextEruptionTime + 0.5f)
		{
			Expire();
		}
		return;
	}

	if (Data.ZoneKind == EMTZoneKind::Vortex && OwnerCharacter.IsValid())
	{
		ApplyVortex(MTCombat::GetHostilesInRadius(OwnerCharacter.Get(), GetActorLocation(), Radius), DeltaSeconds);
	}

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
	case EMTZoneKind::DamageField: ApplyDamageField(Hostiles); break;
	case EMTZoneKind::Vortex: ApplyDamageField(Hostiles); break;
	case EMTZoneKind::Wave: ApplyWave(Hostiles); break;
	default: break;
	}
}

void AMTZoneActor::HitTarget(AMTCharacterBase* Target, const FVector& From, float DamageScale)
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	if (!Owner || !Target)
	{
		return;
	}
	FMTDamageSpec Spec;
	Spec.Damage = Data.Damage * DamageScale;
	Spec.Stagger = Data.Stagger * DamageScale;
	Spec.Knockback = Data.Knockback;
	Spec.Element = Data.Element;
	Spec.bIsMagic = Data.Element != EMTElement::None;
	Spec.SourceAbility = Data.AbilityID;
	Spec.Instigator = Owner;
	Spec.HitLocation = Target->GetActorLocation();
	FVector Dir = (Target->GetActorLocation() - From).GetSafeNormal2D();
	if (Data.ZoneKind == EMTZoneKind::Wave || Dir.IsNearlyZero())
	{
		Dir = MoveDirection;
	}
	Spec.HitDirection = Dir;
	MTCombat::ApplyElementInteractions(Spec, Target);
	const FMTDamageResult Result = Target->ReceiveCombatHit(Spec);
	if (Data.Element == EMTElement::Fire && Result.DamageDealt > 0.f)
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

void AMTZoneActor::ApplyDamageField(const TArray<AMTCharacterBase*>& Targets)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Interval = FMath::Max(0.1f, Data.PulseInterval);
	for (AMTCharacterBase* Target : Targets)
	{
		float& Last = LastHitTime.FindOrAdd(Target, -1000.f);
		if (Now - Last >= Interval)
		{
			Last = Now;
			HitTarget(Target, GetActorLocation());
		}
	}
}

void AMTZoneActor::ApplyBurst()
{
	bBurstDone = true;
	if (AMTCharacterBase* Owner = OwnerCharacter.Get())
	{
		for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, GetActorLocation(), Radius))
		{
			HitTarget(Target, GetActorLocation());
		}
	}
	MTCombat::PlaySound(this, Data.FX.ImpactSound, GetActorLocation());
	MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Impact, TEXT("Impact"), GetActorTransform(), Radius / FMath::Max(50.f, Data.AOERadius), nullptr, NAME_None, OwnerCharacter.Get());
}

void AMTZoneActor::ApplyVortex(const TArray<AMTCharacterBase*>& Targets, float DeltaSeconds)
{
	// Pull toward the eye and swirl around it; light enemies are lifted off their feet near the centre.
	const FVector Center = GetActorLocation();
	for (AMTCharacterBase* Target : Targets)
	{
		UCharacterMovementComponent* Move = Target->GetCharacterMovement();
		UMTAttributeComponent* Attr = Target->GetAttributes();
		if (!Move || !Attr || Attr->bCrowdControlImmune)
		{
			continue;
		}
		const float Strength = Attr->bIsWeak ? 1.f : 0.45f;
		const FVector In = (Center - Target->GetActorLocation()).GetSafeNormal2D();
		const FVector Around = FVector::CrossProduct(FVector::UpVector, In);
		Move->Velocity += (In * 1100.f + Around * 700.f) * Strength * DeltaSeconds;
		if (Attr->bIsWeak && FVector::Dist2D(Center, Target->GetActorLocation()) < Radius * 0.45f && Move->IsMovingOnGround())
		{
			Target->LaunchCharacter(FVector(0.f, 0.f, 420.f), false, true);
		}
	}
}

void AMTZoneActor::ApplyWave(const TArray<AMTCharacterBase*>& Targets)
{
	for (AMTCharacterBase* Target : Targets)
	{
		if (SweptOnce.Contains(Target))
		{
			continue;
		}
		SweptOnce.Add(Target);
		HitTarget(Target, GetActorLocation());
		UMTAttributeComponent* Attr = Target->GetAttributes();
		if (Attr && !Attr->bCrowdControlImmune)
		{
			Target->LaunchCharacter(MoveDirection * FMath::Max(500.f, Data.ZoneMoveSpeed * 0.9f) + FVector(0.f, 0.f, 260.f), true, true);
		}
	}
}

void AMTZoneActor::MoveZone(float DeltaSeconds)
{
	// A tornado drifts toward the caster's target; walls of fire and waves roll straight on.
	if (Data.ZoneKind == EMTZoneKind::Vortex && OwnerCharacter.IsValid())
	{
		if (const AActor* Target = OwnerCharacter->GetLockTarget())
		{
			const FVector To = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (!To.IsNearlyZero())
			{
				MoveDirection = FMath::VInterpNormalRotationTo(MoveDirection, To, DeltaSeconds, 70.f).GetSafeNormal2D();
			}
		}
	}
	const FVector Previous = GetActorLocation();
	const FVector Next = GroundAt(GetWorld(), Previous + MoveDirection * Data.ZoneMoveSpeed * DeltaSeconds);
	SetActorLocationAndRotation(Next, MoveDirection.Rotation());
	TrailDistance += FVector::Dist2D(Previous, Next);
	if (TrailDistance >= 240.f)
	{
		TrailDistance = 0.f;
		MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Trail"), FTransform(MoveDirection.Rotation(), Next), 1.f, nullptr, NAME_None, OwnerCharacter.Get());
	}
}

void AMTZoneActor::EruptLine()
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	if (!Owner)
	{
		return;
	}
	// Spikes march from the caster to the aim point, one after another.
	const int32 Count = FMath::Max(1, Data.PulseCount);
	const float Alpha = (EruptionsDone + 1.f) / Count;
	const FVector Point = GroundAt(GetWorld(), FMath::Lerp(LineStart, GetActorLocation(), Alpha));
	const float BurstRadius = Data.InnerRadius > 0.f ? Data.InnerRadius : 150.f;
	MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Impact, TEXT("Eruption"), FTransform(MoveDirection.Rotation(), Point), 1.f, nullptr, NAME_None, Owner);
	for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, Point, BurstRadius))
	{
		if (!SweptOnce.Contains(Target))
		{
			SweptOnce.Add(Target);
			HitTarget(Target, Point);
		}
	}
	MTCombat::PlaySound(this, Data.FX.ImpactSound, Point);
}

void AMTZoneActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MTCombat::StopSpellFX(ZoneVFX.Get());
	ZoneVFX.Reset();
	Super::EndPlay(EndPlayReason);
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
			const FMTDamageResult Result = Target->ReceiveCombatHit(Chill);
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
	if (!bVortexSpawned)
	{
		bVortexSpawned = true;
		MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Vortex"), FTransform(GroundAt(GetWorld(), GetActorLocation())), Radius / FMath::Max(50.f, Data.AOERadius),
			nullptr, NAME_None, Owner);
	}
	// Several eruptions scattered inside the circle, each with its own small blast radius.
	const int32 Bursts = 3;
	const float BurstRadius = FMath::Max(90.f, Radius * 0.38f);
	FRandomStream Stream(GetUniqueID() + EruptionsDone * 7919);
	TSet<AMTCharacterBase*> HitThisPulse;
	for (int32 i = 0; i < Bursts; ++i)
	{
		const float Angle = Stream.FRandRange(0.f, 2.f * PI);
		const float Dist = Radius * FMath::Sqrt(Stream.FRand()) * 0.75f;
		const FVector Point = GroundAt(GetWorld(), GetActorLocation() + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f));
		MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Impact, TEXT("Eruption"), FTransform(Point), BurstRadius / 150.f, nullptr, NAME_None, Owner);
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
			const FMTDamageResult Result = Target->ReceiveCombatHit(Spec);
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
	MTCombat::StopSpellFX(ZoneVFX.Get());
	ZoneVFX.Reset();
	MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Dissipation, TEXT("Dissipation"), GetActorTransform(), Radius / 400.f, nullptr, NAME_None, OwnerCharacter.Get());
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

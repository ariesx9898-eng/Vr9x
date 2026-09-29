#include "Combat/MTZoneActor.h"
#include "CollisionQueryParams.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "VFX/MTSpellVFX.h"
#include "Combat/MTCombatStatics.h"
#include "Combat/MTTelegraphSubsystem.h"
#include "Combat/MTProjectile.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbilityComponent.h"
#include "Core/MTGameplayTags.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
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
			|| Kind == EMTZoneKind::Vortex || Kind == EMTZoneKind::Wave || Kind == EMTZoneKind::Arc;
	}

	/** Feet of a character (bottom of its capsule). */
	FVector FeetOfCharacter(const AMTCharacterBase* Character)
	{
		return Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
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
	SizeScale = FMath::Max(0.05f, AreaMultiplier);
	Radius = FMath::Max(50.f, Data.AOERadius * SizeScale);
	BaseRadius = Radius;
	InnerRadius = Data.InnerRadius * SizeScale;
	NextEruptionTime = Data.ActivationDelay;
	NextBurnTime = Data.ActivationDelay;
	LifeDuration = Data.Duration > 0.f ? Data.Duration : 3.f;
	CastOrigin = InOwner ? GroundAt(GetWorld(), InOwner->GetActorLocation()) : GetActorLocation();

	if (bAttachToOwner && InOwner)
	{
		AttachToActor(InOwner, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	else if (InOwner)
	{
		// Direction of the cast (caster to aim point, horizontal). Waves start just in front of the caster and roll that
		// way; arcs and line eruptions start at the caster; a burst goes off around the caster; a tornado forms at the
		// aim point and drifts.
		FVector ToAim = GetActorLocation() - InOwner->GetActorLocation();
		ToAim.Z = 0.f;
		MoveDirection = ToAim.IsNearlyZero() ? InOwner->GetActorForwardVector().GetSafeNormal2D() : ToAim.GetSafeNormal();
		if (Data.ZoneKind == EMTZoneKind::Arc)
		{
			SetActorLocation(CastOrigin);
		}
		else if (Data.ZoneMoveSpeed > 0.f && Data.ZoneKind != EMTZoneKind::Vortex)
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
	ArcRadius = Data.GetParam(TEXT("StartRadius"), 150.f) * SizeScale;
	NextScorchRadius = ArcRadius + 400.f;

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
	else if (Data.ZoneKind != EMTZoneKind::Arc)
	{
		// Runtime zone effect, authored at the row's AOERadius and sized with the zone (charge, awakening).
		const float AreaScale = Radius / FMath::Max(50.f, Data.AOERadius);
		ZoneVFX = MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Zone"), GetActorTransform(), FMath::Max(0.05f, Data.FX.PresetScale) * AreaScale,
			Root, NAME_None, InOwner);
	}
	if (IsNewZoneKind(Data.ZoneKind) || (ZoneVFX.IsValid() && Data.ZoneKind != EMTZoneKind::Aura))
	{
		// These carry their own ground marks (mud that transforms, fire circles, waves): a flat circle would fight them.
		Decal->SetVisibility(false);
	}
	MTCombat::SpawnFX(this, Data.FX.Formation, GetActorLocation(), FRotator::ZeroRotator, Radius / 400.f);
	// The zone's own voice where it appears (the ground turning to mud, the circles igniting, the wave rising); line
	// eruptions keep the accent for their final spike.
	const bool bZoneAccent = Data.ZoneKind != EMTZoneKind::LineEruptions && !Data.FX.AccentSound.IsNull();
	MTCombat::PlaySound(this, bZoneAccent ? Data.FX.AccentSound : Data.FX.CastSound, GetActorLocation());

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

	const float Duration = LifeDuration;

	if (Data.ZoneKind == EMTZoneKind::Arc)
	{
		ApplyArc(DeltaSeconds);
		if (Age >= Duration)
		{
			Expire();
		}
		return;
	}

	if (Data.ZoneMoveSpeed > 0.f)
	{
		MoveZone(DeltaSeconds);
		if (bExpired)
		{
			return; // a wave broke on a wall or ran its course
		}
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
		const int32 Count = GetPulseLimit();
		if (Age >= NextEruptionTime && EruptionsDone < Count)
		{
			EruptLine();
			++EruptionsDone;
			NextEruptionTime = Age + Data.PulseInterval;
		}
		// Spikes crumble back into the ground a while after they rose.
		const float CrumbleAfter = Data.GetParam(TEXT("CrumbleAfter"), 1.2f);
		for (int32 i = 0; i < SpikeSpots.Num(); ++i)
		{
			if (!SpikeCrumbled[i] && Age >= SpikeTimes[i] + CrumbleAfter)
			{
				SpikeCrumbled[i] = true;
				const float CrumbleScale = SpikeIsFinal[i] ? Data.GetParam(TEXT("FinalRadius"), 300.f) / FMath::Max(1.f, InnerRadius > 0.f ? InnerRadius : 150.f) : 1.f;
				MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Crumble"), FTransform(MoveDirection.Rotation(), SpikeSpots[i]), CrumbleScale, nullptr, NAME_None,
					OwnerCharacter.Get());
			}
		}
		if (EruptionsDone >= Count && Age >= NextEruptionTime + FMath::Max(0.5f, CrumbleAfter + 0.1f))
		{
			Expire();
		}
		return;
	}

	if (Data.ZoneKind == EMTZoneKind::Vortex)
	{
		UpdateVortex(DeltaSeconds);
	}

	if (Data.ZoneKind == EMTZoneKind::Wave && Data.Params.Contains(TEXT("Band")))
	{
		CarryWave(DeltaSeconds);
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
		const int32 Count = GetPulseLimit();
		if (Age >= NextEruptionTime && EruptionsDone < Count)
		{
			Erupt();
			++EruptionsDone;
			NextEruptionTime = Age + Data.PulseInterval;
		}
		if (Age >= NextBurnTime && EruptionsDone < Count + 1 && Data.GetParam(TEXT("AreaBurnDps"), 0.f) > 0.f)
		{
			NextBurnTime = Age + 0.5f;
			ApplyAreaBurn();
		}
		if (EruptionsDone >= Count && Age >= NextEruptionTime)
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
	// The upgraded kinds (and Quagmire) hit a little beyond what is drawn: forgiving, never invisible.
	const bool bForgiving = IsNewZoneKind(Data.ZoneKind) || Data.ZoneKind == EMTZoneKind::Mire;
	const TArray<AMTCharacterBase*> Hostiles = MTCombat::GetHostilesInRadius(Owner, GetActorLocation(), bForgiving ? Data.HitRadius(Radius) : Radius);
	switch (Data.ZoneKind)
	{
	case EMTZoneKind::Mire: ApplyMire(Hostiles); break;
	case EMTZoneKind::Freeze: ApplyFreeze(Hostiles); break;
	case EMTZoneKind::Storm: ApplyStorm(); break;
	case EMTZoneKind::Aura: ApplyAura(Hostiles); break;
	case EMTZoneKind::DamageField: ApplyDamageField(Hostiles); break;
	case EMTZoneKind::Vortex: ApplyDamageField(Hostiles); break;
	case EMTZoneKind::Wave:
		if (!Data.Params.Contains(TEXT("Band")))
		{
			ApplyWave(Hostiles); // older rows: one push per target
		}
		break;
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
	FVector Dir = (Target->GetActorLocation() - From).GetSafeNormal2D();
	if (Data.ZoneKind == EMTZoneKind::Wave || Dir.IsNearlyZero())
	{
		Dir = MoveDirection;
	}
	FMTDamageSpec Spec = MTCombat::MakeAbilityHit(Data, Owner, Target->GetActorLocation(), Dir, DamageScale * Strength);
	Spec.Stagger = Data.Stagger * DamageScale * Strength;
	HitTargetWith(Target, Spec);
}

void AMTZoneActor::HitTargetWith(AMTCharacterBase* Target, FMTDamageSpec Spec)
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	if (!Owner || !Target || !Target->IsAlive())
	{
		return;
	}
	MTCombat::ApplyElementInteractions(Spec, Target);
	const FMTDamageResult Result = Target->ReceiveCombatHit(Spec);
	if (Result.DamageDealt > 0.f)
	{
		MTCombat::ApplyBurn(Data, Target, Owner, true);
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
		for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, GetActorLocation(), Data.HitRadius(Radius)))
		{
			HitTarget(Target, GetActorLocation());
		}
		// A counter move: a moment of invulnerability, and light spells in flight are thrown back out.
		const float Invulnerable = Data.GetParam(TEXT("Invulnerable"), 0.f);
		if (Invulnerable > 0.f && Owner->GetAttributes())
		{
			Owner->GetAttributes()->SetInvulnerableFor(Invulnerable);
		}
		const float DeflectRadius = Data.GetParam(TEXT("DeflectRadius"), 0.f);
		UMTTelegraphSubsystem* Telegraphs = DeflectRadius > 0.f ? UMTTelegraphSubsystem::Get(this) : nullptr;
		if (Telegraphs)
		{
			for (AMTProjectile* Spell : Telegraphs->GetSpellsNear(GetActorLocation(), DeflectRadius, Owner))
			{
				if (Spell && Spell->IsDeflectable())
				{
					FVector Away = (Spell->GetActorLocation() - GetActorLocation()).GetSafeNormal();
					if (Away.IsNearlyZero())
					{
						Away = -Spell->GetProjectileVelocity().GetSafeNormal();
					}
					Spell->Deflect(Away);
				}
			}
		}
	}
	MTCombat::PlaySound(this, Data.FX.ImpactSound, GetActorLocation());
	MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Impact, TEXT("Impact"), GetActorTransform(), Radius / FMath::Max(50.f, Data.AOERadius), nullptr, NAME_None, OwnerCharacter.Get());
}

void AMTZoneActor::PushTarget(AMTCharacterBase* Target, const FVector& Velocity, bool bOverride, TMap<TWeakObjectPtr<AMTCharacterBase>, float>& Until)
{
	UCharacterMovementComponent* Move = Target ? Target->GetCharacterMovement() : nullptr;
	const UWorld* World = GetWorld();
	if (!Move || !World || Move->MovementMode == MOVE_None)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	float& Next = Until.FindOrAdd(Target, -1.f);
	if (Now < Next)
	{
		return;
	}
	// Short root-motion pushes: movement code keeps colliding and following the floor, and nothing accumulates.
	const float PushSeconds = 0.12f;
	Next = Now + PushSeconds;
	TSharedPtr<FRootMotionSource_ConstantForce> Push = MakeShared<FRootMotionSource_ConstantForce>();
	Push->InstanceName = bOverride ? FName(TEXT("MTZoneCarry")) : FName(TEXT("MTZonePull"));
	Push->AccumulateMode = bOverride ? ERootMotionAccumulateMode::Override : ERootMotionAccumulateMode::Additive;
	Push->Priority = static_cast<uint16>(bOverride ? 600 : 50);
	Push->Force = Velocity;
	Push->Duration = PushSeconds;
	Push->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
	Move->ApplyRootMotionSource(Push);
}

void AMTZoneActor::UpdateVortex(float DeltaSeconds)
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	const UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}
	// It grows as it lives (Tornado: VR 360 -> 560); a destabilised one is smaller.
	const float Life01 = FMath::Clamp(Age / FMath::Max(0.1f, LifeDuration), 0.f, 1.f);
	const float GrowTo = Data.GetParam(TEXT("GrowTo"), Data.AOERadius) * SizeScale;
	Radius = FMath::Max(50.f, FMath::Lerp(BaseRadius, GrowTo, Life01) * VortexShrink);
	if (AMTSpellVFX* Funnel = ZoneVFX.Get())
	{
		Funnel->SetEffectScale(FMath::Max(0.05f, Data.FX.PresetScale) * Radius / FMath::Max(50.f, Data.AOERadius));
	}
	const float FormTime = Data.GetParam(TEXT("FormTime"), 0.f);
	const float Formed = FormTime > 0.f ? FMath::Clamp(Age / FormTime, 0.f, 1.f) : 1.f;
	const float CoreRadius = Data.HitRadius(Radius);
	const float PullRadius = CoreRadius * FMath::Max(1.f, Data.GetParam(TEXT("PullScale"), 1.f));
	const float PullSpeed = Data.GetParam(TEXT("PullSpeed"), 650.f) * Strength * Formed;
	const float HeavyPullSpeed = Data.GetParam(TEXT("HeavyPullSpeed"), 250.f) * Strength * Formed;
	const float LiftTime = Data.GetParam(TEXT("LiftTime"), 0.f);
	const float Now = World->GetTimeSeconds();
	const FVector Center = GetActorLocation();

	for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, Center, PullRadius))
	{
		if (Lifted.Contains(Target))
		{
			continue;
		}
		UMTAttributeComponent* Attr = Target->GetAttributes();
		if (!Attr)
		{
			continue;
		}
		const bool bHeavy = Attr->bCrowdControlImmune;
		const float Dist = FVector::Dist2D(Center, Target->GetActorLocation());
		// Small enemies caught in the core are lifted into the funnel.
		const float* Cooldown = LiftCooldownUntil.Find(Target);
		if (LiftTime > 0.f && Formed >= 1.f && Attr->bIsWeak && !bHeavy && Dist <= CoreRadius * 0.6f && (!Cooldown || Now >= *Cooldown))
		{
			StartLift(Target);
			continue;
		}
		// Everything else is dragged in and around (a spiral, not a straight line); heavy targets resist.
		const FVector In = (Center - Target->GetActorLocation()).GetSafeNormal2D();
		const FVector Around = FVector::CrossProduct(FVector::UpVector, In);
		const float Speed = bHeavy ? HeavyPullSpeed : PullSpeed;
		if (Speed > 0.f && Dist > 40.f)
		{
			PushTarget(Target, (In + Around * 0.45f).GetSafeNormal() * Speed, false, PullUntil);
		}
		if (Dist <= CoreRadius)
		{
			FMTStatusEffect Drag;
			Drag.Id = TEXT("TornadoDrag");
			Drag.Duration = 0.3f;
			Drag.Movement.SpeedMultiplier = FMath::Lerp(1.f, Data.GetParam(TEXT("HeavySlow"), 0.5f), Strength);
			Drag.GrantedTags.AddTag(MTTags::State_InWindField);
			Attr->AddStatusEffect(Drag);
		}
	}
	UpdateLifts(DeltaSeconds);

	// The funnel bends weak spells passing through it.
	if (UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this))
	{
		for (AMTProjectile* Spell : Telegraphs->GetSpellsNear(Center, CoreRadius, Owner))
		{
			if (Spell && Spell->IsDeflectable())
			{
				const FVector Vel = Spell->GetProjectileVelocity().GetSafeNormal();
				const FVector Out = (Spell->GetActorLocation() - Center).GetSafeNormal2D();
				Spell->Deflect((Vel + FVector::CrossProduct(FVector::UpVector, Out) * 0.9f + FVector(0.f, 0.f, 0.35f)).GetSafeNormal());
			}
		}
	}
}

void AMTZoneActor::StartLift(AMTCharacterBase* Target)
{
	UCharacterMovementComponent* Move = Target ? Target->GetCharacterMovement() : nullptr;
	const UWorld* World = GetWorld();
	if (!Move || !World)
	{
		return;
	}
	const FVector Rel = Target->GetActorLocation() - GetActorLocation();
	FLiftState State;
	State.Start = World->GetTimeSeconds();
	State.Angle = FMath::Atan2(Rel.Y, Rel.X);
	State.OrbitRadius = FMath::Max(60.f, Rel.Size2D());
	State.BaseZ = Target->GetActorLocation().Z;
	// Kinematic while in the funnel: no walking, no casting.
	if (UMTAbilityComponent* TargetAbilities = Target->GetAbilities())
	{
		TargetAbilities->CancelAll();
	}
	Move->StopMovementImmediately();
	Move->DisableMovement();
	State.FX = MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Lift"), Target->GetActorTransform(), 1.f, Target->GetRootComponent(), NAME_None,
		OwnerCharacter.Get());
	Lifted.Add(Target, State);
}

void AMTZoneActor::UpdateLifts(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float LiftTime = FMath::Max(0.1f, Data.GetParam(TEXT("LiftTime"), 1.2f));
	const float LiftHeight = Data.GetParam(TEXT("LiftHeight"), 320.f);
	const float OrbitDegPerSecond = Data.GetParam(TEXT("OrbitSpeed"), 300.f);
	const FVector Center = GetActorLocation();
	for (auto It = Lifted.CreateIterator(); It; ++It)
	{
		AMTCharacterBase* Target = It.Key().Get();
		FLiftState& State = It.Value();
		if (!Target || !Target->IsAlive())
		{
			MTCombat::StopSpellFX(State.FX.Get());
			It.RemoveCurrent();
			continue;
		}
		const float T = (Now - State.Start) / LiftTime;
		if (T >= 1.f)
		{
			// Dropped: flung out of the funnel.
			if (UCharacterMovementComponent* Move = Target->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Falling);
			}
			const FVector Out = (Target->GetActorLocation() - Center).GetSafeNormal2D();
			Target->LaunchCharacter(Out * 450.f + FVector(0.f, 0.f, 120.f), true, true);
			MTCombat::StopSpellFX(State.FX.Get());
			LiftCooldownUntil.Add(Target, Now + 1.5f);
			It.RemoveCurrent();
			continue;
		}
		State.Angle += FMath::DegreesToRadians(OrbitDegPerSecond) * DeltaSeconds;
		const float Orbit = FMath::Lerp(State.OrbitRadius, Radius * 0.5f, FMath::Min(1.f, T * 3.f));
		const float Height = LiftHeight * FMath::Sin(FMath::Min(1.f, T * 1.6f) * HALF_PI);
		const FVector Wanted(Center.X + FMath::Cos(State.Angle) * Orbit, Center.Y + FMath::Sin(State.Angle) * Orbit, State.BaseZ + Height);
		Target->SetActorLocation(Wanted, true);
	}
}

void AMTZoneActor::ReleaseAllLifts()
{
	for (auto It = Lifted.CreateIterator(); It; ++It)
	{
		if (AMTCharacterBase* Target = It.Key().Get())
		{
			if (UCharacterMovementComponent* Move = Target->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Falling);
			}
		}
		MTCombat::StopSpellFX(It.Value().FX.Get());
	}
	Lifted.Reset();
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

void AMTZoneActor::CarryWave(float DeltaSeconds)
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	if (!Owner)
	{
		return;
	}
	// The front: full width (forgiving), a band a few metres deep around the crest.
	const float HalfWidth = Data.HitRadius(Data.GetParam(TEXT("Width"), Radius * 2.f) * 0.5f * SizeScale);
	const float Band = Data.GetParam(TEXT("Band"), 300.f);
	const float CarrySpeed = Data.GetParam(TEXT("CarrySpeed"), Data.ZoneMoveSpeed * 0.85f);
	const FVector Center = GetActorLocation();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, MoveDirection);
	const float Query = FMath::Sqrt(FMath::Square(HalfWidth) + FMath::Square(Band));
	for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, Center, Query))
	{
		const FVector Rel = Target->GetActorLocation() - Center;
		const float Along = FVector::DotProduct(Rel, MoveDirection);
		const float Across = FVector::DotProduct(Rel, Side);
		if (FMath::Abs(Across) > HalfWidth || Along < -Band || Along > Band * 0.5f)
		{
			continue;
		}
		if (!SweptOnce.Contains(Target))
		{
			// First contact: the wave's weight lands (the throw comes when it breaks).
			SweptOnce.Add(Target);
			FMTDamageSpec Spec = MTCombat::MakeAbilityHit(Data, Owner, Target->GetActorLocation(), MoveDirection, Strength);
			Spec.Knockback = 0.f;
			Spec.Launch = 0.f;
			HitTargetWith(Target, Spec);
			MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Splash"), FTransform(MoveDirection.Rotation(), FeetOfCharacter(Target)), 1.f, nullptr, NAME_None, Owner);
		}
		const UMTAttributeComponent* Attr = Target->GetAttributes();
		if (Attr && !Attr->bCrowdControlImmune && CarrySpeed > 0.f)
		{
			Carried.Add(Target);
			PushTarget(Target, MoveDirection * CarrySpeed * Strength, true, CarryUntil);
		}
	}
}

void AMTZoneActor::ReleaseCarried()
{
	for (const TWeakObjectPtr<AMTCharacterBase>& Weak : Carried)
	{
		AMTCharacterBase* Target = Weak.Get();
		if (Target && Target->IsAlive())
		{
			// The wave breaks and throws what it carried.
			Target->LaunchCharacter(MoveDirection * Data.Knockback * Strength + FVector(0.f, 0.f, Data.Launch * Strength), true, true);
		}
	}
	Carried.Reset();
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
	if (Data.ZoneKind == EMTZoneKind::Wave)
	{
		// A wave that meets a wall breaks against it.
		FHitResult Block;
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTWaveBlock), false, this);
		const FVector Crest = Previous + FVector(0.f, 0.f, 160.f);
		if (GetWorld()->LineTraceSingleByObjectType(Block, Crest, Crest + MoveDirection * 140.f, Objects, Params) && Block.ImpactNormal.Z < 0.5f)
		{
			MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Splash"), FTransform((-MoveDirection).Rotation(), Block.ImpactPoint), 2.f, nullptr, NAME_None,
				OwnerCharacter.Get());
			Expire();
			return;
		}
	}
	const FVector Next = GroundAt(GetWorld(), Previous + MoveDirection * Data.ZoneMoveSpeed * DeltaSeconds);
	SetActorLocationAndRotation(Next, MoveDirection.Rotation());
	const float Step = FVector::Dist2D(Previous, Next);
	Travelled += Step;
	TrailDistance += Step;
	if (TrailDistance >= Data.GetParam(TEXT("TrailSpacing"), 240.f))
	{
		TrailDistance = 0.f;
		MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Trail"), FTransform(MoveDirection.Rotation(), Next), 1.f, nullptr, NAME_None, OwnerCharacter.Get());
	}
	const float MaxDistance = Data.GetParam(TEXT("Distance"), 0.f);
	if (MaxDistance > 0.f && Travelled >= MaxDistance)
	{
		Expire();
	}
}

void AMTZoneActor::ApplyArc(float DeltaSeconds)
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	const float TravelTime = FMath::Max(0.05f, Data.GetParam(TEXT("TravelTime"), 0.9f));
	const float StartRadius = Data.GetParam(TEXT("StartRadius"), 150.f) * SizeScale;
	const float EndRadius = Data.GetParam(TEXT("EndRadius"), Data.AOERadius) * SizeScale;
	const float HalfAngle = 0.5f * FMath::Clamp(Data.GetParam(TEXT("ArcDegrees"), 120.f), 10.f, 360.f);
	const float Band = Data.GetParam(TEXT("Band"), 180.f);
	const bool bRolling = Age <= TravelTime;
	const float T = FMath::Clamp(Age / TravelTime, 0.f, 1.f);
	// Fast out of the hand, slowing as it spreads.
	ArcRadius = FMath::Lerp(StartRadius, EndRadius, 1.f - FMath::Square(1.f - T));
	Radius = ArcRadius;

	const int32 SegmentCount = FMath::Clamp(FMath::RoundToInt(Data.GetParam(TEXT("Segments"), 11.f)), 2, 24);
	if (bRolling && ArcSegments.Num() == 0)
	{
		for (int32 i = 0; i < SegmentCount; ++i)
		{
			ArcSegments.Add(MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Segment"), FTransform(MoveDirection.Rotation(), CastOrigin),
				FMath::Max(0.05f, Data.FX.PresetScale), nullptr, NAME_None, Owner));
		}
	}
	if (!bRolling && !bArcStopped)
	{
		bArcStopped = true;
		for (const TWeakObjectPtr<AMTSpellVFX>& Segment : ArcSegments)
		{
			MTCombat::StopSpellFX(Segment.Get());
		}
	}
	if (bRolling)
	{
		for (int32 i = 0; i < ArcSegments.Num(); ++i)
		{
			AMTSpellVFX* Segment = ArcSegments[i].Get();
			if (!Segment)
			{
				continue;
			}
			const float Angle = FMath::Lerp(-HalfAngle, HalfAngle, ArcSegments.Num() > 1 ? i / float(ArcSegments.Num() - 1) : 0.5f);
			const FVector Out = MoveDirection.RotateAngleAxis(Angle, FVector::UpVector);
			Segment->SetActorLocationAndRotation(GroundAt(GetWorld(), CastOrigin + Out * ArcRadius), Out.Rotation());
		}
		// Scorched ground along the path, a row every 4 m on alternate segments.
		if (ArcRadius >= NextScorchRadius)
		{
			NextScorchRadius += 400.f;
			for (int32 i = 0; i < SegmentCount; i += 2)
			{
				const float Angle = FMath::Lerp(-HalfAngle, HalfAngle, i / float(SegmentCount - 1));
				const FVector Out = MoveDirection.RotateAngleAxis(Angle, FVector::UpVector);
				MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Trail"), FTransform(Out.Rotation(), GroundAt(GetWorld(), CastOrigin + Out * (ArcRadius - 60.f))),
					1.f, nullptr, NAME_None, Owner);
			}
		}
	}

	// Each enemy is hit once, when the front reaches it (forgiving band and angle).
	if (Owner && bRolling)
	{
		const float Reach = ArcRadius + Data.HitRadius(Band * 0.5f);
		const float HitHalfAngle = FMath::Min(180.f, Data.HitRadius(HalfAngle));
		for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, CastOrigin, Reach))
		{
			if (SweptOnce.Contains(Target))
			{
				continue;
			}
			const FVector To = (Target->GetActorLocation() - CastOrigin).GetSafeNormal2D();
			const float Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(To, MoveDirection), -1.f, 1.f)));
			if (!To.IsNearlyZero() && Off > HitHalfAngle)
			{
				continue;
			}
			SweptOnce.Add(Target);
			HitTarget(Target, CastOrigin);
		}
	}
}

void AMTZoneActor::EruptLine()
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	if (!Owner)
	{
		return;
	}
	const int32 Count = GetPulseLimit();
	const int32 Index = EruptionsDone;
	// Fixed spacing along the aim (Spike1..3, then the enormous Final), or the older even spacing to the aim point.
	const bool bFixed = Data.Params.Contains(TEXT("Final"));
	const bool bFinal = bFixed && Index >= Count - 1;
	FVector Point;
	if (bFixed)
	{
		static const TCHAR* const SpikeKeys[] = { TEXT("Spike1"), TEXT("Spike2"), TEXT("Spike3") };
		const float Distance = bFinal ? Data.GetParam(TEXT("Final"), 1300.f) : Data.GetParam(SpikeKeys[FMath::Min(Index, 2)], 320.f * (Index + 1));
		Point = GroundAt(GetWorld(), CastOrigin + MoveDirection * Distance);
	}
	else
	{
		const float Alpha = (Index + 1.f) / Count;
		Point = GroundAt(GetWorld(), FMath::Lerp(LineStart, GetActorLocation(), Alpha));
	}
	const float SpikeRadius = InnerRadius > 0.f ? InnerRadius : 150.f;
	const float VisualRadius = bFinal ? Data.GetParam(TEXT("FinalRadius"), SpikeRadius * 2.f) * SizeScale : SpikeRadius;
	if (bFinal)
	{
		MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Final"), FTransform(MoveDirection.Rotation(), Point), 1.f, nullptr, NAME_None, Owner);
		MTCombat::PlaySound(this, Data.FX.AccentSound.IsNull() ? Data.FX.ImpactSound : Data.FX.AccentSound, Point);
	}
	else
	{
		MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Impact, TEXT("Eruption"), FTransform(MoveDirection.Rotation(), Point), 1.f, nullptr, NAME_None, Owner);
		MTCombat::PlaySound(this, Data.FX.ImpactSound, Point);
	}
	for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, Point, Data.HitRadius(VisualRadius)))
	{
		if (bFinal)
		{
			// The final lance throws everything on it into the air (heavy targets take 35%).
			FMTDamageSpec Spec = MTCombat::MakeAbilityHit(Data, Owner, Point, (Target->GetActorLocation() - Point).GetSafeNormal2D(), Strength);
			Spec.Damage = Data.GetParam(TEXT("FinalDamage"), Data.Damage * 2.f) * Strength;
			Spec.Stagger = Data.Stagger * 1.5f;
			Spec.Launch = Data.GetParam(TEXT("FinalLaunch"), Data.Launch);
			Spec.Knockback = Data.Knockback * 0.5f;
			HitTargetWith(Target, Spec);
		}
		else if (!SweptOnce.Contains(Target))
		{
			SweptOnce.Add(Target);
			HitTarget(Target, Point);
		}
	}
	SpikeSpots.Add(Point);
	SpikeTimes.Add(Age);
	SpikeIsFinal.Add(bFinal);
	SpikeCrumbled.Add(false);
}

void AMTZoneActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseAllLifts();
	for (const TWeakObjectPtr<AMTSpellVFX>& Segment : ArcSegments)
	{
		MTCombat::StopSpellFX(Segment.Get());
	}
	ArcSegments.Reset();
	MTCombat::StopSpellFX(ZoneVFX.Get());
	ZoneVFX.Reset();
	Super::EndPlay(EndPlayReason);
}

void AMTZoneActor::ApplyMire(const TArray<AMTCharacterBase*>& Targets)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	// Quagmire rows describe the mud (depth, transformation); older rows use their flat ZoneMovement.
	const bool bDepthModel = Data.Params.Contains(TEXT("SpeedLoss"));
	const float TransformTime = Data.GetParam(TEXT("TransformTime"), 0.f);
	const float Ramp = TransformTime > 0.f ? FMath::Clamp(Age / TransformTime, 0.f, 1.f) : 1.f;
	const float EdgeDepth = Data.GetParam(TEXT("EdgeDepth"), 1.f);
	for (AMTCharacterBase* Target : Targets)
	{
		UMTAttributeComponent* Attr = Target->GetAttributes();
		if (!Attr)
		{
			continue;
		}
		FMTStatusEffect Effect;
		Effect.Id = TEXT("Quagmire");
		Effect.Duration = ZonePulseInterval * 2.5f; // lingers briefly after leaving the mud
		Effect.GrantedTags.AddTag(MTTags::State_InQuagmire);
		if (!bDepthModel)
		{
			Effect.Movement = Data.ZoneMovement;
			Effect.Movement.bRooted = false; // Quagmire slows, never immobilises
			Attr->AddStatusEffect(Effect);
			continue;
		}
		// Deepest at the centre, still sticky at the edge; the mud only bites once the ground has liquefied.
		const float Edge01 = FMath::Clamp(FVector::Dist2D(Target->GetActorLocation(), GetActorLocation()) / FMath::Max(1.f, Radius), 0.f, 1.f);
		const float Depth = FMath::Lerp(1.f, EdgeDepth, FMath::SmoothStep(0.f, 1.f, Edge01)) * Ramp * Strength;
		const bool bHeavy = Attr->bCrowdControlImmune;
		const float Penalty = Depth * (bHeavy ? Data.GetParam(TEXT("HeavyPenalty"), 0.6f) : 1.f);
		Effect.Movement.SpeedMultiplier = FMath::Clamp(1.f - Data.GetParam(TEXT("SpeedLoss"), 0.72f) * Penalty, 0.1f, 1.f);
		Effect.Movement.AccelerationMultiplier = FMath::Clamp(1.f - Data.GetParam(TEXT("AccelLoss"), 0.5f) * Penalty, 0.1f, 1.f);
		Effect.Movement.JumpMultiplier = FMath::Clamp(1.f - Data.GetParam(TEXT("JumpLoss"), 0.7f) * Penalty, 0.1f, 1.f);
		Effect.Movement.DodgeDistanceMultiplier = FMath::Clamp(1.f - Data.GetParam(TEXT("DodgeLoss"), 0.6f) * Penalty, 0.1f, 1.f);
		Effect.Movement.bRooted = false;
		Attr->AddStatusEffect(Effect);

		// Bodies sink into it (visual), heavy ones less.
		Target->SetMudSink(Data.GetParam(TEXT("SinkDepth"), 30.f) * Depth * (bHeavy ? Data.GetParam(TEXT("HeavySink"), 0.35f) : 1.f), 1.5f);

		UCharacterMovementComponent* Move = Target->GetCharacterMovement();
		const float Speed = Move ? Move->Velocity.Size2D() : 0.f;
		if (!MireEntered.Contains(Target) && Ramp > 0.3f)
		{
			// Momentum dies on entry: a fast target visibly bogs down, with a splash.
			MireEntered.Add(Target);
			if (Move && Speed > 250.f)
			{
				const float Keep = 1.f - Data.GetParam(TEXT("MomentumLoss"), 0.6f) * Depth;
				Move->Velocity.X *= Keep;
				Move->Velocity.Y *= Keep;
				MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Splash"), FTransform(FeetOfCharacter(Target)), 1.f, nullptr, NAME_None, OwnerCharacter.Get());
			}
		}
		if (Speed > 60.f && Ramp >= 1.f)
		{
			// Ripples and displaced mud behind anything that moves.
			float& Last = LastRippleTime.FindOrAdd(Target, -1000.f);
			if (Now - Last >= 0.3f)
			{
				Last = Now;
				MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Ripple"), FTransform(Move->Velocity.Rotation(), FeetOfCharacter(Target)), 1.f, nullptr, NAME_None,
					OwnerCharacter.Get());
			}
		}
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
	// Several pillars per volley, each with its own blast radius; most of them go where the enemies are.
	const int32 Bursts = FMath::Clamp(FMath::RoundToInt(Data.GetParam(TEXT("PillarsPerPulse"), 3.f)), 1, 8);
	const int32 Aimed = FMath::Clamp(FMath::RoundToInt(Bursts * Data.GetParam(TEXT("EnemyBias"), 0.f)), 0, Bursts);
	const float BurstVisual = InnerRadius > 0.f ? InnerRadius : FMath::Max(90.f, Radius * 0.38f);
	const float BurstAuthored = Data.InnerRadius > 0.f ? Data.InnerRadius : 150.f;
	FRandomStream Stream(GetUniqueID() + EruptionsDone * 7919);
	const TArray<AMTCharacterBase*> Inside = Aimed > 0 ? MTCombat::GetHostilesInRadius(Owner, GetActorLocation(), Radius) : TArray<AMTCharacterBase*>();
	TSet<AMTCharacterBase*> HitThisPulse;
	for (int32 i = 0; i < Bursts; ++i)
	{
		FVector Point;
		if (i < Aimed && Inside.Num() > 0)
		{
			const AMTCharacterBase* Victim = Inside[Stream.RandRange(0, Inside.Num() - 1)];
			const float Jitter = Stream.FRandRange(0.f, 80.f);
			const float JitterAngle = Stream.FRandRange(0.f, 2.f * PI);
			Point = Victim->GetActorLocation() + FVector(FMath::Cos(JitterAngle) * Jitter, FMath::Sin(JitterAngle) * Jitter, 0.f);
		}
		else
		{
			const float Angle = Stream.FRandRange(0.f, 2.f * PI);
			const float Dist = Radius * FMath::Sqrt(Stream.FRand()) * 0.8f;
			Point = GetActorLocation() + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		}
		Point = GroundAt(GetWorld(), Point);
		MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Impact, TEXT("Eruption"), FTransform(Point), BurstVisual / BurstAuthored, nullptr, NAME_None, Owner);
		for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, Point, Data.HitRadius(BurstVisual)))
		{
			if (HitThisPulse.Contains(Target))
			{
				continue;
			}
			HitThisPulse.Add(Target);
			HitTarget(Target, Point);
		}
	}
	MTCombat::PlaySound(this, Data.FX.ImpactSound, GetActorLocation());
	if (DecalMID)
	{
		DecalMID->SetScalarParameterValue(TEXT("RingOnly"), 0.f);
	}
}

void AMTZoneActor::ApplyAreaBurn()
{
	AMTCharacterBase* Owner = OwnerCharacter.Get();
	if (!Owner)
	{
		return;
	}
	// The whole circle is burning ground: damage over time, no flinches.
	const float Dps = Data.GetParam(TEXT("AreaBurnDps"), 0.f) * Strength;
	for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, GetActorLocation(), Data.HitRadius(Radius)))
	{
		if (UMTAttributeComponent* Attr = Target->GetAttributes())
		{
			FMTStatusEffect Burn;
			Burn.Id = TEXT("InfernoGround");
			Burn.Duration = 0.75f;
			Burn.HealthPerSecond = -Dps;
			Burn.GrantedTags.AddTag(MTTags::State_Burning);
			Burn.Instigator = Owner;
			Attr->AddStatusEffect(Burn);
		}
	}
}

void AMTZoneActor::Destabilize(float TimeScale, float StrengthScale)
{
	if (bExpired)
	{
		return;
	}
	const float Remaining = FMath::Max(0.f, LifeDuration - Age);
	LifeDuration = Age + Remaining * FMath::Clamp(TimeScale, 0.f, 1.f);
	Strength *= FMath::Clamp(StrengthScale, 0.f, 1.f);
	const int32 Count = GetPulseLimit();
	if (EruptionsDone < Count)
	{
		PulseLimit = EruptionsDone + FMath::CeilToInt((Count - EruptionsDone) * FMath::Clamp(TimeScale, 0.f, 1.f));
	}
	if (Data.ZoneKind == EMTZoneKind::Vortex)
	{
		VortexShrink *= FMath::Clamp(StrengthScale, 0.1f, 1.f);
	}
	// The magic visibly loses its colour and hold.
	if (AMTSpellVFX* Loop = ZoneVFX.Get())
	{
		Loop->SetTint(FLinearColor(0.7f, 0.72f, 0.8f));
	}
}

void AMTZoneActor::UpdateDecalLook(float DeltaSeconds)
{
	if (!DecalMID)
	{
		return;
	}
	const float Duration = LifeDuration;
	// Fade in over 0.3 s (Quagmire: over its transformation), fade out over the last 0.6 s.
	const float FadeInTime = FMath::Max(0.3f, Data.GetParam(TEXT("TransformTime"), 0.3f));
	const float In = FMath::Clamp(Age / FadeInTime, 0.f, 1.f);
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
	ReleaseAllLifts();
	if (Data.ZoneKind == EMTZoneKind::Wave)
	{
		ReleaseCarried();
	}
	for (const TWeakObjectPtr<AMTSpellVFX>& Segment : ArcSegments)
	{
		MTCombat::StopSpellFX(Segment.Get());
	}
	MTCombat::StopSpellFX(ZoneVFX.Get());
	ZoneVFX.Reset();
	// Authored at the row's AOERadius (Quagmire drying at VR 900 grows with a charged quagmire).
	MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Dissipation, TEXT("Dissipation"), GetActorTransform(), Radius / FMath::Max(50.f, Data.AOERadius), nullptr, NAME_None,
		OwnerCharacter.Get());
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

#include "Abilities/MTAbilityBehaviors.h"
#include "VFX/MTSpellVFX.h"
#include "Abilities/MTAbilityComponent.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Combat/MTProjectile.h"
#include "Combat/MTZoneActor.h"
#include "Combat/MTEarthWall.h"
#include "Combat/MTCombatStatics.h"
#include "Combat/MTTelegraphSubsystem.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"

namespace
{
	/** Snap a point to the walkable ground below/above it. */
	bool TraceGround(const UWorld* World, const FVector& Point, FVector& OutGround, const AActor* Ignore)
	{
		if (!World)
		{
			return false;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTGroundTrace), false, Ignore);
		if (World->LineTraceSingleByChannel(Hit, Point + FVector(0.f, 0.f, 500.f), Point - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params))
		{
			OutGround = Hit.ImpactPoint;
			return true;
		}
		return false;
	}

	/** Moves a character to TargetLocation over Duration using a collision-aware root motion source. */
	uint16 ApplyMoveTo(ACharacter* Character, const FVector& TargetLocation, float Duration, FName Name)
	{
		UCharacterMovementComponent* CMC = Character ? Character->GetCharacterMovement() : nullptr;
		if (!CMC)
		{
			return 0;
		}
		TSharedPtr<FRootMotionSource_MoveToForce> MoveTo = MakeShared<FRootMotionSource_MoveToForce>();
		MoveTo->InstanceName = Name;
		MoveTo->AccumulateMode = ERootMotionAccumulateMode::Override;
		MoveTo->Priority = 900;
		MoveTo->StartLocation = Character->GetActorLocation();
		MoveTo->TargetLocation = TargetLocation;
		MoveTo->Duration = FMath::Max(0.05f, Duration);
		MoveTo->bRestrictSpeedToExpected = false;
		MoveTo->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
		MoveTo->FinishVelocityParams.ClampVelocity = 300.f;
		return CMC->ApplyRootMotionSource(MoveTo);
	}
}

// ---------------------------------------------------------------- Projectile

AMTProjectile* UMTAbility_Projectile::FireProjectile(AMTCharacterBase* Owner, const FMTAbilityData& Row, const FVector& From, const FVector& AimPoint, float ChargeAlpha)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FVector Direction = (AimPoint - From).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Owner->GetActorForwardVector();
	}
	if (Row.Motion == EMTProjectileMotion::Arc)
	{
		Direction = (Direction + FVector(0.f, 0.f, 0.25f)).GetSafeNormal();
	}

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMTProjectile* Projectile = World->SpawnActor<AMTProjectile>(AMTProjectile::StaticClass(), From, Direction.Rotation(), Params);
	if (Projectile)
	{
		const FMTStatModifier Mods = Owner->GetAttributes() ? Owner->GetAttributes()->GetStatModifier() : FMTStatModifier();
		Projectile->InitProjectile(Row, Owner, ChargeAlpha, Direction, Mods.ProjectileSpeedMultiplier, Mods.DamageMultiplier);
	}
	return Projectile;
}

void UMTAbility_Projectile::ExecuteAction()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	const float Charge = GetChargeAlpha();
	FireProjectile(Owner, Data, GetCastLocation(), GetAimPoint(), Charge);
	if (Data.FX.CameraShakeScale > 0.f)
	{
		PlaySubtleCameraShake(Data.FX.CameraShakeScale * (0.5f + 0.5f * Charge));
	}
}

// ---------------------------------------------------------------- Zone

FVector UMTAbility_Zone::ResolveGroundTarget() const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	const FVector Origin = Owner->GetActorLocation();
	FVector Target = GetAimPoint();
	FVector Offset = Target - Origin;
	Offset.Z = 0.f;
	if (Offset.Size() > Data.Range)
	{
		Target = Origin + Offset.GetSafeNormal() * Data.Range;
	}
	FVector Ground;
	if (TraceGround(Owner->GetWorld(), Target, Ground, Owner))
	{
		return Ground;
	}
	return FVector(Target.X, Target.Y, Origin.Z - Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

float UMTAbility_Zone::GetPendingRadius() const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	const float Area = (Owner && Owner->GetAttributes()) ? Owner->GetAttributes()->GetStatModifier().AreaMultiplier : 1.f;
	return Data.AOERadius * Area * FMath::Lerp(1.f, FMath::Max(0.1f, Data.ChargeRadiusScale), GetChargeAlpha());
}

void UMTAbility_Zone::TickAnticipation(float DeltaTime)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	// Earth Spikes: the aim line along the ground from the caster's feet. Quagmire: a faint ring spreading on the
	// target, growing with the charge. Zones without these phases spawn nothing.
	const bool bLine = Data.ZoneKind == EMTZoneKind::LineEruptions;
	FTransform Where;
	float ScaleNow = 1.f;
	if (bLine)
	{
		FVector Forward = (GetAimPoint() - Owner->GetActorLocation()).GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
		}
		Where = FTransform(Forward.Rotation(), MTCombat::GroundBelow(Owner, Owner->GetActorLocation()));
	}
	else
	{
		Where = FTransform(FRotator::ZeroRotator, ResolveGroundTarget());
		ScaleNow = GetPendingRadius() / FMath::Max(50.f, Data.AOERadius);
	}
	AMTSpellVFX* Telegraph = TelegraphVFX.Get();
	if (!Telegraph && !bTelegraphTried)
	{
		bTelegraphTried = true;
		TelegraphVFX = SpawnPhaseFX(bLine ? TEXT("Aim") : TEXT("Target"), Where, ScaleNow);
		Telegraph = TelegraphVFX.Get();
	}
	if (Telegraph)
	{
		Telegraph->SetActorLocationAndRotation(Where.GetLocation(), Where.GetRotation());
		if (!bLine)
		{
			Telegraph->SetEffectScale(ScaleNow * FMath::Max(0.05f, Data.FX.PresetScale));
		}
	}
}

void UMTAbility_Zone::OnEnded(bool bWasCancelled)
{
	MTCombat::StopSpellFX(TelegraphVFX.Get());
	TelegraphVFX.Reset();
	bTelegraphTried = false;
}

void UMTAbility_Zone::ExecuteAction()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	MTCombat::StopSpellFX(TelegraphVFX.Get());
	TelegraphVFX.Reset();
	if (!World)
	{
		return;
	}
	const bool bAttached = Data.ZoneKind == EMTZoneKind::Aura;
	const FVector Location = bAttached ? Owner->GetActorLocation() : ResolveGroundTarget();
	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AMTZoneActor* Zone = World->SpawnActor<AMTZoneActor>(AMTZoneActor::StaticClass(), Location, FRotator::ZeroRotator, Params))
	{
		// Area stat x the charge (a fully charged Quagmire covers a battlefield).
		const float Area = (Owner->GetAttributes() ? Owner->GetAttributes()->GetStatModifier().AreaMultiplier : 1.f)
			* FMath::Lerp(1.f, FMath::Max(0.1f, Data.ChargeRadiusScale), GetChargeAlpha());
		Zone->InitZone(Data, Owner, Area, bAttached);
	}
	PlaySubtleCameraShake(Data.FX.CameraShakeScale);
}

// ---------------------------------------------------------------- Sequence

void UMTAbility_Sequence::ExecuteAction()
{
	NextStep = 0;
	StepTimer = 0.f;
	if (AMTCharacterBase* Owner = GetOwnerCharacter())
	{
		Owner->SetAbilityMoveMultiplier(Data.MoveSpeedWhileActive);
	}
}

void UMTAbility_Sequence::TickAction(float DeltaTime)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UMTDataRegistry* Registry = UMTDataRegistry::Get(Owner);
	if (!Owner || !Registry || NextStep >= Data.Sequence.Num())
	{
		FinishAction();
		return;
	}
	StepTimer += DeltaTime;
	const FMTSequenceStep& Step = Data.Sequence[NextStep];
	if (StepTimer < Step.Delay)
	{
		return;
	}
	StepTimer = 0.f;
	if (const FMTAbilityData* Row = Registry->FindAbility(Step.AbilityId))
	{
		// Re-aim every step: the caster keeps tracking while repositioning.
		if (!Step.MontageSection.IsNone())
		{
			JumpMontageToSection(Step.MontageSection);
		}
		MTCombat::SpawnSpellFX(Owner, Row->FX, Row->FX.Formation, TEXT("Formation"), FTransform(GetAimRotation(), GetCastLocation()), 0.7f, nullptr, NAME_None, Owner);
		PlaySound(Row->FX.CastSound, GetCastLocation());
		UMTAbility_Projectile::FireProjectile(Owner, *Row, GetCastLocation(), GetAimPoint(), 0.f);
	}
	++NextStep;
	if (NextStep >= Data.Sequence.Num())
	{
		FinishAction();
	}
}

void UMTAbility_Sequence::OnEnded(bool bWasCancelled)
{
	if (AMTCharacterBase* Owner = GetOwnerCharacter())
	{
		Owner->SetAbilityMoveMultiplier(1.f);
	}
}

// ---------------------------------------------------------------- Dash

void UMTAbility_Dash::ExecuteAction()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		FinishAction();
		return;
	}
	const FMTStatModifier Mods = Owner->GetAttributes() ? Owner->GetAttributes()->GetStatModifier() : FMTStatModifier();
	float Distance = Data.DashDistance * Mods.DashMultiplier;
	DashDuration = FMath::Max(0.08f, Data.DashDuration / FMath::Max(0.5f, Mods.DashMultiplier));

	FVector Direction = Owner->GetLastMovementInputVector().GetSafeNormal2D();
	AActor* Target = GetLockedTarget();
	const float ArriveOffset = Data.GetParam(TEXT("ArriveOffset"), 0.f);
	const float TargetRange = Data.GetParam(TEXT("TargetRange"), BIG_NUMBER);
	FVector FlankPoint = FVector::ZeroVector;
	bool bFlank = false;
	if (Data.bDashTowardTarget && Target && ArriveOffset > 0.f && FVector::Dist2D(Target->GetActorLocation(), Owner->GetActorLocation()) <= TargetRange)
	{
		// Dragon Step: reappear beside / slightly behind the target on the side of the approach, facing it.
		const FVector Approach = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		const FVector Across = FVector::CrossProduct(FVector::UpVector, Approach);
		const float HeightFix = Owner->GetSimpleCollisionHalfHeight() - Target->GetSimpleCollisionHalfHeight();
		const UCapsuleComponent* Capsule = Owner->GetCapsuleComponent();
		const FCollisionShape Probe = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() * 0.9f, Capsule->GetScaledCapsuleHalfHeight() * 0.8f);
		FCollisionQueryParams FlankParams(SCENE_QUERY_STAT(MTDashFlank), false, Owner);
		for (const float Side : { 1.f, -1.f })
		{
			const FVector Candidate = Target->GetActorLocation() + (Across * Side * 0.94f + Approach * 0.35f) * ArriveOffset + FVector(0.f, 0.f, HeightFix);
			if (!Owner->GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Probe, FlankParams))
			{
				FlankPoint = Candidate;
				bFlank = true;
				break;
			}
		}
		if (bFlank)
		{
			Direction = (FlankPoint - Owner->GetActorLocation()).GetSafeNormal2D();
			Distance = FVector::Dist2D(FlankPoint, Owner->GetActorLocation());
			DashDuration = FMath::Max(0.06f, Data.GetParam(TEXT("TargetDuration"), DashDuration));
			StepTarget = Target;
		}
	}
	if (!bFlank && Data.bDashTowardTarget && Target && ArriveOffset <= 0.f)
	{
		const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
		Direction = ToTarget.GetSafeNormal2D();
		// Stop just outside striking range instead of running through the target.
		Distance = FMath::Min(Distance, FMath::Max(0.f, ToTarget.Size2D() - 140.f));
	}
	else if (!bFlank && ArriveOffset > 0.f)
	{
		// No target in reach: an ultra-long directional dash, far beyond a dodge.
		Distance = Data.GetParam(TEXT("FreeDistance"), Distance) * Mods.DashMultiplier;
		DashDuration = FMath::Max(0.08f, Data.GetParam(TEXT("FreeDuration"), DashDuration));
		StepTarget.Reset();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = Owner->GetActorForwardVector().GetSafeNormal2D();
	}

	FVector Destination = bFlank ? FlankPoint : Owner->GetActorLocation() + Direction * Distance;
	// Shorten the dash if a wall is in the way (capsule sweep), so we never tunnel. The step target itself is not a wall.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTDashSweep), false, Owner);
	if (AActor* Stepped = StepTarget.Get())
	{
		Params.AddIgnoredActor(Stepped);
	}
	const UCapsuleComponent* OwnerCapsule = Owner->GetCapsuleComponent();
	if (Owner->GetWorld()->SweepSingleByChannel(Hit, Owner->GetActorLocation(), Destination, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(OwnerCapsule->GetScaledCapsuleRadius() * 0.9f, OwnerCapsule->GetScaledCapsuleHalfHeight() * 0.8f), Params))
	{
		Destination = Hit.Location;
	}

	Owner->SetActorRotation(Direction.Rotation());
	if (UMotionWarpingComponent* Warp = Owner->GetMotionWarping())
	{
		if (!Data.WarpTargetName.IsNone())
		{
			Warp->AddOrUpdateWarpTargetFromLocationAndRotation(Data.WarpTargetName, Destination, Direction.Rotation());
		}
	}
	if (Data.bDashInvulnerable && Owner->GetAttributes())
	{
		Owner->GetAttributes()->SetInvulnerableFor(DashDuration);
	}
	Owner->GetStateTags().AddTag(MTTags::State_Dodging);
	const FVector Feet = Owner->GetActorLocation() - FVector(0.f, 0.f, Owner->GetSimpleCollisionHalfHeight());
	SpawnPhaseFX(TEXT("Launch"), FTransform(Direction.Rotation(), Feet));
	RootMotionId = ApplyMoveTo(Owner, Destination, DashDuration, TEXT("MTDash"));
	SpawnFX(Data.FX.Travel, Owner->GetActorLocation(), Direction.Rotation(), true);
	DashVFX = SpawnPhaseFX(TEXT("Travel"), FTransform(Direction.Rotation(), Owner->GetActorLocation() - FVector(0.f, 0.f, Owner->GetSimpleCollisionHalfHeight() * 0.9f)),
		1.f, Owner->GetRootComponent());
	if (AMTSpellVFX* Trail = DashVFX.Get())
	{
		// Snapped to the root (capsule centre): keep the dust at the feet.
		Trail->SetActorRelativeLocation(FVector(0.f, 0.f, -Owner->GetSimpleCollisionHalfHeight() * 0.9f));
	}
}

void UMTAbility_Dash::TickAction(float DeltaTime)
{
	if (PhaseTime >= DashDuration)
	{
		MTCombat::StopSpellFX(DashVFX.Get());
		DashVFX.Reset();
		if (AMTCharacterBase* Owner = GetOwnerCharacter())
		{
			Owner->GetStateTags().RemoveTag(MTTags::State_Dodging);
			if (AActor* Stepped = StepTarget.Get())
			{
				// Arrived beside the target: face it, ring the ground, and open the follow-up (Dragon Crush) window.
				const FVector ToTarget = (Stepped->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
				if (!ToTarget.IsNearlyZero())
				{
					Owner->SetActorRotation(ToTarget.Rotation());
				}
				SpawnPhaseFX(TEXT("Arrive"), FTransform(Owner->GetActorRotation(), Owner->GetActorLocation() - FVector(0.f, 0.f, Owner->GetSimpleCollisionHalfHeight())));
				PlaySound(Data.FX.AccentSound, Owner->GetActorLocation());
				if (!Data.ComboFollowUp.IsNone() && Owner->GetAbilities())
				{
					Owner->GetAbilities()->OpenComboWindow(Data.ComboFollowUp, Data.GetParam(TEXT("ComboWindow"), 0.9f), Stepped);
				}
			}
			// Attack dashes (Dragon Step's arrival palm, lunges, pounces, dives) strike what they arrive at; movement
			// dashes (Gale Step, Tide Rush) have no damage or stagger and only mark the landing.
			const float Radius = Data.AOERadius > 0.f ? Data.AOERadius : 120.f;
			if ((Data.Damage <= 0.f && Data.Stagger <= 0.f)
				|| StrikeHostilesInRadius(Owner->GetActorLocation() + Owner->GetActorForwardVector() * Radius * 0.6f, Data.HitRadius(Radius)) == 0)
			{
				SpawnFX(Data.FX.Impact, Owner->GetActorLocation(), Owner->GetActorRotation());
			}
		}
		FinishAction();
	}
}

void UMTAbility_Dash::OnEnded(bool bWasCancelled)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (Owner && RootMotionId != 0 && Owner->GetCharacterMovement())
	{
		Owner->GetCharacterMovement()->RemoveRootMotionSourceByID(RootMotionId);
	}
	if (Owner)
	{
		Owner->GetStateTags().RemoveTag(MTTags::State_Dodging);
	}
	RootMotionId = 0;
	StepTarget.Reset();
	MTCombat::StopSpellFX(DashVFX.Get());
	DashVFX.Reset();
}

// ---------------------------------------------------------------- Counter (Disturb Magic)

void UMTAbility_Counter::ExecuteAction()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	const float Bonus = (Owner && Owner->GetAttributes()) ? Owner->GetAttributes()->GetStatModifier().CounterWindowBonus : 0.f;
	Window = FMath::Max(0.05f, Data.CounterWindow + Bonus);
	Disrupted = 0;
	if (Owner)
	{
		Owner->GetStateTags().AddTag(MTTags::State_Countering);
	}
}

void UMTAbility_Counter::TickAction(float DeltaTime)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(Owner);
	if (Owner && Telegraphs)
	{
		for (AMTProjectile* Spell : Telegraphs->GetSpellsNear(Owner->GetActorLocation(), Data.CounterRadius, Owner))
		{
			if (Spell && Spell->IsDisruptable())
			{
				const FVector Where = Spell->GetActorLocation();
				Spell->Disrupt(Owner);
				++Disrupted;
				MTCombat::SpawnSpellFX(Owner, Data.FX, Data.FX.Impact, TEXT("Impact"), FTransform(Where), 1.f, nullptr, NAME_None, Owner);
			}
		}
	}
	if (PhaseTime >= Window)
	{
		if (Owner)
		{
			Owner->GetStateTags().RemoveTag(MTTags::State_Countering);
			if (Disrupted > 0)
			{
				// Success: Dragon God Knowledge triggers, recovery is shortened by the cooldown refund.
				Owner->NotifyPerfectDefense(TEXT("DisturbMagic"));
				if (UMTAbilityComponent* Abilities = Owner->GetAbilities())
				{
					Abilities->StartCooldown(Data.AbilityID, GetEffectiveCooldown() * 0.4f);
					Abilities->NotifyAbilityHit(Data.AbilityID, 0.f);
				}
			}
		}
		FinishAction();
	}
}

void UMTAbility_Counter::OnEnded(bool bWasCancelled)
{
	if (AMTCharacterBase* Owner = GetOwnerCharacter())
	{
		Owner->GetStateTags().RemoveTag(MTTags::State_Countering);
	}
}

// ---------------------------------------------------------------- Buff / Awakening

void UMTAbility_Buff::ExecuteAction()
{
	bBuffApplied = false;
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		FinishAction();
		return;
	}
	if (Data.TransformationTime > 0.f)
	{
		// Transformation sequence: the character stops, mana gathers, aura expands.
		// Not invulnerable - only a partial damage reduction while locked in place.
		Owner->GetStateTags().AddTag(MTTags::State_Transforming);
		Owner->SetAbilityMoveMultiplier(0.f);
		FMTStatusEffect Guard;
		Guard.Id = TEXT("TransformationGuard");
		Guard.Duration = Data.TransformationTime;
		Guard.Stats.DamageResistance = 0.4f;
		Guard.Stats.StaggerResistance = 0.6f;
		Owner->GetAttributes()->AddStatusEffect(Guard);
		SpawnFX(Data.FX.Formation, Owner->GetActorLocation(), Owner->GetActorRotation(), true);
		// The build-up and the reveal are one effect timed to the transformation.
		SpawnPhaseFX(TEXT("Cast"), FTransform(Owner->GetActorRotation(), Owner->GetActorLocation() - FVector(0.f, 0.f, Owner->GetSimpleCollisionHalfHeight())),
			1.f, Owner->GetRootComponent());
	}
	else
	{
		ApplyBuff();
		FinishAction();
	}
}

void UMTAbility_Buff::TickAction(float DeltaTime)
{
	if (!bBuffApplied && PhaseTime >= Data.TransformationTime)
	{
		if (AMTCharacterBase* Owner = GetOwnerCharacter())
		{
			Owner->GetStateTags().RemoveTag(MTTags::State_Transforming);
			Owner->SetAbilityMoveMultiplier(1.f);
		}
		ApplyBuff();
		FinishAction(); // control returns immediately after the reveal
	}
}

void UMTAbility_Buff::ApplyBuff()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || bBuffApplied)
	{
		return;
	}
	bBuffApplied = true;

	FMTStatusEffect Effect;
	Effect.Id = Data.AbilityID;
	Effect.Duration = FMath::Max(0.1f, Data.Duration);
	Effect.Stats = Data.BuffStats;
	Effect.Movement = Data.BuffMovement;
	Effect.HealthPerSecond = Data.BuffHealthPerSecond;
	Effect.ManaPerSecond = Data.BuffManaPerSecond;
	if (Data.BuffStaminaRestore > 0.f) { Owner->GetAttributes()->RestoreStamina(Data.BuffStaminaRestore); }
	if (Data.BuffPoiseRestore > 0.f) { Owner->GetAttributes()->RestorePoise(Data.BuffPoiseRestore); }
	if (Data.bIsAwakening) { Effect.GrantedTags.AddTag(MTTags::State_Awakened); }
	if (Data.bGrantsForesight) { Effect.GrantedTags.AddTag(MTTags::State_Foresight); }
	if (Data.ZoneKind == EMTZoneKind::Aura && Data.AOERadius > 0.f) { Effect.GrantedTags.AddTag(MTTags::State_Aura); }
	Effect.Instigator = Owner;
	Owner->GetAttributes()->AddStatusEffect(Effect);

	if (Data.AbilityOverrides.Num() > 0 && Owner->GetAbilities())
	{
		Owner->GetAbilities()->PushAbilityOverrides(Data.AbilityOverrides);
	}

	// Pressure aura (Saint Dragon Aura / Dragon God) follows the owner.
	if (Data.ZoneKind == EMTZoneKind::Aura && Data.AOERadius > 0.f)
	{
		FActorSpawnParameters Params;
		Params.Owner = Owner;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AMTZoneActor* Aura = Owner->GetWorld()->SpawnActor<AMTZoneActor>(AMTZoneActor::StaticClass(), Owner->GetActorLocation(), FRotator::ZeroRotator, Params))
		{
			FMTAbilityData AuraData = Data;
			AuraData.Duration = Effect.Duration;
			Aura->InitZone(AuraData, Owner, 1.f, true);
		}
	}

	SpawnFX(Data.FX.Impact, Owner->GetActorLocation(), Owner->GetActorRotation(), true);
	PlaySubtleCameraShake(Data.FX.CameraShakeScale);
	{
		const FTransform Feet(Owner->GetActorRotation(), Owner->GetActorLocation() - FVector(0.f, 0.f, Owner->GetSimpleCollisionHalfHeight()));
		if (Data.TransformationTime <= 0.f)
		{
			SpawnPhaseFX(TEXT("Cast"), Feet, 1.f, Owner->GetRootComponent());
		}
		MTCombat::StopSpellFX(AuraVFX.Get());
		AuraVFX = SpawnPhaseFX(TEXT("Aura"), Feet, 1.f, Owner->GetRootComponent());
		if (AMTSpellVFX* Aura = AuraVFX.Get())
		{
			Aura->SetActorRelativeLocation(FVector(0.f, 0.f, -Owner->GetSimpleCollisionHalfHeight()));
		}
	}

	TWeakObjectPtr<UMTAbility_Buff> WeakThis(this);
	Owner->GetWorldTimerManager().SetTimer(ExpireHandle, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ExpireBuff();
		}
	}), Effect.Duration, false);
}

void UMTAbility_Buff::ExpireBuff()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || !bBuffApplied)
	{
		return;
	}
	bBuffApplied = false;
	if (Data.AbilityOverrides.Num() > 0 && Owner->GetAbilities())
	{
		Owner->GetAbilities()->PopAbilityOverrides(Data.AbilityOverrides);
	}
	Owner->GetAttributes()->RemoveStatusEffect(Data.AbilityID);
	SpawnFX(Data.FX.Dissipation, Owner->GetActorLocation(), Owner->GetActorRotation(), true);
	MTCombat::StopSpellFX(AuraVFX.Get());
	AuraVFX.Reset();
}

void UMTAbility_Buff::OnEnded(bool bWasCancelled)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (Owner)
	{
		Owner->GetStateTags().RemoveTag(MTTags::State_Transforming);
		Owner->SetAbilityMoveMultiplier(1.f);
	}
	// A transformation interrupted before the reveal refunds nothing but grants nothing.
}

// ---------------------------------------------------------------- Structure (Earth Fortress)

void UMTAbility_Structure::ExecuteAction()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	FVector Forward = (GetAimPoint() - Owner->GetActorLocation()).GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = Owner->GetActorForwardVector();
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const int32 Count = FMath::Clamp(Data.StructureCount, 1, 7);
	const float Spacing = Data.StructureExtent.Y * 2.f + 20.f;
	const float Distance = FMath::Clamp(Data.GetParam(TEXT("ArcRadius"), Data.Range), 200.f, 900.f);
	const bool bArc = Data.Params.Contains(TEXT("ArcRadius"));
	const float RiseStep = Data.GetParam(TEXT("RiseStep"), 0.f);
	const float TiltDeg = Data.GetParam(TEXT("Tilt"), 0.f);
	const float HeightJitter = Data.GetParam(TEXT("HeightJitter"), 0.f);
	FRandomStream Stream(static_cast<int32>(World->GetTimeSeconds() * 1000.0) ^ static_cast<int32>(GetUniqueID()));

	for (int32 i = 0; i < Count; ++i)
	{
		const float Offset = (i - (Count - 1) * 0.5f) * Spacing;
		FVector Point;
		FRotator Facing;
		if (bArc)
		{
			// Segments sit edge to edge on a circle around the caster: a curved, connected wall.
			const float AngleDeg = FMath::RadiansToDegrees(Offset / Distance);
			const FVector Out = Forward.RotateAngleAxis(AngleDeg, FVector::UpVector);
			Point = Owner->GetActorLocation() + Out * Distance;
			Facing = FRotationMatrix::MakeFromXZ(Out, FVector::UpVector).Rotator();
		}
		else
		{
			// Slight arc so the fortress wraps around the caster.
			const float ArcBack = FMath::Abs(Offset) * 0.35f;
			Point = Owner->GetActorLocation() + Forward * (Distance - ArcBack) + Right * Offset;
			Facing = FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).Rotator() + FRotator(0.f, (Offset / Spacing) * -12.f, 0.f);
		}
		FVector Ground;
		if (!TraceGround(World, Point, Ground, Owner))
		{
			continue;
		}
		// Naturally fractured: each slab leans a little and stands a little higher or lower than its neighbours.
		const float Tall = 1.f + Stream.FRandRange(-HeightJitter, HeightJitter);
		const FRotator Lean(Stream.FRandRange(-TiltDeg, TiltDeg), 0.f, Stream.FRandRange(-TiltDeg, TiltDeg));
		const FRotator SegmentRotation = (FQuat(Facing) * FQuat(Lean)).Rotator();
		const FVector Center = Ground + FVector(0.f, 0.f, Data.StructureExtent.Z * Tall);
		// Never raise a wall inside a character or existing blocking geometry.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTWallCheck), false, Owner);
		if (World->OverlapBlockingTestByChannel(Center + FVector(0.f, 0.f, 10.f), SegmentRotation.Quaternion(), ECC_Pawn, FCollisionShape::MakeBox(Data.StructureExtent * 0.9f), Params))
		{
			continue;
		}
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Owner;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AMTEarthWall* Wall = World->SpawnActor<AMTEarthWall>(AMTEarthWall::StaticClass(), Center, SegmentRotation, SpawnParams))
		{
			// Centre first, then outward in pairs.
			const int32 Rank = FMath::RoundToInt(FMath::Abs(i - (Count - 1) * 0.5f));
			Wall->InitWall(Data, Owner, Data.StructureHealth, Data.Duration, RiseStep * Rank, i % 3, Tall);
		}
	}
	PlaySubtleCameraShake(Data.FX.CameraShakeScale);
}

// ---------------------------------------------------------------- Melee

void UMTAbility_Melee::ExecuteAction()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	// Small magnetism toward a locked target within reach (motion warping style).
	if (AActor* Target = GetLockedTarget())
	{
		const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
		const float Dist = ToTarget.Size2D();
		if (Dist < Data.Range + 250.f && Dist > 120.f)
		{
			const FVector Dest = Owner->GetActorLocation() + ToTarget.GetSafeNormal2D() * FMath::Min(Dist - 110.f, 180.f);
			Owner->SetActorRotation(ToTarget.GetSafeNormal2D().Rotation());
			ApplyMoveTo(Owner, Dest, 0.1f, TEXT("MTMeleeLunge"));
		}
	}

	const float Radius = Data.AOERadius > 0.f ? Data.AOERadius : 110.f;
	const FVector Center = Owner->GetActorLocation() + Owner->GetActorForwardVector() * FMath::Min(Data.Range * 0.6f, 180.f);
	StrikeHostilesInRadius(Center, Data.HitRadius(Radius));
}

// ---------------------------------------------------------------- Factory

TSubclassOf<UMTAbility> MTAbilityFactory::ClassForBehavior(EMTAbilityBehavior Behavior)
{
	switch (Behavior)
	{
	case EMTAbilityBehavior::Projectile: return UMTAbility_Projectile::StaticClass();
	case EMTAbilityBehavior::Zone: return UMTAbility_Zone::StaticClass();
	case EMTAbilityBehavior::Sequence: return UMTAbility_Sequence::StaticClass();
	case EMTAbilityBehavior::Dash: return UMTAbility_Dash::StaticClass();
	case EMTAbilityBehavior::Counter: return UMTAbility_Counter::StaticClass();
	case EMTAbilityBehavior::Buff: return UMTAbility_Buff::StaticClass();
	case EMTAbilityBehavior::Structure: return UMTAbility_Structure::StaticClass();
	case EMTAbilityBehavior::Melee: return UMTAbility_Melee::StaticClass();
	case EMTAbilityBehavior::Barrage: return UMTAbility_Barrage::StaticClass();
	case EMTAbilityBehavior::Disrupt: return UMTAbility_Disrupt::StaticClass();
	case EMTAbilityBehavior::Strike: return UMTAbility_Strike::StaticClass();
	case EMTAbilityBehavior::Serpent: return UMTAbility_Serpent::StaticClass();
	}
	return UMTAbility_Projectile::StaticClass();
}

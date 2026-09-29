// Signature behaviours of the LA PLACE overhaul: Elemental Barrage, Disturb Magic, Dragon Crush and the Water Dragon.
// (Docs/Ability_Overhaul.md sections 3.3, 3.4, 3.6 and 3.8.)
#include "Abilities/MTAbilityBehaviors.h"
#include "Abilities/MTAbilityComponent.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Combat/MTCombatStatics.h"
#include "Combat/MTProjectile.h"
#include "Combat/MTZoneActor.h"
#include "Combat/MTEarthWall.h"
#include "Combat/MTWaterSerpent.h"
#include "Combat/MTTelegraphSubsystem.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameplayTags.h"
#include "VFX/MTSpellVFX.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	const FName MTSig_FinaleStep(TEXT("Finale"));

	/** Collision-aware root-motion move (a lunge) to Destination over Seconds. */
	void MTSig_MoveTo(ACharacter* Character, const FVector& Destination, float Seconds)
	{
		UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
		if (!Move)
		{
			return;
		}
		TSharedPtr<FRootMotionSource_MoveToForce> Lunge = MakeShared<FRootMotionSource_MoveToForce>();
		Lunge->InstanceName = TEXT("MTStrikeLunge");
		Lunge->AccumulateMode = ERootMotionAccumulateMode::Override;
		Lunge->Priority = 900;
		Lunge->StartLocation = Character->GetActorLocation();
		Lunge->TargetLocation = Destination;
		Lunge->Duration = FMath::Max(0.05f, Seconds);
		Lunge->bRestrictSpeedToExpected = false;
		Lunge->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
		Lunge->FinishVelocityParams.ClampVelocity = 200.f;
		Move->ApplyRootMotionSource(Lunge);
	}
}

// ---------------------------------------------------------------- Elemental Barrage

int32 UMTAbility_Barrage::SlotForElement(EMTElement Element)
{
	switch (Element)
	{
	case EMTElement::Fire: return 0;
	case EMTElement::Water: return 1;
	case EMTElement::Earth: return 2;
	case EMTElement::Wind: return 3;
	default: return 2;
	}
}

FVector UMTAbility_Barrage::SlotOffset(int32 Slot) const
{
	// Fire behind upper left, Water behind upper right, Earth lower left, Wind lower right (from the capsule centre).
	static const FVector Anchors[4] = { FVector(-70.f, -100.f, 95.f), FVector(-70.f, 100.f, 95.f), FVector(-30.f, -125.f, 5.f), FVector(-30.f, 125.f, 5.f) };
	const int32 Index = FMath::Clamp(Slot, 0, 3);
	// The set sways around the caster, each formation bobs on its own rhythm, and a shot kicks its formation back.
	const float SwayDeg = Data.GetParam(TEXT("Sway"), 12.f) * FMath::Sin(SwayClock * 1.3f);
	FVector Offset = Anchors[Index].RotateAngleAxis(SwayDeg, FVector::UpVector);
	Offset.Z += 6.f * FMath::Sin(SwayClock * 2.1f + Index * 1.7f);
	Offset.X -= 25.f * Recoil[Index];
	// The finale draws all four together in front of the caster.
	return FMath::Lerp(Offset, FVector(150.f, 0.f, 40.f), FMath::SmoothStep(0.f, 1.f, CollapseAlpha));
}

void UMTAbility_Barrage::SpawnFormations()
{
	bFormationsSpawned = true;
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	static const TCHAR* const OrbPhases[4] = { TEXT("OrbFire"), TEXT("OrbWater"), TEXT("OrbEarth"), TEXT("OrbWind") };
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Local = SlotOffset(i);
		Formations[i] = SpawnPhaseFX(OrbPhases[i], FTransform(Owner->GetActorRotation(), Owner->GetActorTransform().TransformPosition(Local)), 1.f, Owner->GetRootComponent());
		if (AMTSpellVFX* Orb = Formations[i].Get())
		{
			Orb->SetActorRelativeLocation(Local); // attached with a snap to the capsule centre
		}
	}
}

void UMTAbility_Barrage::UpdateFormations(float DeltaTime)
{
	SwayClock += DeltaTime;
	for (float& Kick : Recoil)
	{
		Kick = FMath::Max(0.f, Kick - DeltaTime * 6.f);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		if (AMTSpellVFX* Orb = Formations[i].Get())
		{
			Orb->SetActorRelativeLocation(SlotOffset(i));
		}
	}
}

void UMTAbility_Barrage::StopFormations()
{
	for (TWeakObjectPtr<AMTSpellVFX>& Orb : Formations)
	{
		MTCombat::StopSpellFX(Orb.Get());
		Orb.Reset();
	}
	MTCombat::StopSpellFX(CollapseVFX.Get());
	CollapseVFX.Reset();
}

void UMTAbility_Barrage::TickAnticipation(float DeltaTime)
{
	if (!bFormationsSpawned)
	{
		SpawnFormations();
	}
	UpdateFormations(DeltaTime);
}

void UMTAbility_Barrage::ExecuteAction()
{
	BarrageClock = 0.f;
	ShotTimer = 0.f;
	ShotIndex = 0;
	CollapseAlpha = 0.f;
	bCollapsing = false;
	bFinaleFired = false;
	if (!bFormationsSpawned)
	{
		SpawnFormations();
	}
	if (AMTCharacterBase* Owner = GetOwnerCharacter())
	{
		Owner->SetAbilityMoveMultiplier(Data.MoveSpeedWhileActive); // Rudeus can drift while the barrage runs
	}
}

void UMTAbility_Barrage::TickAction(float DeltaTime)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		FinishAction();
		return;
	}
	BarrageClock += DeltaTime;
	UpdateFormations(DeltaTime);
	const float BarrageTime = Data.GetParam(TEXT("BarrageTime"), 3.f);
	const float Interval = FMath::Max(0.05f, Data.GetParam(TEXT("ShotInterval"), 0.2f));
	const float CollapseTime = FMath::Max(0.05f, Data.GetParam(TEXT("CollapseTime"), 0.35f));
	if (BarrageClock < BarrageTime)
	{
		ShotTimer -= DeltaTime;
		while (ShotTimer <= 0.f)
		{
			FireShot();
			ShotTimer += Interval;
		}
		return;
	}
	if (!bCollapsing)
	{
		// The formations rush together in front of him; he plants to hold the combined orb.
		bCollapsing = true;
		Owner->SetAbilityMoveMultiplier(0.f);
		const FVector Core(150.f, 0.f, 40.f);
		CollapseVFX = SpawnPhaseFX(TEXT("Collapse"), FTransform(Owner->GetActorRotation(), Owner->GetActorTransform().TransformPosition(Core)), 1.f, Owner->GetRootComponent());
		if (AMTSpellVFX* Combined = CollapseVFX.Get())
		{
			Combined->SetActorRelativeLocation(Core);
		}
	}
	CollapseAlpha = FMath::Clamp((BarrageClock - BarrageTime) / CollapseTime, 0.f, 1.f);
	if (CollapseAlpha >= 1.f)
	{
		FireFinale();
		FinishAction();
	}
}

void UMTAbility_Barrage::OnAnimEvent(FName EventName)
{
	// The clip's Finale frame fires the combined orb when the collapse is nearly done (frame-accurate per character).
	if (EventName == MTSig_FinaleStep && bCollapsing && !bFinaleFired && CollapseAlpha >= 0.7f && Phase == EMTAbilityPhase::Action)
	{
		FireFinale();
		FinishAction();
	}
}

void UMTAbility_Barrage::FireShot()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UMTDataRegistry* Registry = UMTDataRegistry::Get(Owner);
	if (!Owner || !Registry)
	{
		return;
	}
	// Stone -> Water -> Wind -> Fire -> ..., each from its own formation.
	TArray<const FMTSequenceStep*> Shots;
	for (const FMTSequenceStep& Each : Data.Sequence)
	{
		if (Each.MontageSection != MTSig_FinaleStep)
		{
			Shots.Add(&Each);
		}
	}
	if (Shots.Num() == 0)
	{
		return;
	}
	const FMTSequenceStep& Shot = *Shots[ShotIndex % Shots.Num()];
	++ShotIndex;
	const FMTAbilityData* Row = Registry->FindAbility(Shot.AbilityId);
	if (!Row)
	{
		return;
	}
	const int32 Slot = SlotForElement(Row->Element);
	const FVector From = Owner->GetActorTransform().TransformPosition(SlotOffset(Slot));
	const FVector Aim = GetAimPoint();
	FRotator Heading = (Aim - From).Rotation();
	const float Spread = Data.GetParam(TEXT("Spread"), 4.f);
	Heading.Yaw += FMath::FRandRange(-Spread, Spread);
	Heading.Pitch += FMath::FRandRange(-Spread, Spread) * 0.5f;
	const FVector ShotTarget = From + Heading.Vector() * FMath::Max(500.f, FVector::Dist(From, Aim));
	static const TCHAR* const MuzzlePhases[4] = { TEXT("MuzzleFire"), TEXT("MuzzleWater"), TEXT("MuzzleEarth"), TEXT("MuzzleWind") };
	SpawnPhaseFX(MuzzlePhases[Slot], FTransform(Heading, From));
	UMTAbility_Projectile::FireProjectile(Owner, *Row, From, ShotTarget, 0.f);
	Recoil[Slot] = 1.f;
}

void UMTAbility_Barrage::FireFinale()
{
	if (bFinaleFired)
	{
		return;
	}
	bFinaleFired = true;
	StopFormations();
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UMTDataRegistry* Registry = UMTDataRegistry::Get(Owner);
	if (!Owner || !Registry)
	{
		return;
	}
	for (const FMTSequenceStep& Each : Data.Sequence)
	{
		if (Each.MontageSection == MTSig_FinaleStep)
		{
			if (const FMTAbilityData* Row = Registry->FindAbility(Each.AbilityId))
			{
				const FVector From = Owner->GetActorTransform().TransformPosition(FVector(150.f, 0.f, 40.f));
				UMTAbility_Projectile::FireProjectile(Owner, *Row, From, GetAimPoint(), 0.f);
			}
			break;
		}
	}
}

void UMTAbility_Barrage::OnEnded(bool bWasCancelled)
{
	StopFormations();
	bFormationsSpawned = false;
	bCollapsing = false;
	CollapseAlpha = 0.f;
	for (float& Kick : Recoil)
	{
		Kick = 0.f;
	}
	if (AMTCharacterBase* Owner = GetOwnerCharacter())
	{
		Owner->SetAbilityMoveMultiplier(1.f);
	}
}

// ---------------------------------------------------------------- Disturb Magic

AMTCharacterBase* UMTAbility_Disrupt::FindCastingTarget() const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return nullptr;
	}
	const float Range = Data.GetParam(TEXT("SearchRange"), 1800.f);
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(Data.GetParam(TEXT("SearchAngle"), 35.f)));
	FVector Facing = (GetAimPoint() - Owner->GetActorLocation()).GetSafeNormal2D();
	if (Facing.IsNearlyZero())
	{
		Facing = Owner->GetActorForwardVector().GetSafeNormal2D();
	}
	AMTCharacterBase* Best = nullptr;
	float BestAlignment = -1.f;
	for (AMTCharacterBase* Candidate : MTCombat::GetHostilesInRadius(Owner, Owner->GetActorLocation(), Range))
	{
		const UMTAbilityComponent* CandidateAbilities = Candidate->GetAbilities();
		const UMTAbility* Casting = CandidateAbilities ? CandidateAbilities->GetActiveAbility() : nullptr;
		if (!Casting || Casting->GetPhase() != EMTAbilityPhase::Anticipation)
		{
			continue;
		}
		const float Alignment = FVector::DotProduct((Candidate->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D(), Facing);
		if (Alignment >= CosLimit && Alignment > BestAlignment)
		{
			BestAlignment = Alignment;
			Best = Candidate;
		}
	}
	return Best;
}

void UMTAbility_Disrupt::SpawnCollapse(EMTElement Element, const FVector& Where)
{
	// Fire collapses into sparks, water loses its shape, earth crumbles, wind disperses.
	const TCHAR* CollapsePhase = TEXT("CollapseArcane");
	switch (Element)
	{
	case EMTElement::Fire: CollapsePhase = TEXT("CollapseFire"); break;
	case EMTElement::Water: CollapsePhase = TEXT("CollapseWater"); break;
	case EMTElement::Earth: CollapsePhase = TEXT("CollapseEarth"); break;
	case EMTElement::Wind: CollapsePhase = TEXT("CollapseWind"); break;
	default: break;
	}
	SpawnPhaseFX(CollapsePhase, FTransform(Where));
	PlaySound(Data.FX.ImpactSound, Where);
}

void UMTAbility_Disrupt::CollapseSpellsNear(const FVector& A, const FVector& B, float Radius)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	const FVector Middle = (A + B) * 0.5f;
	const float Reach = FVector::Dist(A, B) * 0.5f + Radius;
	if (UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(Owner))
	{
		for (AMTProjectile* Spell : Telegraphs->GetSpellsNear(Middle, Reach, Owner))
		{
			if (!Spell || !Spell->IsDisruptable())
			{
				continue;
			}
			const FVector Where = Spell->GetActorLocation();
			if (FMath::PointDistToSegment(Where, A, B) > Radius)
			{
				continue;
			}
			SpawnCollapse(Spell->GetElement(), Where);
			Spell->Disrupt(Owner);
			++Disrupted;
		}
	}
	// A water dragon crossing the pulse (or diving at Orsted) loses its shape.
	for (TActorIterator<AMTWaterSerpent> It(World); It; ++It)
	{
		AMTWaterSerpent* Serpent = *It;
		AMTCharacterBase* SerpentCaster = Serpent ? Serpent->GetCaster() : nullptr;
		if (!Serpent || !Serpent->IsFlying() || !SerpentCaster || !Owner->IsHostileTo(SerpentCaster) || !Serpent->GetAbilityData().bDisruptable)
		{
			continue;
		}
		if (FMath::PointDistToSegment(Serpent->GetHeadLocation(), A, B) <= Radius)
		{
			SpawnCollapse(EMTElement::Water, Serpent->GetHeadLocation());
			Serpent->Collapse();
			++Disrupted;
		}
	}
}

bool UMTAbility_Disrupt::DisruptCaster(AMTCharacterBase* Target)
{
	UMTAbilityComponent* TargetAbilities = Target ? Target->GetAbilities() : nullptr;
	UMTAbility* Forming = TargetAbilities ? TargetAbilities->GetActiveAbility() : nullptr;
	if (!Forming || Forming->GetPhase() != EMTAbilityPhase::Anticipation)
	{
		return false;
	}
	const FMTAbilityData& Row = Forming->GetData();
	if (!Row.bDisruptable || Row.Element == EMTElement::None)
	{
		return false;
	}
	const FName SealedId = Row.AbilityID;
	const EMTElement Element = Row.Element;
	const FName Socket = Row.CastSocket;
	USkeletalMeshComponent* TargetMesh = Target->GetMesh();
	const bool bSocket = TargetMesh && !Socket.IsNone() && TargetMesh->DoesSocketExist(Socket);
	const FVector Where = bSocket ? TargetMesh->GetSocketLocation(Socket)
		: Target->GetActorLocation() + Target->GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f);
	// The spell loses its structure before it manifests, and the hand that shaped it is sealed for a while.
	Forming->Cancel();
	TargetAbilities->LockAbility(SealedId, Data.GetParam(TEXT("SealSeconds"), 3.f));
	SpawnCollapse(Element, Where);
	SpawnPhaseFX(TEXT("Seal"), FTransform(Where), 1.f, bSocket ? static_cast<USceneComponent*>(TargetMesh) : Target->GetRootComponent(), bSocket ? Socket : NAME_None);
	PlaySound(Data.FX.AccentSound, Where);
	++Disrupted;
	return true;
}

void UMTAbility_Disrupt::DestabilizeAround(const FVector& Point)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	// Persistent magic is weakened, not deleted.
	const float Reach = Data.GetParam(TEXT("EndRadius"), 450.f);
	for (TActorIterator<AMTZoneActor> It(World); It; ++It)
	{
		AMTZoneActor* Zone = *It;
		AMTCharacterBase* ZoneOwner = Zone ? Zone->GetOwnerCharacter() : nullptr;
		if (!Zone || !Zone->IsActiveZone() || !ZoneOwner || !Owner->IsHostileTo(ZoneOwner) || !Zone->GetAbilityData().bDisruptable)
		{
			continue;
		}
		if (FVector::Dist2D(Zone->GetActorLocation(), Point) - Zone->GetRadius() > Reach)
		{
			continue;
		}
		Zone->Destabilize(0.5f, 0.6f);
		SpawnPhaseFX(TEXT("Destabilize"), FTransform(Zone->GetActorLocation()), FMath::Clamp(Zone->GetRadius() / 450.f, 0.5f, 4.f));
		++Disrupted;
	}
	for (TActorIterator<AMTEarthWall> It(World); It; ++It)
	{
		AMTEarthWall* Wall = *It;
		const AMTCharacterBase* Builder = Wall ? Cast<AMTCharacterBase>(Wall->GetOwner()) : nullptr;
		if (!Wall || !Wall->IsStanding() || !Builder || !Owner->IsHostileTo(Builder) || FVector::Dist2D(Wall->GetActorLocation(), Point) > Reach + 150.f)
		{
			continue;
		}
		Wall->TakeStructureDamage(Wall->GetMaxHealth() * 0.4f);
		SpawnPhaseFX(TEXT("Destabilize"), FTransform(Wall->GetActorLocation()), 0.6f);
		++Disrupted;
	}
	for (TActorIterator<AMTWaterSerpent> It(World); It; ++It)
	{
		AMTWaterSerpent* Serpent = *It;
		AMTCharacterBase* SerpentCaster = Serpent ? Serpent->GetCaster() : nullptr;
		if (!Serpent || !Serpent->IsFlying() || !SerpentCaster || !Owner->IsHostileTo(SerpentCaster)
			|| FVector::Dist(Serpent->GetHeadLocation(), Point) > Reach + 200.f)
		{
			continue;
		}
		SpawnCollapse(EMTElement::Water, Serpent->GetHeadLocation());
		Serpent->Collapse();
		++Disrupted;
	}
}

void UMTAbility_Disrupt::TickAnticipation(float DeltaTime)
{
	// No wind-up: the ward is up from the moment the hand rises.
	if (AMTCharacterBase* Owner = GetOwnerCharacter())
	{
		const FVector Center = Owner->GetActorLocation();
		CollapseSpellsNear(Center, Center, Data.CounterRadius > 0.f ? Data.CounterRadius : 450.f);
	}
}

void UMTAbility_Disrupt::ExecuteAction()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	const float Bonus = Owner->GetAttributes() ? Owner->GetAttributes()->GetStatModifier().CounterWindowBonus : 0.f;
	WardWindow = FMath::Max(0.05f, Data.CounterWindow + Bonus);
	Owner->GetStateTags().AddTag(MTTags::State_Countering);
	bPulseArrived = false;

	// The pulse goes to the lock, else to a hostile casting in front, else along the aim.
	AActor* Chosen = GetLockedTarget();
	const AMTCharacterBase* ChosenCharacter = Cast<AMTCharacterBase>(Chosen);
	if (!ChosenCharacter || !ChosenCharacter->IsAlive() || !Owner->IsHostileTo(ChosenCharacter))
	{
		Chosen = FindCastingTarget();
	}
	PulseTarget = Chosen;
	PulseStart = GetCastLocation();
	PulseHead = PulseStart;
	if (Chosen)
	{
		PulseDirection = (Chosen->GetActorLocation() - PulseStart).GetSafeNormal();
		PulseRemaining = FVector::Dist(Chosen->GetActorLocation(), PulseStart);
	}
	else
	{
		PulseDirection = (GetAimPoint() - PulseStart).GetSafeNormal();
		if (PulseDirection.IsNearlyZero())
		{
			PulseDirection = Owner->GetActorForwardVector();
		}
		PulseRemaining = Data.GetParam(TEXT("SearchRange"), 1800.f);
	}
	SpawnPhaseFX(TEXT("Cast"), FTransform(PulseDirection.Rotation(), PulseStart));
	PulseVFX = SpawnPhaseFX(TEXT("Pulse"), FTransform(PulseDirection.Rotation(), PulseStart));
}

void UMTAbility_Disrupt::TickAction(float DeltaTime)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		FinishAction();
		return;
	}
	// The ward around Orsted lasts the counter window.
	if (PhaseTime <= WardWindow)
	{
		const FVector Center = Owner->GetActorLocation();
		CollapseSpellsNear(Center, Center, Data.CounterRadius > 0.f ? Data.CounterRadius : 450.f);
	}
	if (!bPulseArrived)
	{
		// The pulse homes on a moving target, so a well-timed answer always lands.
		if (const AActor* Chased = PulseTarget.Get())
		{
			PulseDirection = (Chased->GetActorLocation() - PulseHead).GetSafeNormal();
			PulseRemaining = FVector::Dist(Chased->GetActorLocation(), PulseHead);
		}
		const float Advance = FMath::Min(PulseRemaining, Data.GetParam(TEXT("PulseSpeed"), 6500.f) * DeltaTime);
		const FVector Before = PulseHead;
		PulseHead += PulseDirection * Advance;
		PulseRemaining -= Advance;
		CollapseSpellsNear(Before, PulseHead, Data.GetParam(TEXT("PathRadius"), 220.f));
		if (AMTSpellVFX* Ripple = PulseVFX.Get())
		{
			Ripple->SetActorLocationAndRotation(PulseHead, PulseDirection.Rotation());
		}
		if (PulseRemaining <= 1.f)
		{
			bPulseArrived = true;
			MTCombat::StopSpellFX(PulseVFX.Get());
			PulseVFX.Reset();
			if (AMTCharacterBase* Victim = Cast<AMTCharacterBase>(PulseTarget.Get()))
			{
				SpawnPhaseFX(TEXT("Hit"), FTransform(PulseDirection.Rotation(), PulseHead));
				DisruptCaster(Victim);
			}
			DestabilizeAround(PulseHead);
		}
	}
	if (bPulseArrived && PhaseTime >= WardWindow)
	{
		Owner->GetStateTags().RemoveTag(MTTags::State_Countering);
		if (Disrupted > 0)
		{
			// Success: Dragon God Knowledge triggers and most of the cooldown comes back.
			Owner->NotifyPerfectDefense(TEXT("DisturbMagic"));
			if (UMTAbilityComponent* OwnerAbilities = Owner->GetAbilities())
			{
				const float Refund = FMath::Clamp(Data.GetParam(TEXT("Refund"), 0.6f), 0.f, 1.f);
				OwnerAbilities->StartCooldown(Data.AbilityID, GetEffectiveCooldown() * (1.f - Refund));
				OwnerAbilities->NotifyAbilityHit(Data.AbilityID, 0.f);
			}
		}
		FinishAction();
	}
}

void UMTAbility_Disrupt::OnEnded(bool bWasCancelled)
{
	if (AMTCharacterBase* Owner = GetOwnerCharacter())
	{
		Owner->GetStateTags().RemoveTag(MTTags::State_Countering);
	}
	MTCombat::StopSpellFX(PulseVFX.Get());
	PulseVFX.Reset();
	PulseTarget.Reset();
	bPulseArrived = false;
	Disrupted = 0;
}

// ---------------------------------------------------------------- Dragon Crush

AMTCharacterBase* UMTAbility_Strike::ResolvePrimary() const
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return nullptr;
	}
	const FVector From = Owner->GetActorLocation();
	const FVector Facing = Owner->GetActorForwardVector().GetSafeNormal2D();
	const float LungeRange = Data.GetParam(TEXT("LungeRange"), 450.f);
	auto Usable = [Owner](const AActor* Actor) -> AMTCharacterBase*
	{
		AMTCharacterBase* AsCharacter = const_cast<AMTCharacterBase*>(Cast<AMTCharacterBase>(Actor));
		return (AsCharacter && AsCharacter->IsAlive() && Owner->IsHostileTo(AsCharacter)) ? AsCharacter : nullptr;
	};
	// 1. The enemy Dragon Step just arrived beside.
	if (bComboActive)
	{
		if (AMTCharacterBase* Combo = Usable(ComboTarget.Get()))
		{
			if (FVector::Dist2D(Combo->GetActorLocation(), From) <= LungeRange + 150.f)
			{
				return Combo;
			}
		}
	}
	// 2. The lock, when it is within lunging range and in front.
	if (AMTCharacterBase* Locked = Usable(GetLockedTarget()))
	{
		const FVector ToLocked = Locked->GetActorLocation() - From;
		if (ToLocked.Size2D() <= LungeRange && FVector::DotProduct(ToLocked.GetSafeNormal2D(), Facing) > 0.3f)
		{
			return Locked;
		}
	}
	// 3. The closest enemy in a cone in front.
	const float ConeRange = Data.GetParam(TEXT("ConeRange"), 300.f);
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(Data.GetParam(TEXT("ConeHalfAngle"), 35.f)));
	AMTCharacterBase* Best = nullptr;
	float BestDistance = BIG_NUMBER;
	for (AMTCharacterBase* Candidate : MTCombat::GetHostilesInRadius(Owner, From, ConeRange))
	{
		const FVector ToCandidate = Candidate->GetActorLocation() - From;
		if (FVector::DotProduct(ToCandidate.GetSafeNormal2D(), Facing) < CosHalf)
		{
			continue;
		}
		const float Distance = ToCandidate.Size2D();
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}
	return Best;
}

void UMTAbility_Strike::TickAnticipation(float DeltaTime)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	if (!bPrimaryResolved)
	{
		bPrimaryResolved = true;
		Primary = ResolvePrimary();
	}
	AMTCharacterBase* Victim = Primary.Get();
	if (!Victim)
	{
		return;
	}
	// Square up to the target through the wind-up, and close the gap if it is out of reach.
	const FVector ToVictim = Victim->GetActorLocation() - Owner->GetActorLocation();
	const FVector Flat = ToVictim.GetSafeNormal2D();
	if (!Flat.IsNearlyZero())
	{
		Owner->SetActorRotation(Flat.Rotation());
	}
	const float Reach = 110.f + Victim->GetSimpleCollisionRadius();
	if (!bLunged && ToVictim.Size2D() > Reach + 40.f)
	{
		bLunged = true;
		MTSig_MoveTo(Owner, Victim->GetActorLocation() - Flat * Reach, FMath::Clamp(GetEffectiveCastTime() * 0.6f, 0.08f, 0.25f));
	}
}

void UMTAbility_Strike::ExecuteAction()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	if (!bPrimaryResolved)
	{
		bPrimaryResolved = true;
		Primary = ResolvePrimary();
	}
	AMTCharacterBase* Victim = Primary.Get();
	const float ComboScale = bComboActive ? Data.GetParam(TEXT("ComboDamageScale"), 1.f) : 1.f;
	const float StatScale = Owner->GetAttributes() ? Owner->GetAttributes()->GetStatModifier().DamageMultiplier : 1.f;
	// The palm drives into the ground just in front: the shockwave starts there.
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector Center = MTCombat::GroundBelow(Owner, Owner->GetActorLocation() + Forward * 120.f);
	SpawnPhaseFX(TEXT("Impact"), FTransform(Forward.Rotation(), Center + FVector(0.f, 0.f, 5.f)));
	PlaySound(Data.FX.ImpactSound, Center);

	TArray<AActor*> Frozen;
	Frozen.Add(Owner);
	int32 Hits = 0;
	for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, Center, Data.HitRadius(Data.AOERadius)))
	{
		const bool bPrimaryHit = Target == Victim;
		FVector Out = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		if (Out.IsNearlyZero())
		{
			Out = Forward;
		}
		FMTDamageSpec Spec = MTCombat::MakeAbilityHit(Data, Owner, Target->GetActorLocation(), Out, ComboScale * StatScale);
		Spec.bIsMagic = false; // raw Dragon God force, not a spell
		if (bPrimaryHit)
		{
			// The one it was aimed at takes far more of the force.
			Spec.Damage = Data.GetParam(TEXT("PrimaryDamage"), Data.Damage * 2.f) * ComboScale * StatScale;
			Spec.Knockback = Data.GetParam(TEXT("PrimaryKnockback"), Data.Knockback * 1.6f);
			Spec.Launch = Data.GetParam(TEXT("PrimaryLaunch"), Data.Launch * 1.6f);
			Spec.Stagger = Data.Stagger * 1.5f;
		}
		// A spell forming in the blast is broken (Dragon God Knowledge).
		const bool bInterrupt = Target->GetStateTags().HasTag(MTTags::State_Casting);
		const FMTDamageResult Result = Target->ReceiveCombatHit(Spec);
		if (bInterrupt && Target->GetAbilities())
		{
			Target->GetAbilities()->CancelAll();
			Owner->NotifyPerfectDefense(TEXT("Interrupt"));
		}
		if (Owner->GetAbilities())
		{
			Owner->GetAbilities()->NotifyAbilityHit(Data.AbilityID, Result.DamageDealt);
		}
		if (bPrimaryHit)
		{
			SpawnPhaseFX(TEXT("TargetHit"), FTransform((-Out).Rotation(), Target->GetActorLocation()));
			Frozen.Add(Target);
		}
		++Hits;
	}
	if (Hits > 0)
	{
		// Hit-stop sells the weight; only when something was actually struck.
		MTCombat::HitStop(Frozen, Data.GetParam(TEXT("HitStop"), 0.f));
	}
}

void UMTAbility_Strike::OnEnded(bool bWasCancelled)
{
	Primary.Reset();
	bPrimaryResolved = false;
	bLunged = false;
}

// ---------------------------------------------------------------- Water Dragon

void UMTAbility_Serpent::ExecuteAction()
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AMTWaterSerpent* Serpent = World->SpawnActor<AMTWaterSerpent>(AMTWaterSerpent::StaticClass(), Owner->GetActorLocation(), Owner->GetActorRotation(), SpawnParams))
	{
		const float StatScale = Owner->GetAttributes() ? Owner->GetAttributes()->GetStatModifier().DamageMultiplier : 1.f;
		Serpent->InitSerpent(Data, Owner, GetLockedTarget(), GetAimPoint(), StatScale);
	}
}

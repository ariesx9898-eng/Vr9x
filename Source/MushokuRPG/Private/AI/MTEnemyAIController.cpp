#include "AI/MTEnemyAIController.h"

#include "AI/MTEnemyCharacter.h"
#include "AI/MTBossCharacter.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbilityComponent.h"
#include "Combat/MTTelegraphSubsystem.h"
#include "Combat/MTProjectile.h"
#include "Core/MTGameplayTags.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "TimerManager.h"

namespace MTAIPrivate
{
	using FAttackerList = TArray<TWeakObjectPtr<AMTEnemyAIController>>;

	/** Target -> AI controllers holding a melee engagement slot on it. */
	static TMap<TWeakObjectPtr<AActor>, FAttackerList>& EngagementMap()
	{
		static TMap<TWeakObjectPtr<AActor>, FAttackerList> Map;
		return Map;
	}

	static void PruneEngagements()
	{
		for (auto It = EngagementMap().CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
				continue;
			}
			It.Value().RemoveAll([](const TWeakObjectPtr<AMTEnemyAIController>& Entry) { return !Entry.IsValid(); });
			if (It.Value().Num() == 0)
			{
				It.RemoveCurrent();
			}
		}
	}

	static float CapsuleRadiusOf(const AMTCharacterBase* Character)
	{
		const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		return Capsule ? Capsule->GetScaledCapsuleRadius() : 40.f;
	}

	static float CapsuleHalfHeightOf(const AMTCharacterBase* Character)
	{
		const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		return Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	}
}

AMTEnemyAIController::AMTEnemyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------------------------------------
// Possession
// ---------------------------------------------------------------------------------------------

void AMTEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!InPawn)
	{
		return;
	}
	HomeLocation = InPawn->GetActorLocation();
	bDeadHandled = false;
	bLineageResolved = false;
	StrafeSign = FMath::RandBool() ? 1.f : -1.f;

	if (AMTCharacterBase* Me = Cast<AMTCharacterBase>(InPawn))
	{
		if (UMTAttributeComponent* Attr = Me->GetAttributes())
		{
			Attr->OnDamaged.AddUniqueDynamic(this, &AMTEnemyAIController::HandlePawnDamaged);
		}
	}
	// Random phase so a pack of enemies does not think on the same frame.
	GetWorldTimerManager().SetTimer(EvaluateTimer, this, &AMTEnemyAIController::Evaluate,
		FMath::Max(0.05f, EvaluationInterval), true, FMath::FRandRange(0.05f, EvaluationInterval + 0.05f));
}

void AMTEnemyAIController::OnUnPossess()
{
	if (AMTCharacterBase* Me = Cast<AMTCharacterBase>(GetPawn()))
	{
		if (UMTAttributeComponent* Attr = Me->GetAttributes())
		{
			Attr->OnDamaged.RemoveDynamic(this, &AMTEnemyAIController::HandlePawnDamaged);
		}
	}
	EndBlock();
	GetWorldTimerManager().ClearTimer(EvaluateTimer);
	GetWorldTimerManager().ClearTimer(ReactionTimer);
	GetWorldTimerManager().ClearTimer(ChargeTimer);
	bChargingCast = false;
	ReleaseEngageToken();
	CurrentTarget = nullptr;
	Super::OnUnPossess();
}

void AMTEnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseEngageToken();
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

AMTCharacterBase* AMTEnemyAIController::GetCombatTarget() const
{
	return CurrentTarget.Get();
}

void AMTEnemyAIController::ResetAI()
{
	StopMovement();
	EndBlock();
	GetWorldTimerManager().ClearTimer(ChargeTimer);
	bChargingCast = false;
	SetCombatTarget(nullptr);
	ReleaseEngageToken();
	if (const APawn* MyPawn = GetPawn())
	{
		HomeLocation = MyPawn->GetActorLocation();
	}
	LastAttackTime = -100.f;
	ActionLockUntil = 0.f;
	RetreatUntil = 0.f;
	NextRetreatAllowedTime = 0.f;
	LastThreatKey = 0;
	NumDodges = NumBlocks = NumDisruptAttempts = NumAttacks = 0;
	bDeadHandled = false;
	SetAction(EMTAIAction::Idle);
}

void AMTEnemyAIController::SetCombatTarget(AMTCharacterBase* NewTarget)
{
	if (CurrentTarget.Get() == NewTarget)
	{
		return;
	}
	ReleaseEngageToken();
	CurrentTarget = NewTarget;
	AMTCharacterBase* Me = Cast<AMTCharacterBase>(GetPawn());
	if (NewTarget)
	{
		SetFocus(NewTarget);
		if (Me)
		{
			Me->SetLockTarget(NewTarget);
			Me->SetWalking(false);
		}
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		if (Me)
		{
			Me->SetLockTarget(nullptr);
		}
		StopMovement();
		LastMoveGoalActor = nullptr;
	}
}

void AMTEnemyAIController::HandlePawnDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result)
{
	AMTCharacterBase* Me = Cast<AMTCharacterBase>(GetPawn());
	if (!Me || !Me->IsAlive())
	{
		return;
	}
	const AActor* Source = Spec.Instigator.Get();
	AMTCharacterBase* Attacker = Cast<AMTCharacterBase>(const_cast<AActor*>(Source));
	if (!Attacker && Source)
	{
		Attacker = Cast<AMTCharacterBase>(Source->GetInstigator());
	}
	if (!Attacker || Attacker == Me || !Attacker->IsAlive() || !Me->IsHostileTo(Attacker))
	{
		return;
	}
	// Aggro on hit (sniped from beyond sight range); occasionally switch to whoever hurts us.
	const AMTCharacterBase* Current = CurrentTarget.Get();
	if (!Current || (Current != Attacker && FMath::FRand() < 0.35f))
	{
		SetCombatTarget(Attacker);
	}
}

// ---------------------------------------------------------------------------------------------
// Main evaluation (every EvaluationInterval seconds)
// ---------------------------------------------------------------------------------------------

void AMTEnemyAIController::Evaluate()
{
	AMTCharacterBase* Me = Cast<AMTCharacterBase>(GetPawn());
	UWorld* World = GetWorld();
	if (!Me || !World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();

	if (!Me->IsAlive())
	{
		if (!bDeadHandled)
		{
			bDeadHandled = true;
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
			EndBlock();
			ReleaseEngageToken();
			GetWorldTimerManager().ClearTimer(ReactionTimer);
			SetAction(EMTAIAction::Idle);
		}
		return;
	}

	AMTEnemyCharacter* Enemy = Cast<AMTEnemyCharacter>(Me);
	AMTBossCharacter* Boss = Cast<AMTBossCharacter>(Me);
	RefreshLineage(Me);

	if (Boss && Boss->IsPhaseTransitioning())
	{
		StopMovement();
		return;
	}

	UpdateTarget(Me, Now);
	AMTCharacterBase* Target = CurrentTarget.Get();

	// Boss leash: return to the arena centre (no healing) and never chase outside it.
	if (Boss && (Boss->IsReturningToArena() || (Target && !Boss->IsLocationInsideArena(Target->GetActorLocation(), Boss->TargetLeashTolerance))))
	{
		ReleaseEngageToken();
		const FVector Center = Boss->GetArenaCenter();
		if (FVector::Dist2D(Me->GetActorLocation(), Center) > 250.f && (CurrentAction != EMTAIAction::ReturnToArena || GetMoveStatus() != EPathFollowingStatus::Moving))
		{
			Me->SetWalking(false);
			MoveToLocation(Center, 150.f);
			LastMoveGoalActor = nullptr;
		}
		SetAction(EMTAIAction::ReturnToArena);
		return;
	}

	if (!Target)
	{
		DoPatrol(Me, Now);
		return;
	}

	if (bRetreating && Now >= RetreatUntil)
	{
		bRetreating = false;
		Me->SetSprinting(false);
	}

	// Committed: telegraphs, recovery, casts, dodges, blocks and charged spells run their course.
	if (bChargingCast || Now < ActionLockUntil || Me->IsStaggered() || Me->IsDodging())
	{
		return;
	}
	if (Enemy && Enemy->IsBusy())
	{
		return;
	}
	if (const UMTAbilityComponent* AbilityComp = Me->GetAbilities())
	{
		if (AbilityComp->IsCasting())
		{
			return;
		}
	}

	if (TryReactToThreats(Me, Enemy, Now))
	{
		return;
	}

	if (Enemy)
	{
		bool bSidestep = false;
		if (Enemy->GetDefensiveReaction(Target, bSidestep))
		{
			if (bSidestep)
			{
				const FVector ToTarget = (Target->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
				FVector Side = FVector::CrossProduct(FVector::UpVector, ToTarget) * StrafeSign;
				FVector Probe;
				if (!ProjectToNav(Me->GetActorLocation() + Side * 350.f, Probe))
				{
					Side = -Side;
				}
				if (Me->Dodge(Side))
				{
					++NumDodges;
					SetAction(EMTAIAction::Dodge);
					ActionLockUntil = Now + 0.4f;
					return;
				}
			}
			StartBlock(Me, 1.0f);
			return;
		}
	}

	if (LineageKind == EMTAILineage::Rudeus)
	{
		EvaluateRudeus(Me, Target, Now);
	}
	else
	{
		EvaluateGeneric(Me, Enemy, Target, Now);
	}
}

void AMTEnemyAIController::RefreshLineage(AMTCharacterBase* Me)
{
	const AMTEnemyCharacter* Enemy = Cast<AMTEnemyCharacter>(Me);
	const FName Lineage = Enemy ? Enemy->GetLineageId() : Me->GetCharacterId();
	if (bLineageResolved && Lineage == CachedLineage)
	{
		return;
	}
	bLineageResolved = true;
	CachedLineage = Lineage;

	const FString AsString = Lineage.IsNone() ? FString() : Lineage.ToString();
	if (AsString.Contains(TEXT("Orsted")))
	{
		LineageKind = EMTAILineage::Orsted;
	}
	else if (AsString.Contains(TEXT("Rudeus")))
	{
		LineageKind = EMTAILineage::Rudeus;
	}
	else
	{
		LineageKind = EMTAILineage::Generic;
	}

	if (LineageKind == EMTAILineage::Orsted)
	{
		GetWorldTimerManager().SetTimer(ReactionTimer, this, &AMTEnemyAIController::ReactionTick, FMath::Max(0.02f, ReactionInterval), true);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(ReactionTimer);
	}
}

// ---------------------------------------------------------------------------------------------
// Perception
// ---------------------------------------------------------------------------------------------

void AMTEnemyAIController::UpdateTarget(AMTCharacterBase* Me, float Now)
{
	AMTCharacterBase* Target = CurrentTarget.Get();
	if (Target)
	{
		const float DistSq = FVector::DistSquared(Me->GetActorLocation(), Target->GetActorLocation());
		if (!Target->IsAlive() || DistSq > FMath::Square(LoseTargetRadius))
		{
			SetCombatTarget(nullptr);
			Target = nullptr;
		}
	}

	if (Target && Now < NextPerceptionTime)
	{
		return;
	}
	NextPerceptionTime = Now + (Target ? 1.0f : 0.5f);

	AMTCharacterBase* Seen = FindBestTarget(Me);
	if (!Seen || Seen == Target)
	{
		return;
	}
	if (!Target)
	{
		SetCombatTarget(Seen);
		return;
	}
	// Switch only to a much closer threat.
	const float SeenDist = FVector::Dist(Me->GetActorLocation(), Seen->GetActorLocation());
	const float CurrentDist = FVector::Dist(Me->GetActorLocation(), Target->GetActorLocation());
	if (SeenDist < CurrentDist * 0.5f)
	{
		SetCombatTarget(Seen);
	}
}

AMTCharacterBase* AMTEnemyAIController::FindBestTarget(AMTCharacterBase* Me)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	const FVector MyLocation = Me->GetActorLocation();
	const float SightSq = FMath::Square(SightRadius);

	TArray<TPair<float, AMTCharacterBase*>, TInlineAllocator<8>> Candidates;
	for (TActorIterator<AMTCharacterBase> It(World); It; ++It)
	{
		AMTCharacterBase* Other = *It;
		if (Other == Me || !IsValid(Other) || !Other->IsAlive() || !Me->IsHostileTo(Other))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(MyLocation, Other->GetActorLocation());
		if (DistSq <= SightSq)
		{
			Candidates.Emplace(DistSq, Other);
		}
	}
	Candidates.Sort([](const TPair<float, AMTCharacterBase*>& A, const TPair<float, AMTCharacterBase*>& B) { return A.Key < B.Key; });

	// Line-of-sight traces only for the few nearest candidates.
	const int32 MaxChecks = FMath::Min(3, Candidates.Num());
	for (int32 Index = 0; Index < MaxChecks; ++Index)
	{
		if (LineOfSightTo(Candidates[Index].Value))
		{
			return Candidates[Index].Value;
		}
	}
	return nullptr;
}

bool AMTEnemyAIController::IsAllyBlockingLineOfSight(AMTCharacterBase* Me, AActor* Target) const
{
	const UWorld* World = GetWorld();
	if (!World || !Target)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTAIAllyLOS), false, Me);
	FHitResult Hit;
	const FVector Start = Me->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const FVector End = Target->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	if (World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_Pawn), Params))
	{
		const AMTCharacterBase* Blocker = Cast<AMTCharacterBase>(Hit.GetActor());
		return Blocker && Blocker != Target && !Me->IsHostileTo(Blocker);
	}
	return false;
}

bool AMTEnemyAIController::IsTargetCasting(const AMTCharacterBase* Target)
{
	if (!Target)
	{
		return false;
	}
	if (Target->HasStateTag(MTTags::State_Casting) || Target->HasStateTag(MTTags::State_Charging))
	{
		return true;
	}
	const UMTAbilityComponent* AbilityComp = Target->GetAbilities();
	return AbilityComp && AbilityComp->IsCasting();
}

// ---------------------------------------------------------------------------------------------
// Generic utility evaluation
// ---------------------------------------------------------------------------------------------

void AMTEnemyAIController::EvaluateGeneric(AMTCharacterBase* Me, AMTEnemyCharacter* Enemy, AMTCharacterBase* Target, float Now)
{
	const FVector MyLocation = Me->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Dist = FVector::Dist2D(MyLocation, TargetLocation);

	const FMTEnemyData* Data = (Enemy && Enemy->HasEnemyData()) ? &Enemy->GetEnemyData() : nullptr;
	const float Preferred = Data ? FMath::Max(Data->PreferredRange, 100.f) : 200.f;
	const float Aggression = Data ? FMath::Clamp(Data->Aggression, 0.f, 1.f) : 0.6f;
	const bool bIsBoss = Enemy && Enemy->IsBoss();
	const float HealthFraction = Enemy ? Enemy->GetHealthFraction() : 1.f;

	// ---- Retreat / regroup ----
	if (bRetreating)
	{
		return; // still falling back
	}
	if (!bIsBoss && LineageKind == EMTAILineage::Generic && HealthFraction < RetreatHealthFraction && Now >= NextRetreatAllowedTime)
	{
		if (StartRetreat(Me, Target, Now))
		{
			return;
		}
		NextRetreatAllowedTime = Now + 3.f;
	}

	// ---- Context ----
	const bool bTargetCasting = IsTargetCasting(Target);
	const AMTEnemyCharacter* TargetEnemy = Cast<AMTEnemyCharacter>(Target);
	const bool bTargetPunishable = Target->IsStaggered() || (TargetEnemy && TargetEnemy->IsInRecovery());

	// Hand the melee slot to someone else after a while without swinging.
	if (bHoldsToken && !bIsBoss && Now - LastAttackTime > 4.f && Now - TokenAcquiredTime > 4.f)
	{
		ReleaseEngageToken();
	}

	FMTEnemyAttack Attack;
	const bool bHasAttack = Enemy && Enemy->SelectAttack(Target, Dist, Attack);
	const bool bMeleeAttack = bHasAttack && Attack.MaxRange <= MeleeEngageRange;
	const bool bCanEngage = bHasAttack && (!bMeleeAttack || TryAcquireEngageToken(Target, bIsBoss));

	const float ReadyRange = Enemy ? Enemy->GetMaxReadyAttackRange() : 0.f;
	const bool bMeleeKit = ReadyRange > 0.f && ReadyRange <= MeleeEngageRange;
	float DesiredDist = Preferred;
	if (ReadyRange > 0.f)
	{
		DesiredDist = FMath::Min(Preferred, ReadyRange * 0.85f);
	}
	// Enemies without a slot circle at a ring outside the scrum instead of dog-piling.
	const float RingRadius = FMath::Max(Preferred + 350.f, 650.f);
	const bool bWaitingForSlot = bMeleeKit && !bHoldsToken && !TryAcquireEngageToken(Target, bIsBoss);
	if (bWaitingForSlot)
	{
		DesiredDist = RingRadius;
	}

	// ---- Utility scores ----
	float AttackScore = 0.f;
	if (bCanEngage)
	{
		const float Spacing = FMath::Lerp(1.6f, 0.5f, Aggression); // seconds between attacks
		const float Readiness = FMath::Clamp((Now - LastAttackTime) / Spacing, 0.f, 1.f);
		AttackScore = (0.35f + 0.65f * Aggression) * Readiness;
		if (bTargetCasting)
		{
			AttackScore += 0.45f; // opportunity: interrupt / punish the cast
		}
		if (bTargetPunishable)
		{
			AttackScore += 0.55f; // press the advantage
		}
	}

	float ApproachScore = 0.f;
	if (Dist > DesiredDist + 120.f)
	{
		ApproachScore = 0.45f + FMath::Clamp((Dist - DesiredDist) / 1500.f, 0.f, 0.45f);
		if ((bTargetCasting || bTargetPunishable) && !bWaitingForSlot)
		{
			ApproachScore += 0.2f;
		}
	}

	float StrafeScore = 0.25f + (1.f - Aggression) * 0.3f;
	if (Dist < DesiredDist * 0.6f)
	{
		StrafeScore += 0.15f;
	}

	if (Now >= NextLOSCheckTime)
	{
		NextLOSCheckTime = Now + 0.75f;
		bAllyBlockingLOS = IsAllyBlockingLineOfSight(Me, Target);
	}
	const float RepositionScore = (bAllyBlockingLOS && (!bMeleeAttack || Dist > 400.f)) ? 0.7f : 0.f;

	// ---- Act on the best score ----
	const float Best = FMath::Max(FMath::Max(AttackScore, ApproachScore), FMath::Max(StrafeScore, RepositionScore));
	if (AttackScore > 0.f && AttackScore >= Best && Enemy && Enemy->ExecuteAttack(Attack, Target))
	{
		LastAttackTime = Now;
		++NumAttacks;
		LastMoveGoalActor = nullptr;
		SetAction(EMTAIAction::Attack);
		return;
	}
	if (RepositionScore > 0.f && RepositionScore >= Best && MoveToFlank(Me, Target))
	{
		SetAction(EMTAIAction::Reposition);
		return;
	}
	if (ApproachScore > 0.f && ApproachScore >= Best)
	{
		ApproachTarget(Me, Target, FMath::Max(DesiredDist - 60.f, 30.f));
		return;
	}
	const float StrafeRadius = bWaitingForSlot ? RingRadius : FMath::Clamp(Dist, DesiredDist * 0.8f, FMath::Max(DesiredDist * 1.2f, DesiredDist + 100.f));
	DoStrafe(Me, Target, StrafeRadius, Now);
}

void AMTEnemyAIController::ApproachTarget(AMTCharacterBase* Me, AActor* Target, float AcceptanceRadius)
{
	// Avoid re-pathing every evaluation toward the same goal.
	if (CurrentAction == EMTAIAction::Approach && LastMoveGoalActor.Get() == Target && GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}
	Me->SetWalking(false);
	MoveToActor(Target, AcceptanceRadius, true, true, true);
	LastMoveGoalActor = Target;
	SetAction(EMTAIAction::Approach);
}

// ---------------------------------------------------------------------------------------------
// Rudeus: ranged mage kit, keeps 900-1400 cm
// ---------------------------------------------------------------------------------------------

void AMTEnemyAIController::EvaluateRudeus(AMTCharacterBase* Me, AMTCharacterBase* Target, float Now)
{
	UMTAbilityComponent* AbilityComp = Me->GetAbilities();
	if (!AbilityComp)
	{
		return;
	}
	const FVector MyLocation = Me->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Dist = FVector::Dist2D(MyLocation, TargetLocation);
	const bool bTargetCasting = IsTargetCasting(Target);

	// 1) Target in our face: Quagmire under them.
	if (Dist < QuagmireTriggerRange && Now - LastAttackTime > 0.5f && AbilityComp->GetCooldownRemaining(QuagmireAbilityId) <= 0.f)
	{
		if (AbilityComp->ActivateAbilityById(QuagmireAbilityId))
		{
			LastAttackTime = Now;
			++NumAttacks;
			SetAction(EMTAIAction::Attack);
			return;
		}
	}

	// 2) Stone Cannon (hold, then release) inside the band, or to punish a cast.
	const bool bInBand = Dist >= RudeusMinRange * 0.75f && Dist <= RudeusMaxRange * 1.4f;
	const float Spacing = bTargetCasting ? 0.4f : 1.1f;
	if ((bInBand || bTargetCasting) && Now - LastAttackTime > Spacing
		&& AbilityComp->GetCooldownRemaining(StoneCannonAbilityId) <= 0.f && LineOfSightTo(Target))
	{
		if (StartChargedCast(Me, StoneCannonAbilityId, StoneCannonHoldTime))
		{
			LastAttackTime = Now;
			++NumAttacks;
			SetAction(EMTAIAction::Attack);
			return;
		}
	}

	// 3) Filler from the rest of the kit (basic bolts etc.).
	AMTEnemyCharacter* Enemy = Cast<AMTEnemyCharacter>(Me);
	FMTEnemyAttack Filler;
	if (Enemy && Now - LastAttackTime > 1.3f && Enemy->SelectAttack(Target, Dist, Filler)
		&& Filler.AbilityId != StoneCannonAbilityId && Filler.AbilityId != QuagmireAbilityId)
	{
		if (Enemy->ExecuteAttack(Filler, Target))
		{
			LastAttackTime = Now;
			++NumAttacks;
			SetAction(EMTAIAction::Attack);
			return;
		}
	}

	// 4) Spacing.
	if (Dist < RudeusMinRange)
	{
		FVector Away = (MyLocation - TargetLocation).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = -Me->GetActorForwardVector();
		}
		// Back off diagonally so he does not pin himself against walls.
		const FVector Diagonal = (Away + FVector::CrossProduct(FVector::UpVector, Away) * 0.5f * StrafeSign).GetSafeNormal();
		FVector NavPoint;
		if (ProjectToNav(MyLocation + Diagonal * (RudeusMinRange - Dist + 300.f), NavPoint))
		{
			Me->SetWalking(false);
			MoveToLocation(NavPoint, 60.f, true, true, false, true);
			LastMoveGoalActor = nullptr;
			SetAction(EMTAIAction::Retreat);
			return;
		}
		StrafeSign = -StrafeSign;
	}
	else if (Dist > RudeusMaxRange)
	{
		ApproachTarget(Me, Target, (RudeusMinRange + RudeusMaxRange) * 0.5f);
		return;
	}
	DoStrafe(Me, Target, FMath::Clamp(Dist, RudeusMinRange, RudeusMaxRange), Now);
}

bool AMTEnemyAIController::StartChargedCast(AMTCharacterBase* Me, FName AbilityId, float HoldTime)
{
	UMTAbilityComponent* AbilityComp = Me ? Me->GetAbilities() : nullptr;
	if (!AbilityComp || AbilityId.IsNone() || !AbilityComp->ActivateAbilityById(AbilityId))
	{
		return false;
	}
	bChargingCast = true;
	ChargingAbilityId = AbilityId;
	StopMovement();
	LastMoveGoalActor = nullptr;
	GetWorldTimerManager().SetTimer(ChargeTimer, this, &AMTEnemyAIController::ReleaseChargedCast, FMath::Max(0.05f, HoldTime), false);
	return true;
}

void AMTEnemyAIController::ReleaseChargedCast()
{
	if (AMTCharacterBase* Me = Cast<AMTCharacterBase>(GetPawn()))
	{
		if (UMTAbilityComponent* AbilityComp = Me->GetAbilities())
		{
			AbilityComp->ReleaseAbilityById(ChargingAbilityId);
		}
	}
	bChargingCast = false;
	ChargingAbilityId = NAME_None;
}

// ---------------------------------------------------------------------------------------------
// Threat reactions
// ---------------------------------------------------------------------------------------------

bool AMTEnemyAIController::ComputeTimeToImpact(AMTProjectile* Spell, const AMTCharacterBase* Me, float& OutTimeToImpact)
{
	if (!IsValid(Spell) || !Me)
	{
		return false;
	}
	const FVector Velocity = Spell->GetProjectileVelocity();
	const float Speed = Velocity.Size();
	if (Speed < 50.f)
	{
		return false;
	}
	const FVector Direction = Velocity / Speed;
	const FVector SpellLocation = Spell->GetActorLocation();
	const FVector MyLocation = Me->GetActorLocation();
	const float Along = FVector::DotProduct(MyLocation - SpellLocation, Direction);
	if (Along <= 0.f)
	{
		return false; // moving away
	}
	const float Radius = MTAIPrivate::CapsuleRadiusOf(Me);
	const float HalfHeight = MTAIPrivate::CapsuleHalfHeightOf(Me);
	const FVector Miss = (SpellLocation + Direction * Along) - MyLocation;
	if (FVector(Miss.X, Miss.Y, 0.f).Size() > Radius + 110.f || FMath::Abs(Miss.Z) > HalfHeight + 80.f)
	{
		return false; // not heading at us
	}
	OutTimeToImpact = FMath::Max(0.f, Along - Radius) / Speed;
	return true;
}

bool AMTEnemyAIController::TryReactToThreats(AMTCharacterBase* Me, AMTEnemyCharacter* Enemy, float Now)
{
	UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this);
	if (!Telegraphs)
	{
		return false;
	}
	const FVector MyLocation = Me->GetActorLocation();
	const float Radius = MTAIPrivate::CapsuleRadiusOf(Me);

	bool bHasThreat = false;
	float Soonest = TNumericLimits<float>::Max();
	FVector ThreatDirection = FVector::ZeroVector;
	FVector ThreatCenter = MyLocation;
	float ThreatRadius = 0.f;
	uint32 ThreatKey = 0;

	// Announced attacks (enemy/boss telegraphs, player telegraphs if any).
	const TArray<FMTAttackTelegraph> Threats = Telegraphs->GetThreatsTo(MyLocation, Radius + 40.f, 0.5f, Me);
	for (const FMTAttackTelegraph& Threat : Threats)
	{
		const float TimeToImpact = Threat.ImpactTime - Now;
		if (TimeToImpact < 0.05f || TimeToImpact > 0.45f || TimeToImpact >= Soonest)
		{
			continue;
		}
		const AActor* Attacker = Threat.Attacker.Get();
		if (Attacker && (Attacker == Me || !Me->IsHostileTo(Attacker)))
		{
			continue;
		}
		Soonest = TimeToImpact;
		bHasThreat = true;
		ThreatDirection = Threat.Direction;
		ThreatCenter = Threat.ImpactLocation;
		ThreatRadius = Threat.Radius;
		ThreatKey = HashCombine(GetTypeHash(Threat.AttackId), GetTypeHash(FMath::RoundToInt(Threat.ImpactTime * 20.f))) ^ PointerHash(Attacker);
	}

	// Live spells flying at us.
	const bool bOrstedCanCounter = LineageKind == EMTAILineage::Orsted && Me->GetAbilities()
		&& Me->GetAbilities()->GetCooldownRemaining(DisturbMagicAbilityId) <= 0.f;
	const TArray<AMTProjectile*> Spells = Telegraphs->GetSpellsNear(MyLocation, 1200.f, Me);
	for (AMTProjectile* Spell : Spells)
	{
		float TimeToImpact = 0.f;
		if (!ComputeTimeToImpact(Spell, Me, TimeToImpact) || TimeToImpact < 0.05f || TimeToImpact > 0.45f || TimeToImpact >= Soonest)
		{
			continue;
		}
		if (bOrstedCanCounter && Spell->IsDisruptable())
		{
			continue; // the reaction tick collapses it with Disturb Magic instead
		}
		Soonest = TimeToImpact;
		bHasThreat = true;
		ThreatDirection = Spell->GetProjectileVelocity().GetSafeNormal();
		ThreatCenter = MyLocation;
		ThreatRadius = 0.f;
		ThreatKey = PointerHash(Spell);
	}

	if (!bHasThreat || ThreatKey == LastThreatKey)
	{
		return false; // nothing, or already rolled for this threat
	}
	LastThreatKey = ThreatKey;

	const float Reactivity = (Enemy && Enemy->HasEnemyData()) ? FMath::Clamp(Enemy->GetEnemyData().Reactivity, 0.f, 1.f) : 0.3f;
	const float Roll = FMath::FRand();
	if (Roll < Reactivity)
	{
		const FVector DodgeDirection = ComputeDodgeDirection(Me, ThreatDirection, ThreatCenter, ThreatRadius);
		if (Me->Dodge(DodgeDirection))
		{
			++NumDodges;
			StopMovement();
			LastMoveGoalActor = nullptr;
			SetAction(EMTAIAction::Dodge);
			ActionLockUntil = Now + 0.4f;
			return true;
		}
	}
	if (Roll < FMath::Min(1.f, Reactivity * 1.6f))
	{
		StartBlock(Me, BlockDuration);
		return true;
	}
	return false;
}

FVector AMTEnemyAIController::ComputeDodgeDirection(AMTCharacterBase* Me, const FVector& ThreatDirection, const FVector& ThreatCenter, float ThreatRadius) const
{
	const FVector MyLocation = Me->GetActorLocation();
	FVector Forward = ThreatDirection;
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		Forward = Me->GetActorForwardVector();
	}
	FVector Away = MyLocation - ThreatCenter;
	Away.Z = 0.f;

	FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
	if (FVector::DotProduct(Side, Away) < 0.f || (Away.IsNearlyZero() && StrafeSign < 0.f))
	{
		Side = -Side;
	}
	// Large AoE: get out radially. Lines/projectiles: sidestep.
	FVector Result = (ThreatRadius > 250.f && Away.SizeSquared() > 1.f) ? Away.GetSafeNormal() : Side;

	FVector Probe;
	if (!ProjectToNav(MyLocation + Result * 350.f, Probe))
	{
		Result = -Result;
		if (!ProjectToNav(MyLocation + Result * 350.f, Probe))
		{
			Result = -Forward;
		}
	}
	return Result;
}

void AMTEnemyAIController::StartBlock(AMTCharacterBase* Me, float Duration)
{
	if (!Me)
	{
		return;
	}
	if (UMTAttributeComponent* Attr = Me->GetAttributes())
	{
		Attr->SetBlocking(true);
	}
	Me->GetStateTags().AddTag(MTTags::State_Blocking);
	bBlocking = true;
	++NumBlocks;
	StopMovement();
	LastMoveGoalActor = nullptr;
	SetAction(EMTAIAction::Block);
	const UWorld* World = GetWorld();
	ActionLockUntil = (World ? World->GetTimeSeconds() : 0.f) + Duration;
	GetWorldTimerManager().SetTimer(BlockTimer, this, &AMTEnemyAIController::EndBlock, FMath::Max(0.05f, Duration), false);
}

void AMTEnemyAIController::EndBlock()
{
	GetWorldTimerManager().ClearTimer(BlockTimer);
	if (!bBlocking)
	{
		return;
	}
	bBlocking = false;
	if (AMTCharacterBase* Me = Cast<AMTCharacterBase>(GetPawn()))
	{
		if (UMTAttributeComponent* Attr = Me->GetAttributes())
		{
			Attr->SetBlocking(false);
		}
		Me->GetStateTags().RemoveTag(MTTags::State_Blocking);
	}
}

// ---------------------------------------------------------------------------------------------
// Orsted reaction tick: Disturb Magic timing (spell 0.15-0.3 s from impact)
// ---------------------------------------------------------------------------------------------

void AMTEnemyAIController::ReactionTick()
{
	AMTCharacterBase* Me = Cast<AMTCharacterBase>(GetPawn());
	if (!Me || !Me->IsAlive() || LineageKind != EMTAILineage::Orsted || Me->IsStaggered())
	{
		return;
	}
	UMTAbilityComponent* AbilityComp = Me->GetAbilities();
	if (!AbilityComp || AbilityComp->IsCasting() || AbilityComp->GetCooldownRemaining(DisturbMagicAbilityId) > 0.f)
	{
		return;
	}
	UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this);
	if (!Telegraphs)
	{
		return;
	}

	const TArray<AMTProjectile*> Spells = Telegraphs->GetSpellsNear(Me->GetActorLocation(), DisturbMagicRadius, Me);
	for (AMTProjectile* Spell : Spells)
	{
		if (!IsValid(Spell) || !Spell->IsDisruptable())
		{
			continue;
		}
		const AActor* Caster = Spell->GetInstigatorActor();
		if (Caster == Me || (Caster && !Me->IsHostileTo(Caster)))
		{
			continue;
		}
		float TimeToImpact = 0.f;
		if (!ComputeTimeToImpact(Spell, Me, TimeToImpact))
		{
			continue;
		}
		if (TimeToImpact < DisturbMinTimeToImpact || TimeToImpact > DisturbMaxTimeToImpact)
		{
			continue;
		}
		// Orsted abandons a wind-up to collapse the spell.
		if (AMTEnemyCharacter* Enemy = Cast<AMTEnemyCharacter>(Me))
		{
			Enemy->CancelPendingAttack();
		}
		if (AbilityComp->ActivateAbilityById(DisturbMagicAbilityId))
		{
			++NumDisruptAttempts;
			StopMovement();
			LastMoveGoalActor = nullptr;
			SetAction(EMTAIAction::Counter);
			UE_LOG(LogMushoku, Log, TEXT("[AI] %s: Disturb Magic vs %s (%.2fs to impact)"), *Me->GetName(), *Spell->GetName(), TimeToImpact);
			return;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Movement behaviours
// ---------------------------------------------------------------------------------------------

bool AMTEnemyAIController::ProjectToNav(const FVector& Point, FVector& OutPoint) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		OutPoint = Point; // no navmesh (test maps): trust the point
		return true;
	}
	FNavLocation NavLocation;
	if (NavSystem->ProjectPointToNavigation(Point, NavLocation, FVector(150.f, 150.f, 300.f)))
	{
		OutPoint = NavLocation.Location;
		return true;
	}
	return false;
}

void AMTEnemyAIController::DoPatrol(AMTCharacterBase* Me, float Now)
{
	if (CurrentAction != EMTAIAction::Patrol && CurrentAction != EMTAIAction::Idle)
	{
		StopMovement();
		Me->SetSprinting(false);
		bRetreating = false;
		SetAction(EMTAIAction::Idle);
		NextPatrolTime = Now + FMath::FRandRange(1.f, 3.f);
	}
	if (Now < NextPatrolTime || GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}
	NextPatrolTime = Now + FMath::FRandRange(4.f, 9.f);
	if (PatrolRadius <= 0.f)
	{
		SetAction(EMTAIAction::Idle);
		return;
	}
	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Destination;
		if (NavSystem->GetRandomReachablePointInRadius(HomeLocation, PatrolRadius, Destination))
		{
			Me->SetWalking(true);
			MoveToLocation(Destination.Location, 60.f);
			LastMoveGoalActor = nullptr;
			SetAction(EMTAIAction::Patrol);
			return;
		}
	}
	SetAction(EMTAIAction::Idle);
}

void AMTEnemyAIController::DoStrafe(AMTCharacterBase* Me, AActor* Target, float Radius, float Now)
{
	if (Now >= NextStrafeFlipTime)
	{
		StrafeSign = FMath::RandBool() ? 1.f : -1.f;
		NextStrafeFlipTime = Now + FMath::FRandRange(1.8f, 3.5f);
	}
	if (CurrentAction == EMTAIAction::Strafe && GetMoveStatus() == EPathFollowingStatus::Moving && Now < NextStrafeMoveTime)
	{
		return;
	}
	const FVector TargetLocation = Target->GetActorLocation();
	FVector FromTarget = Me->GetActorLocation() - TargetLocation;
	FromTarget.Z = 0.f;
	if (FromTarget.IsNearlyZero())
	{
		FromTarget = -Target->GetActorForwardVector();
	}
	const float Angle = FMath::Atan2(FromTarget.Y, FromTarget.X) + FMath::DegreesToRadians(35.f) * StrafeSign;
	const FVector Desired = TargetLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

	FVector NavPoint;
	if (ProjectToNav(Desired, NavPoint))
	{
		Me->SetWalking(true);
		MoveToLocation(NavPoint, 40.f, true, true, false, true);
		LastMoveGoalActor = nullptr;
		NextStrafeMoveTime = Now + 0.75f;
		SetAction(EMTAIAction::Strafe);
	}
	else
	{
		StrafeSign = -StrafeSign; // wall / ledge: circle the other way
	}
}

bool AMTEnemyAIController::MoveToFlank(AMTCharacterBase* Me, AActor* Target)
{
	FVector FromTarget = Me->GetActorLocation() - Target->GetActorLocation();
	FromTarget.Z = 0.f;
	if (FromTarget.SizeSquared() < 1.f)
	{
		return false;
	}
	for (const float Sign : { 1.f, -1.f })
	{
		const FVector Rotated = FromTarget.RotateAngleAxis(60.f * Sign * StrafeSign, FVector::UpVector);
		FVector NavPoint;
		if (ProjectToNav(Target->GetActorLocation() + Rotated, NavPoint))
		{
			Me->SetWalking(false);
			MoveToLocation(NavPoint, 50.f, true, true, false, true);
			LastMoveGoalActor = nullptr;
			return true;
		}
	}
	return false;
}

bool AMTEnemyAIController::StartRetreat(AMTCharacterBase* Me, AActor* Target, float Now)
{
	const FVector MyLocation = Me->GetActorLocation();
	FVector Away = MyLocation - Target->GetActorLocation();
	Away.Z = 0.f;
	Away = Away.GetSafeNormal();
	if (Away.IsNearlyZero())
	{
		Away = -Me->GetActorForwardVector();
	}

	// Regroup: bias the fallback point toward nearby allies.
	FVector AllySum = FVector::ZeroVector;
	int32 AllyCount = 0;
	for (TActorIterator<AMTEnemyCharacter> It(GetWorld()); It; ++It)
	{
		const AMTEnemyCharacter* Other = *It;
		if (Other == Me || !Other->IsAlive() || Me->IsHostileTo(Other))
		{
			continue;
		}
		if (FVector::DistSquared(Other->GetActorLocation(), MyLocation) < FMath::Square(1800.f))
		{
			AllySum += Other->GetActorLocation();
			++AllyCount;
		}
	}
	FVector Goal = MyLocation + Away * 800.f;
	if (AllyCount > 0)
	{
		Goal = FMath::Lerp(Goal, AllySum / static_cast<float>(AllyCount), 0.5f);
	}

	FVector NavPoint;
	if (!ProjectToNav(Goal, NavPoint))
	{
		return false;
	}
	ReleaseEngageToken();
	Me->SetWalking(false);
	Me->SetSprinting(true);
	MoveToLocation(NavPoint, 80.f);
	LastMoveGoalActor = nullptr;
	bRetreating = true;
	RetreatUntil = Now + 2.5f;
	NextRetreatAllowedTime = Now + 12.f;
	SetAction(EMTAIAction::Retreat);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Engagement slots
// ---------------------------------------------------------------------------------------------

bool AMTEnemyAIController::TryAcquireEngageToken(AActor* Target, bool bForce)
{
	if (!Target)
	{
		return false;
	}
	if (bHoldsToken && EngagedTarget.Get() == Target)
	{
		return true;
	}
	ReleaseEngageToken();
	MTAIPrivate::PruneEngagements();

	MTAIPrivate::FAttackerList& List = MTAIPrivate::EngagementMap().FindOrAdd(TWeakObjectPtr<AActor>(Target));
	List.RemoveAll([Target](const TWeakObjectPtr<AMTEnemyAIController>& Entry)
	{
		const AMTEnemyAIController* Other = Entry.Get();
		return !Other || !Other->bHoldsToken || Other->EngagedTarget.Get() != Target;
	});
	if (!bForce && List.Num() >= FMath::Max(1, MaxAttackersPerTarget))
	{
		return false;
	}
	List.Add(TWeakObjectPtr<AMTEnemyAIController>(this));
	EngagedTarget = Target;
	bHoldsToken = true;
	const UWorld* World = GetWorld();
	TokenAcquiredTime = World ? World->GetTimeSeconds() : 0.f;
	return true;
}

void AMTEnemyAIController::ReleaseEngageToken()
{
	if (bHoldsToken)
	{
		if (MTAIPrivate::FAttackerList* List = MTAIPrivate::EngagementMap().Find(EngagedTarget))
		{
			List->RemoveAll([this](const TWeakObjectPtr<AMTEnemyAIController>& Entry) { return !Entry.IsValid() || Entry.Get() == this; });
			if (List->Num() == 0)
			{
				MTAIPrivate::EngagementMap().Remove(EngagedTarget);
			}
		}
	}
	bHoldsToken = false;
	EngagedTarget = nullptr;
}

int32 AMTEnemyAIController::GetEngagedAttackerCount(const AActor* Target)
{
	const MTAIPrivate::FAttackerList* List = MTAIPrivate::EngagementMap().Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Target)));
	if (!List)
	{
		return 0;
	}
	int32 Count = 0;
	for (const TWeakObjectPtr<AMTEnemyAIController>& Entry : *List)
	{
		if (Entry.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

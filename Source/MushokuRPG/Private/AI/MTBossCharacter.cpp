#include "AI/MTBossCharacter.h"

#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbilityComponent.h"
#include "Core/MTGameEvents.h"
#include "Core/MTGameplayTags.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Quests/MTQuestSubsystem.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MTBoss"

AMTBossCharacter::AMTBossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CorpseLifeSpan = 0.f; // bosses stay down for the victory moment
}

void AMTBossCharacter::BeginPlay()
{
	Super::BeginPlay(); // loads the enemy row

	if (UMTAttributeComponent* Attr = GetAttributes())
	{
		Attr->bCrowdControlImmune = true;
		Attr->bIsWeak = false;
	}
	ResolvedArenaCenter = ArenaCenter.IsZero() ? GetActorLocation() : ArenaCenter;
	CurrentPhase = ComputePhaseForHealth(GetHealthFraction());

	GetWorldTimerManager().SetTimer(LeashTimer, this, &AMTBossCharacter::UpdateLeash, 0.5f, true, 0.5f);

	if (!EnemyId.IsNone())
	{
		if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
		{
			Quests->RegisterQuestActor(EnemyId, this); // DefeatBoss markers
		}
	}
}

void AMTBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!EnemyId.IsNone())
	{
		if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
		{
			Quests->UnregisterQuestActor(EnemyId, this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AMTBossCharacter::GetUsableAttacks(TArray<FMTEnemyAttack>& OutAttacks) const
{
	TArray<FMTEnemyAttack> All;
	Super::GetUsableAttacks(All);
	OutAttacks.Reset(All.Num());
	for (const FMTEnemyAttack& Attack : All)
	{
		if (Attack.MinPhase <= CurrentPhase)
		{
			OutAttacks.Add(Attack);
		}
	}
}

bool AMTBossCharacter::IsInRecoveryWindow() const
{
	return !bPhaseTransition && IsInRecovery();
}

bool AMTBossCharacter::IsLocationInsideArena(const FVector& Location, float Tolerance) const
{
	return FVector::Dist2D(Location, ResolvedArenaCenter) <= ArenaRadius + Tolerance;
}

int32 AMTBossCharacter::ComputePhaseForHealth(float HealthFraction) const
{
	if (!bHasEnemyData)
	{
		return 0;
	}
	int32 Phase = 0;
	for (const float Threshold : EnemyData.PhaseThresholds)
	{
		if (Threshold > 0.f && Threshold < 1.f && HealthFraction <= Threshold)
		{
			++Phase;
		}
	}
	return Phase;
}

void AMTBossCharacter::HandleDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result)
{
	Super::HandleDamaged(Spec, Result);
	if (!IsAlive() || bPhaseTransition)
	{
		return;
	}
	const int32 NewPhase = ComputePhaseForHealth(GetHealthFraction());
	if (NewPhase > CurrentPhase)
	{
		EnterPhase(NewPhase);
	}
}

void AMTBossCharacter::EnterPhase(int32 NewPhase)
{
	CurrentPhase = NewPhase;
	bPhaseTransition = true;

	CancelPendingAttack();
	if (UMTAbilityComponent* AbilityComp = GetAbilities())
	{
		AbilityComp->CancelAll();
	}
	if (AController* MyController = GetController())
	{
		MyController->StopMovement();
	}
	if (UMTAttributeComponent* Attr = GetAttributes())
	{
		Attr->SetInvulnerableFor(PhaseTransitionDuration);
	}
	PlaySoftMontage(PhaseTransitionMontage);

	// The AI treats the transition as busy time.
	if (const UWorld* World = GetWorld())
	{
		RecoveryEndTime = World->GetTimeSeconds() + PhaseTransitionDuration;
	}

	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		const FText Name = (bHasEnemyData && !EnemyData.DisplayName.IsEmpty()) ? EnemyData.DisplayName : FText::FromName(EnemyId);
		Events->Notify(FText::Format(LOCTEXT("PhaseChange", "{0} unleashes its true strength! (Phase {1})"), Name, FText::AsNumber(CurrentPhase + 1)),
			FLinearColor(1.f, 0.45f, 0.2f));
	}
	UE_LOG(LogMushoku, Log, TEXT("Boss %s entered phase %d"), *EnemyId.ToString(), CurrentPhase);

	OnPhaseChanged.Broadcast(CurrentPhase);
	ReceivePhaseChanged(CurrentPhase);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &AMTBossCharacter::EndPhaseTransition, FMath::Max(0.1f, PhaseTransitionDuration), false);
}

void AMTBossCharacter::EndPhaseTransition()
{
	bPhaseTransition = false;
	// A big hit during the roar may have crossed another threshold.
	const int32 PendingPhase = ComputePhaseForHealth(GetHealthFraction());
	if (IsAlive() && PendingPhase > CurrentPhase)
	{
		EnterPhase(PendingPhase);
	}
}

void AMTBossCharacter::OnAttackActivated(const FMTEnemyAttack& Attack, bool bActivated, float CastTime)
{
	Super::OnAttackActivated(Attack, bActivated, CastTime);
	if (!bActivated || !Attack.bIsImportant || Attack.TelegraphTime < BigAttackTelegraphThreshold)
	{
		return;
	}
	// Big attacks leave an explicit, readable punish window.
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Recovery = FMath::Max(0.f, Attack.RecoveryTime) * BigAttackRecoveryMultiplier;
	RecoveryEndTime = World->GetTimeSeconds() + CastTime + Recovery;
	ReceiveRecoveryWindow(CastTime + Recovery);
}

bool AMTBossCharacter::GetDefensiveReaction(const AMTCharacterBase* Target, bool& bOutSidestep)
{
	bOutSidestep = false;
	const UWorld* World = GetWorld();
	if (!World || !Target || bPhaseTransition || IsBusy())
	{
		return false;
	}
	const float Now = World->GetTimeSeconds();
	if (Now < NextDefensiveTime || !Target->HasStateTag(MTTags::State_Charging))
	{
		return false;
	}
	NextDefensiveTime = Now + DefensiveReactionCooldown;
	if (FMath::FRand() > DefensiveReactionChance)
	{
		return false;
	}
	bOutSidestep = FMath::FRand() < SidestepChance;
	return true;
}

void AMTBossCharacter::UpdateLeash()
{
	if (!IsAlive())
	{
		return;
	}
	const float DistFromCenter = FVector::Dist2D(GetActorLocation(), ResolvedArenaCenter);
	if (!bReturningToArena && DistFromCenter > ArenaRadius)
	{
		// Pulled out of the arena: walk back. No regeneration (no leash-heal exploits either way).
		bReturningToArena = true;
		CancelPendingAttack();
		UE_LOG(LogMushoku, Verbose, TEXT("Boss %s leashing back to arena."), *EnemyId.ToString());
	}
	else if (bReturningToArena && DistFromCenter < ArenaRadius * 0.35f)
	{
		bReturningToArena = false;
	}
}

void AMTBossCharacter::HandleDeath(AActor* Killer)
{
	Super::HandleDeath(Killer);
	if (bBossDefeatHandled)
	{
		return;
	}
	bBossDefeatHandled = true;
	GetWorldTimerManager().ClearTimer(LeashTimer);
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	bPhaseTransition = false;

	if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
	{
		Quests->UnregisterQuestActor(EnemyId, this);
	}
	if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Progression->RecordBossDefeated(EnemyId);
		Progression->AddSpins(EMTRollCategory::Character, 1);
		Progression->AddSpins(EMTRollCategory::Element, 1);
	}
	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		const FText Name = (bHasEnemyData && !EnemyData.DisplayName.IsEmpty()) ? EnemyData.DisplayName : FText::FromName(EnemyId);
		Events->Notify(FText::Format(LOCTEXT("BossDefeated", "{0} defeated! +1 Character Spin, +1 Element Spin"), Name), FLinearColor(1.f, 0.8f, 0.3f));
		Events->OnBossDefeated.Broadcast(EnemyId);
	}
	UE_LOG(LogMushoku, Log, TEXT("Boss defeated: %s"), *EnemyId.ToString());
}

#undef LOCTEXT_NAMESPACE

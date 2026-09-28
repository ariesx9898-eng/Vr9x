#include "AI/MTEnemyCharacter.h"

#include "AI/MTEnemyAIController.h"
#include "Quests/MTPickupActor.h"
#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbilityComponent.h"
#include "Combat/MTTelegraphSubsystem.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameplayTags.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "TimerManager.h"

AMTEnemyCharacter::AMTEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AMTEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	TeamId = FGenericTeamId(2);
	DropClass = AMTPickupActor::StaticClass();
	ApplyAIMovementSettings();
}

void AMTEnemyCharacter::ApplyAIMovementSettings()
{
	// Face the controller's focus (target) while moving: strafing and readable wind-ups.
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = true;
		if (Movement->RotationRate.Yaw <= 0.f)
		{
			Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
		}
	}
}

void AMTEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	InitializeFromData();
}

void AMTEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void AMTEnemyCharacter::InitializeFromData()
{
	GameplayId = EnemyId;
	SetGenericTeamId(FGenericTeamId(2));

	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTEnemyData* Row = (Registry && !EnemyId.IsNone()) ? Registry->FindEnemy(EnemyId) : nullptr;
	if (!Row)
	{
		bHasEnemyData = false;
		UE_LOG(LogMushoku, Warning, TEXT("Enemy %s: no enemy row '%s' - using defaults."), *GetName(), *EnemyId.ToString());
		IdentityTags.AddTag(MTTags::Enemy);
		return;
	}
	EnemyData = *Row;
	bHasEnemyData = true;

	const bool bHasLineage = !EnemyData.CharacterLineage.IsNone();
	if (bHasLineage)
	{
		// Lineage opponents (arena Orsted / Rudeus) use the full player kit.
		ApplyCharacterLineage(EnemyData.CharacterLineage);
	}
	else
	{
		if (!EnemyData.Mesh.IsNull())
		{
			if (USkeletalMesh* LoadedMesh = EnemyData.Mesh.LoadSynchronous())
			{
				GetMesh()->SetSkeletalMeshAsset(LoadedMesh);
			}
		}
		if (!EnemyData.AnimClass.IsNull())
		{
			if (UClass* AnimClass = EnemyData.AnimClass.LoadSynchronous())
			{
				GetMesh()->SetAnimInstanceClass(AnimClass);
			}
		}
	}

	if (UMTAttributeComponent* Attr = GetAttributes())
	{
		if (!bHasLineage || !bUseLineageAttributes)
		{
			Attr->InitializeAttributes(EnemyData.MaxHealth, EnemyMaxMana, 150.f, EnemyData.MaxPoise, EnemyMaxMana * 0.1f);
		}
		Attr->bIsWeak = EnemyData.bIsWeak;
		Attr->bCrowdControlImmune = EnemyData.bCrowdControlImmune || EnemyData.bIsBoss;
	}

	if (!bHasLineage && EnemyData.MoveSpeed > 0.f)
	{
		BaseRunSpeed = EnemyData.MoveSpeed;
		BaseWalkSpeed = EnemyData.MoveSpeed * 0.45f;
		BaseSprintSpeed = EnemyData.MoveSpeed * 1.35f;
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = EnemyData.MoveSpeed;
		}
		UpdateMovementFromModifiers();
	}
	if (EnemyData.Scale > 0.f && !FMath::IsNearlyEqual(EnemyData.Scale, 1.f))
	{
		SetActorScale3D(FVector(EnemyData.Scale));
	}

	IdentityTags.AppendTags(EnemyData.Tags);
	IdentityTags.AddTag(MTTags::Enemy);
	if (EnemyData.bIsBoss)
	{
		IdentityTags.AddTag(MTTags::Enemy_Boss);
	}

	LineageAttacks.Reset();
	if (EnemyData.Attacks.Num() == 0 && bHasLineage)
	{
		BuildAttacksFromLineage();
	}
	ApplyAIMovementSettings();
}

void AMTEnemyCharacter::BuildAttacksFromLineage()
{
	const FMTCharacterData* Lineage = GetCharacterData();
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	if (!Lineage || !Registry)
	{
		return;
	}
	TArray<FName> AbilityIds;
	if (!Lineage->BasicAbility.IsNone())
	{
		AbilityIds.Add(Lineage->BasicAbility);
	}
	AbilityIds.Append(Lineage->Abilities);

	for (const FName& AbilityId : AbilityIds)
	{
		const FMTAbilityData* Ability = Registry->FindAbility(AbilityId);
		if (!Ability)
		{
			continue;
		}
		// Counters, buffs and structures are situational; the AI controller uses them reactively.
		if (Ability->Behavior == EMTAbilityBehavior::Counter || Ability->Behavior == EMTAbilityBehavior::Buff
			|| Ability->Behavior == EMTAbilityBehavior::Structure)
		{
			continue;
		}
		FMTEnemyAttack Attack;
		Attack.AttackId = AbilityId;
		Attack.AbilityId = AbilityId;
		switch (Ability->Behavior)
		{
		case EMTAbilityBehavior::Melee:
			Attack.MinRange = 0.f;
			Attack.MaxRange = FMath::Clamp(Ability->Range, 150.f, 400.f);
			Attack.TelegraphTime = 0.3f;
			Attack.RecoveryTime = 0.45f;
			break;
		case EMTAbilityBehavior::Dash:
			Attack.MinRange = 350.f;
			Attack.MaxRange = FMath::Max(Ability->DashDistance, 700.f);
			Attack.TelegraphTime = 0.25f;
			Attack.RecoveryTime = 0.35f;
			Attack.Weight = 0.7f;
			break;
		case EMTAbilityBehavior::Zone:
			Attack.MinRange = 0.f;
			Attack.MaxRange = FMath::Clamp(Ability->Range, 300.f, 1200.f);
			Attack.TelegraphTime = 0.35f;
			Attack.RecoveryTime = 0.6f;
			Attack.Weight = 0.6f;
			break;
		default:
			Attack.MinRange = 250.f;
			Attack.MaxRange = FMath::Clamp(Ability->Range, 600.f, 2200.f);
			Attack.TelegraphTime = 0.35f;
			Attack.RecoveryTime = 0.5f;
			break;
		}
		const bool bBasic = AbilityId == Lineage->BasicAbility;
		Attack.Weight *= bBasic ? 1.5f : 1.f;
		Attack.bIsImportant = !bBasic;
		LineageAttacks.Add(Attack);
	}
}

// ---------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------

bool AMTEnemyCharacter::IsInRecovery() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < RecoveryEndTime;
}

bool AMTEnemyCharacter::IsBusy() const
{
	if (bTelegraphing || IsInRecovery())
	{
		return true;
	}
	const UMTAbilityComponent* AbilityComp = GetAbilities();
	return AbilityComp && AbilityComp->IsCasting();
}

bool AMTEnemyCharacter::IsBoss() const
{
	return bHasEnemyData && EnemyData.bIsBoss;
}

float AMTEnemyCharacter::GetHealthFraction() const
{
	const UMTAttributeComponent* Attr = GetAttributes();
	return (Attr && Attr->GetMaxHealth() > 0.f) ? Attr->GetHealth() / Attr->GetMaxHealth() : 0.f;
}

FName AMTEnemyCharacter::GetLineageId() const
{
	return (bHasEnemyData && !EnemyData.CharacterLineage.IsNone()) ? EnemyData.CharacterLineage : GetCharacterId();
}

void AMTEnemyCharacter::GetUsableAttacks(TArray<FMTEnemyAttack>& OutAttacks) const
{
	OutAttacks = (bHasEnemyData && EnemyData.Attacks.Num() > 0) ? EnemyData.Attacks : LineageAttacks;
}

float AMTEnemyCharacter::GetMaxReadyAttackRange() const
{
	TArray<FMTEnemyAttack> Attacks;
	GetUsableAttacks(Attacks);
	const UMTAbilityComponent* AbilityComp = GetAbilities();
	float Best = 0.f;
	for (const FMTEnemyAttack& Attack : Attacks)
	{
		if (!Attack.AbilityId.IsNone() && (!AbilityComp || AbilityComp->GetCooldownRemaining(Attack.AbilityId) <= 0.f))
		{
			Best = FMath::Max(Best, Attack.MaxRange);
		}
	}
	return Best;
}

bool AMTEnemyCharacter::SelectAttack(const AActor* Target, float Distance, FMTEnemyAttack& OutAttack)
{
	TArray<FMTEnemyAttack> Attacks;
	GetUsableAttacks(Attacks);
	const UMTAbilityComponent* AbilityComp = GetAbilities();

	TArray<TPair<int32, float>, TInlineAllocator<8>> Candidates;
	float TotalWeight = 0.f;
	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		const FMTEnemyAttack& Attack = Attacks[Index];
		if (Attack.AbilityId.IsNone() || Distance < Attack.MinRange || Distance > Attack.MaxRange)
		{
			continue;
		}
		if (AbilityComp && AbilityComp->GetCooldownRemaining(Attack.AbilityId) > 0.f)
		{
			continue;
		}
		float Weight = FMath::Max(Attack.Weight, 0.01f);
		if (Attacks.Num() > 1 && Attack.AttackId == LastAttackId)
		{
			Weight *= 0.35f; // avoid repeating the same move back to back
		}
		Candidates.Emplace(Index, Weight);
		TotalWeight += Weight;
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (const TPair<int32, float>& Candidate : Candidates)
	{
		Roll -= Candidate.Value;
		if (Roll <= 0.f)
		{
			OutAttack = Attacks[Candidate.Key];
			return true;
		}
	}
	OutAttack = Attacks[Candidates.Last().Key];
	return true;
}

// ---------------------------------------------------------------------------------------------
// Attack pipeline
// ---------------------------------------------------------------------------------------------

bool AMTEnemyCharacter::ExecuteAttack(const FMTEnemyAttack& Attack, AActor* Target)
{
	UWorld* World = GetWorld();
	UMTAbilityComponent* AbilityComp = GetAbilities();
	if (!World || !AbilityComp || !IsValid(Target) || !IsAlive() || IsBusy() || IsStaggered() || Attack.AbilityId.IsNone())
	{
		return false;
	}
	if (AbilityComp->GetCooldownRemaining(Attack.AbilityId) > 0.f)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTAbilityData* Ability = Registry ? Registry->FindAbility(Attack.AbilityId) : nullptr;

	const FVector MyLocation = GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	FVector ToTarget = TargetLocation - MyLocation;
	ToTarget.Z = 0.f;
	const float Distance = ToTarget.Size();
	const FVector Direction = Distance > KINDA_SMALL_NUMBER ? ToTarget / Distance : GetActorForwardVector();
	const FRotator FacingRotation(0.f, Direction.Rotation().Yaw, 0.f);

	// Plant and face: wind-ups must read clearly.
	if (AController* MyController = GetController())
	{
		MyController->StopMovement();
	}
	SetLockTarget(Target);
	if (Attack.TelegraphTime < 0.25f)
	{
		SetActorRotation(FacingRotation);
	}

	FMTAttackTelegraph Telegraph;
	Telegraph.Attacker = this;
	Telegraph.AttackId = Attack.AttackId.IsNone() ? Attack.AbilityId : Attack.AttackId;
	Telegraph.PredictedAttackerLocation = MyLocation;
	Telegraph.PredictedAttackerRotation = FacingRotation;
	Telegraph.Direction = Direction;
	Telegraph.Radius = (Ability && Ability->AOERadius > 0.f) ? Ability->AOERadius : 150.f;
	Telegraph.bIsMagic = Ability && Ability->Element != EMTElement::None;
	Telegraph.bIsImportant = Attack.bIsImportant;

	const float CastTime = Ability ? FMath::Max(0.f, Ability->CastTime) : 0.f;
	float TravelTime = 0.f;
	const EMTAbilityBehavior Behavior = Ability ? Ability->Behavior : EMTAbilityBehavior::Melee;
	switch (Behavior)
	{
	case EMTAbilityBehavior::Projectile:
	case EMTAbilityBehavior::Sequence:
		Telegraph.ImpactLocation = TargetLocation;
		Telegraph.Length = Distance;
		TravelTime = (Ability && Ability->ProjectileSpeed > 0.f) ? Distance / Ability->ProjectileSpeed : 0.f;
		break;
	case EMTAbilityBehavior::Dash:
		Telegraph.PredictedAttackerLocation = TargetLocation - Direction * 120.f;
		Telegraph.ImpactLocation = TargetLocation;
		Telegraph.Length = Distance;
		break;
	case EMTAbilityBehavior::Melee:
		Telegraph.ImpactLocation = MyLocation + Direction * FMath::Min(Distance, FMath::Max(Attack.MaxRange, 100.f));
		break;
	default:
		Telegraph.ImpactLocation = TargetLocation;
		break;
	}
	// When the hit actually lands: telegraph wait + the ability's own wind-up (+ flight time).
	Telegraph.ImpactTime = Now + FMath::Max(0.f, Attack.TelegraphTime) + CastTime + TravelTime;

	if (UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this))
	{
		Telegraphs->PublishTelegraph(Telegraph);
	}

	PendingAttack = Attack;
	PendingTarget = Target;
	LastAttackId = Attack.AttackId;
	bTelegraphing = true;
	ReceiveAttackTelegraphed(Telegraph.AttackId, Attack.TelegraphTime);

	if (Attack.TelegraphTime <= KINDA_SMALL_NUMBER)
	{
		ReleasePendingAttack();
	}
	else
	{
		GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &AMTEnemyCharacter::ReleasePendingAttack, Attack.TelegraphTime, false);
	}
	return true;
}

void AMTEnemyCharacter::ReleasePendingAttack()
{
	if (!bTelegraphing)
	{
		return;
	}
	bTelegraphing = false;
	UWorld* World = GetWorld();
	if (!World || !IsAlive())
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (IsStaggered())
	{
		RecoveryEndTime = Now + 0.3f;
		ReceiveAttackReleased(PendingAttack.AttackId, false);
		return;
	}

	if (AActor* Target = PendingTarget.Get())
	{
		FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
		ToTarget.Z = 0.f;
		if (!ToTarget.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));
		}
		SetLockTarget(Target);
	}

	UMTAbilityComponent* AbilityComp = GetAbilities();
	const bool bActivated = AbilityComp && AbilityComp->ActivateAbilityById(PendingAttack.AbilityId);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTAbilityData* Ability = Registry ? Registry->FindAbility(PendingAttack.AbilityId) : nullptr;
	const float CastTime = Ability ? FMath::Max(0.f, Ability->CastTime) : 0.f;

	OnAttackActivated(PendingAttack, bActivated, CastTime);
	ReceiveAttackReleased(PendingAttack.AttackId, bActivated);
	if (!bActivated)
	{
		UE_LOG(LogMushoku, Verbose, TEXT("Enemy %s: ability %s failed to activate."), *GetName(), *PendingAttack.AbilityId.ToString());
	}
}

void AMTEnemyCharacter::OnAttackActivated(const FMTEnemyAttack& Attack, bool bActivated, float CastTime)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	RecoveryEndTime = Now + (bActivated ? CastTime + FMath::Max(0.f, Attack.RecoveryTime) : 0.25f);
}

void AMTEnemyCharacter::CancelPendingAttack()
{
	if (bTelegraphing)
	{
		bTelegraphing = false;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	}
}

// ---------------------------------------------------------------------------------------------
// Death: rewards + drops
// ---------------------------------------------------------------------------------------------

void AMTEnemyCharacter::HandleDeath(AActor* Killer)
{
	const bool bFirstDeath = !bDeathHandled;
	bDeathHandled = true;
	CancelPendingAttack();

	Super::HandleDeath(Killer); // broadcasts OnEnemyKilled

	if (!bFirstDeath)
	{
		return;
	}
	GrantKillRewards(Killer);
	SpawnDrops();
	// Overrides the base 10 s default: 0 keeps the body (bosses).
	SetLifeSpan(FMath::Max(0.f, CorpseLifeSpan));
}

void AMTEnemyCharacter::NotifyPerfectDefense(FName Kind)
{
	Super::NotifyPerfectDefense(Kind);
	OnPerfectDefense.Broadcast(this, Kind);
}

void AMTEnemyCharacter::GrantKillRewards(AActor* Killer)
{
	if (!bHasEnemyData || EnemyData.XPReward <= 0)
	{
		return;
	}
	// Kills by other AI (infighting, allies) do not pay out.
	if (const APawn* KillerPawn = Cast<APawn>(Killer))
	{
		if (!KillerPawn->IsPlayerControlled() && KillerPawn->GetController() != nullptr)
		{
			return;
		}
	}
	if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Progression->AddXP(EnemyData.XPReward);
	}
}

void AMTEnemyCharacter::SpawnDrops()
{
	UWorld* World = GetWorld();
	if (!World || !bHasEnemyData || EnemyData.DropTable.Num() == 0 || !DropClass)
	{
		return;
	}
	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Base = GetActorLocation() - FVector(0.f, 0.f, HalfHeight - 40.f);

	int32 DropIndex = 0;
	for (const TPair<FName, float>& Entry : EnemyData.DropTable)
	{
		if (Entry.Key.IsNone() || Entry.Value <= 0.f)
		{
			continue;
		}
		// Value = drop chance (0..1). Values above 1 guarantee floor(Value) plus a chance for one more.
		int32 Count = FMath::FloorToInt(Entry.Value);
		if (FMath::FRand() < Entry.Value - static_cast<float>(Count))
		{
			++Count;
		}
		if (Count <= 0)
		{
			continue;
		}
		const float Angle = static_cast<float>(DropIndex++) * 2.39996f; // golden-angle spread
		const FVector Offset(FMath::Cos(Angle) * 70.f, FMath::Sin(Angle) * 70.f, 0.f);
		const FTransform SpawnTransform(FRotator::ZeroRotator, Base + Offset);
		AMTPickupActor* Drop = World->SpawnActorDeferred<AMTPickupActor>(DropClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Drop)
		{
			continue;
		}
		Drop->ItemId = Entry.Key;
		Drop->Count = Count;
		Drop->RespawnTime = 0.f;
		Drop->PickupDelay = 0.4f;
		Drop->FinishSpawning(SpawnTransform);
		Drop->SetLifeSpan(120.f);
	}
}

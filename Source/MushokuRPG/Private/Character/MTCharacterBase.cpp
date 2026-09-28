#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbilityComponent.h"
#include "Abilities/MTAbility.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameEvents.h"
#include "Core/MTGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "MotionWarpingComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

AMTCharacterBase::AMTCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	Attributes = CreateDefaultSubobject<UMTAttributeComponent>(TEXT("Attributes"));
	Abilities = CreateDefaultSubobject<UMTAbilityComponent>(TEXT("Abilities"));
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	CMC->bOrientRotationToMovement = true;
	CMC->RotationRate = FRotator(0.f, 540.f, 0.f);
	CMC->GravityScale = 1.6f;          // snappier arcs than default, not floaty
	CMC->AirControl = 0.35f;
	CMC->JumpZVelocity = 520.f;
	CMC->BrakingFrictionFactor = 1.f;
	CMC->bUseSeparateBrakingFriction = true;
	CMC->BrakingFriction = 6.f;
	CMC->GroundFriction = 8.f;
	CMC->MaxAcceleration = 2048.f;
	CMC->BrakingDecelerationWalking = 2048.f;
	CMC->bCanWalkOffLedgesWhenCrouching = true;
	CMC->PerchRadiusThreshold = 12.f;  // avoid perching on prop edges
	CMC->SetWalkableFloorAngle(46.f);
	CMC->NavAgentProps.bCanCrouch = false;

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	ElementSlots.Init(EMTElement::None, 2);
}

void AMTCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	Attributes->OnDeath.AddDynamic(this, &AMTCharacterBase::HandleDeath);
	Attributes->OnDamaged.AddDynamic(this, &AMTCharacterBase::HandleDamaged);
	UpdateMovementFromModifiers();
}

const FMTCharacterData* AMTCharacterBase::GetCharacterData() const
{
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	return Registry ? Registry->FindCharacter(CharacterId) : nullptr;
}

EMTStance AMTCharacterBase::GetStance() const
{
	const FMTCharacterData* Data = GetCharacterData();
	return Data ? Data->Stance : EMTStance::Generic;
}

void AMTCharacterBase::ApplyCharacterLineage(FName NewCharacterId)
{
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTCharacterData* Data = Registry ? Registry->FindCharacter(NewCharacterId) : nullptr;
	if (!Data)
	{
		UE_LOG(LogMushoku, Warning, TEXT("%s: unknown character lineage '%s'"), *GetName(), *NewCharacterId.ToString());
		return;
	}
	CharacterId = NewCharacterId;
	if (GameplayId.IsNone())
	{
		GameplayId = NewCharacterId;
	}

	// Body: capsule first, then mesh so feet sit exactly on the capsule bottom (no floating/sinking).
	GetCapsuleComponent()->SetCapsuleSize(Data->CapsuleRadius, Data->CapsuleHalfHeight);
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, Data->MeshOffsetZ));
	if (USkeletalMesh* Mesh = Data->Mesh.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(Mesh);
	}
	else
	{
		UE_LOG(LogMushoku, Warning, TEXT("%s: mesh for '%s' not imported yet (%s)"), *GetName(), *NewCharacterId.ToString(), *Data->Mesh.ToString());
	}
	if (UClass* AnimClass = Data->AnimClass.LoadSynchronous())
	{
		GetMesh()->SetAnimInstanceClass(AnimClass);
	}

	BaseWalkSpeed = Data->WalkSpeed;
	BaseRunSpeed = Data->RunSpeed;
	BaseSprintSpeed = Data->SprintSpeed;
	BaseAcceleration = Data->Acceleration;
	BaseJumpZ = Data->JumpZVelocity;
	BaseDodgeDistance = Data->DodgeDistance;
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	CMC->BrakingDecelerationWalking = Data->BrakingDeceleration;
	CMC->RotationRate = FRotator(0.f, Data->RotationRateYaw, 0.f);

	Attributes->InitializeAttributes(Data->MaxHealth * RaceHealthMultiplier, Data->MaxMana * RaceManaMultiplier,
		Data->MaxStamina * RaceStaminaMultiplier, Data->MaxPoise, Data->ManaRegen);
	LineagePassiveStats = Data->PassiveStats;
	FMTStatModifier Combined = LineagePassiveStats;
	Combined.Combine(RacePassiveStats);
	Attributes->SetPermanentModifiers(Combined, RacePassiveMovement);

	Abilities->CancelAll();
	Abilities->SetSlot(EMTAbilitySlot::Basic, Data->BasicAbility);
	Abilities->SetSlot(EMTAbilitySlot::Character1, Data->Abilities.IsValidIndex(0) ? Data->Abilities[0] : NAME_None);
	Abilities->SetSlot(EMTAbilitySlot::Character2, Data->Abilities.IsValidIndex(1) ? Data->Abilities[1] : NAME_None);
	Abilities->SetSlot(EMTAbilitySlot::Character3, Data->Abilities.IsValidIndex(2) ? Data->Abilities[2] : NAME_None);
	Abilities->SetSlot(EMTAbilitySlot::Special, Data->SpecialAbility);
	Abilities->SetSlot(EMTAbilitySlot::Awakening, Data->AwakeningAbility);

	UpdateMovementFromModifiers();
	OnLineageChanged.Broadcast(CharacterId);
}

void AMTCharacterBase::ApplyRace(EMTRace NewRace)
{
	Race = NewRace;
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTRaceData* Data = Registry ? Registry->FindRace(NewRace) : nullptr;
	RaceHealthMultiplier = Data ? Data->MaxHealthMultiplier : 1.f;
	RaceManaMultiplier = Data ? Data->MaxManaMultiplier : 1.f;
	RaceStaminaMultiplier = Data ? Data->MaxStaminaMultiplier : 1.f;
	RacePassiveStats = Data ? Data->PassiveStats : FMTStatModifier();
	RacePassiveMovement = Data ? Data->PassiveMovement : FMTMovementModifier();
	Abilities->SetSlot(EMTAbilitySlot::RaceActive, Data ? Data->ActiveAbility : NAME_None);
	Abilities->SetSlot(EMTAbilitySlot::RaceTransformation, Data ? Data->TransformationAbility : NAME_None);

	// Re-apply the lineage so attribute maxima include the race multipliers.
	if (!CharacterId.IsNone())
	{
		ApplyCharacterLineage(CharacterId);
	}
	else
	{
		Attributes->SetPermanentModifiers(RacePassiveStats, RacePassiveMovement);
	}
}

void AMTCharacterBase::ApplyElementSlot(int32 SlotIndex, EMTElement Element)
{
	if (!ElementSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	ElementSlots[SlotIndex] = Element;
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTElementData* Data = (Registry && Element != EMTElement::None) ? Registry->FindElement(Element) : nullptr;
	const EMTAbilitySlot First = SlotIndex == 0 ? EMTAbilitySlot::ElementA1 : EMTAbilitySlot::ElementB1;
	for (int32 i = 0; i < 3; ++i)
	{
		const FName Id = (Data && Data->Abilities.IsValidIndex(i)) ? Data->Abilities[i] : NAME_None;
		Abilities->SetSlot((EMTAbilitySlot)((int32)First + i), Id);
	}
}

bool AMTCharacterBase::IsAlive() const
{
	return Attributes && Attributes->IsAlive();
}

bool AMTCharacterBase::IsHostileTo(const AActor* Other) const
{
	const IGenericTeamAgentInterface* OtherTeam = Cast<const IGenericTeamAgentInterface>(Other);
	if (!Other || Other == this || !OtherTeam)
	{
		return false;
	}
	const FGenericTeamId OtherId = OtherTeam->GetGenericTeamId();
	// Team 3 = neutral villagers: never hostile to anyone.
	if (OtherId.GetId() == 3 || TeamId.GetId() == 3)
	{
		return false;
	}
	return OtherId != TeamId;
}

bool AMTCharacterBase::IsStaggered() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < StaggerUntil;
}

float AMTCharacterBase::GetTimeSinceDodgeStart() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() - DodgeStartTime : 100.f;
}

FVector AMTCharacterBase::GetAimPoint() const
{
	if (const AActor* Target = LockTarget.Get())
	{
		// Aim at the chest, not the feet.
		return Target->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	}
	return GetActorLocation() + GetActorForwardVector() * 2000.f;
}

void AMTCharacterBase::SetSprinting(bool bInSprint)
{
	bSprinting = bInSprint;
	UpdateMovementFromModifiers();
}

void AMTCharacterBase::SetWalking(bool bInWalk)
{
	bWalking = bInWalk;
	UpdateMovementFromModifiers();
}

void AMTCharacterBase::UpdateMovementFromModifiers()
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!CMC || !Attributes)
	{
		return;
	}
	const FMTMovementModifier Mod = Attributes->GetMovementModifier();
	float Speed = bWalking ? BaseWalkSpeed : (bSprinting ? BaseSprintSpeed : BaseRunSpeed);
	if (Abilities && Abilities->IsCasting())
	{
		Speed = FMath::Min(Speed, BaseRunSpeed * 0.45f); // casting slows but does not root
	}
	Speed *= Mod.SpeedMultiplier * AbilityMoveMultiplier;
	if (Mod.bRooted || IsStaggered())
	{
		Speed = 0.f;
	}
	CMC->MaxWalkSpeed = Speed;
	CMC->MaxAcceleration = BaseAcceleration * Mod.AccelerationMultiplier;
	CMC->JumpZVelocity = BaseJumpZ * Mod.JumpMultiplier;
}

void AMTCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Mirror status-effect tags into the state container (one source of truth for queries).
	if (Attributes)
	{
		StateTags.RemoveTags(SyncedStatusTags);
		SyncedStatusTags = Attributes->GetStatusTags();
		StateTags.AppendTags(SyncedStatusTags);
	}

	if (bSprinting && Attributes && GetVelocity().SizeSquared2D() > 100.f)
	{
		if (!Attributes->SpendStamina(SprintStaminaPerSecond * DeltaSeconds) || Attributes->GetStamina() <= 0.f)
		{
			SetSprinting(false);
		}
	}

	if (bDodging && GetTimeSinceDodgeStart() >= DodgeDuration)
	{
		bDodging = false;
		StateTags.RemoveTag(MTTags::State_Dodging);
	}
	if (!IsStaggered())
	{
		StateTags.RemoveTag(MTTags::State_Staggered);
	}

	UpdateMovementFromModifiers();
}

bool AMTCharacterBase::Dodge(FVector WorldDirection)
{
	if (!IsAlive() || bDodging || IsStaggered() || !GetCharacterMovement()->IsMovingOnGround())
	{
		return false;
	}
	if (Attributes->GetStamina() < DodgeStaminaCost * 0.5f)
	{
		return false;
	}
	// Dodge cancels casting recovery (and anticipation - the formation mana is lost).
	if (UMTAbility* Active = Abilities->GetActiveAbility())
	{
		Active->Cancel();
	}
	Attributes->SpendStamina(DodgeStaminaCost);

	FVector Dir = WorldDirection.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = -GetActorForwardVector();
	}
	const FMTMovementModifier Mod = Attributes->GetMovementModifier();
	const float Distance = BaseDodgeDistance * Mod.DodgeDistanceMultiplier;

	FVector Destination = GetActorLocation() + Dir * Distance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTDodgeSweep), false, this);
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (GetWorld()->SweepSingleByChannel(Hit, GetActorLocation(), Destination, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() * 0.9f, Capsule->GetScaledCapsuleHalfHeight() * 0.8f), Params))
	{
		Destination = Hit.Location;
	}

	TSharedPtr<FRootMotionSource_MoveToForce> MoveTo = MakeShared<FRootMotionSource_MoveToForce>();
	MoveTo->InstanceName = TEXT("MTDodge");
	MoveTo->AccumulateMode = ERootMotionAccumulateMode::Override;
	MoveTo->Priority = 1000;
	MoveTo->StartLocation = GetActorLocation();
	MoveTo->TargetLocation = Destination;
	MoveTo->Duration = DodgeDuration;
	MoveTo->bRestrictSpeedToExpected = false;
	MoveTo->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	MoveTo->FinishVelocityParams.ClampVelocity = BaseRunSpeed * 0.6f;
	GetCharacterMovement()->ApplyRootMotionSource(MoveTo);

	bDodging = true;
	DodgeStartTime = GetWorld()->GetTimeSeconds();
	StateTags.AddTag(MTTags::State_Dodging);
	Attributes->SetInvulnerableFor(DodgeInvulnerability);

	// Directional montage relative to facing (the character does not turn to dodge).
	const float Forward = FVector::DotProduct(Dir, GetActorForwardVector());
	const float Right = FVector::DotProduct(Dir, GetActorRightVector());
	const TSoftObjectPtr<UAnimMontage>& Montage = FMath::Abs(Forward) >= FMath::Abs(Right)
		? (Forward >= 0.f ? DodgeForwardMontage : DodgeBackMontage)
		: (Right >= 0.f ? DodgeRightMontage : DodgeLeftMontage);
	PlaySoftMontage(Montage, 1.f);
	return true;
}

void AMTCharacterBase::NotifyPerfectDefense(FName Kind)
{
	// Orsted passive - Dragon God Knowledge: successful counters, perfect dodges and interrupts
	// grant a short efficiency window. Nothing is granted just for existing.
	const FMTCharacterData* Data = GetCharacterData();
	if (!Data || Data->Passive != TEXT("DragonGodKnowledge"))
	{
		return;
	}
	FMTStatusEffect Knowledge;
	Knowledge.Id = TEXT("DragonGodKnowledge");
	Knowledge.Duration = 4.f;
	Knowledge.Stats.CastTimeMultiplier = 0.8f;
	Knowledge.Stats.CooldownMultiplier = 0.85f;
	Knowledge.Stats.DamageMultiplier = 1.12f;
	Knowledge.GrantedTags.AddTag(MTTags::State_KnowledgeBuff);
	Attributes->AddStatusEffect(Knowledge);
	Attributes->AddAwakeningMeter(8.f);
	UE_LOG(LogMushoku, Verbose, TEXT("%s: Dragon God Knowledge triggered by %s"), *GetName(), *Kind.ToString());
}

FMTDamageResult AMTCharacterBase::ReceiveHit(const FMTDamageSpec& Spec)
{
	if (!IsAlive())
	{
		return FMTDamageResult();
	}
	// Perfect dodge: a hit that arrives during the first part of a dodge is avoided and,
	// for Orsted, feeds Dragon God Knowledge.
	const FMTDamageResult Result = Attributes->ApplyDamage(Spec);
	if (Result.bDodged && bDodging && GetTimeSinceDodgeStart() < DodgeInvulnerability)
	{
		NotifyPerfectDefense(TEXT("PerfectDodge"));
	}
	return Result;
}

void AMTCharacterBase::HandleDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result)
{
	if (Result.bKilled)
	{
		return;
	}
	PlayHitReaction(Result.Reaction, Spec.HitDirection);

	if (Spec.Knockback > 0.f && (Result.Reaction == EMTHitReaction::Knockback || Result.Reaction == EMTHitReaction::Knockdown))
	{
		const FVector Push = Spec.HitDirection.GetSafeNormal2D() * Spec.Knockback + FVector(0.f, 0.f, Result.Reaction == EMTHitReaction::Knockdown ? 250.f : 60.f);
		LaunchCharacter(Push, true, true);
	}
}

void AMTCharacterBase::PlayHitReaction(EMTHitReaction Reaction, const FVector& FromDirection)
{
	if (Reaction == EMTHitReaction::None)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Reaction == EMTHitReaction::Stagger || Reaction == EMTHitReaction::Knockdown)
	{
		StaggerUntil = Now + (Reaction == EMTHitReaction::Knockdown ? 1.4f : 0.8f);
		StateTags.AddTag(MTTags::State_Staggered);
		Abilities->CancelAll();
		PlaySoftMontage(Reaction == EMTHitReaction::Knockdown ? KnockdownMontage : StaggerMontage);
		return;
	}
	if (Reaction == EMTHitReaction::Knockback)
	{
		StaggerUntil = Now + 0.45f;
	}
	// Flinch: additive-friendly directional montage, does not interrupt casting.
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(-FromDirection.GetSafeNormal2D());
	const TSoftObjectPtr<UAnimMontage>& Montage = FMath::Abs(Local.X) >= FMath::Abs(Local.Y)
		? (Local.X >= 0.f ? HitReactFront : HitReactBack)
		: (Local.Y >= 0.f ? HitReactRight : HitReactLeft);
	PlaySoftMontage(Montage);
}

void AMTCharacterBase::HandleDeath(AActor* Killer)
{
	StateTags.AddTag(MTTags::State_Dead);
	Abilities->CancelAll();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	if (PlaySoftMontage(DeathMontage) <= 0.f)
	{
		// No death animation yet: ragdoll so the body never T-poses or freezes upright.
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetSimulatePhysics(true);
	}
	if (!IsPlayerControlled())
	{
		if (UMTGameEvents* Events = UMTGameEvents::Get(this))
		{
			Events->OnEnemyKilled.Broadcast(GameplayId, IdentityTags, Killer);
		}
		SetLifeSpan(10.f);
	}
}

float AMTCharacterBase::PlaySoftMontage(const TSoftObjectPtr<UAnimMontage>& Montage, float PlayRate, FName Section)
{
	if (Montage.IsNull())
	{
		return 0.f;
	}
	UAnimMontage* Loaded = Montage.LoadSynchronous();
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Loaded || !Anim)
	{
		return 0.f;
	}
	const float Length = Anim->Montage_Play(Loaded, PlayRate);
	if (Length > 0.f && !Section.IsNone())
	{
		Anim->Montage_JumpToSection(Section, Loaded);
	}
	return Length;
}

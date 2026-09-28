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
#include "Animation/AnimSequenceBase.h"
#include "Animation/MTNativeAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

namespace
{
	/** Slot every code-driven animation plays in (the native anim instance evaluates it; an AnimBP needs a node for it). */
	const FName MTAnimSlotName(TEXT("DefaultSlot"));
}

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
	// Animation: the lineage's AnimClass (e.g. an Animation Blueprint parented to UMTAnimInstance) when it loads,
	// otherwise the graph-free native instance that plays the AnimSet directly.
	UClass* AnimClass = nullptr;
	if (!Data->AnimClass.IsNull())
	{
		AnimClass = Data->AnimClass.LoadSynchronous();
		if (!AnimClass)
		{
			UE_LOG(LogMushoku, Warning, TEXT("%s: AnimClass for '%s' failed to load (%s) - using UMTNativeAnimInstance"),
				*GetName(), *NewCharacterId.ToString(), *Data->AnimClass.ToString());
		}
	}
	if (!AnimClass)
	{
		AnimClass = UMTNativeAnimInstance::StaticClass();
	}
	const UAnimInstance* CurrentAnimInstance = GetMesh()->GetAnimInstance();
	if (!CurrentAnimInstance || CurrentAnimInstance->GetClass() != AnimClass)
	{
		// Only on change: re-applying the lineage (ApplyRace) must not reset a running anim instance.
		GetMesh()->SetAnimInstanceClass(AnimClass);
	}
	ApplyAnimSetDefaults(Registry->FindAnimSet(NewCharacterId));

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
	Attributes->HealthRegen = Data ? Data->PassiveHealthRegen : 0.f;
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

	// Directional animation relative to facing (the character does not turn to dodge).
	const float Forward = FVector::DotProduct(Dir, GetActorForwardVector());
	const float Right = FVector::DotProduct(Dir, GetActorRightVector());
	const TSoftObjectPtr<UAnimSequenceBase>& DodgeAnim = FMath::Abs(Forward) >= FMath::Abs(Right)
		? (Forward >= 0.f ? DodgeForwardMontage : DodgeBackMontage)
		: (Right >= 0.f ? DodgeRightMontage : DodgeLeftMontage);
	PlayAnimAsset(DodgeAnim, 1.f, NAME_None, 1, 0.08f, 0.2f);
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
		PlayAnimAsset(Reaction == EMTHitReaction::Knockdown ? KnockdownMontage : StaggerMontage, 1.f, NAME_None, 1, 0.08f, 0.25f);
		return;
	}
	if (Reaction == EMTHitReaction::Knockback)
	{
		StaggerUntil = Now + 0.45f;
	}
	// Flinch: directional reaction; the ability keeps running (only its animation is interrupted).
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(-FromDirection.GetSafeNormal2D());
	const TSoftObjectPtr<UAnimSequenceBase>& HitAnim = FMath::Abs(Local.X) >= FMath::Abs(Local.Y)
		? (Local.X >= 0.f ? HitReactFront : HitReactBack)
		: (Local.Y >= 0.f ? HitReactRight : HitReactLeft);
	PlayAnimAsset(HitAnim, 1.f, NAME_None, 1, 0.06f, 0.2f);
}

void AMTCharacterBase::HandleDeath(AActor* Killer)
{
	StateTags.AddTag(MTTags::State_Dead);
	Abilities->CancelAll();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// The native anim instance holds the Death clip's last frame after this blends out (no pop back to idle).
	if (PlayAnimAsset(DeathMontage, 1.f, NAME_None, 1, 0.1f, 0.25f) <= 0.f)
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
	return PlayAnimAsset(TSoftObjectPtr<UAnimSequenceBase>(Montage.ToSoftObjectPath()), PlayRate, Section);
}

TSoftObjectPtr<UAnimSequenceBase> AMTCharacterBase::ResolveLineageAnim(const TSoftObjectPtr<UAnimSequenceBase>& Asset) const
{
	const FString Name = Asset.ToSoftObjectPath().GetAssetName(); // e.g. A_Rudeus_StoneCannon_Charge
	int32 Separator = INDEX_NONE;
	if (!Name.StartsWith(TEXT("A_")) || !Name.RightChop(2).FindChar(TEXT('_'), Separator))
	{
		return Asset;
	}
	const FName Key(*Name.RightChop(2 + Separator + 1)); // character names contain no '_': the rest is the key
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTAnimSetData* AnimSet = Registry ? Registry->FindAnimSet(CharacterId) : nullptr;
	const TSoftObjectPtr<UAnimSequenceBase>* Own = AnimSet ? AnimSet->Anims.Find(Key) : nullptr;
	return (Own && !Own->IsNull()) ? *Own : Asset;
}

float AMTCharacterBase::PlayAnimAsset(const TSoftObjectPtr<UAnimSequenceBase>& RequestedAsset, float PlayRate, FName Section,
	int32 LoopCount, float BlendIn, float BlendOut)
{
	if (RequestedAsset.IsNull())
	{
		return 0.f;
	}
	const TSoftObjectPtr<UAnimSequenceBase> Asset = ResolveLineageAnim(RequestedAsset);
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return 0.f;
	}
	const FSoftObjectPath Path = Asset.ToSoftObjectPath();
	if (FailedAnimPaths.Contains(Path))
	{
		return 0.f;
	}
	UAnimSequenceBase* Loaded = Asset.LoadSynchronous();
	if (!Loaded)
	{
		FailedAnimPaths.Add(Path);
		UE_LOG(LogMushoku, Warning, TEXT("%s: animation %s is not imported yet (not retried)"), *GetName(), *Path.ToString());
		return 0.f;
	}

	// Authored montage: play it as-is (sections, notifies, its own blend settings).
	if (UAnimMontage* Montage = Cast<UAnimMontage>(Loaded))
	{
		const float Length = Anim->Montage_Play(Montage, PlayRate);
		if (Length > 0.f)
		{
			if (!Section.IsNone() && Montage->GetSectionIndex(Section) != INDEX_NONE)
			{
				Anim->Montage_JumpToSection(Section, Montage);
			}
			LastPlayedMontage = Montage;
		}
		return Length;
	}

	// Plain sequence: wrap it in a dynamic montage in DefaultSlot (no montage asset needed). Sections do not apply.
	UAnimMontage* Dynamic = Anim->PlaySlotAnimationAsDynamicMontage(Loaded, MTAnimSlotName, BlendIn, BlendOut, PlayRate, FMath::Max(1, LoopCount));
	if (!Dynamic)
	{
		return 0.f;
	}
	LastPlayedMontage = Dynamic;
	return Dynamic->GetPlayLength();
}

UAnimMontage* AMTCharacterBase::GetLastPlayedMontage() const
{
	return LastPlayedMontage.Get();
}

void AMTCharacterBase::StopPlayedAnim(UAnimMontage* Montage, float BlendOut)
{
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* ToStop = Montage ? Montage : LastPlayedMontage.Get();
	if (Anim && ToStop)
	{
		Anim->Montage_Stop(BlendOut, ToStop);
	}
}

void AMTCharacterBase::ApplyAnimSetDefaults(const FMTAnimSetData* AnimSet)
{
	struct FAnimBinding
	{
		const TCHAR* Key;
		TSoftObjectPtr<UAnimSequenceBase> AMTCharacterBase::* Member;
	};
	static const FAnimBinding Bindings[] =
	{
		{ TEXT("DodgeForward"), &AMTCharacterBase::DodgeForwardMontage },
		{ TEXT("DodgeBack"), &AMTCharacterBase::DodgeBackMontage },
		{ TEXT("DodgeLeft"), &AMTCharacterBase::DodgeLeftMontage },
		{ TEXT("DodgeRight"), &AMTCharacterBase::DodgeRightMontage },
		{ TEXT("HitFront"), &AMTCharacterBase::HitReactFront },
		{ TEXT("HitBack"), &AMTCharacterBase::HitReactBack },
		{ TEXT("HitLeft"), &AMTCharacterBase::HitReactLeft },
		{ TEXT("HitRight"), &AMTCharacterBase::HitReactRight },
		{ TEXT("Stagger"), &AMTCharacterBase::StaggerMontage },
		{ TEXT("Knockdown"), &AMTCharacterBase::KnockdownMontage },
		{ TEXT("Death"), &AMTCharacterBase::DeathMontage },
	};

	for (const FAnimBinding& Binding : Bindings)
	{
		const FName Key(Binding.Key);
		TSoftObjectPtr<UAnimSequenceBase>& Value = this->*Binding.Member;
		// Ours to replace only if unset, or still exactly what a previous lineage's AnimSet put there.
		const FSoftObjectPath* AutoFilled = AutoFilledAnims.Find(Key);
		const bool bOwnedByAnimSet = AutoFilled && *AutoFilled == Value.ToSoftObjectPath();
		if (!Value.IsNull() && !bOwnedByAnimSet)
		{
			AutoFilledAnims.Remove(Key); // a designer value: never touched again
			continue;
		}
		const TSoftObjectPtr<UAnimSequenceBase>* FromSet = AnimSet ? AnimSet->Anims.Find(Key) : nullptr;
		if (FromSet && !FromSet->IsNull())
		{
			Value = *FromSet;
			AutoFilledAnims.Add(Key, Value.ToSoftObjectPath());
		}
		else if (bOwnedByAnimSet)
		{
			Value.Reset(); // the new lineage has no such clip: do not keep the previous lineage's
			AutoFilledAnims.Remove(Key);
		}
	}
}

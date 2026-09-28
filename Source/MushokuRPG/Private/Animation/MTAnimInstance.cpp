#include "Animation/MTAnimInstance.h"
#include "Character/MTCharacterBase.h"
#include "Abilities/MTAbilityComponent.h"
#include "Abilities/MTAbility.h"
#include "Core/MTGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"

void UMTAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Character = Cast<AMTCharacterBase>(TryGetPawnOwner());
	if (Character.IsValid())
	{
		LastYaw = Character->GetActorRotation().Yaw;
	}
}

void UMTAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	// Game thread: snapshot only. Heavy math happens in the thread-safe update.
	AMTCharacterBase* Owner = Character.Get();
	if (!Owner)
	{
		Owner = Cast<AMTCharacterBase>(TryGetPawnOwner());
		Character = Owner;
		if (!Owner)
		{
			return;
		}
	}
	const UCharacterMovementComponent* CMC = Owner->GetCharacterMovement();
	Velocity = Owner->GetVelocity();
	CurrentAccel = CMC ? CMC->GetCurrentAcceleration() : FVector::ZeroVector;
	ActorRotation = Owner->GetActorRotation();
	AimRotation = Owner->GetBaseAimRotation();
	bIsInAir = CMC && CMC->IsFalling();
	bSnapRootMotion = CMC && CMC->HasRootMotionSources();

	const FGameplayTagContainer& Tags = Owner->GetStateTags();
	bSnapCasting = Tags.HasTag(MTTags::State_Casting);
	bSnapCharging = Tags.HasTag(MTTags::State_Charging);
	bSnapAwakened = Tags.HasTag(MTTags::State_Awakened);
	bSnapTransforming = Tags.HasTag(MTTags::State_Transforming);
	bSnapStaggered = Owner->IsStaggered();
	bSnapDodging = Owner->IsDodging() || Tags.HasTag(MTTags::State_Dodging);
	bSnapDead = !Owner->IsAlive();
	bSnapSprinting = Owner->IsSprinting();
	bSnapLocked = Owner->GetLockTarget() != nullptr;
	SnapStance = Owner->GetStance();
	WorldTime = Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() : 0.f;

	SnapChargeAlpha = 0.f;
	if (const UMTAbilityComponent* Abilities = Owner->GetAbilities())
	{
		if (const UMTAbility* Active = Abilities->GetActiveAbility())
		{
			SnapChargeAlpha = Active->GetChargeAlpha();
		}
	}
	if (const FMTCharacterData* Data = Owner->GetCharacterData())
	{
		WalkSpeedRef = Data->WalkSpeed;
		RunSpeedRef = Data->RunSpeed;
		SprintSpeedRef = Data->SprintSpeed;
	}
}

void UMTAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	if (!Character.IsValid() || DeltaSeconds <= 0.f)
	{
		return;
	}

	// Ground speed and a continuous gait value so walk->run->sprint blends without snapping.
	GroundSpeed = Velocity.Size2D();
	if (GroundSpeed <= WalkSpeedRef)
	{
		GaitValue = GroundSpeed / FMath::Max(1.f, WalkSpeedRef);
	}
	else if (GroundSpeed <= RunSpeedRef)
	{
		GaitValue = 1.f + (GroundSpeed - WalkSpeedRef) / FMath::Max(1.f, RunSpeedRef - WalkSpeedRef);
	}
	else
	{
		GaitValue = 2.f + FMath::Clamp((GroundSpeed - RunSpeedRef) / FMath::Max(1.f, SprintSpeedRef - RunSpeedRef), 0.f, 1.f);
	}
	Acceleration = CurrentAccel.Size2D();
	bShouldMove = GroundSpeed > 5.f && Acceleration > 0.f;
	bIsSprinting = bSnapSprinting && GroundSpeed > RunSpeedRef * 0.9f;
	bIsStrafing = bSnapLocked;
	Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, ActorRotation);

	// Lean from yaw rate (additive), smoothed so it never twitches.
	const float YawDelta = FMath::FindDeltaAngleDegrees(LastYaw, ActorRotation.Yaw);
	const float TargetLean = FMath::Clamp((YawDelta / DeltaSeconds) / 180.f, -1.f, 1.f) * FMath::Clamp(GroundSpeed / FMath::Max(1.f, RunSpeedRef), 0.f, 1.f);
	LeanAmount = FMath::FInterpTo(LeanAmount, TargetLean, DeltaSeconds, 6.f);

	// Turn in place: while idle the root counter-rotates; past the threshold a turn anim plays
	// (the AnimBP curve "TurnYawWeight" is expected to unwind RootYawOffset during the turn).
	if (!bShouldMove && !bIsInAir && !bSnapRootMotion)
	{
		RootYawOffset = FMath::UnwindDegrees(RootYawOffset - YawDelta);
		bTurningLeft = RootYawOffset > TurnInPlaceThreshold;
		bTurningRight = RootYawOffset < -TurnInPlaceThreshold;
		if (FMath::Abs(RootYawOffset) > 120.f)
		{
			RootYawOffset = FMath::Clamp(RootYawOffset, -120.f, 120.f); // never over-twist the spine
		}
	}
	else
	{
		RootYawOffset = FMath::FInterpTo(RootYawOffset, 0.f, DeltaSeconds, 12.f);
		bTurningLeft = bTurningRight = false;
	}
	LastYaw = ActorRotation.Yaw;

	// Aim offset (cast direction) relative to the root.
	const FRotator AimDelta = (AimRotation - ActorRotation).GetNormalized();
	AimYaw = FMath::Clamp(AimDelta.Yaw + RootYawOffset, -90.f, 90.f);
	AimPitch = FMath::Clamp(AimDelta.Pitch, -60.f, 60.f);

	// Air / landing.
	VerticalSpeed = Velocity.Z;
	bIsJumping = bIsInAir && VerticalSpeed > 50.f;
	bIsFalling = bIsInAir && VerticalSpeed <= 50.f;
	if (bIsInAir)
	{
		LandingImpactSpeed = FMath::Max(LandingImpactSpeed, -VerticalSpeed);
		TimeSinceLanded = 0.f;
	}
	else
	{
		if (bWasInAir)
		{
			bHardLanding = LandingImpactSpeed > HardLandingSpeed;
		}
		TimeSinceLanded += DeltaSeconds;
		if (TimeSinceLanded > 0.5f)
		{
			LandingImpactSpeed = 0.f;
			bHardLanding = false;
		}
	}
	bWasInAir = bIsInAir;

	// Combat layering with eased weights.
	bIsCasting = bSnapCasting;
	bIsCharging = bSnapCharging;
	ChargeAlpha = SnapChargeAlpha;
	bIsAwakened = bSnapAwakened;
	bIsTransforming = bSnapTransforming;
	bIsStaggered = bSnapStaggered;
	bIsDodging = bSnapDodging;
	bIsDead = bSnapDead;
	Stance = SnapStance;
	if (bIsCasting || bIsStaggered || bSnapLocked)
	{
		LastCombatTime = WorldTime;
	}
	bInCombatStance = (WorldTime - LastCombatTime) < CombatIdleLinger;
	UpperBodyCastWeight = FMath::FInterpTo(UpperBodyCastWeight, bIsCasting ? 1.f : 0.f, DeltaSeconds, bIsCasting ? 14.f : 6.f);

	const float TargetFootIK = (bIsInAir || bIsDodging || bSnapRootMotion || bIsDead) ? 0.f : 1.f;
	FootIKAlpha = FMath::FInterpTo(FootIKAlpha, TargetFootIK, DeltaSeconds, 10.f);
}

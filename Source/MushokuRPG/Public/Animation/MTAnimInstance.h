// Locomotion/combat animation state shared by every character anim instance.
// All values are computed in NativeThreadSafeUpdateAnimation (multithreaded anim update).
// Consumers: UMTNativeAnimInstance (graph-free, the default) reads them through its proxy;
// a future Animation Blueprint parented to this class reads the same variables in its graph.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Core/MTTypes.h"
#include "MTAnimInstance.generated.h"

class AMTCharacterBase;
class UCharacterMovementComponent;

UCLASS()
class MUSHOKURPG_API UMTAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	/** The native (graph-free) proxy snapshots these variables on the game thread in PreUpdate. */
	friend struct FMTNativeAnimInstanceProxy;

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	// --- Locomotion (drive blend spaces / state machine) ---
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") float GroundSpeed = 0.f;
	/** Speed normalised: 0 idle, 1 walk, 2 run, 3 sprint (drives the 1D blend space, no snapping). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") float GaitValue = 0.f;
	/** -180..180 movement direction relative to facing (strafing when locked on). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") float Direction = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") bool bShouldMove = false;
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") bool bIsSprinting = false;
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") bool bIsStrafing = false;
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") float Acceleration = 0.f;
	/** Additive lean (-1..1) from yaw rate, smoothed. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") float LeanAmount = 0.f;

	// --- Air ---
	UPROPERTY(BlueprintReadOnly, Category = "Air") bool bIsInAir = false;
	UPROPERTY(BlueprintReadOnly, Category = "Air") bool bIsJumping = false;
	UPROPERTY(BlueprintReadOnly, Category = "Air") bool bIsFalling = false;
	UPROPERTY(BlueprintReadOnly, Category = "Air") float VerticalSpeed = 0.f;
	/** Set on the frame of landing; > threshold = hard landing. */
	UPROPERTY(BlueprintReadOnly, Category = "Air") float LandingImpactSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Air") bool bHardLanding = false;
	UPROPERTY(BlueprintReadOnly, Category = "Air") float TimeSinceLanded = 10.f;

	// --- Turn in place / aim ---
	/** Root yaw offset for turn-in-place (counter-rotates the root while idle). */
	UPROPERTY(BlueprintReadOnly, Category = "TurnInPlace") float RootYawOffset = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "TurnInPlace") bool bTurningLeft = false;
	UPROPERTY(BlueprintReadOnly, Category = "TurnInPlace") bool bTurningRight = false;
	UPROPERTY(BlueprintReadOnly, Category = "Aim") float AimYaw = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Aim") float AimPitch = 0.f;

	// --- Combat layering ---
	UPROPERTY(BlueprintReadOnly, Category = "Combat") bool bIsCasting = false;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") bool bIsCharging = false;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") float ChargeAlpha = 0.f;
	/** Upper-body cast layer weight, eased in/out so locomotion never pops. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") float UpperBodyCastWeight = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") bool bInCombatStance = false;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") bool bIsAwakened = false;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") bool bIsTransforming = false;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") bool bIsStaggered = false;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") bool bIsDodging = false;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") bool bIsDead = false;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") EMTStance Stance = EMTStance::Generic;

	// --- IK ---
	/** Foot IK alpha: off in air / during dodges and root-motion dashes. */
	UPROPERTY(BlueprintReadOnly, Category = "IK") float FootIKAlpha = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning") float WalkSpeedRef = 200.f;
	UPROPERTY(EditDefaultsOnly, Category = "Tuning") float RunSpeedRef = 450.f;
	UPROPERTY(EditDefaultsOnly, Category = "Tuning") float SprintSpeedRef = 700.f;
	UPROPERTY(EditDefaultsOnly, Category = "Tuning") float TurnInPlaceThreshold = 70.f;
	UPROPERTY(EditDefaultsOnly, Category = "Tuning") float HardLandingSpeed = 900.f;
	/** Seconds after the last attack/hit to drop back from combat idle to relaxed idle. */
	UPROPERTY(EditDefaultsOnly, Category = "Tuning") float CombatIdleLinger = 6.f;

private:
	// Game-thread snapshot copied for the worker thread.
	TWeakObjectPtr<AMTCharacterBase> Character;
	FVector Velocity = FVector::ZeroVector;
	FVector CurrentAccel = FVector::ZeroVector;
	FRotator ActorRotation = FRotator::ZeroRotator;
	FRotator AimRotation = FRotator::ZeroRotator;
	bool bWasInAir = false;
	bool bSnapCasting = false;
	bool bSnapCharging = false;
	bool bSnapAwakened = false;
	bool bSnapTransforming = false;
	bool bSnapStaggered = false;
	bool bSnapDodging = false;
	bool bSnapDead = false;
	bool bSnapSprinting = false;
	bool bSnapLocked = false;
	bool bSnapRootMotion = false;
	float SnapChargeAlpha = 0.f;
	float LastYaw = 0.f;
	float LastCombatTime = -100.f;
	float WorldTime = 0.f;
	EMTStance SnapStance = EMTStance::Generic;
};

#include "Abilities/MTAbility.h"
#include "Abilities/MTAbilityComponent.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Combat/MTCombatStatics.h"
#include "Core/MTGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"

void UMTAbility::Initialize(UMTAbilityComponent* InComponent, const FMTAbilityData& InData)
{
	Component = InComponent;
	Data = InData;
}

AMTCharacterBase* UMTAbility::GetOwnerCharacter() const
{
	return Component.IsValid() ? Component->GetOwnerCharacter() : nullptr;
}

float UMTAbility::GetEffectiveCastTime() const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	float Mult = 1.f;
	if (Owner && Owner->GetAttributes())
	{
		Mult = Owner->GetAttributes()->GetStatModifier().CastTimeMultiplier;
	}
	// Chantless casting shortens startup but big spells keep a readable minimum wind-up.
	const float MinReadable = Data.CastTime >= 0.6f ? Data.CastTime * 0.45f : 0.04f;
	return FMath::Max(MinReadable, Data.CastTime * Mult);
}

float UMTAbility::GetEffectiveManaCost() const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	float Mult = 1.f;
	if (Owner && Owner->GetAttributes())
	{
		Mult = Owner->GetAttributes()->GetStatModifier().ManaCostMultiplier;
		// Cumulonimbus: water magic is more efficient inside the storm.
		if (Data.Element == EMTElement::Water && Owner->GetStateTags().HasTag(MTTags::State_InStorm))
		{
			Mult *= 0.7f;
		}
	}
	return Data.ManaCost * Mult;
}

float UMTAbility::GetEffectiveCooldown() const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	const float Mult = (Owner && Owner->GetAttributes()) ? Owner->GetAttributes()->GetStatModifier().CooldownMultiplier : 1.f;
	return Data.Cooldown * Mult;
}

bool UMTAbility::CanActivate(FText* OutReason) const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || !Owner->IsAlive())
	{
		return false;
	}
	if (Owner->IsStaggered())
	{
		if (OutReason) { *OutReason = NSLOCTEXT("MT", "Staggered", "Staggered"); }
		return false;
	}
	UMTAttributeComponent* Attr = Owner->GetAttributes();
	if (Attr && Attr->GetMana() < GetEffectiveManaCost())
	{
		if (OutReason) { *OutReason = NSLOCTEXT("MT", "NoMana", "Not enough mana"); }
		return false;
	}
	if (Attr && Data.StaminaCost > 0.f && Attr->GetStamina() < Data.StaminaCost * 0.5f)
	{
		if (OutReason) { *OutReason = NSLOCTEXT("MT", "NoStamina", "Not enough stamina"); }
		return false;
	}
	if (Data.AwakeningMeterCost > 0.f && Attr && Attr->GetAwakeningMeter() + KINDA_SMALL_NUMBER < Data.AwakeningMeterCost)
	{
		if (OutReason) { *OutReason = NSLOCTEXT("MT", "NoAwakening", "Awakening meter not full"); }
		return false;
	}
	return true;
}

bool UMTAbility::TryActivate()
{
	FText Reason;
	if (!CanActivate(&Reason))
	{
		if (Component.IsValid() && !Reason.IsEmpty())
		{
			Component->OnAbilityFailed.Broadcast(Data.AbilityID, Reason);
		}
		return false;
	}

	AMTCharacterBase* Owner = GetOwnerCharacter();
	UMTAttributeComponent* Attr = Owner->GetAttributes();

	// Chargeable abilities pay the base cost now and the charge surcharge on release.
	CommittedManaCost = GetEffectiveManaCost();
	if (!Attr->SpendMana(CommittedManaCost))
	{
		return false;
	}
	if (Data.StaminaCost > 0.f)
	{
		Attr->SpendStamina(Data.StaminaCost);
	}
	if (Data.AwakeningMeterCost > 0.f)
	{
		Attr->ConsumeAwakeningMeter(Data.AwakeningMeterCost);
	}

	bInputHeld = true;
	bReleasedDuringAnticipation = false;
	ChargeTime = 0.f;
	ReleasedChargeAlpha = 0.f;
	Owner->GetStateTags().AppendTags(Data.ActivationTags);
	Owner->GetStateTags().AddTag(MTTags::State_Casting);
	if (Data.bChargeable)
	{
		Owner->GetStateTags().AddTag(MTTags::State_Charging);
	}

	EnterPhase(EMTAbilityPhase::Anticipation);
	ActiveAnimMontage.Reset();
	bChargeLoopStarted = false;
	PlayMontage(Data.MontageStartSection);
	SpawnFX(Data.FX.Formation, GetCastLocation(), GetAimRotation(), true);
	PlaySound(Data.FX.CastSound, GetCastLocation());
	Component->NotifyAbilityStarted(this);
	return true;
}

void UMTAbility::InputReleased()
{
	bInputHeld = false;
	if (Phase == EMTAbilityPhase::Anticipation)
	{
		bReleasedDuringAnticipation = true;
	}
}

float UMTAbility::GetChargeAlpha() const
{
	if (!Data.bChargeable || Data.MaxChargeTime <= 0.f)
	{
		return 0.f;
	}
	if (Phase != EMTAbilityPhase::Anticipation)
	{
		return ReleasedChargeAlpha;
	}
	return FMath::Clamp(ChargeTime / Data.MaxChargeTime, 0.f, 1.f);
}

float UMTAbility::GetChargedValue(float Base, float FullChargeScale) const
{
	return Base * FMath::Lerp(1.f, FullChargeScale, GetChargeAlpha());
}

void UMTAbility::Tick(float DeltaTime)
{
	if (!IsActive())
	{
		return;
	}
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || !Owner->IsAlive())
	{
		Cancel();
		return;
	}
	PhaseTime += DeltaTime;

	switch (Phase)
	{
	case EMTAbilityPhase::Anticipation:
	{
		const float CastTime = GetEffectiveCastTime();
		if (Data.bChargeable)
		{
			// Anticipation clip -> seamless hold loop for as long as the ability stays in its charge phase.
			UpdateChargeAnimation();
			// Hold to charge. Charging can never exceed MaxChargeTime: at full charge the
			// spell is held (stable stance) until release or a 1.5 s overhold, then fires.
			if (bInputHeld && !bReleasedDuringAnticipation)
			{
				ChargeTime = FMath::Min(ChargeTime + DeltaTime, Data.MaxChargeTime);
				if (ChargeTime >= Data.MaxChargeTime && PhaseTime > Data.MaxChargeTime + 1.5f)
				{
					bReleasedDuringAnticipation = true;
				}
				break;
			}
			if (PhaseTime < CastTime)
			{
				break; // minimum formation time even on a tap
			}
			ReleasedChargeAlpha = GetChargeAlpha();
			// Charge surcharge (bounded by MaxChargeTime -> ChargeManaScale).
			const float Extra = CommittedManaCost * (FMath::Lerp(1.f, Data.ChargeManaScale, ReleasedChargeAlpha) - 1.f);
			if (Extra > 0.f && Owner->GetAttributes() && !Owner->GetAttributes()->SpendMana(Extra))
			{
				// Could not pay the surcharge: fire at the charge level that is affordable.
				const float Affordable = Owner->GetAttributes()->GetMana();
				Owner->GetAttributes()->SpendMana(Affordable);
				const float Denominator = FMath::Max(KINDA_SMALL_NUMBER, CommittedManaCost * (Data.ChargeManaScale - 1.f));
				ReleasedChargeAlpha = FMath::Clamp(Affordable / Denominator, 0.f, ReleasedChargeAlpha);
			}
			Owner->GetStateTags().RemoveTag(MTTags::State_Charging);
			PlayReleaseAnimation();
		}
		else if (PhaseTime < CastTime)
		{
			break;
		}

		EnterPhase(EMTAbilityPhase::Action);
		Component->StartCooldown(Data.AbilityID, GetEffectiveCooldown());
		ExecuteAction();
		if (IsInstantAction())
		{
			FinishAction();
		}
		break;
	}
	case EMTAbilityPhase::Action:
		TickAction(DeltaTime);
		break;
	case EMTAbilityPhase::Recovery:
		if (PhaseTime >= Data.RecoveryTime)
		{
			EnterPhase(EMTAbilityPhase::Finished);
			Owner->GetStateTags().RemoveTags(Data.ActivationTags);
			OnEnded(false);
			Component->NotifyAbilityEnded(this, false);
			EnterPhase(EMTAbilityPhase::Idle);
		}
		break;
	default:
		break;
	}
}

void UMTAbility::FinishAction()
{
	if (Phase != EMTAbilityPhase::Action)
	{
		return;
	}
	if (AMTCharacterBase* Owner = GetOwnerCharacter())
	{
		Owner->GetStateTags().RemoveTag(MTTags::State_Casting);
		Owner->SetAbilityMoveMultiplier(1.f);
	}
	EnterPhase(EMTAbilityPhase::Recovery);
}

void UMTAbility::Cancel()
{
	if (!IsActive())
	{
		return;
	}
	if (AMTCharacterBase* Owner = GetOwnerCharacter())
	{
		Owner->GetStateTags().RemoveTag(MTTags::State_Casting);
		Owner->GetStateTags().RemoveTag(MTTags::State_Charging);
		Owner->GetStateTags().RemoveTags(Data.ActivationTags);
		Owner->SetAbilityMoveMultiplier(1.f);
	}
	// Cancelled during wind-up: no cooldown, but the mana spent on formation is lost.
	StopMontage(0.15f);
	OnEnded(true);
	EnterPhase(EMTAbilityPhase::Idle);
	if (Component.IsValid())
	{
		Component->NotifyAbilityEnded(this, true);
	}
}

void UMTAbility::EnterPhase(EMTAbilityPhase NewPhase)
{
	Phase = NewPhase;
	PhaseTime = 0.f;
}

AActor* UMTAbility::GetLockedTarget() const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	return Owner ? Owner->GetLockTarget() : nullptr;
}

FVector UMTAbility::GetAimPoint() const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	return Owner ? Owner->GetAimPoint() : FVector::ZeroVector;
}

FVector UMTAbility::GetCastLocation() const
{
	const AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}
	const USkeletalMeshComponent* Mesh = Owner->GetMesh();
	if (Mesh && !Data.CastSocket.IsNone() && Mesh->DoesSocketExist(Data.CastSocket))
	{
		return Mesh->GetSocketLocation(Data.CastSocket);
	}
	// Fallback: in front of the chest.
	return Owner->GetActorLocation() + Owner->GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f);
}

FRotator UMTAbility::GetAimRotation() const
{
	return (GetAimPoint() - GetCastLocation()).Rotation();
}

float UMTAbility::PlayMontage(FName Section, float PlayRate)
{
	if (Data.Montage.IsNull())
	{
		return 0.f;
	}
	// Faster casting plays the anticipation faster instead of cutting it.
	const float CastScale = Data.CastTime > 0.f ? FMath::Clamp(Data.CastTime / FMath::Max(0.05f, GetEffectiveCastTime()), 0.75f, 2.f) : 1.f;
	return PlayAbilityAnim(Data.Montage, PlayRate * CastScale, Section);
}

float UMTAbility::PlayAbilityAnim(const TSoftObjectPtr<UAnimSequenceBase>& Anim, float PlayRate, FName Section,
	int32 LoopCount, float BlendIn, float BlendOut)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || Anim.IsNull())
	{
		return 0.f;
	}
	const float Length = Owner->PlayAnimAsset(Anim, PlayRate, Section, LoopCount, BlendIn, BlendOut);
	if (Length > 0.f)
	{
		ActiveAnimMontage = Owner->GetLastPlayedMontage();
	}
	return Length;
}

void UMTAbility::JumpMontageToSection(FName Section)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || Section.IsNone())
	{
		return;
	}
	// Only an authored montage has sections; a plain sequence plays as a single-section dynamic montage.
	UAnimInstance* Anim = Owner->GetMesh() ? Owner->GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = Cast<UAnimMontage>(Data.Montage.Get());
	if (Anim && Montage && ActiveAnimMontage.Get() == Montage && Anim->Montage_IsPlaying(Montage)
		&& Montage->GetSectionIndex(Section) != INDEX_NONE)
	{
		Anim->Montage_JumpToSection(Section, Montage);
	}
}

void UMTAbility::StopMontage(float BlendOut)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UAnimMontage* Montage = ActiveAnimMontage.Get();
	if (Owner && Montage)
	{
		// Montage_Stop is a no-op if something else (a hit reaction, a dodge) already replaced it.
		Owner->StopPlayedAnim(Montage, BlendOut);
	}
	ActiveAnimMontage.Reset();
	bChargeLoopStarted = false;
}

void UMTAbility::UpdateChargeAnimation()
{
	if (bChargeLoopStarted || Data.ChargeLoopAnim.IsNull() || Phase != EMTAbilityPhase::Anticipation)
	{
		return;
	}
	AMTCharacterBase* Owner = GetOwnerCharacter();
	UAnimInstance* Anim = (Owner && Owner->GetMesh()) ? Owner->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}
	UAnimMontage* Anticipation = ActiveAnimMontage.Get();
	bool bStartLoop = false;
	if (Anticipation && Anim->Montage_IsPlaying(Anticipation))
	{
		// Start just before the anticipation's own auto blend-out (0.18 s before its end) so the pose never dips
		// toward locomotion. The anticipation keeps playing while it crossfades out over the loop's blend-in, so it
		// still reaches (almost) its final frame, which is the hold pose the loop starts in.
		const float Rate = FMath::Max(0.01f, FMath::Abs(Anim->Montage_GetPlayRate(Anticipation)));
		const float Remaining = (Anticipation->GetPlayLength() - Anim->Montage_GetPosition(Anticipation)) / Rate;
		bStartLoop = Remaining <= 0.2f;
	}
	else
	{
		// Anticipation finished or never played. Wait while another montage (a hit flinch) is still playing.
		const UAnimMontage* Current = Anim->GetCurrentActiveMontage();
		bStartLoop = Current == nullptr || Current == Anticipation;
	}
	if (!bStartLoop)
	{
		return;
	}
	// Large loop count: the hold lasts until release/cancel stops it. Played at rate 1 (holding is not casting).
	bChargeLoopStarted = true;
	PlayAbilityAnim(Data.ChargeLoopAnim, 1.f, NAME_None, 1000, 0.15f, 0.1f);
}

void UMTAbility::PlayReleaseAnimation()
{
	if (!Data.ReleaseAnim.IsNull())
	{
		StopMontage(0.05f); // hold loop (or a still-running anticipation)
		PlayAbilityAnim(Data.ReleaseAnim, 1.f, NAME_None, 1, 0.05f, 0.2f);
		return;
	}
	// Authored montage fallback: jump to its release section, or restart it there if the hold loop replaced it.
	UAnimMontage* Authored = Cast<UAnimMontage>(Data.Montage.Get());
	if (Authored && !Data.MontageReleaseSection.IsNone() && Authored->GetSectionIndex(Data.MontageReleaseSection) != INDEX_NONE)
	{
		AMTCharacterBase* Owner = GetOwnerCharacter();
		UAnimInstance* Anim = (Owner && Owner->GetMesh()) ? Owner->GetMesh()->GetAnimInstance() : nullptr;
		if (Anim && ActiveAnimMontage.Get() == Authored && Anim->Montage_IsPlaying(Authored))
		{
			JumpMontageToSection(Data.MontageReleaseSection);
		}
		else
		{
			StopMontage(0.05f);
			PlayAbilityAnim(Data.Montage, 1.f, Data.MontageReleaseSection, 1, 0.05f, 0.2f);
		}
		return;
	}
	if (bChargeLoopStarted)
	{
		StopMontage(0.2f); // nothing to release into: let go of the hold pose
	}
}

void UMTAbility::SpawnFX(const TSoftObjectPtr<UNiagaraSystem>& System, const FVector& Location, const FRotator& Rotation, bool bAttachToOwner)
{
	MTCombat::SpawnFX(GetOwnerCharacter(), System, Location, Rotation);
}

void UMTAbility::PlaySound(const TSoftObjectPtr<USoundBase>& Sound, const FVector& Location)
{
	MTCombat::PlaySound(GetOwnerCharacter(), Sound, Location);
}

void UMTAbility::PlaySubtleCameraShake(float Scale)
{
	// Deliberately tiny: FOV/camera nudge via the player's camera manager only.
	AMTCharacterBase* Owner = GetOwnerCharacter();
	APlayerController* PC = Owner ? Cast<APlayerController>(Owner->GetController()) : nullptr;
	if (!PC || !PC->PlayerCameraManager || Scale <= 0.f)
	{
		return;
	}
	static const TCHAR* ShakePath = TEXT("/Game/Camera/CS_MT_Subtle.CS_MT_Subtle_C");
	UClass* ShakeClass = StaticLoadClass(UCameraShakeBase::StaticClass(), nullptr, ShakePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (ShakeClass)
	{
		PC->ClientStartCameraShake(ShakeClass, FMath::Min(Scale, 0.35f));
	}
}

#include "Character/MTAttributeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UMTAttributeComponent::UMTAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
}

void UMTAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMTAttributeComponent::InitializeAttributes(float InMaxHealth, float InMaxMana, float InMaxStamina, float InMaxPoise, float InManaRegen)
{
	MaxHealth = FMath::Max(1.f, InMaxHealth);
	MaxMana = FMath::Max(0.f, InMaxMana);
	MaxStamina = FMath::Max(1.f, InMaxStamina);
	MaxPoise = FMath::Max(1.f, InMaxPoise);
	ManaRegen = FMath::Max(0.f, InManaRegen);
	Health = MaxHealth;
	Mana = MaxMana;
	Stamina = MaxStamina;
	Poise = MaxPoise;
	OnHealthChanged.Broadcast(Health, MaxHealth);
	OnManaChanged.Broadcast(Mana, MaxMana);
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}

void UMTAttributeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsAlive())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// Status effects: tick durations and periodic health/mana changes.
	float HealthDelta = HealthRegen * DeltaTime;
	float ManaDelta = ManaRegen * DeltaTime;
	for (int32 i = StatusEffects.Num() - 1; i >= 0; --i)
	{
		FMTStatusEffect& Effect = StatusEffects[i];
		HealthDelta += Effect.HealthPerSecond * DeltaTime;
		ManaDelta += Effect.ManaPerSecond * DeltaTime;
		if (Effect.Duration > 0.f)
		{
			Effect.Remaining -= DeltaTime;
			if (Effect.Remaining <= 0.f)
			{
				const FName Id = Effect.Id;
				StatusEffects.RemoveAt(i);
				OnStatusRemoved.Broadcast(Id);
			}
		}
	}

	if (!FMath::IsNearlyZero(HealthDelta))
	{
		if (HealthDelta < 0.f)
		{
			// Damage over time bypasses combo scaling and poise.
			Health = FMath::Clamp(Health + HealthDelta, 0.f, MaxHealth);
			OnHealthChanged.Broadcast(Health, MaxHealth);
			if (Health <= 0.f)
			{
				OnDeath.Broadcast(nullptr);
			}
		}
		else if (Health < MaxHealth)
		{
			Health = FMath::Min(MaxHealth, Health + HealthDelta);
			OnHealthChanged.Broadcast(Health, MaxHealth);
		}
	}

	if (ManaDelta > 0.f && Mana < MaxMana)
	{
		Mana = FMath::Min(MaxMana, Mana + ManaDelta);
		OnManaChanged.Broadcast(Mana, MaxMana);
	}
	else if (ManaDelta < 0.f && Mana > 0.f)
	{
		Mana = FMath::Max(0.f, Mana + ManaDelta);
		OnManaChanged.Broadcast(Mana, MaxMana);
	}

	if (Now - LastStaminaSpendTime > StaminaRegenDelay && Stamina < MaxStamina)
	{
		Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegen * DeltaTime);
		OnStaminaChanged.Broadcast(Stamina, MaxStamina);
	}

	// Poise recovers after a short delay; a broken poise bar refills fully.
	if (Now - LastPoiseHitTime > 1.5f && Poise < MaxPoise)
	{
		Poise = FMath::Min(MaxPoise, Poise + PoiseRegen * DeltaTime);
	}

	if (ComboHits > 0 && Now - LastComboHitTime > ComboWindow)
	{
		ComboHits = 0;
		ComboAttacker.Reset();
	}
}

FMTDamageResult UMTAttributeComponent::ApplyDamage(const FMTDamageSpec& Spec)
{
	FMTDamageResult Result;
	if (!IsAlive())
	{
		return Result;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (IsInvulnerable())
	{
		Result.bDodged = true;
		return Result;
	}

	// Combo scaling: repeated hits from the same attacker lose value; after the cap the
	// target gets a short hit-stun immunity so there are no infinite combos.
	AActor* Attacker = Spec.Instigator.Get();
	if (Attacker && ComboAttacker.Get() == Attacker && Now - LastComboHitTime <= ComboWindow)
	{
		++ComboHits;
	}
	else
	{
		ComboAttacker = Attacker;
		ComboHits = 1;
	}
	LastComboHitTime = Now;
	Result.ComboScale = FMath::Max(ComboMinScale, 1.f - ComboDecayPerHit * (ComboHits - 1));

	const FMTStatModifier Mods = GetStatModifier();
	float Resist = FMath::Clamp(Mods.DamageResistance, 0.f, 0.9f);
	if (Spec.bIsMagic)
	{
		Resist = FMath::Clamp(Resist + Mods.MagicResistance, 0.f, 0.9f);
	}

	float Damage = Spec.Damage * (1.f - Resist) * Result.ComboScale;
	// Stacked auras/awakenings cap at 75% stagger resistance: never fully unstaggerable.
	float StaggerAmount = Spec.Stagger * (1.f - FMath::Clamp(Mods.StaggerResistance, 0.f, 0.75f)) * Result.ComboScale;

	// Guard: blocking works against attacks from the front and costs stamina.
	if (bBlocking && GetOwner())
	{
		const FVector Facing = GetOwner()->GetActorForwardVector();
		if (FVector::DotProduct(Facing, -Spec.HitDirection.GetSafeNormal()) > 0.25f && Stamina > 0.f)
		{
			Result.bBlocked = true;
			Damage *= 0.25f;
			StaggerAmount *= 0.35f;
			SpendStamina(FMath::Max(8.f, Spec.Stagger * 0.4f));
		}
	}

	Damage = FMath::Max(0.f, Damage);
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	Result.DamageDealt = Damage;

	// Poise / reactions.
	LastPoiseHitTime = Now;
	Poise -= StaggerAmount;
	const bool bHitStunImmune = Now < HitStunImmuneUntil;
	if (!bHitStunImmune)
	{
		if (Poise <= 0.f)
		{
			Poise = MaxPoise;
			Result.Reaction = (Spec.Knockback > 600.f && !bCrowdControlImmune) ? EMTHitReaction::Knockdown : EMTHitReaction::Stagger;
		}
		else if (Spec.Knockback > 250.f && !bCrowdControlImmune)
		{
			Result.Reaction = EMTHitReaction::Knockback;
		}
		else if (StaggerAmount > 0.f)
		{
			Result.Reaction = EMTHitReaction::Flinch;
		}
	}
	if (ComboHits >= MaxComboHitsBeforeBurst)
	{
		HitStunImmuneUntil = Now + 1.5f;
		ComboHits = 0;
	}

	OnHealthChanged.Broadcast(Health, MaxHealth);
	if (Health <= 0.f)
	{
		Result.bKilled = true;
		StatusEffects.Reset();
	}
	OnDamaged.Broadcast(Spec, Result);
	if (Result.bKilled)
	{
		OnDeath.Broadcast(Attacker);
	}
	return Result;
}

void UMTAttributeComponent::Heal(float Amount)
{
	if (!IsAlive() || Amount <= 0.f)
	{
		return;
	}
	Health = FMath::Min(MaxHealth, Health + Amount);
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

bool UMTAttributeComponent::SpendMana(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}
	if (Mana < Amount)
	{
		return false;
	}
	Mana -= Amount;
	OnManaChanged.Broadcast(Mana, MaxMana);
	return true;
}

bool UMTAttributeComponent::SpendStamina(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}
	LastStaminaSpendTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Stamina <= 0.f)
	{
		return false;
	}
	// Allow the last action to overdraw slightly so a nearly-empty bar still dodges once.
	Stamina = FMath::Max(0.f, Stamina - Amount);
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);
	return true;
}

void UMTAttributeComponent::RestoreMana(float Amount)
{
	Mana = FMath::Clamp(Mana + Amount, 0.f, MaxMana);
	OnManaChanged.Broadcast(Mana, MaxMana);
}

void UMTAttributeComponent::RestoreStamina(float Amount)
{
	Stamina = FMath::Clamp(Stamina + Amount, 0.f, MaxStamina);
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}

void UMTAttributeComponent::RestorePoise(float Amount)
{
	Poise = FMath::Clamp(Poise + Amount, 0.f, MaxPoise);
}

void UMTAttributeComponent::AddAwakeningMeter(float Amount)
{
	AwakeningMeter = FMath::Clamp(AwakeningMeter + Amount, 0.f, 100.f);
}

bool UMTAttributeComponent::ConsumeAwakeningMeter(float Amount)
{
	if (AwakeningMeter + KINDA_SMALL_NUMBER < Amount)
	{
		return false;
	}
	AwakeningMeter = FMath::Max(0.f, AwakeningMeter - Amount);
	return true;
}

void UMTAttributeComponent::AddStatusEffect(const FMTStatusEffect& Effect)
{
	FMTStatusEffect Copy = Effect;
	if (bCrowdControlImmune && Copy.Movement.bRooted)
	{
		// Bosses resist hard control: convert roots into a mild slow.
		Copy.Movement.bRooted = false;
		Copy.Movement.SpeedMultiplier = FMath::Max(Copy.Movement.SpeedMultiplier, 0.75f);
		Copy.Movement.AccelerationMultiplier = FMath::Max(Copy.Movement.AccelerationMultiplier, 0.75f);
	}
	Copy.Remaining = Copy.Duration;

	for (FMTStatusEffect& Existing : StatusEffects)
	{
		if (Existing.Id == Copy.Id)
		{
			Existing = Copy;
			return;
		}
	}
	StatusEffects.Add(Copy);
	OnStatusAdded.Broadcast(Copy.Id);
}

void UMTAttributeComponent::RemoveStatusEffect(FName EffectId)
{
	const int32 Removed = StatusEffects.RemoveAll([EffectId](const FMTStatusEffect& E) { return E.Id == EffectId; });
	if (Removed > 0)
	{
		OnStatusRemoved.Broadcast(EffectId);
	}
}

bool UMTAttributeComponent::HasStatusEffect(FName EffectId) const
{
	return StatusEffects.ContainsByPredicate([EffectId](const FMTStatusEffect& E) { return E.Id == EffectId; });
}

FMTStatModifier UMTAttributeComponent::GetStatModifier() const
{
	FMTStatModifier Result = PermanentStats;
	for (const FMTStatusEffect& Effect : StatusEffects)
	{
		Result.Combine(Effect.Stats);
	}
	return Result;
}

FMTMovementModifier UMTAttributeComponent::GetMovementModifier() const
{
	FMTMovementModifier Result = PermanentMovement;
	for (const FMTStatusEffect& Effect : StatusEffects)
	{
		Result.Combine(Effect.Movement);
	}
	if (bCrowdControlImmune)
	{
		Result.bRooted = false;
	}
	// Never let stacked slows fully immobilise (Quagmire must not freeze enemies).
	Result.SpeedMultiplier = FMath::Clamp(Result.SpeedMultiplier, 0.15f, 3.f);
	Result.AccelerationMultiplier = FMath::Clamp(Result.AccelerationMultiplier, 0.15f, 3.f);
	return Result;
}

FGameplayTagContainer UMTAttributeComponent::GetStatusTags() const
{
	FGameplayTagContainer Tags;
	for (const FMTStatusEffect& Effect : StatusEffects)
	{
		Tags.AppendTags(Effect.GrantedTags);
	}
	return Tags;
}

void UMTAttributeComponent::SetPermanentModifiers(const FMTStatModifier& Stats, const FMTMovementModifier& Movement)
{
	PermanentStats = Stats;
	PermanentMovement = Movement;
}

void UMTAttributeComponent::SetInvulnerableFor(float Seconds)
{
	if (const UWorld* World = GetWorld())
	{
		InvulnerableUntil = FMath::Max(InvulnerableUntil, World->GetTimeSeconds() + Seconds);
	}
}

bool UMTAttributeComponent::IsInvulnerable() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < InvulnerableUntil;
}

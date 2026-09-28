// Shared enums and small structs used by every gameplay system.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MTTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMushoku, Log, All);

UENUM(BlueprintType)
enum class EMTRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Legendary,
	Mythic
};

UENUM(BlueprintType)
enum class EMTElement : uint8
{
	None,
	Fire,
	Water,
	Earth,
	Wind,
	// Non-elemental schools used by character kits (Orsted's techniques, Demon Eye).
	Arcane
};

UENUM(BlueprintType)
enum class EMTRace : uint8
{
	Human,
	Migurd,
	Superd,
	Beast,
	Elf,
	Dwarf,
	DragonTribe,
	ImmortalDemon,
	Ogre,
	SeaRace
};

UENUM(BlueprintType)
enum class EMTMagicRank : uint8
{
	Beginner,
	Intermediate,
	Advanced,
	Saint,
	King,
	Emperor,
	God
};

UENUM(BlueprintType)
enum class EMTAdventurerRank : uint8
{
	F, E, D, C, B, A, S
};

UENUM(BlueprintType)
enum class EMTRollCategory : uint8
{
	Character,
	Element,
	Race
};

UENUM(BlueprintType)
enum class EMTMasteryTrack : uint8
{
	Character,
	Element,
	Race
};

/** Which reusable behaviour class an ability row uses. */
UENUM(BlueprintType)
enum class EMTAbilityBehavior : uint8
{
	Projectile,   // Stone Cannon, Fire Burst, Water Cannon, Air Blade, basic bolts
	Zone,         // Quagmire, Flame Field, Frost Prison, Cumulonimbus, Tempest Domain
	Sequence,     // Elemental Barrage
	Dash,         // Dragon Step, Gale Step
	Counter,      // Disturb Magic
	Buff,         // Saint Dragon Aura, Demon Eye, awakenings, race transformations
	Structure,    // Earth Fortress
	Melee         // Orsted palm strikes, enemy claws
};

/** Projectile flight model. */
UENUM(BlueprintType)
enum class EMTProjectileMotion : uint8
{
	Straight,
	Arc,
	Piercing,
	Wave       // wind blades: wide thin hit shape, slight sine drift
};

/** How a zone applies its payload. */
UENUM(BlueprintType)
enum class EMTZoneKind : uint8
{
	Mire,        // Quagmire: movement debuffs, stone synergy
	Eruptions,   // Flame Field: delayed eruptions inside the ring
	Freeze,      // Frost Prison: root in centre, slow outside
	Storm,       // Cumulonimbus: rain, water efficiency buff
	WindField,   // Tempest Domain: pushes weak enemies, deflects projectiles
	Aura         // Saint Dragon Aura pressure field (attached to owner)
};

/** Behaviour-agnostic hit reaction classes. */
UENUM(BlueprintType)
enum class EMTHitReaction : uint8
{
	None,
	Flinch,
	Stagger,
	Knockback,
	Knockdown
};

UENUM(BlueprintType)
enum class EMTStance : uint8
{
	Rudeus,   // light, forward-leaning mage stance
	Orsted,   // upright, still, economical
	Generic
};

/** Multipliers applied to CharacterMovement by status effects (1 = unchanged). */
USTRUCT(BlueprintType)
struct FMTMovementModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpeedMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AccelerationMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float JumpMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DodgeDistanceMultiplier = 1.f;
	/** Hard root. Ignored by targets with CrowdControlImmune (bosses). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRooted = false;

	void Combine(const FMTMovementModifier& Other)
	{
		SpeedMultiplier *= Other.SpeedMultiplier;
		AccelerationMultiplier *= Other.AccelerationMultiplier;
		JumpMultiplier *= Other.JumpMultiplier;
		DodgeDistanceMultiplier *= Other.DodgeDistanceMultiplier;
		bRooted |= Other.bRooted;
	}
};

/** Generic stat modifiers granted by buffs, auras, awakenings and passives. */
USTRUCT(BlueprintType)
struct FMTStatModifier
{
	GENERATED_BODY()

	/** Multiplies outgoing damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageMultiplier = 1.f;
	/** Fraction of incoming damage removed (0..0.9). Additive across effects, clamped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageResistance = 0.f;
	/** Extra fraction removed from magic damage only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MagicResistance = 0.f;
	/** Fraction of incoming stagger removed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StaggerResistance = 0.f;
	/** Multiplies mana costs (0.8 = 20% cheaper). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ManaCostMultiplier = 1.f;
	/** Multiplies cast / startup times (0.7 = 30% faster). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CastTimeMultiplier = 1.f;
	/** Multiplies cooldowns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CooldownMultiplier = 1.f;
	/** Multiplies AoE radii of zones created by this character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AreaMultiplier = 1.f;
	/** Extra seconds added to counter / parry timing windows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CounterWindowBonus = 0.f;
	/** Multiplies dash distance/speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DashMultiplier = 1.f;
	/** Multiplies projectile speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectileSpeedMultiplier = 1.f;
	/** Seconds of advance warning added to Demon Eye foresight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ForesightLeadBonus = 0.f;

	void Combine(const FMTStatModifier& O)
	{
		DamageMultiplier *= O.DamageMultiplier;
		DamageResistance += O.DamageResistance;
		MagicResistance += O.MagicResistance;
		StaggerResistance += O.StaggerResistance;
		ManaCostMultiplier *= O.ManaCostMultiplier;
		CastTimeMultiplier *= O.CastTimeMultiplier;
		CooldownMultiplier *= O.CooldownMultiplier;
		AreaMultiplier *= O.AreaMultiplier;
		CounterWindowBonus += O.CounterWindowBonus;
		DashMultiplier *= O.DashMultiplier;
		ProjectileSpeedMultiplier *= O.ProjectileSpeedMultiplier;
		ForesightLeadBonus += O.ForesightLeadBonus;
	}
};

/** A timed status effect living on a UMTAttributeComponent. */
USTRUCT(BlueprintType)
struct FMTStatusEffect
{
	GENERATED_BODY()

	/** Effects with the same Id refresh instead of stacking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Remaining = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTMovementModifier Movement;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTStatModifier Stats;
	/** Health change per second (negative = damage over time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HealthPerSecond = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ManaPerSecond = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer GrantedTags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TWeakObjectPtr<AActor> Instigator;
};

/** Everything needed to resolve one hit. */
USTRUCT(BlueprintType)
struct FMTDamageSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 0.f;
	/** Poise damage; when a target's poise breaks it staggers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stagger = 0.f;
	/** Horizontal impulse in cm/s applied via LaunchCharacter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Knockback = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTElement Element = EMTElement::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsMagic = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SourceAbility;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TWeakObjectPtr<AActor> Instigator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector HitLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector HitDirection = FVector::ForwardVector;
};

/** Result returned to the attacker (for mastery, hit-stop, UI numbers). */
USTRUCT(BlueprintType)
struct FMTDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) float DamageDealt = 0.f;
	UPROPERTY(BlueprintReadOnly) EMTHitReaction Reaction = EMTHitReaction::None;
	UPROPERTY(BlueprintReadOnly) bool bKilled = false;
	UPROPERTY(BlueprintReadOnly) bool bBlocked = false;
	UPROPERTY(BlueprintReadOnly) bool bDodged = false;
	/** Combo scaling that was applied (1 = none). */
	UPROPERTY(BlueprintReadOnly) float ComboScale = 1.f;
};

/**
 * An incoming attack announced ahead of time. Enemies/bosses publish these through
 * UMTTelegraphSubsystem; Demon Eye renders them as predictive silhouettes and AI uses
 * them to decide when to dodge or counter.
 */
USTRUCT(BlueprintType)
struct FMTAttackTelegraph
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Attacker;
	UPROPERTY(BlueprintReadOnly) FName AttackId;
	/** World location the attacker will be at when the attack lands. */
	UPROPERTY(BlueprintReadOnly) FVector PredictedAttackerLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FRotator PredictedAttackerRotation = FRotator::ZeroRotator;
	/** Centre of the danger area. */
	UPROPERTY(BlueprintReadOnly) FVector ImpactLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector Direction = FVector::ForwardVector;
	UPROPERTY(BlueprintReadOnly) float Radius = 200.f;
	UPROPERTY(BlueprintReadOnly) float Length = 0.f; // >0 = line/cone attack
	/** World time (seconds) the hit lands. */
	UPROPERTY(BlueprintReadOnly) float ImpactTime = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bIsMagic = false;
	UPROPERTY(BlueprintReadOnly) bool bIsImportant = true;
};

namespace MTUtil
{
	inline FString RarityToString(EMTRarity R)
	{
		switch (R)
		{
		case EMTRarity::Common: return TEXT("Common");
		case EMTRarity::Uncommon: return TEXT("Uncommon");
		case EMTRarity::Rare: return TEXT("Rare");
		case EMTRarity::Legendary: return TEXT("Legendary");
		case EMTRarity::Mythic: return TEXT("Mythic");
		}
		return TEXT("?");
	}

	inline FLinearColor RarityColor(EMTRarity R)
	{
		switch (R)
		{
		case EMTRarity::Common: return FLinearColor(0.75f, 0.75f, 0.72f);
		case EMTRarity::Uncommon: return FLinearColor(0.35f, 0.8f, 0.45f);
		case EMTRarity::Rare: return FLinearColor(0.3f, 0.55f, 1.f);
		case EMTRarity::Legendary: return FLinearColor(1.f, 0.72f, 0.2f);
		case EMTRarity::Mythic: return FLinearColor(0.85f, 0.25f, 0.95f);
		}
		return FLinearColor::White;
	}

	inline FString ElementToString(EMTElement E)
	{
		switch (E)
		{
		case EMTElement::None: return TEXT("None");
		case EMTElement::Fire: return TEXT("Fire");
		case EMTElement::Water: return TEXT("Water");
		case EMTElement::Earth: return TEXT("Earth");
		case EMTElement::Wind: return TEXT("Wind");
		case EMTElement::Arcane: return TEXT("Arcane");
		}
		return TEXT("?");
	}

	inline FLinearColor ElementColor(EMTElement E)
	{
		switch (E)
		{
		case EMTElement::Fire: return FLinearColor(1.f, 0.42f, 0.12f);
		case EMTElement::Water: return FLinearColor(0.2f, 0.55f, 1.f);
		case EMTElement::Earth: return FLinearColor(0.62f, 0.48f, 0.3f);
		case EMTElement::Wind: return FLinearColor(0.55f, 0.95f, 0.8f);
		case EMTElement::Arcane: return FLinearColor(0.7f, 0.5f, 1.f);
		default: return FLinearColor(0.85f, 0.85f, 0.85f);
		}
	}

	inline FString MagicRankToString(EMTMagicRank R)
	{
		static const TCHAR* Names[] = { TEXT("Beginner"), TEXT("Intermediate"), TEXT("Advanced"), TEXT("Saint"), TEXT("King"), TEXT("Emperor"), TEXT("God") };
		return Names[FMath::Clamp((int32)R, 0, 6)];
	}

	inline FString AdventurerRankToString(EMTAdventurerRank R)
	{
		static const TCHAR* Names[] = { TEXT("F"), TEXT("E"), TEXT("D"), TEXT("C"), TEXT("B"), TEXT("A"), TEXT("S") };
		return Names[FMath::Clamp((int32)R, 0, 6)];
	}
}

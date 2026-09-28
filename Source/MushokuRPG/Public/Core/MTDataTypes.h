// Data rows. Each row type doubles as a DataTable row (FTableRowBase) and as a JSON record
// loaded at runtime from Content/Data/*.json by UMTDataRegistry, so designers can iterate
// without recompiling and the game still runs before any .uasset data exists.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/MTTypes.h"
#include "MTDataTypes.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;
class UTexture2D;
class USkeletalMesh;
class UAnimInstance;
class UStaticMesh;
class UMaterialInterface;

/** Presentation for one phase of a spell (formation / travel / impact / dissipation). */
USTRUCT(BlueprintType)
struct FMTSpellFX
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UNiagaraSystem> Formation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UNiagaraSystem> Travel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UNiagaraSystem> Impact;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UNiagaraSystem> Dissipation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USoundBase> CastSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USoundBase> TravelSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USoundBase> ImpactSound;
	/** Mesh used for the physical body of the spell (stone slug, ice shard). Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UStaticMesh> BodyMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UMaterialInterface> BodyMaterial;
	/** Ground decal material for zones (avoids coplanar geometry / z-fighting). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UMaterialInterface> DecalMaterial;
	/** Very subtle camera shake scale (0 = none). Clamped to 0.35 at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CameraShakeScale = 0.f;
};

/** One step of a Sequence ability (Elemental Barrage). */
USTRUCT(BlueprintType)
struct FMTSequenceStep
{
	GENERATED_BODY()

	/** Ability row fired at this step (must be a Projectile row). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AbilityId;
	/** Seconds after the previous step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Delay = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MontageSection;
};

/** Data for a single ability. Field names follow the design spec. */
USTRUCT(BlueprintType)
struct FMTAbilityData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AbilityID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTAbilityBehavior Behavior = EMTAbilityBehavior::Projectile;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTElement Element = EMTElement::None;

	/** Character lineage required (None = any). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CharacterRequirement;
	/** Element required in an element slot (None = no requirement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTElement ElementRequirement = EMTElement::None;
	/** Race required (only checked when bRequiresRace). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTRace RaceRequirement = EMTRace::Human;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresRace = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ManaCost = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StaminaCost = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Cooldown = 1.f;
	/** Anticipation time before the action frame. Chantless casting shortens this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CastTime = 0.2f;
	/** Recovery time after the action (player locked out of new abilities, can move). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RecoveryTime = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Range = 2500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AOERadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stagger = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Knockback = 0.f;
	/** Seconds the effect persists (zones, buffs, structures). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 0.f;

	// --- Charge (hold to charge) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bChargeable = false;
	/** Max charge seconds; charging stops here (no infinite charge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxChargeTime = 0.f;
	/** Multipliers reached at full charge (linear from 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChargeSpeedScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChargeDamageScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChargeStaggerScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChargeManaScale = 1.f;

	// --- Projectile ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTProjectileMotion Motion = EMTProjectileMotion::Straight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectileSpeed = 3000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectileRadius = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectileGravity = 0.f;
	/** Can Orsted's Disturb Magic collapse this spell? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDisruptable = true;

	// --- Zone ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTZoneKind ZoneKind = EMTZoneKind::Mire;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTMovementModifier ZoneMovement;
	/** Stronger modifier used inside InnerRadius (Frost Prison centre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTMovementModifier ZoneInnerMovement;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float InnerRadius = 0.f;
	/** Delay before the zone's main payload (Flame Field eruption warning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ActivationDelay = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PulseCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PulseInterval = 0.5f;

	// --- Dash ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DashDistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DashDuration = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDashTowardTarget = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDashInvulnerable = false;

	// --- Counter ---
	/** Seconds the counter stance is active (timing window). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CounterWindow = 0.f;
	/** Radius around the user in which incoming spells are disrupted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CounterRadius = 0.f;

	// --- Buff / aura / awakening ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTStatModifier BuffStats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTMovementModifier BuffMovement;
	/** Health / mana regained per second while the buff lasts (Immortal State). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BuffHealthPerSecond = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BuffManaPerSecond = 0.f;
	/** Instant stamina / poise restored on activation (Human Second Wind). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BuffStaminaRestore = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BuffPoiseRestore = 0.f;
	/** Seconds of the uninterruptible transformation sequence before control returns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TransformationTime = 0.f;
	/** Abilities that change while this buff is active (awakening upgrades). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FName> AbilityOverrides;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsAwakening = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGrantsForesight = false;
	/** Awakening resource: requires this much Awakening meter (0..100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AwakeningMeterCost = 0.f;

	// --- Structure ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StructureCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StructureHealth = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector StructureExtent = FVector(40.f, 160.f, 140.f);

	// --- Sequence ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMTSequenceStep> Sequence;
	/** Movement speed multiplier while executing (lets the caster reposition slightly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MoveSpeedWhileActive = 0.35f;

	// --- Presentation ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimMontage> Montage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MontageStartSection;
	/** Section jumped to when a held/charged ability is released. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MontageReleaseSection;
	/** Name of the motion-warp target used by the montage (Dragon Step). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WarpTargetName;
	/** Socket spells spawn from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CastSocket = TEXT("hand_r");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTSpellFX FX;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Icon;

	// --- Progression ---
	/** e.g. "Level:5", "Quest:Q_Buena_Intro", "Mastery:Rudeus:3". Empty = unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString UnlockRequirement;
	/** Mastery level of the owning track required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MasteryRequirement = 0;
	/** Mastery XP granted on successful use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MasteryXP = 5.f;
	/** Minimum magic rank of the element required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTMagicRank RankRequirement = EMTMagicRank::Beginner;
	/** Tags applied to the owner while the ability runs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer ActivationTags;
};

/** Character lineage (the "bloodline" analogue). */
USTRUCT(BlueprintType)
struct FMTCharacterData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CharacterID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTRarity Rarity = EMTRarity::Rare;
	/** Relative weight inside its rarity tier in the roll pool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RollWeight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTStance Stance = EMTStance::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftClassPtr<UAnimInstance> AnimClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CapsuleRadius = 34.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CapsuleHalfHeight = 82.f;
	/** Mesh relative Z so feet touch the capsule bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MeshOffsetZ = -82.f;

	// Locomotion tuning (cm/s). Orsted: slower walk, very high acceleration = controlled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WalkSpeed = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RunSpeed = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SprintSpeed = 700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Acceleration = 2048.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BrakingDeceleration = 2048.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RotationRateYaw = 540.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float JumpZVelocity = 520.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DodgeDistance = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth = 1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxMana = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ManaRegen = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxStamina = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxPoise = 100.f;

	/** Basic attack (LMB). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BasicAbility;
	/** Three signature abilities (keys 1-3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Abilities;
	/** Special (key F): Demon Eye for Rudeus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SpecialAbility;
	/** Awakening (key G). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AwakeningAbility;
	/** Passive id handled in code: "ChantlessCasting", "DragonGodKnowledge". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Passive;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText PassiveDescription;
	/** Always-on passive stat changes (kept small by design). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTStatModifier PassiveStats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor AuraColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Portrait;
};

/** Element definition: exactly three abilities per element. */
USTRUCT(BlueprintType)
struct FMTElementData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTElement Element = EMTElement::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTRarity Rarity = EMTRarity::Common;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RollWeight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Abilities;
};

/** Race definition. Canon vs gameplay-original is tracked per feature. */
USTRUCT(BlueprintType)
struct FMTRaceData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTRace Race = EMTRace::Human;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RaceID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTRarity Rarity = EMTRarity::Common;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RollWeight = 1.f;
	/** False = data exists but the race is not yet rollable (framework proof scope). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bImplemented = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText PassiveName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText PassiveDescription;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTStatModifier PassiveStats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTMovementModifier PassiveMovement;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealthMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxManaMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxStaminaMultiplier = 1.f;
	/** Passive health regeneration in HP/s (Immortal Demon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PassiveHealthRegen = 0.f;

	/** Active racial ability (key R). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ActiveAbility;
	/** Transformation / awakening (key T). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TransformationAbility;
	/** "CANON" or "GAMEPLAY ORIGINAL" per feature, shown in UI and docs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PassiveCanonStatus = TEXT("GAMEPLAY ORIGINAL");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TransformationCanonStatus = TEXT("GAMEPLAY ORIGINAL");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString LoreNote;
};

UENUM(BlueprintType)
enum class EMTQuestType : uint8
{
	Basic,
	Combat,
	Elite,
	Boss,
	Story,
	WorldEvent
};

UENUM(BlueprintType)
enum class EMTObjectiveType : uint8
{
	Kill,        // TargetId = enemy id or tag, Count
	Gather,      // TargetId = item id, Count (picked up in world)
	Deliver,     // TargetId = item id, SecondaryId = npc id
	TalkTo,      // TargetId = npc id
	Reach,       // TargetId = location id
	Defend,      // TargetId = npc/object id, Duration seconds
	Escort,      // TargetId = npc id, SecondaryId = destination location id
	Interact,    // TargetId = interactable object id, Count
	Puzzle,      // TargetId = puzzle id (completed by puzzle actor)
	DefeatBoss,  // TargetId = boss id
	UseAbility   // TargetId = ability id, Count (magical tasks)
};

USTRUCT(BlueprintType)
struct FMTQuestObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTObjectiveType Type = EMTObjectiveType::Kill;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TargetId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SecondaryId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	/** Optional world location id used for the HUD marker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MarkerLocationId;
};

USTRUCT(BlueprintType)
struct FMTQuestReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 XP = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Gold = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AdventurerPoints = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CharacterSpins = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ElementSpins = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RaceSpins = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, int32> Items;
	/** Mastery XP granted to the currently equipped character track. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MasteryXP = 0.f;
};

USTRUCT(BlueprintType)
struct FMTQuestData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName QuestID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTQuestType Type = EMTQuestType::Basic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Summary;
	/** NPC id that offers the quest (None for world events). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName GiverNPC;
	/** NPC id to turn in to (None = auto-complete). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TurnInNPC;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTAdventurerRank RequiredRank = EMTAdventurerRank::F;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RequiredLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> PrerequisiteQuests;
	/** Objectives are sequential stages for Story quests, parallel for others. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMTQuestObjective> Objectives;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSequentialObjectives = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRepeatable = false;
	/** Seconds before a repeatable quest can be taken again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RepeatCooldown = 300.f;
	/** Elite modifiers, e.g. "Enraged", "ManaShielded", "Swift". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Modifiers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMTQuestReward Reward;
	/** Region id (Fittoa, Roa ...) for world-event scheduling and map filters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Region = TEXT("Fittoa");
	/** Dialogue lines spoken by the giver when offering the quest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FText> OfferDialogue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FText> CompleteDialogue;
};

/** One attack in an enemy's or boss's repertoire. */
USTRUCT(BlueprintType)
struct FMTEnemyAttack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AttackId;
	/** Ability row executed for the attack (reuses the player ability framework). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AbilityId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxRange = 250.f;
	/** Telegraph duration before the hit (drives Demon Eye and dodge windows). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TelegraphTime = 0.6f;
	/** Punishable window after the attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RecoveryTime = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Weight = 1.f;
	/** Boss phase index from which this attack is available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinPhase = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsImportant = true;
};

USTRUCT(BlueprintType)
struct FMTEnemyData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EnemyID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer Tags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftClassPtr<UAnimInstance> AnimClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxPoise = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MoveSpeed = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Scale = 1.f;
	/** Weak NPCs are pushed by Saint Dragon Aura / Tempest Domain pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsWeak = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsBoss = false;
	/** Immune to roots/hard control (bosses). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCrowdControlImmune = false;
	/** Preferred engagement distance for the utility AI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PreferredRange = 200.f;
	/** 0..1 how likely it is to dodge/block when it sees an attack coming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Reactivity = 0.3f;
	/** 0..1 how aggressive (attack vs reposition bias). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Aggression = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMTEnemyAttack> Attacks;
	/** Health fractions at which boss phases begin, e.g. [0.66, 0.33]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<float> PhaseThresholds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 XPReward = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, float> DropTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTElement Element = EMTElement::None;
	/** Character lineage to use instead of an enemy kit (Orsted AI in the test arena). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CharacterLineage;
};

USTRUCT(BlueprintType)
struct FMTItemData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTRarity Rarity = EMTRarity::Common;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxStack = 99;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Value = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bQuestItem = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Icon;
};

/** Named world locations (quest markers, fast travel, discovery). */
USTRUCT(BlueprintType)
struct FMTLocationData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LocationID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Region = TEXT("Fittoa");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DiscoveryRadius = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFastTravel = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTAdventurerRank RequiredRank = EMTAdventurerRank::F;
};

/** Roll table tuning (pity etc.) for one category. */
USTRUCT(BlueprintType)
struct FMTRollConfig : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMTRollCategory Category = EMTRollCategory::Character;
	/** Base probability per rarity tier (normalised at runtime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<EMTRarity, float> RarityWeights;
	/** Spins without a Legendary+ before soft pity starts raising the odds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SoftPityStart = 40;
	/** Extra Legendary+ probability added per spin after soft pity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SoftPityStep = 0.02f;
	/** Guaranteed Legendary+ at this many spins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HardPity = 70;
	/** Guaranteed Mythic at this many spins without a Mythic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MythicHardPity = 160;
	/** Re-roll once when the result is already owned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDuplicateProtection = true;
};

// Shared base for the player, enemies, bosses and NPC combatants.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPath.h"
#include "Core/MTDataTypes.h"
#include "MTCharacterBase.generated.h"

class UMTAttributeComponent;
class UMTAbilityComponent;
class UMotionWarpingComponent;
class UAnimMontage;
class UAnimSequenceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnLineageChanged, FName, CharacterId);

UCLASS(Abstract)
class MUSHOKURPG_API AMTCharacterBase : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AMTCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	// IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& InTeamId) override { TeamId = InTeamId; }

	/** Applies a character lineage (mesh, anim, stats, abilities, movement tuning). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Character")
	virtual void ApplyCharacterLineage(FName CharacterId);

	/** Applies race passives (movement/stat modifiers, attribute multipliers, racial slots). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Character")
	virtual void ApplyRace(EMTRace Race);

	/** Assigns the 3 abilities of an element into element slot A (0) or B (1). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Character")
	virtual void ApplyElementSlot(int32 SlotIndex, EMTElement Element);

	/** Central entry point for all damage. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Combat")
	virtual FMTDamageResult ReceiveHit(const FMTDamageSpec& Spec);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Combat")
	bool IsHostileTo(const AActor* Other) const;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Combat")
	bool IsAlive() const;

	/** Dodge in a world direction (zero = backwards). Uses stamina, grants i-frames. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Movement")
	virtual bool Dodge(FVector WorldDirection);

	/** Called by the Counter ability or passive when a counter/perfect dodge succeeds. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Combat")
	virtual void NotifyPerfectDefense(FName Kind);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Combat") AActor* GetLockTarget() const { return LockTarget.Get(); }
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Combat") void SetLockTarget(AActor* NewTarget) { LockTarget = NewTarget; }

	/** Point this character aims spells at (lock-on target or look direction). */
	virtual FVector GetAimPoint() const;

	/** Seconds since the last dodge started (perfect-dodge checks). */
	float GetTimeSinceDodgeStart() const;
	bool IsDodging() const { return bDodging; }
	bool IsStaggered() const;

	/**
	 * Plays any animation asset without needing montage assets: an authored UAnimMontage via Montage_Play (+ optional
	 * section), anything else (a plain AnimSequence) as a dynamic montage in DefaultSlot. Returns the play length
	 * (0 if the asset is unset, missing or could not play). The montage instance is remembered, see GetLastPlayedMontage.
	 */
	float PlayAnimAsset(const TSoftObjectPtr<UAnimSequenceBase>& Asset, float PlayRate = 1.f, FName Section = NAME_None,
		int32 LoopCount = 1, float BlendIn = 0.12f, float BlendOut = 0.18f);
	/** Montage started by the most recent PlayAnimAsset call (the dynamic montage for plain sequences). */
	UAnimMontage* GetLastPlayedMontage() const;
	/**
	 * Data names clips by key: /Game/Characters/<C>/Animations/A_<C>_<Key>. When this character's AnimSet has the same
	 * <Key>, its own clip is returned instead (Orsted casting an element spell plays A_Orsted_CastTwoHand, not Rudeus's).
	 * Anything else (enemy montages, keys the lineage does not have) is returned unchanged.
	 */
	TSoftObjectPtr<UAnimSequenceBase> ResolveLineageAnim(const TSoftObjectPtr<UAnimSequenceBase>& Asset) const;
	/** Stops a montage started through PlayAnimAsset (nullptr = the most recent one). */
	void StopPlayedAnim(UAnimMontage* Montage = nullptr, float BlendOut = 0.2f);

	/** Legacy entry point, forwards to PlayAnimAsset. Returns duration (0 if unavailable). */
	float PlaySoftMontage(const TSoftObjectPtr<UAnimMontage>& Montage, float PlayRate = 1.f, FName Section = NAME_None);

	/** Gameplay state tags (State.Casting, State.Awakened...). */
	FGameplayTagContainer& GetStateTags() { return StateTags; }
	const FGameplayTagContainer& GetStateTags() const { return StateTags; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Character") bool HasStateTag(FGameplayTag Tag) const { return StateTags.HasTag(Tag); }

	UFUNCTION(BlueprintPure, Category = "Mushoku|Character") FName GetCharacterId() const { return CharacterId; }
	const FMTCharacterData* GetCharacterData() const;
	EMTRace GetRace() const { return Race; }
	EMTElement GetElementInSlot(int32 Index) const { return ElementSlots.IsValidIndex(Index) ? ElementSlots[Index] : EMTElement::None; }
	EMTStance GetStance() const;
	bool HasElement(EMTElement Element) const { return ElementSlots.Contains(Element); }

	UMTAttributeComponent* GetAttributes() const { return Attributes; }
	UMTAbilityComponent* GetAbilities() const { return Abilities; }
	UMotionWarpingComponent* GetMotionWarping() const { return MotionWarping; }

	/** Movement state requested by input (walk/run/sprint). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Movement") void SetSprinting(bool bInSprint);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Movement") void SetWalking(bool bInWalk);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Movement") bool IsSprinting() const { return bSprinting; }

	/** Extra movement multiplier while an ability runs (Elemental Barrage reposition). */
	void SetAbilityMoveMultiplier(float Multiplier) { AbilityMoveMultiplier = Multiplier; }

	/** Id used by quests/kill events (enemy row id or character id). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Character") FName GameplayId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Character") FGameplayTagContainer IdentityTags;

	UPROPERTY(BlueprintAssignable) FMTOnLineageChanged OnLineageChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION() virtual void HandleDeath(AActor* Killer);
	UFUNCTION() virtual void HandleDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result);

	/** Recomputes CharacterMovement speeds from lineage + status modifiers. */
	virtual void UpdateMovementFromModifiers();
	virtual void PlayHitReaction(EMTHitReaction Reaction, const FVector& FromDirection);
	/**
	 * Fills the reaction animations below from the lineage's AnimSet. Only touches values that are unset or that were
	 * auto-filled by a previous lineage; anything a designer set (Blueprint defaults or at runtime) is kept.
	 */
	void ApplyAnimSetDefaults(const FMTAnimSetData* AnimSet);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku") TObjectPtr<UMTAttributeComponent> Attributes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku") TObjectPtr<UMTAbilityComponent> Abilities;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku") TObjectPtr<UMotionWarpingComponent> MotionWarping;

	/**
	 * Directional hit reactions (front/back/left/right), stagger/knockdown, dodges and death. Any UAnimSequenceBase:
	 * a plain AnimSequence (played as a dynamic montage) or an authored montage. Unset values are filled from the
	 * lineage's AnimSet (keys HitFront, HitBack, HitLeft, HitRight, Stagger, Knockdown, DodgeForward, DodgeBack,
	 * DodgeLeft, DodgeRight, Death) in ApplyCharacterLineage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> HitReactFront;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> HitReactBack;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> HitReactLeft;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> HitReactRight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> StaggerMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> KnockdownMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> DodgeForwardMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> DodgeBackMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> DodgeLeftMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> DodgeRightMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Animation") TSoftObjectPtr<UAnimSequenceBase> DeathMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Movement") float DodgeStaminaCost = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Movement") float DodgeDuration = 0.38f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Movement") float DodgeInvulnerability = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Movement") float SprintStaminaPerSecond = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Team") FGenericTeamId TeamId = FGenericTeamId(1);

	FName CharacterId;
	EMTRace Race = EMTRace::Human;
	TArray<EMTElement> ElementSlots;
	FGameplayTagContainer StateTags;
	/** Tags mirrored from status effects last tick (removed/re-added each tick). */
	FGameplayTagContainer SyncedStatusTags;
	TWeakObjectPtr<AActor> LockTarget;

	bool bSprinting = false;
	bool bWalking = false;
	bool bDodging = false;
	float DodgeStartTime = -100.f;
	FVector DodgeVelocity = FVector::ZeroVector;
	float StaggerUntil = -1.f;
	float AbilityMoveMultiplier = 1.f;

	// Cached lineage locomotion values.
	float BaseWalkSpeed = 200.f;
	float BaseRunSpeed = 450.f;
	float BaseSprintSpeed = 700.f;
	float BaseAcceleration = 2048.f;
	float BaseJumpZ = 520.f;
	float BaseDodgeDistance = 450.f;
	float RaceHealthMultiplier = 1.f;
	float RaceManaMultiplier = 1.f;
	float RaceStaminaMultiplier = 1.f;
	FMTStatModifier LineagePassiveStats;
	FMTStatModifier RacePassiveStats;
	FMTMovementModifier RacePassiveMovement;

	/** AnimSet key -> path this character auto-filled (so a lineage switch replaces only those, never designer values). */
	TMap<FName, FSoftObjectPath> AutoFilledAnims;
	/** Paths that failed to load once; not retried (avoids a load attempt + warning on every hit). */
	TSet<FSoftObjectPath> FailedAnimPaths;
	TWeakObjectPtr<UAnimMontage> LastPlayedMontage;
};

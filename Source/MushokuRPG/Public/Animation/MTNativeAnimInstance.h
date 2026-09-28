// Graph-free anim instance: animates a character from its AnimSet (Content/Data/AnimSets.json) with no Animation
// Blueprint and no montage assets. A custom FAnimInstanceProxy replaces the anim graph, the same pattern
// UAnimSingleNodeInstance and ULiveLinkInstance use:
//   PreUpdate (game thread) - snapshot the UMTAnimInstance variables; resolve + load the lineage's clips on change
//   Update    (worker)      - advance clocks, crossfade weights, montage slot weights
//   Evaluate  (worker)      - sample + blend the clips, then the DefaultSlot montages with upper-body layering
// UMTAnimInstance::NativeThreadSafeUpdateAnimation still runs, so an Animation Blueprint parented to UMTAnimInstance
// can replace this class later with no code change (set AnimClass in Characters.json).
#pragma once

#include "CoreMinimal.h"
#include "Animation/MTAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "BoneIndices.h"
#include "MTNativeAnimInstance.generated.h"

class UAnimSequenceBase;
class UMTNativeAnimInstance;

/** Clips the native instance samples itself. Every other AnimSet key plays through the montage slot. */
enum class EMTNativeClip : uint8
{
	Idle,
	CombatIdle,
	Walk,
	WalkBack,
	StrafeLeft,
	StrafeRight,
	Run,
	Sprint,
	RunStrafeLeft,
	RunStrafeRight,
	RunBack,
	JumpStart,
	Rise,
	Fall,
	Land,
	HardLand,
	Death,
	Num
};

/** Base-pose layers (under the montage slot); state changes crossfade between them. */
enum class EMTNativeLayer : uint8
{
	Ground,		// idle / combat idle / walk / run / sprint / 4-way directional (walk and run) when locked on
	JumpStart,	// one-shot when leaving the ground upward
	Rise,		// loop while going up
	Fall,		// loop while going down
	Land,		// Land or HardLand one-shot, crossfades back to Ground
	Dead,		// holds the Death clip once the death montage has blended out
	Num
};

/** One clip slot after fallback resolution. The raw pointer is kept alive by UMTNativeAnimInstance::ClipRefs. */
struct FMTNativeClipSlot
{
	const UAnimSequenceBase* Clip = nullptr;
	float Length = 0.f;
	/** Authored ground speed (cm/s) of the clip actually used; 0 for non-locomotion clips. */
	float RefSpeed = 0.f;
	/** Sample backwards (WalkBack borrowing Walk). */
	bool bReverse = false;
	/** Hold frame 0 (an idle borrowing a locomotion clip). */
	bool bFreeze = false;
	/** Borrowed from another key. */
	bool bFallback = false;
};

/** One weighted clip sample for Evaluate (built by Update). */
struct FMTNativeSample
{
	const UAnimSequenceBase* Clip = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
	bool bLoop = false;
};

/** Game-thread copy of the UMTAnimInstance variables the worker thread needs. */
struct FMTNativeAnimVars
{
	float GroundSpeed = 0.f;
	float GaitValue = 0.f;
	float Direction = 0.f;
	float VerticalSpeed = 0.f;
	float UpperBodyCastWeight = 0.f;
	bool bIsStrafing = false;
	bool bIsInAir = false;
	bool bHardLanding = false;
	bool bIsCasting = false;
	bool bInCombatStance = false;
	bool bIsDodging = false;
	bool bIsStaggered = false;
	bool bIsDead = false;
};

/** Proxy that stands in for the anim graph. Plain C++ (not a USTRUCT): nothing in it needs reflection. */
struct FMTNativeAnimInstanceProxy : public FAnimInstanceProxy
{
public:
	explicit FMTNativeAnimInstanceProxy(UAnimInstance* InAnimInstance);

protected:
	// FAnimInstanceProxy interface
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void ResetRuntimeState();
	void EnsureSlotRegistered();
	/** Runs the per-update work exactly once, from whichever update hook the engine calls first. */
	void AdvanceOnce(float DeltaSeconds);
	void UpdateBaseLayers(float DeltaSeconds);
	void EnterBaseLayer(EMTNativeLayer Layer, float BlendTime);
	void UpdateLocomotion(float DeltaSeconds);
	void UpdateUpperBodyLayer(float DeltaSeconds);
	void BuildSamples();
	void AddSample(EMTNativeClip ClipId, float Time, bool bLoop, float Weight);
	bool HasClip(EMTNativeClip ClipId) const { return ClipSlots[(int32)ClipId].Clip != nullptr; }
	/** Per-compact-bone mask of the upper-body subtree; rebuilt whenever the required bones change. */
	bool RefreshUpperBodyMask(const FBoneContainer& Bones);

	// --- written on the game thread (Initialize / PreUpdate) ---
	FMTNativeAnimVars AnimVars;
	FMTNativeClipSlot ClipSlots[(int32)EMTNativeClip::Num];
	FName ResolvedCharacterId;
	bool bClipsResolved = false;
	FName MontageSlot;
	FName UpperBodyBone;
	float MinPlayRate = 0.6f;
	float MaxPlayRate = 1.5f;
	bool bLoggedMaskBone = false;
	bool bAdvancedThisUpdate = false;

	// --- worker thread: Update ---
	FGraphTraversalCounter SlotRegistrationCounter;
	float SlotNodeWeight = 0.f;
	float SlotSourceWeight = 1.f;
	float SlotTotalNodeWeight = 0.f;
	EMTNativeLayer ActiveBaseLayer = EMTNativeLayer::Ground;
	float BaseLayerBlendTime = 0.2f;
	float BaseLayerWeights[(int32)EMTNativeLayer::Num] = {};
	float BaseLayerTimes[(int32)EMTNativeLayer::Num] = {};
	EMTNativeClip LandClip = EMTNativeClip::Land;
	float AirTime = 0.f;
	float LocoWeights[(int32)EMTNativeClip::Num] = {};
	float SmoothedGait = 0.f;
	float LocoPhase = 0.f;
	float LocoPlayRate = 1.f;
	float IdleTime = 0.f;
	float CombatIdleTime = 0.f;
	float CombatBlend = 0.f;
	float StrafeBlend = 0.f;
	/** Forward (Walk), Right, Back (WalkBack), Left, smoothed. */
	float DirWeights[4] = { 1.f, 0.f, 0.f, 0.f };
	bool bCastLayerLatched = false;
	float CastLayerAlpha = 0.f;
	float MoveLayerAlpha = 0.f;
	float UpperBodyAlpha = 0.f;
	TArray<FMTNativeSample, TInlineAllocator<16>> SampleList;

	// --- worker thread: Evaluate ---
	TArray<uint8> UpperBodyMask;
	TArray<FBoneIndexType> MaskBoneIndices;
	const UObject* MaskAsset = nullptr;
	FName MaskBoneName;
	bool bMaskBuilt = false;
	bool bMaskValid = false;
	/** Set on the worker thread, logged once from PreUpdate. */
	bool bMaskBoneMissing = false;
};

/**
 * Default anim instance for every lineage (used when Characters.json AnimClass is unset or fails to load).
 * Not Blueprintable on purpose: an Animation Blueprint should be parented to UMTAnimInstance instead, because
 * this class's proxy bypasses the anim graph.
 */
UCLASS(Transient, NotBlueprintable)
class MUSHOKURPG_API UMTNativeAnimInstance : public UMTAnimInstance
{
	GENERATED_BODY()

	friend struct FMTNativeAnimInstanceProxy;

public:
	/** Montage slot the proxy evaluates. AMTCharacterBase::PlayAnimAsset plays sequences into DefaultSlot. */
	UPROPERTY(EditDefaultsOnly, Category = "Native Animation") FName MontageSlotName = TEXT("DefaultSlot");
	/** Used when the character's AnimSet leaves UpperBodyRootBone empty. */
	UPROPERTY(EditDefaultsOnly, Category = "Native Animation") FName DefaultUpperBodyRootBone = TEXT("spine_C0_1_jnt_061");
	/** Locomotion play-rate clamp (ground speed / authored speed): keeps feet planted without hurried steps. */
	UPROPERTY(EditDefaultsOnly, Category = "Native Animation") float MinLocomotionPlayRate = 0.6f;
	UPROPERTY(EditDefaultsOnly, Category = "Native Animation") float MaxLocomotionPlayRate = 1.5f;

	/** AnimSet key of a native clip ("Idle", "Walk", ...). */
	static FName GetClipKey(EMTNativeClip Clip);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	/**
	 * Game thread. Loads the clips of CharacterId's AnimSet (synchronously, once per lineage), applies fallbacks and
	 * fills OutSlots (EMTNativeClip::Num entries). Logs what is missing once per resolve.
	 */
	void ResolveClips(FName CharacterId, FMTNativeClipSlot* OutSlots, FName& OutUpperBodyBone);

	/** Strong references to every clip the proxy samples through raw pointers (keeps them from being GC'd). */
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequenceBase>> ClipRefs;
};

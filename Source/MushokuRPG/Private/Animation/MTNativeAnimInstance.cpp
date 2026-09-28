#include "Animation/MTNativeAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "BoneContainer.h"
#include "Character/MTCharacterBase.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTTypes.h"

namespace MTNativeAnim
{
	static const FName DefaultSlotName(TEXT("DefaultSlot"));
	constexpr int32 NumClips = (int32)EMTNativeClip::Num;
	constexpr int32 NumLayers = (int32)EMTNativeLayer::Num;
	/** Weights below this are dropped. */
	constexpr float MinWeight = 0.001f;
	/** Same threshold FAnimNode_Slot uses (ZERO_ANIMWEIGHT_THRESH). */
	constexpr float SlotWeightThreshold = 0.00001f;
	/** Below this ground speed (cm/s) the legs are considered planted. */
	constexpr float MovingSpeed = 20.f;
	/** Leaving the ground faster than this (cm/s, upward) plays JumpStart. */
	constexpr float JumpStartMinSpeed = 50.f;
	/** Walking off a ledge only switches to Fall after this long in the air (steps and slopes stay grounded). */
	constexpr float MinAirTimeForFall = 0.12f;
	/** Clocks are clamped so hours of idling never lose float precision. */
	constexpr float MaxLayerTime = 10000.f;

	float MoveTowards(float Current, float Target, float MaxDelta)
	{
		return Current < Target ? FMath::Min(Current + MaxDelta, Target) : FMath::Max(Current - MaxDelta, Target);
	}

	/** Ease a 0..1 linear ramp so crossfades start and end softly. */
	float Ease(float Linear)
	{
		return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(Linear, 0.f, 1.f));
	}

	float WrapTime(float Time, float Length)
	{
		if (Length <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		float Wrapped = FMath::Fmod(Time, Length);
		if (Wrapped < 0.f)
		{
			Wrapped += Length;
		}
		return Wrapped;
	}

	/** 4-way directional weights (Forward, Right, Back, Left) for a movement direction in degrees (+ = right). */
	void DirectionalWeights(float DirectionDeg, float Out[4])
	{
		const float Direction = FMath::UnwindDegrees(DirectionDeg);
		const float Abs = FMath::Abs(Direction);
		const int32 Side = Direction >= 0.f ? 1 : 3;
		Out[0] = Out[1] = Out[2] = Out[3] = 0.f;
		if (Abs <= 90.f)
		{
			const float T = Abs / 90.f;
			Out[0] = 1.f - T;
			Out[Side] = T;
		}
		else
		{
			const float T = (Abs - 90.f) / 90.f;
			Out[Side] = 1.f - T;
			Out[2] = T;
		}
	}

	/** Accum = lerp(Accum, Source, Alpha) per bone (translation/scale linear, rotation shortest-path nlerp) and curves. */
	void BlendPoseInto(FPoseContext& Accum, const FPoseContext& Source, float Alpha)
	{
		for (const FCompactPoseBoneIndex BoneIndex : Accum.Pose.ForEachBoneIndex())
		{
			Accum.Pose[BoneIndex].BlendWith(Source.Pose[BoneIndex], Alpha);
		}
		Accum.Curve.LerpTo(Source.Curve, Alpha);
	}
}

// =====================================================================================================================
// UMTNativeAnimInstance
// =====================================================================================================================

FAnimInstanceProxy* UMTNativeAnimInstance::CreateAnimInstanceProxy()
{
	return new FMTNativeAnimInstanceProxy(this);
}

void UMTNativeAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	Super::DestroyAnimInstanceProxy(InProxy);
}

FName UMTNativeAnimInstance::GetClipKey(EMTNativeClip Clip)
{
	static const FName Keys[] =
	{
		TEXT("Idle"), TEXT("CombatIdle"), TEXT("Walk"), TEXT("WalkBack"), TEXT("StrafeLeft"), TEXT("StrafeRight"),
		TEXT("Run"), TEXT("Sprint"), TEXT("JumpStart"), TEXT("Rise"), TEXT("Fall"), TEXT("Land"), TEXT("HardLand"),
		TEXT("Death")
	};
	static_assert(UE_ARRAY_COUNT(Keys) == (int32)EMTNativeClip::Num, "Keep the key table in sync with EMTNativeClip");
	const int32 Index = (int32)Clip;
	return (Index >= 0 && Index < (int32)EMTNativeClip::Num) ? Keys[Index] : NAME_None;
}

void UMTNativeAnimInstance::ResolveClips(FName CharacterId, FMTNativeClipSlot* OutSlots, FName& OutUpperBodyBone)
{
	using namespace MTNativeAnim;
	using E = EMTNativeClip;

	ClipRefs.Reset();
	for (int32 Index = 0; Index < NumClips; ++Index)
	{
		OutSlots[Index] = FMTNativeClipSlot();
	}
	OutUpperBodyBone = DefaultUpperBodyRootBone;

	const FString OwnerName = GetNameSafe(GetOwningActor());
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTAnimSetData* AnimSet = (Registry && !CharacterId.IsNone()) ? Registry->FindAnimSet(CharacterId) : nullptr;
	if (!AnimSet)
	{
		if (!CharacterId.IsNone())
		{
			UE_LOG(LogMushoku, Warning, TEXT("%s: no AnimSet for character '%s' (Content/Data/AnimSets.json) - reference pose until one exists."),
				*OwnerName, *CharacterId.ToString());
		}
		return;
	}
	if (!AnimSet->UpperBodyRootBone.IsNone())
	{
		OutUpperBodyBone = AnimSet->UpperBodyRootBone;
	}

	// Load every key the proxy samples (game thread, synchronous, once per lineage).
	UAnimSequenceBase* Loaded[NumClips] = {};
	TArray<FString> Missing;
	for (int32 Index = 0; Index < NumClips; ++Index)
	{
		const FName Key = GetClipKey((E)Index);
		const TSoftObjectPtr<UAnimSequenceBase>* Soft = AnimSet->Anims.Find(Key);
		UAnimSequenceBase* Asset = (Soft && !Soft->IsNull()) ? Soft->LoadSynchronous() : nullptr;
		UAnimSequence* Sequence = Cast<UAnimSequence>(Asset);
		if (Asset && !Sequence)
		{
			UE_LOG(LogMushoku, Warning, TEXT("%s: AnimSet '%s' key %s (%s) is not a plain AnimSequence - locomotion keys cannot be montages."),
				*OwnerName, *CharacterId.ToString(), *Key.ToString(), *Asset->GetPathName());
		}
		else if (Sequence && Sequence->IsValidAdditive())
		{
			UE_LOG(LogMushoku, Warning, TEXT("%s: AnimSet '%s' key %s is an additive animation - ignored."), *OwnerName, *CharacterId.ToString(), *Key.ToString());
			Sequence = nullptr;
		}
		else if (Sequence && Sequence->GetPlayLength() <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogMushoku, Warning, TEXT("%s: AnimSet '%s' key %s has zero length - ignored."), *OwnerName, *CharacterId.ToString(), *Key.ToString());
			Sequence = nullptr;
		}
		if (Sequence)
		{
			Loaded[Index] = Sequence;
			ClipRefs.Add(Sequence);
		}
		else
		{
			Missing.Add(Key.ToString());
		}
	}

	auto RefSpeedOf = [AnimSet](E Clip) -> float
	{
		switch (Clip)
		{
		case E::Walk: return AnimSet->WalkSpeedRef;
		case E::WalkBack: return AnimSet->WalkBackSpeedRef;
		case E::StrafeLeft:
		case E::StrafeRight: return AnimSet->StrafeSpeedRef;
		case E::Run: return AnimSet->RunSpeedRef;
		case E::Sprint: return AnimSet->SprintSpeedRef;
		default: return 0.f;
		}
	};

	struct FCandidate
	{
		E Source;
		bool bReverse;
		bool bFreeze;
	};
	TArray<FString> Borrowed;
	auto Resolve = [&](E Slot, std::initializer_list<FCandidate> Chain)
	{
		for (const FCandidate& Candidate : Chain)
		{
			UAnimSequenceBase* Clip = Loaded[(int32)Candidate.Source];
			if (!Clip)
			{
				continue;
			}
			FMTNativeClipSlot& Out = OutSlots[(int32)Slot];
			Out.Clip = Clip;
			Out.Length = Clip->GetPlayLength();
			Out.RefSpeed = RefSpeedOf(Candidate.Source);
			Out.bReverse = Candidate.bReverse;
			Out.bFreeze = Candidate.bFreeze;
			Out.bFallback = Candidate.Source != Slot;
			if (Out.bFallback)
			{
				Borrowed.Add(FString::Printf(TEXT("%s->%s%s"), *GetClipKey(Slot).ToString(), *GetClipKey(Candidate.Source).ToString(),
					Candidate.bReverse ? TEXT(" (reversed)") : (Candidate.bFreeze ? TEXT(" (frozen)") : TEXT(""))));
			}
			return;
		}
	};

	// Fallback chains: never T-pose while any usable clip exists.
	Resolve(E::Idle, { { E::Idle, false, false }, { E::CombatIdle, false, false }, { E::Walk, false, true }, { E::Run, false, true }, { E::Sprint, false, true } });
	Resolve(E::CombatIdle, { { E::CombatIdle, false, false }, { E::Idle, false, false }, { E::Walk, false, true }, { E::Run, false, true } });
	Resolve(E::Walk, { { E::Walk, false, false }, { E::Run, false, false }, { E::Sprint, false, false } });
	Resolve(E::Run, { { E::Run, false, false }, { E::Walk, false, false }, { E::Sprint, false, false } });
	Resolve(E::Sprint, { { E::Sprint, false, false }, { E::Run, false, false }, { E::Walk, false, false } });
	Resolve(E::WalkBack, { { E::WalkBack, false, false }, { E::Walk, true, false }, { E::Run, true, false } });
	Resolve(E::StrafeLeft, { { E::StrafeLeft, false, false }, { E::Walk, false, false }, { E::Run, false, false } });
	Resolve(E::StrafeRight, { { E::StrafeRight, false, false }, { E::Walk, false, false }, { E::Run, false, false } });
	Resolve(E::JumpStart, { { E::JumpStart, false, false } });
	Resolve(E::Rise, { { E::Rise, false, false }, { E::Fall, false, false } });
	Resolve(E::Fall, { { E::Fall, false, false }, { E::Rise, false, false } });
	Resolve(E::Land, { { E::Land, false, false }, { E::HardLand, false, false } });
	Resolve(E::HardLand, { { E::HardLand, false, false }, { E::Land, false, false } });
	Resolve(E::Death, { { E::Death, false, false } });

	if (Missing.Num() == 0)
	{
		UE_LOG(LogMushoku, Log, TEXT("%s: native animation ready for '%s' (%d clips, upper-body bone %s)."),
			*OwnerName, *CharacterId.ToString(), ClipRefs.Num(), *OutUpperBodyBone.ToString());
	}
	else if (ClipRefs.Num() == 0)
	{
		UE_LOG(LogMushoku, Warning, TEXT("%s: none of the '%s' AnimSet clips could be loaded (import Rudeus_Animated.glb with mt_setup_rudeus.py) - reference pose."),
			*OwnerName, *CharacterId.ToString());
	}
	else
	{
		UE_LOG(LogMushoku, Warning, TEXT("%s: AnimSet '%s' is missing [%s]; fallbacks [%s]."),
			*OwnerName, *CharacterId.ToString(), *FString::Join(Missing, TEXT(", ")), *FString::Join(Borrowed, TEXT(", ")));
	}
}

// =====================================================================================================================
// FMTNativeAnimInstanceProxy
// =====================================================================================================================

FMTNativeAnimInstanceProxy::FMTNativeAnimInstanceProxy(UAnimInstance* InAnimInstance)
	: FAnimInstanceProxy(InAnimInstance)
{
}

void FMTNativeAnimInstanceProxy::ResetRuntimeState()
{
	using namespace MTNativeAnim;
	for (int32 Index = 0; Index < NumLayers; ++Index)
	{
		BaseLayerWeights[Index] = 0.f;
		BaseLayerTimes[Index] = 0.f;
	}
	for (int32 Index = 0; Index < NumClips; ++Index)
	{
		LocoWeights[Index] = 0.f;
	}
	ActiveBaseLayer = EMTNativeLayer::Ground;
	BaseLayerWeights[(int32)EMTNativeLayer::Ground] = 1.f;
	BaseLayerBlendTime = 0.2f;
	LandClip = EMTNativeClip::Land;
	AirTime = 0.f;
	SmoothedGait = 0.f;
	LocoPhase = 0.f;
	LocoPlayRate = 1.f;
	IdleTime = 0.f;
	CombatIdleTime = 0.f;
	CombatBlend = 0.f;
	StrafeBlend = 0.f;
	DirWeights[0] = 1.f;
	DirWeights[1] = DirWeights[2] = DirWeights[3] = 0.f;
	bCastLayerLatched = false;
	CastLayerAlpha = 0.f;
	MoveLayerAlpha = 0.f;
	UpperBodyAlpha = 0.f;
	SlotNodeWeight = 0.f;
	SlotSourceWeight = 1.f;
	SlotTotalNodeWeight = 0.f;
	SampleList.Reset();
	bMaskBuilt = false;
	bMaskValid = false;
	bAdvancedThisUpdate = false;
}

void FMTNativeAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	MontageSlot = MTNativeAnim::DefaultSlotName;
	if (const UMTNativeAnimInstance* Instance = Cast<UMTNativeAnimInstance>(InAnimInstance))
	{
		if (!Instance->MontageSlotName.IsNone())
		{
			MontageSlot = Instance->MontageSlotName;
		}
	}
	ResetRuntimeState();
	bClipsResolved = false; // mesh / class (re)initialisation: resolve the clips again on the next PreUpdate
	EnsureSlotRegistered();
}

void FMTNativeAnimInstanceProxy::EnsureSlotRegistered()
{
	// Same handshake FAnimNode_Slot::Initialize_AnyThread uses: register again whenever the proxy has reset its slot
	// trackers, so montage weights and montage notifies ("is this slot relevant?") work without a graph.
	if (!SlotRegistrationCounter.IsSynchronized_Counter(GetSlotNodeInitializationCounter()))
	{
		SlotRegistrationCounter.SynchronizeWith(GetSlotNodeInitializationCounter());
		RegisterSlotNodeWithAnimInstance(MontageSlot);
	}
}

void FMTNativeAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	bAdvancedThisUpdate = false;

	UMTNativeAnimInstance* Instance = Cast<UMTNativeAnimInstance>(InAnimInstance);
	if (!Instance)
	{
		return;
	}

	// Game thread: copy what UMTAnimInstance computed in its (thread-safe) update. The worker thread only reads
	// this copy, so the pose trails the gameplay state by at most one frame.
	const UMTAnimInstance* Source = Instance;
	AnimVars.GroundSpeed = Source->GroundSpeed;
	AnimVars.GaitValue = Source->GaitValue;
	AnimVars.Direction = Source->Direction;
	AnimVars.VerticalSpeed = Source->VerticalSpeed;
	AnimVars.UpperBodyCastWeight = Source->UpperBodyCastWeight;
	AnimVars.bIsStrafing = Source->bIsStrafing;
	AnimVars.bIsInAir = Source->bIsInAir;
	AnimVars.bHardLanding = Source->bHardLanding;
	AnimVars.bIsCasting = Source->bIsCasting;
	AnimVars.bInCombatStance = Source->bInCombatStance;
	AnimVars.bIsDodging = Source->bIsDodging;
	AnimVars.bIsStaggered = Source->bIsStaggered;
	AnimVars.bIsDead = Source->bIsDead;

	MinPlayRate = FMath::Max(0.05f, Instance->MinLocomotionPlayRate);
	MaxPlayRate = FMath::Max(MinPlayRate, Instance->MaxLocomotionPlayRate);

	// Resolve (and synchronously load) the lineage's clips once per lineage change.
	const AMTCharacterBase* Character = Source->Character.Get();
	const FName LineageId = Character ? Character->GetCharacterId() : NAME_None;
	if (!bClipsResolved || LineageId != ResolvedCharacterId)
	{
		Instance->ResolveClips(LineageId, ClipSlots, UpperBodyBone);
		ResolvedCharacterId = LineageId;
		bClipsResolved = true;
		SampleList.Reset(); // never evaluate pointers from the previous lineage
		bMaskBuilt = false;
		bMaskBoneMissing = false;
		bLoggedMaskBone = false;
	}

	if (bMaskBoneMissing && !bLoggedMaskBone)
	{
		bLoggedMaskBone = true;
		UE_LOG(LogMushoku, Warning, TEXT("%s: upper-body bone '%s' is not in the skeleton - casting on the move uses the full-body montage pose."),
			*GetNameSafe(Instance->GetOwningActor()), *UpperBodyBone.ToString());
	}
}

void FMTNativeAnimInstanceProxy::Update(float DeltaSeconds)
{
	AdvanceOnce(DeltaSeconds);
}

void FMTNativeAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	FAnimInstanceProxy::UpdateAnimationNode(InContext);
	// Update() is the intended hook. This is a safety net in case an engine version only drives
	// UpdateAnimationNode for graph-less instances; AdvanceOnce guarantees a single advance per update.
	AdvanceOnce(InContext.GetDeltaTime());
}

void FMTNativeAnimInstanceProxy::AdvanceOnce(float DeltaSeconds)
{
	if (bAdvancedThisUpdate)
	{
		return;
	}
	bAdvancedThisUpdate = true;

	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.25f);

	// Montage slot: mirror FAnimNode_Slot::Update_AnyThread (weights, and the relevancy montage notifies check).
	EnsureSlotRegistered();
	GetSlotWeight(MontageSlot, SlotNodeWeight, SlotSourceWeight, SlotTotalNodeWeight);
	UpdateSlotNodeWeight(MontageSlot, SlotNodeWeight, 1.f);

	UpdateBaseLayers(Dt);
	UpdateLocomotion(Dt);
	UpdateUpperBodyLayer(Dt);
	BuildSamples();
}

void FMTNativeAnimInstanceProxy::EnterBaseLayer(EMTNativeLayer Layer, float BlendTime)
{
	ActiveBaseLayer = Layer;
	BaseLayerBlendTime = FMath::Max(0.01f, BlendTime);
	BaseLayerTimes[(int32)Layer] = 0.f;
}

void FMTNativeAnimInstanceProxy::UpdateBaseLayers(float Dt)
{
	using namespace MTNativeAnim;
	using L = EMTNativeLayer;
	const FMTNativeAnimVars& V = AnimVars;

	for (float& Time : BaseLayerTimes)
	{
		Time = FMath::Min(Time + Dt, MaxLayerTime);
	}
	AirTime = V.bIsInAir ? AirTime + Dt : 0.f;

	const bool bHasAirLoops = HasClip(EMTNativeClip::Rise) || HasClip(EMTNativeClip::Fall);
	const bool bMoving = V.GroundSpeed > MovingSpeed;
	const L AirLoop = V.VerticalSpeed > 0.f ? L::Rise : L::Fall;
	L Next = ActiveBaseLayer;
	float Blend = 0.2f;

	// Touchdown: Land (or HardLand) one-shot when authored, otherwise straight back to locomotion.
	auto Landing = [this, &V](float& OutBlend) -> L
	{
		const EMTNativeClip Wanted = V.bHardLanding ? EMTNativeClip::HardLand : EMTNativeClip::Land;
		if (HasClip(Wanted))
		{
			LandClip = Wanted;
			OutBlend = 0.12f;
			return L::Land;
		}
		OutBlend = 0.2f;
		return L::Ground;
	};

	if (V.bIsDead && HasClip(EMTNativeClip::Death))
	{
		Next = L::Dead;
		Blend = 0.2f;
	}
	else
	{
		switch (ActiveBaseLayer)
		{
		case L::Dead:
			Next = L::Ground; // revived / respawned
			Blend = 0.25f;
			break;
		case L::Ground:
			if (V.bIsInAir && bHasAirLoops)
			{
				if (V.VerticalSpeed > JumpStartMinSpeed && HasClip(EMTNativeClip::JumpStart))
				{
					Next = L::JumpStart;
					Blend = 0.12f;
				}
				else if (V.VerticalSpeed > JumpStartMinSpeed || AirTime >= MinAirTimeForFall)
				{
					Next = AirLoop;
					Blend = 0.25f;
				}
			}
			break;
		case L::JumpStart:
			if (!V.bIsInAir)
			{
				Next = Landing(Blend);
			}
			else if (BaseLayerTimes[(int32)L::JumpStart] >= FMath::Max(0.f, ClipSlots[(int32)EMTNativeClip::JumpStart].Length - 0.1f)
				|| V.VerticalSpeed < -150.f)
			{
				Next = AirLoop;
				Blend = 0.2f;
			}
			break;
		case L::Rise:
			if (!V.bIsInAir)
			{
				Next = Landing(Blend);
			}
			else if (V.VerticalSpeed <= 0.f)
			{
				Next = L::Fall;
				Blend = 0.25f;
			}
			break;
		case L::Fall:
			if (!V.bIsInAir)
			{
				Next = Landing(Blend);
			}
			else if (V.VerticalSpeed > 150.f)
			{
				Next = L::Rise;
				Blend = 0.2f;
			}
			break;
		case L::Land:
			if (V.bIsInAir && bHasAirLoops && AirTime >= MinAirTimeForFall)
			{
				Next = AirLoop;
				Blend = 0.2f;
			}
			else
			{
				// Let the landing finish when standing; hand the legs back quickly when the player keeps moving.
				const float Length = ClipSlots[(int32)LandClip].Length;
				const float Time = BaseLayerTimes[(int32)L::Land];
				if (Time >= FMath::Max(0.f, Length - 0.2f) || (bMoving && Time >= 0.15f))
				{
					Next = L::Ground;
					Blend = bMoving ? 0.2f : 0.25f;
				}
			}
			break;
		default:
			break;
		}
	}
	if (Next != ActiveBaseLayer)
	{
		EnterBaseLayer(Next, Blend);
	}

	// Crossfade: the active layer ramps up linearly over its blend time, the others share the rest in proportion,
	// so an interrupted blend continues from wherever it was (no pops).
	const int32 Active = (int32)ActiveBaseLayer;
	BaseLayerWeights[Active] = FMath::Min(1.f, BaseLayerWeights[Active] + Dt / BaseLayerBlendTime);
	const float Remaining = 1.f - BaseLayerWeights[Active];
	float Others = 0.f;
	for (int32 Index = 0; Index < NumLayers; ++Index)
	{
		if (Index != Active)
		{
			Others += BaseLayerWeights[Index];
		}
	}
	if (Others <= KINDA_SMALL_NUMBER || Remaining <= MinWeight)
	{
		for (int32 Index = 0; Index < NumLayers; ++Index)
		{
			BaseLayerWeights[Index] = Index == Active ? 1.f : 0.f;
		}
	}
	else
	{
		const float Scale = Remaining / Others;
		for (int32 Index = 0; Index < NumLayers; ++Index)
		{
			if (Index != Active)
			{
				BaseLayerWeights[Index] *= Scale;
			}
		}
	}
}

void FMTNativeAnimInstanceProxy::UpdateLocomotion(float Dt)
{
	using namespace MTNativeAnim;
	using E = EMTNativeClip;
	const FMTNativeAnimVars& V = AnimVars;

	for (float& Weight : LocoWeights)
	{
		Weight = 0.f;
	}

	// Gait 0 idle, 1 walk, 2 run, 3 sprint: adjacent pairs interpolate (lightly smoothed against jitter).
	SmoothedGait = FMath::FInterpTo(SmoothedGait, FMath::Clamp(V.GaitValue, 0.f, 3.f), Dt, 10.f);
	const float Gait = SmoothedGait;
	const float WIdle = FMath::Clamp(1.f - Gait, 0.f, 1.f);
	const float WWalk = Gait <= 1.f ? Gait : FMath::Clamp(2.f - Gait, 0.f, 1.f);
	const float WRun = Gait <= 1.f ? 0.f : (Gait <= 2.f ? Gait - 1.f : FMath::Clamp(3.f - Gait, 0.f, 1.f));
	const float WSprint = FMath::Clamp(Gait - 2.f, 0.f, 1.f);
	const float WMove = 1.f - WIdle;

	// Locked on at walking pace: 4-way directional blend of Walk / StrafeRight / WalkBack / StrafeLeft.
	const bool bDirectional = V.bIsStrafing && Gait <= 1.5f;
	StrafeBlend = MoveTowards(StrafeBlend, bDirectional ? 1.f : 0.f, Dt / 0.2f);
	float TargetDir[4];
	DirectionalWeights(V.Direction, TargetDir);
	float DirSum = 0.f;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		DirWeights[Index] = FMath::FInterpTo(DirWeights[Index], TargetDir[Index], Dt, 12.f);
		DirSum += DirWeights[Index];
	}
	if (DirSum > KINDA_SMALL_NUMBER)
	{
		for (float& Weight : DirWeights)
		{
			Weight /= DirSum;
		}
	}
	const float Strafe = Ease(StrafeBlend);

	LocoWeights[(int32)E::Walk] = WWalk * (1.f - Strafe) + WMove * Strafe * DirWeights[0];
	LocoWeights[(int32)E::StrafeRight] = WMove * Strafe * DirWeights[1];
	LocoWeights[(int32)E::WalkBack] = WMove * Strafe * DirWeights[2];
	LocoWeights[(int32)E::StrafeLeft] = WMove * Strafe * DirWeights[3];
	LocoWeights[(int32)E::Run] = WRun * (1.f - Strafe);
	LocoWeights[(int32)E::Sprint] = WSprint * (1.f - Strafe);

	// Relaxed vs combat idle (0.3 s eased), each on its own looping clock.
	CombatBlend = MoveTowards(CombatBlend, V.bInCombatStance ? 1.f : 0.f, Dt / 0.3f);
	const float Combat = Ease(CombatBlend);
	LocoWeights[(int32)E::Idle] = WIdle * (1.f - Combat);
	LocoWeights[(int32)E::CombatIdle] = WIdle * Combat;
	IdleTime = WrapTime(IdleTime + Dt, ClipSlots[(int32)E::Idle].Length);
	CombatIdleTime = WrapTime(CombatIdleTime + Dt, ClipSlots[(int32)E::CombatIdle].Length);

	// One shared normalised phase for every moving clip (0 = left-foot contact, 0.5 = right-foot contact), so blends
	// never desync the feet. Play rate = ground speed / blended authored speed, clamped.
	float MoveWeight = 0.f;
	float RefSpeedSum = 0.f;
	float LengthSum = 0.f;
	bool bBorrowed = false;
	for (const E Clip : { E::Walk, E::WalkBack, E::StrafeLeft, E::StrafeRight, E::Run, E::Sprint })
	{
		const float Weight = LocoWeights[(int32)Clip];
		const FMTNativeClipSlot& Slot = ClipSlots[(int32)Clip];
		if (Weight <= MinWeight || !Slot.Clip)
		{
			continue;
		}
		MoveWeight += Weight;
		RefSpeedSum += Weight * FMath::Max(1.f, Slot.RefSpeed);
		LengthSum += Weight * Slot.Length;
		bBorrowed |= Slot.bFallback;
	}
	if (MoveWeight > MinWeight)
	{
		const float BlendedRefSpeed = RefSpeedSum / MoveWeight;
		const float BlendedCycle = FMath::Max(0.05f, LengthSum / MoveWeight);
		// A borrowed clip (Run standing in for Walk) needs a wider range to keep its feet planted.
		const float MinRate = bBorrowed ? FMath::Min(MinPlayRate, 0.3f) : MinPlayRate;
		const float MaxRate = bBorrowed ? FMath::Max(MaxPlayRate, 2.f) : MaxPlayRate;
		LocoPlayRate = FMath::Clamp(V.GroundSpeed / BlendedRefSpeed, MinRate, MaxRate);
		LocoPhase = FMath::Frac(LocoPhase + Dt * LocoPlayRate / BlendedCycle);
	}
	else
	{
		LocoPhase = 0.f; // standing still: the next start begins on the left-foot contact
		LocoPlayRate = 1.f;
	}
}

void FMTNativeAnimInstanceProxy::UpdateUpperBodyLayer(float Dt)
{
	using namespace MTNativeAnim;
	const FMTNativeAnimVars& V = AnimVars;

	// Casting keeps the layer on (weighted by UpperBodyCastWeight). After the cast the montage usually still plays its
	// recovery, so the layer stays latched until the slot empties; full-body reactions (dodge, stagger, death) take
	// the legs back immediately.
	const bool bSlotActive = SlotNodeWeight > SlotWeightThreshold;
	if (V.bIsCasting)
	{
		bCastLayerLatched = true;
	}
	else if (!bSlotActive || V.bIsDodging || V.bIsStaggered || V.bIsDead)
	{
		bCastLayerLatched = false;
	}
	const float CastTarget = V.bIsCasting ? V.UpperBodyCastWeight : (bCastLayerLatched ? 1.f : 0.f);
	CastLayerAlpha = FMath::FInterpTo(CastLayerAlpha, CastTarget, Dt, 12.f);

	// Only when the legs are busy (moving or airborne); standing casts use the full-body montage pose.
	const bool bLegsBusy = V.bIsInAir || V.GroundSpeed > MovingSpeed;
	MoveLayerAlpha = FMath::FInterpTo(MoveLayerAlpha, bLegsBusy ? 1.f : 0.f, Dt, 8.f);

	UpperBodyAlpha = FMath::Clamp(CastLayerAlpha * MoveLayerAlpha, 0.f, 1.f);
	if (UpperBodyAlpha < MinWeight)
	{
		UpperBodyAlpha = 0.f;
	}
}

void FMTNativeAnimInstanceProxy::AddSample(EMTNativeClip ClipId, float Time, bool bLoop, float Weight)
{
	using namespace MTNativeAnim;
	const FMTNativeClipSlot& Slot = ClipSlots[(int32)ClipId];
	if (Weight <= MinWeight || !Slot.Clip || Slot.Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	float SampleTime = Slot.bFreeze ? 0.f : (bLoop ? WrapTime(Time, Slot.Length) : FMath::Clamp(Time, 0.f, Slot.Length));
	if (Slot.bReverse)
	{
		SampleTime = Slot.Length - SampleTime;
	}
	// Fallbacks can map two slots to the same clip at the same time: sample it once.
	for (FMTNativeSample& Existing : SampleList)
	{
		if (Existing.Clip == Slot.Clip && FMath::IsNearlyEqual(Existing.Time, SampleTime, 0.001f))
		{
			Existing.Weight += Weight;
			return;
		}
	}
	FMTNativeSample& Sample = SampleList.AddDefaulted_GetRef();
	Sample.Clip = Slot.Clip;
	Sample.Time = SampleTime;
	Sample.Weight = Weight;
	Sample.bLoop = bLoop;
}

void FMTNativeAnimInstanceProxy::BuildSamples()
{
	using namespace MTNativeAnim;
	using E = EMTNativeClip;
	using L = EMTNativeLayer;

	SampleList.Reset();
	const float Ground = BaseLayerWeights[(int32)L::Ground];
	if (Ground > MinWeight)
	{
		AddSample(E::Idle, IdleTime, true, Ground * LocoWeights[(int32)E::Idle]);
		AddSample(E::CombatIdle, CombatIdleTime, true, Ground * LocoWeights[(int32)E::CombatIdle]);
		for (const E Clip : { E::Walk, E::WalkBack, E::StrafeLeft, E::StrafeRight, E::Run, E::Sprint })
		{
			AddSample(Clip, LocoPhase * ClipSlots[(int32)Clip].Length, true, Ground * LocoWeights[(int32)Clip]);
		}
	}
	AddSample(E::JumpStart, BaseLayerTimes[(int32)L::JumpStart], false, BaseLayerWeights[(int32)L::JumpStart]);
	AddSample(E::Rise, BaseLayerTimes[(int32)L::Rise], true, BaseLayerWeights[(int32)L::Rise]);
	AddSample(E::Fall, BaseLayerTimes[(int32)L::Fall], true, BaseLayerWeights[(int32)L::Fall]);
	AddSample(LandClip, BaseLayerTimes[(int32)L::Land], false, BaseLayerWeights[(int32)L::Land]);
	AddSample(E::Death, BaseLayerTimes[(int32)L::Dead], false, BaseLayerWeights[(int32)L::Dead]);
}

bool FMTNativeAnimInstanceProxy::RefreshUpperBodyMask(const FBoneContainer& Bones)
{
	const UObject* Asset = Bones.GetAsset();
	const TArray<FBoneIndexType>& BoneIndices = Bones.GetBoneIndicesArray();
	if (bMaskBuilt && MaskBoneName == UpperBodyBone && MaskAsset == Asset && MaskBoneIndices == BoneIndices)
	{
		return bMaskValid;
	}
	bMaskBuilt = true;
	bMaskValid = false;
	MaskBoneName = UpperBodyBone;
	MaskAsset = Asset;
	MaskBoneIndices = BoneIndices;

	const int32 NumBones = Bones.GetCompactPoseNumBones();
	UpperBodyMask.Reset();
	UpperBodyMask.SetNumZeroed(NumBones);

	const int32 MeshIndex = UpperBodyBone.IsNone() ? INDEX_NONE : Bones.GetPoseBoneIndexForBoneName(UpperBodyBone);
	const FCompactPoseBoneIndex RootIndex = MeshIndex != INDEX_NONE
		? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex))
		: FCompactPoseBoneIndex(INDEX_NONE);
	if (!RootIndex.IsValid())
	{
		bMaskBoneMissing = true;
		return false;
	}
	// Compact indices are ordered parent-first, so one forward pass marks the whole subtree.
	for (int32 Index = RootIndex.GetInt(); Index < NumBones; ++Index)
	{
		const FCompactPoseBoneIndex BoneIndex(Index);
		if (BoneIndex == RootIndex)
		{
			UpperBodyMask[Index] = 1;
			continue;
		}
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(BoneIndex);
		UpperBodyMask[Index] = (Parent.IsValid() && UpperBodyMask[Parent.GetInt()] != 0) ? 1 : 0;
	}
	bMaskValid = true;
	return true;
}

bool FMTNativeAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	using namespace MTNativeAnim;

	// 1) Base pose: weighted blend of the sample list Update() built (running normalised lerp == weighted average).
	float Accumulated = 0.f;
	bool bHasPose = false;
	for (const FMTNativeSample& Sample : SampleList)
	{
		if (!Sample.Clip)
		{
			continue;
		}
		FPoseContext SamplePose(this);
		{
			FAnimationPoseData SamplePoseData(SamplePose);
			Sample.Clip->GetAnimationPose(SamplePoseData,
				FAnimExtractContext(static_cast<double>(Sample.Time), false /*root motion*/, FDeltaTimeRecord(), Sample.bLoop));
		}
		Accumulated += Sample.Weight;
		if (!bHasPose)
		{
			Output.Pose.CopyBonesFrom(SamplePose.Pose);
			Output.Curve.CopyFrom(SamplePose.Curve);
			bHasPose = true;
		}
		else
		{
			BlendPoseInto(Output, SamplePose, Sample.Weight / FMath::Max(Accumulated, KINDA_SMALL_NUMBER));
		}
	}
	if (bHasPose)
	{
		Output.Pose.NormalizeRotations();
	}
	else
	{
		Output.ResetToRefPose(); // no clips at all (not imported yet): bind pose, never garbage
	}

	// 2) Montage slot, exactly like a DefaultSlot node with the base pose as its source.
	if (SlotNodeWeight > SlotWeightThreshold)
	{
		FPoseContext SlotPose(this);
		{
			FAnimationPoseData SourcePoseData(Output);
			FAnimationPoseData SlotPoseData(SlotPose);
			SlotEvaluatePose(MontageSlot, SourcePoseData, SlotSourceWeight, SlotPoseData, SlotNodeWeight, SlotTotalNodeWeight);
		}

		// 3) Upper-body layering: casting while moving/airborne keeps locomotion on the pelvis and legs and applies
		//    the montage to the upper-spine subtree. Standing (or not casting) uses the full-body slot pose.
		const float LayerAlpha = UpperBodyAlpha;
		if (LayerAlpha > MinWeight && RefreshUpperBodyMask(Output.Pose.GetBoneContainer()))
		{
			const float LowerBodySlotAlpha = 1.f - LayerAlpha;
			for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
			{
				const int32 Index = BoneIndex.GetInt();
				if (UpperBodyMask.IsValidIndex(Index) && UpperBodyMask[Index] != 0)
				{
					Output.Pose[BoneIndex] = SlotPose.Pose[BoneIndex];
				}
				else if (LowerBodySlotAlpha > MinWeight)
				{
					Output.Pose[BoneIndex].BlendWith(SlotPose.Pose[BoneIndex], LowerBodySlotAlpha);
				}
			}
			Output.Pose.NormalizeRotations();
		}
		else
		{
			Output.Pose.CopyBonesFrom(SlotPose.Pose);
		}
		Output.Curve.CopyFrom(SlotPose.Curve);
		Output.CustomAttributes.CopyFrom(SlotPose.CustomAttributes);
	}
	return true;
}

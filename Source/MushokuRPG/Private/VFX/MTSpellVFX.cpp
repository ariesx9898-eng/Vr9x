#include "VFX/MTSpellVFX.h"
#include "VFX/MTVFXLibrary.h"
#include "VFX/MTVFXSubsystem.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTPlayerCharacter.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarMTVFXQuality(
	TEXT("mt.VFX.Quality"),
	3,
	TEXT("Spell effect particle density: 0 = x0.35, 1 = x0.55, 2 = x0.8, 3 = full (default). Hero layers always run at full ")
	TEXT("density; effects beyond 40 m / 80 m of the camera also run at x0.5 / x0.25."),
	ECVF_Default);

namespace
{
	const FName MTName_Color(TEXT("Color"));
	const FName MTName_Intensity(TEXT("Intensity"));
	const FName MTName_Opacity(TEXT("Opacity"));
	const FName MTName_Texture(TEXT("Texture"));
	const FName MTName_SpinSpeed(TEXT("SpinSpeed"));
	const FName MTName_Age(TEXT("Age"));

	constexpr float OneShotSafety = 30.f;        // a one-shot may outlive its duration this long (particles, fades)
	constexpr float LoopSafetyDetached = 12.f;   // a free-standing loop nobody stopped (pulses, targets, aim lines)
	constexpr float FormationSafety = 4.f;       // a free-standing formation loop (a charge lasts at most ~3.3 s)
	constexpr float LoopSafetyAttached = 300.f;  // a loop riding on something (auras, zones, projectiles)
	constexpr int32 MaxLiveEffects = 120;
	constexpr double MidDistance = 4000.0;
	constexpr double FarDistance = 8000.0;
	constexpr int32 GroundTraceBudget = 24;      // per-particle ground traces per effect per frame
	constexpr float LargeShape = 200.f;          // shapes wider than this snap each particle to the ground under it

	FVector RandomInCone(FRandomStream& Stream, const FVector& Dir, float ConeDeg)
	{
		if (ConeDeg >= 179.f)
		{
			return Stream.GetUnitVector();
		}
		return Stream.VRandCone(Dir.GetSafeNormal(), FMath::DegreesToRadians(FMath::Max(0.f, ConeDeg)));
	}

	/** mt.VFX.Quality -> density multiplier for non-hero emitters. */
	float QualityDensity()
	{
		static const float Levels[4] = { 0.35f, 0.55f, 0.8f, 1.f };
		return Levels[FMath::Clamp(CVarMTVFXQuality.GetValueOnGameThread(), 0, 3)];
	}

	bool GetViewLocation(const UWorld* World, FVector& OutLocation)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			OutLocation = PC->PlayerCameraManager->GetCameraLocation();
			return true;
		}
		return false;
	}

	/** Frequently spawned one-shots: dropped past the live-effect cap and recycled through the pool. */
	bool IsCheapPreset(FName Name)
	{
		static TMap<FName, bool> Known;
		if (const bool* Found = Known.Find(Name))
		{
			return *Found;
		}
		const FString Text = Name.ToString();
		const bool bCheap = Text.Contains(TEXT("Hit.")) || Text.Contains(TEXT(".Trail")) || Text.Contains(TEXT(".Ripple"))
			|| Text.Contains(TEXT(".Muzzle")) || Text.Contains(TEXT(".Pierce")) || Text.Contains(TEXT("Dodge."));
		Known.Add(Name, bCheap);
		return bCheap;
	}

	bool IsEmissiveMaterial(const FString& Path)
	{
		return Path.IsEmpty() || Path == MTVFX::Paths::MatGlow || Path == MTVFX::Paths::MatSprite;
	}
}

// ---------------------------------------------------------------------------------------------
// Curves
// ---------------------------------------------------------------------------------------------

float FMTVFXCurve::Eval(float T) const
{
	if (Keys.Num() == 0)
	{
		return 1.f;
	}
	if (T <= Keys[0].X)
	{
		return Keys[0].Y;
	}
	for (int32 i = 1; i < Keys.Num(); ++i)
	{
		if (T <= Keys[i].X)
		{
			const float Span = FMath::Max(KINDA_SMALL_NUMBER, Keys[i].X - Keys[i - 1].X);
			const float A = (T - Keys[i - 1].X) / Span;
			return FMath::Lerp(Keys[i - 1].Y, Keys[i].Y, FMath::SmoothStep(0.f, 1.f, A));
		}
	}
	return Keys.Last().Y;
}

FMTVFXCurve FMTVFXCurve::FadeInOut(float In, float Out)
{
	In = FMath::Clamp(In, 0.f, 0.95f);
	Out = FMath::Clamp(Out, 0.f, 1.f - In);
	return FMTVFXCurve({ { 0.f, 0.f }, { In, 1.f }, { 1.f - Out, 1.f }, { 1.f, 0.f } });
}

FMTVFXCurve FMTVFXCurve::Grow(float From, float At)
{
	return FMTVFXCurve({ { 0.f, From }, { FMath::Clamp(At, 0.01f, 1.f), 1.f }, { 1.f, 1.f } });
}

// ---------------------------------------------------------------------------------------------
// Actor
// ---------------------------------------------------------------------------------------------

AMTSpellVFX::AMTSpellVFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	SetCanBeDamaged(false);
	SetReplicates(false);
}

bool AMTSpellVFX::HasPreset(FName Name)
{
	return MTVFX::FindPreset(Name) != nullptr;
}

AMTSpellVFX* AMTSpellVFX::SpawnPreset(UObject* WorldContext, FName Name, const FTransform& Transform, float InScale,
	USceneComponent* AttachTo, FName Socket, AActor* Source)
{
	UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || Name.IsNone())
	{
		return nullptr;
	}

	// Styles: "<Name>@<CharacterId>" (Orsted's cleaner, pale-gold variants) before the shared preset.
	FName Resolved = Name;
	const FMTVFXDesc* Preset = nullptr;
	if (const AMTCharacterBase* Character = Cast<AMTCharacterBase>(Source))
	{
		const FName StyleId = Character->GetCharacterId();
		if (!StyleId.IsNone() && MTVFX::HasStyleVariants(Name))
		{
			const FName Styled(*FString::Printf(TEXT("%s@%s"), *Name.ToString(), *StyleId.ToString()));
			if (const FMTVFXDesc* StyledPreset = MTVFX::FindPreset(Styled))
			{
				Preset = StyledPreset;
				Resolved = Styled;
			}
		}
	}
	if (!Preset)
	{
		Preset = MTVFX::FindPreset(Name);
	}
	if (!Preset)
	{
		return nullptr;
	}

	UMTVFXSubsystem* Registry = World->GetSubsystem<UMTVFXSubsystem>();
	const bool bCheap = IsCheapPreset(Resolved);
	if (bCheap && Registry && Registry->GetLiveEffects() >= MaxLiveEffects)
	{
		return nullptr; // past the live-effect cap the cheap one-shots (hits, trails, ripples, muzzles) go first
	}

	FTransform Placement(Transform.GetRotation(), Transform.GetLocation());
	if (Preset->bUpright)
	{
		Placement.SetRotation(FRotator(0.f, Transform.Rotator().Yaw, 0.f).Quaternion());
	}

	const bool bPool = bCheap && !Preset->bLoop;
	AMTSpellVFX* FX = (bPool && Registry) ? Registry->TakeFromPool(Resolved) : nullptr;
	if (FX)
	{
		FX->LeavePool(Placement);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		FX = World->SpawnActor<AMTSpellVFX>(AMTSpellVFX::StaticClass(), Placement, Params);
		if (!FX)
		{
			return nullptr;
		}
	}
	FX->AttachForPlay(AttachTo, Socket, Placement.GetRotation());
	FX->Play(*Preset, Resolved, FMath::Max(0.05f, InScale), Source, bPool);
	return FX;
}

void AMTSpellVFX::AttachForPlay(USceneComponent* Parent, FName SocketName, const FQuat& WorldRotation)
{
	bSpawnedAttached = Parent != nullptr;
	bKeepAimRotation = Parent != nullptr && !SocketName.IsNone();
	AimRelativeToParent = FQuat::Identity;
	if (Root)
	{
		Root->SetUsingAbsoluteRotation(bKeepAimRotation);
	}
	if (!Parent)
	{
		return;
	}
	AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	if (bKeepAimRotation)
	{
		// A hand socket: follow the hand, but keep pointing where the spell will fly (a slug, a lance, a crescent),
		// turning with the caster rather than with the bones of the hand.
		SetActorRotation(WorldRotation);
		const AActor* ParentActor = GetAttachParentActor();
		AimRelativeToParent = ParentActor ? ParentActor->GetActorQuat().Inverse() * WorldRotation : WorldRotation;
	}
}

void AMTSpellVFX::Play(const FMTVFXDesc& InDesc, FName InName, float InScale, AActor* InSource, bool bInPoolable)
{
	Scale = InScale;
	bPoolable = bInPoolable;
	if (!bBuilt || PresetName != InName)
	{
		ClearLayers();
		Desc = InDesc;
		PresetName = InName;
		bFormationPreset = PresetName.ToString().Contains(TEXT(".Formation"));
		CreateLayers();
		bBuilt = true;
	}
	Restart(InSource);
	if (!bCountedLive)
	{
		if (UMTVFXSubsystem* Registry = UMTVFXSubsystem::Get(this))
		{
			Registry->NoteEffectStarted();
			bCountedLive = true;
		}
	}
	// Show the first frame immediately (no pop on the frame the effect is born).
	Tick(0.f);
}

void AMTSpellVFX::ClearLayers()
{
	for (UActorComponent* Comp : Owned)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	Owned.Reset();
	Emitters.Reset();
	MeshLayers.Reset();
	LightLayers.Reset();
	bBuilt = false;
}

void AMTSpellVFX::CreateLayers()
{
	for (const FMTVFXMeshLayer& Def : Desc.Meshes)
	{
		FMeshState& State = MeshLayers.AddDefaulted_GetRef();
		State.Def = Def;
		State.bChargeTinted = IsEmissiveMaterial(Def.MaterialPath);
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		UStaticMesh* LayerMesh = MTVFX::LoadMesh(Def.MeshPath);
		Comp->SetStaticMesh(LayerMesh);
		const FVector Native = LayerMesh ? FVector(LayerMesh->GetBounds().BoxExtent) : FVector(50.f);
		State.NativeScale = Def.Size / Native.ComponentMax(FVector(1.f));
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
		Comp->SetGenerateOverlapEvents(false);
		Comp->bReceivesDecals = false;
		Comp->SetupAttachment(Root);
		Comp->SetRelativeLocation(Def.Offset * Scale);
		State.BaseRotation = Def.Rotation.Quaternion();
		Comp->SetRelativeRotation(State.BaseRotation);
		Comp->SetRelativeScale3D(FVector(KINDA_SMALL_NUMBER));
		Comp->SetVisibility(false);
		Comp->RegisterComponent();
		if (UMaterialInterface* Base = MTVFX::LoadMaterial(Def.MaterialPath))
		{
			State.MID = UMaterialInstanceDynamic::Create(Base, this);
			for (const TPair<FName, float>& Param : Def.Scalars)
			{
				State.MID->SetScalarParameterValue(Param.Key, Param.Value);
			}
			for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
			{
				Comp->SetMaterial(i, State.MID);
			}
		}
		State.Comp = Comp;
		Owned.Add(Comp);
	}

	// Particle capacity follows this spawn's density (quality x distance); pooled presets keep their full capacity since
	// they are reused at other distances.
	float CapacityDensity = QualityDensity();
	FVector View = FVector::ZeroVector;
	if (GetViewLocation(GetWorld(), View))
	{
		const double Distance = FVector::Dist(View, GetActorLocation());
		CapacityDensity *= Distance > FarDistance ? 0.25f : (Distance > MidDistance ? 0.5f : 1.f);
	}
	const bool bFullCapacity = IsCheapPreset(PresetName);

	UStaticMesh* PlaneMesh = MTVFX::LoadMesh(MTVFX::Paths::Plane);
	for (const FMTVFXEmitter& Def : Desc.Emitters)
	{
		FEmitterState& State = Emitters.AddDefaulted_GetRef();
		State.Def = Def;
		const int32 FullCapacity = FMath::Clamp(Def.MaxParticles, 1, 512);
		State.Def.MaxParticles = (Def.bHero || bFullCapacity) ? FullCapacity
			: FMath::Clamp(FMath::CeilToInt(static_cast<float>(FullCapacity) * CapacityDensity), 1, FullCapacity);
		const bool bMesh = Def.Render == EMTVFXRender::Mesh;
		State.bChargeTinted = !bMesh && Def.Render != EMTVFXRender::SpriteSmoke && IsEmissiveMaterial(Def.MaterialPath);
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		UStaticMesh* ParticleMesh = bMesh ? MTVFX::LoadMesh(Def.MeshPath) : PlaneMesh;
		ISM->SetStaticMesh(ParticleMesh);
		State.MeshRadius = (bMesh && ParticleMesh) ? FMath::Max(1.f, ParticleMesh->GetBounds().SphereRadius) : 50.f;
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCastShadow(bMesh);
		ISM->SetGenerateOverlapEvents(false);
		ISM->bReceivesDecals = false;
		ISM->SetupAttachment(Root);
		ISM->RegisterComponent();
		ISM->SetNumCustomDataFloats(4);
		const TCHAR* DefaultMaterial = Def.Render == EMTVFXRender::SpriteSmoke ? MTVFX::Paths::MatSmoke : MTVFX::Paths::MatSprite;
		UMaterialInterface* Base = MTVFX::LoadMaterial(bMesh ? (Def.MaterialPath.IsEmpty() ? FString(MTVFX::Paths::MatRock) : Def.MaterialPath)
			: (Def.MaterialPath.IsEmpty() ? FString(DefaultMaterial) : Def.MaterialPath));
		if (Base)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			const FString TexPath = !Def.Texture.IsEmpty() ? Def.Texture
				: FString(Def.Render == EMTVFXRender::SpriteSmoke ? MTVFX::Paths::TexPuff
					: Def.Render == EMTVFXRender::Stretched ? MTVFX::Paths::TexStreak : MTVFX::Paths::TexDot);
			if (UTexture* Tex = MTVFX::LoadTexture(TexPath))
			{
				MID->SetTextureParameterValue(MTName_Texture, Tex);
			}
			MID->SetScalarParameterValue(MTName_Intensity, Def.Intensity);
			MID->SetScalarParameterValue(MTName_Opacity, 1.f);
			MID->SetVectorParameterValue(MTName_Color, Def.ParamColor);
			for (const TPair<FName, float>& Param : Def.Scalars)
			{
				MID->SetScalarParameterValue(Param.Key, Param.Value);
			}
			ISM->SetMaterial(0, MID);
			State.MID = MID;
		}
		State.Particles.SetNum(State.Def.MaxParticles);
		State.Transforms.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector(KINDA_SMALL_NUMBER)), State.Def.MaxParticles);
		ISM->AddInstances(State.Transforms, false, true);
		State.ISM = ISM;
		Owned.Add(ISM);
	}

	for (const FMTVFXLight& Def : Desc.Lights)
	{
		FLightState& State = LightLayers.AddDefaulted_GetRef();
		State.Def = Def;
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(Def.Offset * Scale);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(0.f);
		Light->SetAttenuationRadius(Def.Radius * Scale);
		Light->SetLightColor(Def.Color);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		State.Comp = Light;
		Owned.Add(Light);
	}
}

void AMTSpellVFX::Restart(AActor* InSource)
{
	++PlayCount;
	Random.Initialize(static_cast<int32>((GetUniqueID() * 2654435761u) ^ (PlayCount * 40503u) ^ 0x5A17u));
	Age = 0.f;
	StopTime = -1.f;
	bStopping = false;
	Tint = FLinearColor::White;
	ChargeAlpha = 0.f;
	GroundTracesLeft = GroundTraceBudget;
	HeldDecals.Reset();
	DecalSpawned.Init(false, Desc.Decals.Num());
	ShakeDone.Init(false, Desc.Shakes.Num());
	DestroyGhosts();
	GhostsMade = 0;
	NextGhostTime = 0.f;
	GhostSource.Reset();
	if (Desc.Afterimages.Count > 0)
	{
		if (const ACharacter* Character = Cast<ACharacter>(InSource))
		{
			GhostSource = Character->GetMesh();
		}
	}

	// Density of this spawn: the quality level x the distance to the camera (decided once, here).
	float Density = QualityDensity();
	FVector View = FVector::ZeroVector;
	if (GetViewLocation(GetWorld(), View))
	{
		const double Distance = FVector::Dist(View, GetActorLocation());
		Density *= Distance > FarDistance ? 0.25f : (Distance > MidDistance ? 0.5f : 1.f);
	}

	float SourceGroundZ = 0.f;
	const bool bSourceGround = TraceGround(GetActorLocation(), SourceGroundZ);
	const FVector ActorLocation = GetActorLocation();
	const TArray<float> Cleared({ 0.f, 0.f, 0.f, 0.f });
	for (FEmitterState& State : Emitters)
	{
		const FMTVFXEmitter& Def = State.Def;
		State.Density = Def.bHero ? 1.f : Density;
		State.BurstCount = Def.Burst > 0 ? FMath::Max(1, FMath::RoundToInt(static_cast<float>(Def.Burst) * State.Density)) : 0;
		State.SpawnDebt = 0.f;
		State.bBurstDone = false;
		State.AppliedIntensity = -1.f;
		State.bWasAny = false;
		State.GroundZ = SourceGroundZ;
		State.bHasGround = bSourceGround && (Def.bBounce || Def.bFollowGround);
		State.bTraceEach = Def.bFollowGround && Def.bWorldSpace
			&& (bSpawnedAttached || Def.Radius > LargeShape || Def.Extent.GetMax() > LargeShape);
		for (FParticle& P : State.Particles)
		{
			P.bAlive = false;
			P.bResting = false;
		}
		if (UInstancedStaticMeshComponent* ISM = State.ISM)
		{
			for (int32 i = 0; i < State.Transforms.Num(); ++i)
			{
				State.Transforms[i] = FTransform(FQuat::Identity, ActorLocation, FVector(KINDA_SMALL_NUMBER));
				if (PlayCount > 1)
				{
					ISM->SetCustomData(i, Cleared, false);
				}
			}
			if (PlayCount > 1)
			{
				ISM->BatchUpdateInstancesTransforms(0, State.Transforms, true, true, true);
			}
		}
	}
	for (FMeshState& State : MeshLayers)
	{
		State.SpinAngle = 0.f;
		if (State.Comp)
		{
			State.Comp->SetVisibility(false);
		}
	}
	for (FLightState& State : LightLayers)
	{
		State.AppliedRadius = -1.f;
		if (State.Comp)
		{
			State.Comp->SetIntensity(0.f);
		}
	}
}

void AMTSpellVFX::Stop()
{
	BeginStop(Age);
}

void AMTSpellVFX::BeginStop(float AtTime)
{
	if (bStopping)
	{
		return;
	}
	bStopping = true;
	StopTime = AtTime;
	for (FHeldDecal& Held : HeldDecals)
	{
		if (UDecalComponent* HeldComp = Held.Comp.Get())
		{
			HeldComp->SetFadeOut(0.f, FMath::Max(0.05f, Held.FadeOut), false);
		}
	}
}

void AMTSpellVFX::SetTint(const FLinearColor& InTint)
{
	Tint = InTint;
}

void AMTSpellVFX::SetCharge(float Alpha)
{
	ChargeAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
}

void AMTSpellVFX::SetEffectScale(float NewScale)
{
	// Meshes, emitters and lights pick the new size up on this frame's tick (effects tick after gameplay).
	Scale = FMath::Max(0.05f, NewScale);
}

float AMTSpellVFX::GetEffectScale() const
{
	return Scale;
}

float AMTSpellVFX::ChargeGate(float Threshold) const
{
	return Threshold <= 0.f ? 1.f : FMath::SmoothStep(Threshold, FMath::Min(1.f, Threshold + 0.15f), ChargeAlpha);
}

FLinearColor AMTSpellVFX::ChargeColor(const FLinearColor& In) const
{
	const FLinearColor& Target = Desc.Charge.TintAtFull;
	const float Weight = FMath::Clamp(ChargeAlpha * Target.A, 0.f, 1.f);
	if (Weight <= 0.f)
	{
		return In;
	}
	return FLinearColor(FMath::Lerp(In.R, Target.R, Weight), FMath::Lerp(In.G, Target.G, Weight), FMath::Lerp(In.B, Target.B, Weight), In.A);
}

float AMTSpellVFX::FadeFactor() const
{
	if (!bStopping)
	{
		return 1.f;
	}
	return FMath::Clamp(1.f - (Age - StopTime) / FMath::Max(0.05f, Desc.FadeOut), 0.f, 1.f);
}

void AMTSpellVFX::DestroyGhosts()
{
	for (FAfterimage& Ghost : Ghosts)
	{
		if (Ghost.Comp)
		{
			Ghost.Comp->DestroyComponent();
		}
	}
	Ghosts.Reset();
}

void AMTSpellVFX::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyGhosts();
	if (bCountedLive)
	{
		bCountedLive = false;
		if (UMTVFXSubsystem* Registry = UMTVFXSubsystem::Get(this))
		{
			Registry->NoteEffectEnded();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AMTSpellVFX::Finish()
{
	if (bPoolable && !IsActorBeingDestroyed())
	{
		if (UMTVFXSubsystem* Registry = UMTVFXSubsystem::Get(this))
		{
			if (Registry->ReturnToPool(this))
			{
				EnterPool();
				return;
			}
		}
	}
	Destroy();
}

void AMTSpellVFX::EnterPool()
{
	if (bCountedLive)
	{
		bCountedLive = false;
		if (UMTVFXSubsystem* Registry = UMTVFXSubsystem::Get(this))
		{
			Registry->NoteEffectEnded();
		}
	}
	bInPool = true;
	bStopping = true;
	DestroyGhosts();
	HeldDecals.Reset();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	for (FLightState& State : LightLayers)
	{
		if (State.Comp)
		{
			State.Comp->SetIntensity(0.f);
		}
	}
	for (FMeshState& State : MeshLayers)
	{
		if (State.Comp)
		{
			State.Comp->SetVisibility(false);
		}
	}
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
}

void AMTSpellVFX::LeavePool(const FTransform& Placement)
{
	bInPool = false;
	SetActorTransform(Placement, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
}

void AMTSpellVFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bInPool)
	{
		return;
	}
	Age += DeltaSeconds;
	GroundTracesLeft = GroundTraceBudget;

	if (!Desc.bLoop && !bStopping && Age >= Desc.Duration)
	{
		// One-shots stop emitting at their duration; particles finish their lives.
		BeginStop(Desc.Duration);
	}
	if (Desc.bLoop && !bStopping)
	{
		const bool bOrphaned = bSpawnedAttached && !GetAttachParentActor();
		const float LoopSafety = bSpawnedAttached ? LoopSafetyAttached : (bFormationPreset ? FormationSafety : LoopSafetyDetached);
		if (bOrphaned || Age > LoopSafety)
		{
			// What it rode on is gone (a projectile, a zone), or nobody stopped it: fade out rather than linger.
			BeginStop(Age);
		}
	}
	if (bKeepAimRotation && !bStopping)
	{
		if (const AActor* ParentActor = GetAttachParentActor())
		{
			SetActorRotation(ParentActor->GetActorQuat() * AimRelativeToParent);
		}
	}

	FVector CameraLocation = GetActorLocation() + FVector(-500.f, 0.f, 200.f);
	GetViewLocation(GetWorld(), CameraLocation);

	TickMeshes(DeltaSeconds);
	TickLights();
	SpawnDecals();
	TickHeldDecals();
	TickShakes();
	TickAfterimages(DeltaSeconds);

	bool bAnyParticle = false;
	for (FEmitterState& State : Emitters)
	{
		const float Start = State.Def.Delay;
		const bool bInWindow = Age >= Start && (State.Def.EmitDuration < 0.f || Age <= Start + State.Def.EmitDuration);
		TickEmitter(State, DeltaSeconds, bInWindow && !bStopping, CameraLocation);
		bAnyParticle |= State.bWasAny;
	}

	const bool bFadedOut = bStopping && (Age - StopTime) >= Desc.FadeOut;
	// A faded loop has invisible particles (their alpha follows the fade): no need to wait for them.
	const bool bParticlesDone = !bAnyParticle || (Desc.bLoop && bFadedOut);
	const bool bGhostsDone = Ghosts.Num() == 0 && (Desc.Afterimages.Count == 0 || GhostsMade >= Desc.Afterimages.Count || bStopping);
	bool bDecalsPending = false;
	for (int32 i = 0; i < Desc.Decals.Num(); ++i)
	{
		bDecalsPending |= !DecalSpawned[i] && Desc.Decals[i].Delay > Age && !bStopping;
	}
	bool bHeldFading = false;
	for (const FHeldDecal& Held : HeldDecals)
	{
		bHeldFading |= Held.Comp.IsValid();
	}
	const float Safety = Desc.bLoop ? (bSpawnedAttached ? LoopSafetyAttached : LoopSafetyDetached) : OneShotSafety;
	const bool bExpired = Age > Desc.Duration + Safety + Desc.FadeOut + 5.f;
	if ((bFadedOut && bParticlesDone && bGhostsDone && !bDecalsPending && !bHeldFading) || bExpired)
	{
		Finish();
	}
}

void AMTSpellVFX::TickMeshes(float DeltaSeconds)
{
	const float Fade = FadeFactor();
	const float CurScale = CurrentScale();
	const float SpinFactor = ChargeLerp(Desc.Charge.SpinScale);
	const float Bright = ChargeLerp(Desc.Charge.IntensityScale);
	const float Jitter = Desc.Charge.Vibration * ChargeAlpha * Scale;
	for (FMeshState& State : MeshLayers)
	{
		UStaticMeshComponent* Comp = State.Comp;
		if (!Comp)
		{
			continue;
		}
		const FMTVFXMeshLayer& Def = State.Def;
		const float LayerDuration = Def.Duration > 0.f ? Def.Duration : FMath::Max(0.01f, Desc.Duration - Def.Delay);
		const float Local = Age - Def.Delay;
		const float Gate = ChargeGate(Def.ChargeThreshold);
		const bool bVisible = Local >= 0.f && (Def.Duration < 0.f || Local <= Def.Duration) && Gate > 0.f;
		if (!bVisible)
		{
			Comp->SetVisibility(false);
			continue;
		}
		Comp->SetVisibility(true);
		const float T = FMath::Clamp(Local / LayerDuration, 0.f, 1.f);
		FVector S = State.NativeScale * CurScale * Def.ScaleOverLife.Eval(T);
		if (Def.bWobble)
		{
			const float W = Age * 9.f + static_cast<float>(Def.Offset.X + Def.Offset.Y * 0.37);
			S *= FVector(1.f + 0.07f * FMath::Sin(W), 1.f + 0.07f * FMath::Sin(W * 1.3f + 1.f), 1.f + 0.07f * FMath::Sin(W * 0.8f + 2.f));
		}
		Comp->SetRelativeScale3D(S.ComponentMax(FVector(KINDA_SMALL_NUMBER)));
		FVector Location = (Def.Offset + Def.Velocity * Local) * CurScale;
		if (Jitter > 0.f)
		{
			Location += Random.GetUnitVector() * (Jitter * Random.FRandRange(0.4f, 1.f));
		}
		Comp->SetRelativeLocation(Location);
		if (Def.SpinSpeed != 0.f)
		{
			State.SpinAngle = FMath::Fmod(State.SpinAngle + Def.SpinSpeed * SpinFactor * DeltaSeconds, 360.f);
			const FQuat Spin(Def.SpinAxis.GetSafeNormal(), FMath::DegreesToRadians(State.SpinAngle));
			Comp->SetRelativeRotation(Spin * State.BaseRotation);
		}
		if (State.MID)
		{
			const FLinearColor LayerColor = Def.Color * Tint;
			State.MID->SetVectorParameterValue(MTName_Color, State.bChargeTinted ? ChargeColor(LayerColor) : LayerColor);
			State.MID->SetScalarParameterValue(MTName_Intensity, Def.Intensity * (State.bChargeTinted ? Bright : 1.f));
			State.MID->SetScalarParameterValue(MTName_Opacity, Def.AlphaOverLife.Eval(T) * Fade * Gate);
		}
	}
}

void AMTSpellVFX::TickLights()
{
	const float Fade = FadeFactor();
	const float CurScale = CurrentScale();
	const float Bright = ChargeLerp(Desc.Charge.IntensityScale);
	for (FLightState& State : LightLayers)
	{
		if (!State.Comp)
		{
			continue;
		}
		const FMTVFXLight& Def = State.Def;
		const float LightRadius = Def.Radius * CurScale;
		if (FMath::Abs(LightRadius - State.AppliedRadius) > 1.f)
		{
			State.AppliedRadius = LightRadius;
			State.Comp->SetAttenuationRadius(LightRadius);
			State.Comp->SetRelativeLocation(Def.Offset * CurScale);
		}
		const float LightDuration = Def.Duration > 0.f ? Def.Duration : FMath::Max(0.01f, Desc.Duration - Def.Delay);
		const float Local = Age - Def.Delay;
		float Value = 0.f;
		if (Local >= 0.f && (Def.Duration < 0.f || Local <= Def.Duration))
		{
			const float T = FMath::Clamp(Local / LightDuration, 0.f, 1.f);
			Value = Def.Intensity * Def.IntensityOverLife.Eval(Desc.bLoop && Def.Duration < 0.f ? FMath::Min(T, 0.5f) : T) * Fade * Bright;
			if (Def.Flicker > 0.f)
			{
				Value *= 1.f - Def.Flicker * 0.5f * (1.f + FMath::PerlinNoise1D(Age * 11.f + static_cast<float>(GetUniqueID() % 997)));
			}
		}
		State.Comp->SetIntensity(FMath::Max(0.f, Value));
		State.Comp->SetLightColor(ChargeColor(Def.Color * Tint));
	}
}

void AMTSpellVFX::SpawnDecals()
{
	for (int32 i = 0; i < Desc.Decals.Num(); ++i)
	{
		if (DecalSpawned[i] || Age < Desc.Decals[i].Delay || (bStopping && (Desc.bLoop || Desc.Decals[i].bUntilStop)))
		{
			continue;
		}
		DecalSpawned[i] = true;
		const FMTVFXDecal& Def = Desc.Decals[i];
		UMaterialInterface* Base = MTVFX::LoadDecalMaterial(Def.MaterialPath);
		if (!Base)
		{
			continue;
		}
		// Project onto whatever is under the effect (ground, slopes, walls near the ground).
		const float DecalScale = CurrentScale();
		const FVector Origin = GetActorTransform().TransformPosition(Def.Offset * DecalScale);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTVFXDecal), false);
		Params.AddIgnoredActor(this);
		if (AActor* ParentActor = GetAttachParentActor())
		{
			Params.AddIgnoredActor(ParentActor);
		}
		// Static ground only: a hit on a character must not orient the decal to their body (a stretched square).
		FCollisionObjectQueryParams Ground(ECC_WorldStatic);
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Origin + FVector(0.f, 0.f, 150.f), Origin - FVector(0.f, 0.f, 800.f), Ground, Params))
		{
			continue;
		}
		if (Hit.ImpactNormal.Z < 0.55f)
		{
			Hit.ImpactNormal = FVector::UpVector;
		}
		FRotator Rotation;
		FVector DecalSize;
		if (Def.Length > 0.f)
		{
			// A strip along the effect's X (decal U runs along the decal's local Z).
			FVector Along = FVector::VectorPlaneProject(GetActorForwardVector(), Hit.ImpactNormal).GetSafeNormal();
			if (Along.IsNearlyZero())
			{
				Along = FVector::VectorPlaneProject(GetActorRightVector(), Hit.ImpactNormal).GetSafeNormal();
			}
			Rotation = FRotationMatrix::MakeFromXZ(-Hit.ImpactNormal, Along).Rotator();
			DecalSize = FVector(Def.Depth, Def.Size, Def.Length) * DecalScale;
		}
		else
		{
			Rotation = FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator();
			if (Def.bRandomYaw)
			{
				Rotation.Roll = Random.FRandRange(-180.f, 180.f);
			}
			else
			{
				Rotation.Roll = GetActorRotation().Yaw;
			}
			DecalSize = FVector(Def.Depth, Def.Size, Def.Size) * DecalScale;
		}
		UDecalComponent* Decal = nullptr;
		if (Def.bUntilStop)
		{
			// Held marks ride on the effect (an aim line follows the aim) and fade when it stops.
			Decal = UGameplayStatics::SpawnDecalAttached(Base, DecalSize, Root, NAME_None, Hit.ImpactPoint, Rotation,
				EAttachLocation::KeepWorldPosition, 0.f);
		}
		else
		{
			Decal = UGameplayStatics::SpawnDecalAtLocation(this, Base, DecalSize, Hit.ImpactPoint, Rotation, Def.Lifetime + Def.FadeOut);
		}
		if (!Decal)
		{
			continue;
		}
		UMaterialInstanceDynamic* MID = Decal->CreateDynamicMaterialInstance();
		if (MID)
		{
			MID->SetVectorParameterValue(MTName_Color, Def.Color * Tint);
			MID->SetScalarParameterValue(MTName_Intensity, Def.Intensity);
			MID->SetScalarParameterValue(MTName_SpinSpeed, Def.SpinSpeed);
			MID->SetScalarParameterValue(MTName_Opacity, 1.f);
			MID->SetScalarParameterValue(MTName_Age, 0.f);
			for (const TPair<FName, float>& Param : Def.Scalars)
			{
				MID->SetScalarParameterValue(Param.Key, Param.Value);
			}
		}
		Decal->SetFadeIn(0.f, FMath::Max(0.01f, Def.FadeIn));
		if (!Def.bUntilStop)
		{
			Decal->SetFadeOut(Def.Lifetime, FMath::Max(0.05f, Def.FadeOut), false);
		}
		Decal->SetFadeScreenSize(0.002f);
		Decal->SetSortOrder(5 + Def.SortOrder); // re-creates the render state, so the order holds after registration
		if (Def.bUntilStop)
		{
			FHeldDecal& Held = HeldDecals.AddDefaulted_GetRef();
			Held.Comp = Decal;
			Held.MID = MID;
			Held.BornAge = Age;
			Held.FadeOut = Def.FadeOut;
		}
		if (UMTVFXSubsystem* Registry = UMTVFXSubsystem::Get(this))
		{
			Registry->RegisterDecal(Decal, Def.bUntilStop);
		}
	}
}

void AMTSpellVFX::TickHeldDecals()
{
	for (const FHeldDecal& Held : HeldDecals)
	{
		if (UMaterialInstanceDynamic* HeldMID = Held.MID.Get())
		{
			HeldMID->SetScalarParameterValue(MTName_Age, Age - Held.BornAge);
		}
	}
}

void AMTSpellVFX::TickShakes()
{
	for (int32 i = 0; i < Desc.Shakes.Num(); ++i)
	{
		if (ShakeDone[i] || Age < Desc.Shakes[i].Delay)
		{
			continue;
		}
		ShakeDone[i] = true;
		const FMTVFXShake& Def = Desc.Shakes[i];
		const float Strength = Def.Strength * FMath::Max(0.f, 1.f + Def.ScaleStrength * (Scale - 1.f));
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AMTPlayerCharacter* Player = It->IsValid() ? Cast<AMTPlayerCharacter>((*It)->GetPawn()) : nullptr;
			if (!Player)
			{
				continue;
			}
			const float Dist = static_cast<float>(FVector::Dist(Player->GetActorLocation(), GetActorLocation()));
			const float Falloff = 1.f - FMath::Clamp(Dist / FMath::Max(1.f, Def.Radius * Scale), 0.f, 1.f);
			if (Falloff > 0.f)
			{
				Player->AddCameraShake(FMath::Min(1.f, Strength * Falloff), Def.Duration);
				if (Def.FOVKick > 0.f)
				{
					Player->AddFOVKick(Def.FOVKick * Falloff, Def.Duration);
				}
			}
		}
	}
}

void AMTSpellVFX::TickAfterimages(float DeltaSeconds)
{
	const FMTVFXAfterimages& Def = Desc.Afterimages;
	USkeletalMeshComponent* PoseSource = GhostSource.Get();
	if (PoseSource && !bStopping && GhostsMade < Def.Count && Age >= NextGhostTime)
	{
		NextGhostTime = Age + Def.Interval;
		++GhostsMade;
		UPoseableMeshComponent* Ghost = NewObject<UPoseableMeshComponent>(this);
		Ghost->SetUsingAbsoluteLocation(true);
		Ghost->SetUsingAbsoluteRotation(true);
		Ghost->SetUsingAbsoluteScale(true);
		Ghost->SetSkinnedAssetAndUpdate(PoseSource->GetSkinnedAsset());
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ghost->SetCastShadow(false);
		Ghost->bReceivesDecals = false;
		Ghost->RegisterComponent();
		Ghost->SetWorldTransform(PoseSource->GetComponentTransform());
		Ghost->CopyPoseFromSkeletalComponent(PoseSource);
		FAfterimage& Entry = Ghosts.AddDefaulted_GetRef();
		Entry.Comp = Ghost;
		if (UMaterialInterface* Base = MTVFX::LoadMaterial(MTVFX::Paths::MatGhost))
		{
			for (int32 i = 0; i < Ghost->GetNumMaterials(); ++i)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
				MID->SetVectorParameterValue(MTName_Color, Def.Color * Tint);
				MID->SetScalarParameterValue(MTName_Intensity, Def.Intensity);
				Ghost->SetMaterial(i, MID);
				Entry.MIDs.Add(MID);
			}
		}
	}
	for (int32 i = Ghosts.Num() - 1; i >= 0; --i)
	{
		FAfterimage& Ghost = Ghosts[i];
		Ghost.Age += DeltaSeconds;
		const float Alpha = 1.f - FMath::Clamp(Ghost.Age / FMath::Max(0.05f, Def.Lifetime), 0.f, 1.f);
		for (UMaterialInstanceDynamic* MID : Ghost.MIDs)
		{
			MID->SetScalarParameterValue(MTName_Opacity, Alpha * Alpha);
		}
		if (Alpha <= 0.f)
		{
			if (Ghost.Comp)
			{
				Ghost.Comp->DestroyComponent();
			}
			Ghosts.RemoveAtSwap(i);
		}
	}
}

bool AMTSpellVFX::TraceGround(const FVector& Around, float& OutZ) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTVFXGround), false);
	Params.AddIgnoredActor(this);
	const FVector From = Around + FVector(0.f, 0.f, 300.f);
	if (World->LineTraceSingleByObjectType(Hit, From, From - FVector(0.f, 0.f, 5000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutZ = static_cast<float>(Hit.ImpactPoint.Z);
		return true;
	}
	return false;
}

void AMTSpellVFX::SpawnParticle(FEmitterState& State)
{
	int32 Slot = INDEX_NONE;
	for (int32 i = 0; i < State.Particles.Num(); ++i)
	{
		if (!State.Particles[i].bAlive)
		{
			Slot = i;
			break;
		}
	}
	if (Slot == INDEX_NONE)
	{
		return;
	}
	const FMTVFXEmitter& Def = State.Def;
	FVector Local = Def.Offset;
	FVector Radial = FVector::ZeroVector;
	const float R = Def.Radius;
	switch (Def.Shape)
	{
	case EMTVFXShape::Sphere:
		Radial = Random.GetUnitVector();
		Local += Radial * R * FMath::Pow(Random.FRand(), 1.f / 3.f);
		break;
	case EMTVFXShape::SphereShell:
		Radial = Random.GetUnitVector();
		Local += Radial * R;
		break;
	case EMTVFXShape::Ring:
	case EMTVFXShape::Disc:
	{
		const float HalfArc = FMath::DegreesToRadians(FMath::Clamp(Def.ArcDegrees, 0.f, 360.f) * 0.5f);
		const float A = Def.ArcDegrees >= 360.f ? Random.FRandRange(0.f, 2.f * PI) : Random.FRandRange(-HalfArc, HalfArc);
		Radial = FVector(FMath::Cos(A), FMath::Sin(A), 0.f);
		Local += Radial * (Def.Shape == EMTVFXShape::Disc ? R * FMath::Sqrt(Random.FRand()) : R);
		break;
	}
	case EMTVFXShape::Line:
		Local += FVector(0.f, Random.FRandRange(-Def.Extent.Y, Def.Extent.Y), 0.f);
		Radial = FVector(1.f, 0.f, 0.f);
		break;
	case EMTVFXShape::Box:
		Local += FVector(Random.FRandRange(-Def.Extent.X, Def.Extent.X), Random.FRandRange(-Def.Extent.Y, Def.Extent.Y), Random.FRandRange(-Def.Extent.Z, Def.Extent.Z));
		Radial = Random.GetUnitVector();
		break;
	default:
		Radial = Random.GetUnitVector();
		break;
	}

	FParticle& P = State.Particles[Slot];
	const FTransform& T = GetActorTransform();
	const float CurScale = CurrentScale();
	const FVector LocalScaled = Local * CurScale;
	const FVector Dir = Def.bRadial ? (Radial.IsNearlyZero() ? FVector::UpVector : Radial) : RandomInCone(Random, Def.Direction, Def.ConeDeg);
	const FVector Vel = Dir * Random.FRandRange(Def.SpeedMin, Def.SpeedMax) * CurScale;
	if (Def.bWorldSpace)
	{
		P.Position = T.TransformPosition(LocalScaled);
		P.Velocity = T.TransformVectorNoScale(Vel);
		if (Def.bFollowGround)
		{
			float GroundHeight = State.GroundZ;
			bool bGround = State.bHasGround;
			if (State.bTraceEach && GroundTracesLeft > 0)
			{
				--GroundTracesLeft;
				float Traced = 0.f;
				if (TraceGround(P.Position, Traced))
				{
					GroundHeight = Traced;
					bGround = true;
				}
			}
			if (bGround)
			{
				P.Position.Z = GroundHeight + LocalScaled.Z;
			}
		}
	}
	else
	{
		P.Position = LocalScaled;
		P.Velocity = Vel;
	}
	P.Motion = P.Velocity;
	P.Age = 0.f;
	P.Life = Random.FRandRange(Def.LifeMin, Def.LifeMax);
	P.Size = Random.FRandRange(Def.SizeMin, Def.SizeMax) * CurScale;
	P.Spin = Random.FRandRange(0.f, 360.f);
	P.SpinSpeed = Random.FRandRange(-Def.SpinMax, Def.SpinMax);
	P.Seed = Random.FRand();
	P.bAlive = true;
	P.bResting = false;
}

void AMTSpellVFX::TickEmitter(FEmitterState& State, float DeltaSeconds, bool bEmitting, const FVector& CameraLocation)
{
	const FMTVFXEmitter& Def = State.Def;
	const bool bCharged = ChargeAlpha >= Def.ChargeThreshold;
	if (bCharged && Age >= Def.Delay && !State.bBurstDone && !(bStopping && Age > StopTime + 0.01f && Desc.bLoop))
	{
		State.bBurstDone = true;
		for (int32 i = 0; i < State.BurstCount; ++i)
		{
			SpawnParticle(State);
		}
	}
	if (bEmitting && bCharged && Def.Rate > 0.f)
	{
		State.SpawnDebt += Def.Rate * State.Density * ChargeLerp(Desc.Charge.RateScale) * DeltaSeconds;
		State.SpawnDebt = FMath::Min(State.SpawnDebt, static_cast<float>(State.Particles.Num()));
		while (State.SpawnDebt >= 1.f)
		{
			State.SpawnDebt -= 1.f;
			SpawnParticle(State);
		}
	}

	UInstancedStaticMeshComponent* ISM = State.ISM;
	if (!ISM)
	{
		return;
	}
	if (State.MID)
	{
		const float WantIntensity = Def.Intensity * (State.bChargeTinted ? ChargeLerp(Desc.Charge.IntensityScale) : 1.f);
		if (FMath::Abs(WantIntensity - State.AppliedIntensity) > 0.01f * FMath::Max(1.f, WantIntensity))
		{
			State.AppliedIntensity = WantIntensity;
			State.MID->SetScalarParameterValue(MTName_Intensity, WantIntensity);
		}
	}
	const FTransform& Actor = GetActorTransform();
	const FVector Origin = Actor.GetLocation();
	const FVector Up = Actor.GetRotation().GetUpVector();
	const float Fade = Desc.bLoop ? FadeFactor() : 1.f;
	const float CurScale = CurrentScale();
	const float OrbitSpeed = Def.Orbit * ChargeLerp(Desc.Charge.SpinScale);
	TArray<float> Custom;
	Custom.SetNum(4);
	bool bAny = false;
	for (int32 i = 0; i < State.Particles.Num(); ++i)
	{
		FParticle& P = State.Particles[i];
		FTransform& Out = State.Transforms[i];
		if (!P.bAlive)
		{
			Out.SetScale3D(FVector(KINDA_SMALL_NUMBER));
			continue;
		}
		P.Age += DeltaSeconds;
		if (P.Age >= P.Life)
		{
			P.bAlive = false;
			Out.SetScale3D(FVector(KINDA_SMALL_NUMBER));
			ISM->SetCustomData(i, TArray<float>({ 0.f, 0.f, 0.f, 0.f }), false);
			continue;
		}
		bAny = true;
		const float T = P.Age / P.Life;

		// Integrate.
		if (!P.bResting)
		{
			const FVector Before = P.Position;
			FVector Accel(0.f, 0.f, (Def.Buoyancy - Def.Gravity) * CurScale);
			const FVector Centre = Def.bWorldSpace ? Origin : FVector::ZeroVector;
			const FVector Axis = Def.bWorldSpace ? Up : FVector::UpVector;
			if (Def.Attract != 0.f)
			{
				Accel += (Centre - P.Position).GetSafeNormal() * Def.Attract * CurScale;
			}
			if (OrbitSpeed != 0.f)
			{
				const FVector Rel = P.Position - Centre;
				const FVector Tangent = FVector::CrossProduct(Axis, Rel).GetSafeNormal();
				const float Speed = FMath::DegreesToRadians(OrbitSpeed) * static_cast<float>(Rel.Size2D());
				P.Position += Tangent * Speed * DeltaSeconds;
			}
			P.Velocity += Accel * DeltaSeconds;
			if (Def.Drag > 0.f)
			{
				P.Velocity *= FMath::Exp(-Def.Drag * DeltaSeconds);
			}
			P.Position += P.Velocity * DeltaSeconds;
			if (Def.bBounce && State.bHasGround && Def.bWorldSpace && P.Position.Z < State.GroundZ + 3.f)
			{
				P.Position.Z = State.GroundZ + 3.f;
				P.Velocity.Z = -P.Velocity.Z * 0.28f;
				P.Velocity.X *= 0.55f;
				P.Velocity.Y *= 0.55f;
				if (FMath::Abs(P.Velocity.Z) < 40.f)
				{
					P.bResting = true;
					P.SpinSpeed = 0.f;
				}
			}
			if (DeltaSeconds > KINDA_SMALL_NUMBER)
			{
				P.Motion = (P.Position - Before) / DeltaSeconds;
			}
		}
		else
		{
			P.Motion = FVector::ZeroVector;
		}
		P.Spin += P.SpinSpeed * DeltaSeconds;

		const FVector World = Def.bWorldSpace ? P.Position : Actor.TransformPosition(P.Position);
		const float Size = P.Size * Def.SizeOverLife.Eval(T);
		FQuat Rotation;
		FVector S;
		if (Def.Render == EMTVFXRender::Mesh)
		{
			if (Def.bFlat)
			{
				Rotation = Actor.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(P.Spin));
			}
			else
			{
				Rotation = FQuat(FVector(0.3f, 0.8f, 0.5f).GetSafeNormal(), FMath::DegreesToRadians(P.Spin)) * FQuat(FRotator(P.Seed * 360.f, P.Seed * 720.f, 0.f));
			}
			S = FVector(Size / State.MeshRadius);
		}
		else
		{
			const FVector ToCamera = (CameraLocation - World).GetSafeNormal();
			if (Def.Render == EMTVFXRender::Stretched)
			{
				// Stretched along the actual motion (orbits and pulls included), not just the launch velocity.
				const FVector WorldVel = Def.bWorldSpace ? P.Motion : Actor.TransformVectorNoScale(P.Motion);
				FVector Along = WorldVel - ToCamera * FVector::DotProduct(WorldVel, ToCamera);
				if (Along.IsNearlyZero())
				{
					Along = FVector::CrossProduct(ToCamera, FVector::UpVector);
				}
				Rotation = FRotationMatrix::MakeFromZX(ToCamera, Along).ToQuat();
				const float SpeedStretch = FMath::Clamp(static_cast<float>(WorldVel.Size()) / (600.f * FMath::Max(0.25f, CurScale)), 0.6f, 2.5f);
				S = FVector(Size * Def.Stretch * SpeedStretch / 100.f, Size / 100.f, 1.f);
			}
			else
			{
				Rotation = FQuat(ToCamera, FMath::DegreesToRadians(P.Spin)) * FRotationMatrix::MakeFromZ(ToCamera).ToQuat();
				S = FVector(Size / 100.f, Size / 100.f, 1.f);
			}
		}
		Out = FTransform(Rotation, World, S);

		FLinearColor C = FLinearColor::LerpUsingHSV(Def.ColorStart, Def.ColorEnd, T) * Tint;
		if (State.bChargeTinted)
		{
			C = ChargeColor(C);
		}
		Custom[0] = C.R;
		Custom[1] = C.G;
		Custom[2] = C.B;
		Custom[3] = Def.AlphaOverLife.Eval(T) * Fade;
		ISM->SetCustomData(i, Custom, false);
	}
	if (bAny || State.bWasAny)
	{
		// Also once after the last particle died, to hide it.
		ISM->BatchUpdateInstancesTransforms(0, State.Transforms, true, true, true);
	}
	if (bAny)
	{
		ISM->MarkRenderStateDirty();
	}
	State.bWasAny = bAny;
}

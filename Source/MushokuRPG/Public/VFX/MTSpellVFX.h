// Runtime spell effects built from meshes, sprites, lights and decals: no Niagara assets needed.
// A preset (MTVFXLibrary.cpp) describes layers; AMTSpellVFX animates them over a lifetime.
// Docs/LaPlace/VFX.md describes every preset, the quality / cap / pool rules and how to add a preset.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTSpellVFX.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UDecalComponent;
class UPoseableMeshComponent;
class USkeletalMeshComponent;
class UTexture;

/** Piecewise-linear curve over normalised life (0..1). Empty = constant 1. */
struct MUSHOKURPG_API FMTVFXCurve
{
	TArray<FVector2f> Keys;

	FMTVFXCurve() = default;
	FMTVFXCurve(std::initializer_list<FVector2f> InKeys) : Keys(InKeys) {}
	float Eval(float T) const;

	/** 0 -> 1 in the first In fraction, holds, 1 -> 0 over the last Out fraction. */
	static FMTVFXCurve FadeInOut(float In, float Out);
	/** Rises from From to 1 by At, then holds. */
	static FMTVFXCurve Grow(float From, float At);
};

/** How particles are born. */
enum class EMTVFXShape : uint8
{
	Point,
	Sphere,      // inside a sphere of Radius
	SphereShell, // on the surface of a sphere of Radius
	Ring,        // on a horizontal ring of Radius (local XY)
	Disc,        // inside a horizontal disc of Radius
	Line,        // along local Y from -Extent.Y to +Extent.Y
	Box          // inside a box of Extent
};

/** How particles are drawn. */
enum class EMTVFXRender : uint8
{
	SpriteAdd,    // camera-facing quad, additive glow (sparks, embers, motes, flames)
	SpriteSmoke,  // camera-facing quad, soft translucent (smoke, dust, mist, steam)
	Stretched,    // camera-facing quad stretched along its motion (spark streaks, droplets, flame tongues, wind streaks)
	Mesh          // the given mesh with its own material (rock chunks, shards, water blobs)
};

struct MUSHOKURPG_API FMTVFXEmitter
{
	EMTVFXRender Render = EMTVFXRender::SpriteAdd;
	/** Texture for sprites (defaults: Dot for additive, Puff for smoke, Streak for stretched). */
	FString Texture;
	/** Mesh + material for EMTVFXRender::Mesh (a material also overrides the sprite materials). */
	FString MeshPath;
	FString MaterialPath;

	int32 Burst = 0;             // particles at start
	float Rate = 0.f;            // particles per second while emitting
	float Delay = 0.f;           // seconds before emission starts
	float EmitDuration = -1.f;   // seconds of Rate emission (-1 = until the effect stops)
	int32 MaxParticles = 64;

	float LifeMin = 0.6f, LifeMax = 1.2f;
	EMTVFXShape Shape = EMTVFXShape::Point;
	float Radius = 0.f;
	/** Ring and Disc shapes: only the arc of this many degrees centred on local +X (360 = the whole circle). */
	float ArcDegrees = 360.f;
	FVector Extent = FVector::ZeroVector;
	FVector Offset = FVector::ZeroVector;     // local offset of the spawn shape
	/** Initial velocity: along Direction (local) within ConeDeg, or radially out of the shape when bRadial (negative speeds
	 *  move inward). */
	FVector Direction = FVector::UpVector;
	float ConeDeg = 25.f;
	bool bRadial = false;
	float SpeedMin = 100.f, SpeedMax = 300.f;
	/** Swirl around local Z (deg/s) and pull toward the origin (cm/s^2; negative pushes away) for gathering / vortex effects. */
	float Orbit = 0.f;
	float Attract = 0.f;
	float Gravity = 0.f;         // cm/s^2 downward
	float Drag = 0.f;            // 1/s
	float Buoyancy = 0.f;        // cm/s^2 upward (smoke)
	float SizeMin = 20.f, SizeMax = 40.f; // cm
	FMTVFXCurve SizeOverLife;
	FMTVFXCurve AlphaOverLife = FMTVFXCurve::FadeInOut(0.1f, 0.5f);
	FLinearColor ColorStart = FLinearColor::White;
	FLinearColor ColorEnd = FLinearColor::White;
	float Intensity = 4.f;       // emissive multiplier for additive
	float Stretch = 3.f;         // length / width for Stretched
	float SpinMax = 0.f;         // deg/s random spin (meshes and sprites)
	bool bFlat = false;          // Mesh render: keep the effect's orientation (horizontal ripples/rings) + spin about Z
	bool bWorldSpace = true;     // particles stay where born when the effect moves
	bool bBounce = false;        // settle on the ground (debris)
	bool bFollowGround = false;  // spawn on the ground under the shape point
	/** Exempt from mt.VFX.Quality and distance scaling: the layers that carry the spell's read. */
	bool bHero = false;
	/** Emits (and bursts) only once the charge reaches this; 0 = always. Layers that join as a spell charges. */
	float ChargeThreshold = 0.f;
	/** Material "Color" parameter (mesh particles: the rock seam glow, the water tint); particles are coloured per instance. */
	FLinearColor ParamColor = FLinearColor::White;
	/** Extra scalar parameters for the emitter's material (Glow, FresnelMix, NoiseAmount ...). */
	TMap<FName, float> Scalars;
};

struct MUSHOKURPG_API FMTVFXMeshLayer
{
	FString MeshPath;
	FString MaterialPath;
	FVector Offset = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	/** Half-extent in cm per axis at scale curve 1 (the mesh is scaled to fit, whatever its native size). */
	FVector Size = FVector(50.f);
	FMTVFXCurve ScaleOverLife;
	FMTVFXCurve AlphaOverLife = FMTVFXCurve::FadeInOut(0.1f, 0.3f);
	FLinearColor Color = FLinearColor::White;
	float Intensity = 4.f;
	FVector SpinAxis = FVector::UpVector;
	float SpinSpeed = 0.f;       // deg/s
	float Delay = 0.f;           // seconds after start
	float Duration = -1.f;       // seconds (-1 = effect lifetime)
	/** Extra scalar parameters for the layer's material (NoiseAmount, FresnelMix, Distortion, Glow ...). */
	TMap<FName, float> Scalars;
	bool bWobble = false;        // squash-and-stretch jitter (water, fire cores)
	/** Local drift in cm/s (scaled with the effect) from Offset: rings sliding down a pressure cone, a wave cresting. */
	FVector Velocity = FVector::ZeroVector;
	/** Shown only once the charge reaches this (fading in over the next 0.15 of charge); 0 = always. */
	float ChargeThreshold = 0.f;
};

struct MUSHOKURPG_API FMTVFXLight
{
	FVector Offset = FVector::ZeroVector;
	FLinearColor Color = FLinearColor(1.f, 0.6f, 0.25f);
	float Intensity = 40.f;      // candelas at peak
	float Radius = 600.f;
	FMTVFXCurve IntensityOverLife = FMTVFXCurve::FadeInOut(0.05f, 0.5f);
	float Flicker = 0.f;         // 0..1
	float Delay = 0.f;
	float Duration = -1.f;
};

struct MUSHOKURPG_API FMTVFXDecal
{
	FString MaterialPath;
	float Size = 200.f;          // radius-ish, cm (the half-width of a strip)
	float Depth = 200.f;
	float Delay = 0.f;
	float Lifetime = 8.f;        // stays after the effect, then fades
	float FadeIn = 0.1f;
	float FadeOut = 2.f;
	FLinearColor Color = FLinearColor::White;
	float Intensity = 1.f;
	bool bRandomYaw = true;
	float SpinSpeed = 0.f;       // magic circles
	FVector Offset = FVector::ZeroVector;
	TMap<FName, float> Scalars;
	/**
	 * Lives until the effect is stopped (Stop(), or the end of a one-shot), attached to the effect so it follows it, then
	 * fades over FadeOut. Zone-length marks (Quagmire mud, Inferno circles, the Earth Spikes aim line). Its material gets
	 * "Age" (seconds since the decal appeared) every frame.
	 */
	bool bUntilStop = false;
	/** > 0: a strip Size wide (half-width) and Length long (half-length) along the effect's X, instead of a disc. */
	float Length = 0.f;
	/** Added to the decal sort order (higher draws on top of lower). */
	int32 SortOrder = 0;
};

struct MUSHOKURPG_API FMTVFXShake
{
	float Strength = 0.f;        // 0..1
	float Duration = 0.4f;
	float Radius = 3000.f;       // players further away feel nothing (scaled with the effect)
	float Delay = 0.f;
	/** Field-of-view punch in degrees (ultimates): AMTPlayerCharacter::AddFOVKick, attenuated like the shake. */
	float FOVKick = 0.f;
	/** Strength x (1 + ScaleStrength x (scale - 1)): a charged (bigger) spawn of the same preset shakes harder. */
	float ScaleStrength = 0.f;
};

/** Ghost copies of the owner's pose left behind while moving (Dragon Step). */
struct MUSHOKURPG_API FMTVFXAfterimages
{
	int32 Count = 0;
	float Interval = 0.05f;
	float Lifetime = 0.35f;
	FLinearColor Color = FLinearColor(0.8f, 0.85f, 1.f);
	float Intensity = 2.f;
};

/** How a preset responds to AMTSpellVFX::SetCharge: the values at full charge, interpolated by the charge alpha. */
struct MUSHOKURPG_API FMTVFXCharge
{
	float SizeScale = 1.f;       // meshes, emitter shapes / speeds / sizes, light radii
	float IntensityScale = 1.f;  // emissive layers and lights
	float SpinScale = 1.f;       // mesh spin and particle orbit
	float RateScale = 1.f;       // emitter rates
	/** Colour the emissive layers and lights blend toward at full charge; A = how far (0 = no tint). */
	FLinearColor TintAtFull = FLinearColor(1.f, 1.f, 1.f, 0.f);
	float Vibration = 0.f;       // cm of high-frequency jitter on the mesh layers
};

struct MUSHOKURPG_API FMTVFXDesc
{
	float Duration = 1.f;        // seconds (for loops: the build-up; they run until Stop, then FadeOut)
	bool bLoop = false;
	float FadeOut = 0.4f;
	/** Keep Z up whatever rotation the spawner passes (only the yaw of X is kept): impacts and ground phases. */
	bool bUpright = false;
	TArray<FMTVFXMeshLayer> Meshes;
	TArray<FMTVFXEmitter> Emitters;
	TArray<FMTVFXLight> Lights;
	TArray<FMTVFXDecal> Decals;
	TArray<FMTVFXShake> Shakes;
	FMTVFXAfterimages Afterimages;
	FMTVFXCharge Charge;
};

UCLASS(NotBlueprintable)
class MUSHOKURPG_API AMTSpellVFX : public AActor
{
	GENERATED_BODY()

public:
	AMTSpellVFX();

	/**
	 * Spawns preset Name (e.g. "Fireball.Impact") at Transform. Scale multiplies sizes, speeds and radii.
	 * AttachTo/Socket makes the effect follow a component (formations in the hand, travel on a projectile); on a socket the
	 * effect follows the socket's position but keeps the aim of Transform, turning with the socket owner.
	 * Source (the caster) selects the style "<Name>@<CharacterId>" when one exists. Returns null when the preset is unknown
	 * or skipped (past the live-effect cap, cheap one-shots are dropped). Pooled presets (Hit.*, *.Trail, *.Ripple,
	 * *.Muzzle*, *.Pierce, Dodge.*) are fire-and-forget: do not keep the returned pointer.
	 */
	static AMTSpellVFX* SpawnPreset(UObject* WorldContext, FName Name, const FTransform& Transform, float Scale = 1.f,
		USceneComponent* AttachTo = nullptr, FName Socket = NAME_None, AActor* Source = nullptr);

	/** True when the library has a preset of that name. */
	static bool HasPreset(FName Name);

	/** Loops: stop emitting and fade out (destroys itself afterwards). Held (bUntilStop) decals start fading. */
	void Stop();
	/** Recolours every layer (charge tint, awakening upgrades). */
	void SetTint(const FLinearColor& Tint);
	bool IsLooping() const { return Desc.bLoop; }

	/** 0..1 every frame while charging: the preset responds through FMTVFXDesc::Charge. */
	void SetCharge(float Alpha);
	float GetCharge() const { return ChargeAlpha; }
	/** Live resize (tornado growth, the Quagmire target ring): meshes, emitter shapes / speeds / sizes, light and shake
	 *  radii. Decals keep the size they were spawned with. */
	void SetEffectScale(float NewScale);
	float GetEffectScale() const;
	/** The preset being played, after style resolution (e.g. "Fireball.Formation@Orsted"). */
	FName GetPresetName() const { return PresetName; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FParticle
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector Motion = FVector::ZeroVector; // actual displacement per second (orbit and attraction included)
		float Age = 0.f;
		float Life = 1.f;
		float Size = 1.f;
		float Spin = 0.f;
		float SpinSpeed = 0.f;
		float Seed = 0.f;
		bool bAlive = false;
		bool bResting = false;
	};

	struct FEmitterState
	{
		FMTVFXEmitter Def;
		TObjectPtr<UInstancedStaticMeshComponent> ISM = nullptr;
		TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
		TArray<FParticle> Particles;
		TArray<FTransform> Transforms;
		float SpawnDebt = 0.f;
		bool bBurstDone = false;
		float GroundZ = 0.f;
		bool bHasGround = false;
		bool bTraceEach = false;      // snap every particle to the ground under it (large or moving shapes)
		float MeshRadius = 50.f;
		float Density = 1.f;          // quality x distance for this spawn (1 for hero emitters)
		int32 BurstCount = 0;
		bool bChargeTinted = false;   // emissive: blends toward Charge.TintAtFull
		float AppliedIntensity = -1.f;
		bool bWasAny = false;         // any particle alive after the last tick
	};

	struct FMeshState
	{
		FMTVFXMeshLayer Def;
		TObjectPtr<UStaticMeshComponent> Comp = nullptr;
		TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
		FQuat BaseRotation = FQuat::Identity;
		FVector NativeScale = FVector(1.f);
		float SpinAngle = 0.f;
		bool bChargeTinted = false;
	};

	struct FLightState
	{
		FMTVFXLight Def;
		TObjectPtr<UPointLightComponent> Comp = nullptr;
		float AppliedRadius = -1.f;
	};

	struct FAfterimage
	{
		TObjectPtr<UPoseableMeshComponent> Comp = nullptr;
		TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
		float Age = 0.f;
	};

	/** A bUntilStop decal: attached to this effect, faded when the effect stops. */
	struct FHeldDecal
	{
		TWeakObjectPtr<UDecalComponent> Comp;
		TWeakObjectPtr<UMaterialInstanceDynamic> MID;
		float BornAge = 0.f;
		float FadeOut = 1.f;
	};

	/** Attaches (or not) for this play; on a socket, keeps WorldRotation and turns with the socket's owner. */
	void AttachForPlay(USceneComponent* Parent, FName SocketName, const FQuat& WorldRotation);
	/** Starts playing InDesc (building its components unless this actor already holds that preset's layers). */
	void Play(const FMTVFXDesc& InDesc, FName InName, float InScale, AActor* InSource, bool bInPoolable);
	void CreateLayers();
	void ClearLayers();
	/** Resets every piece of runtime state for a new play (fresh actor or reused from the pool). */
	void Restart(AActor* InSource);
	void BeginStop(float AtTime);
	/** End of life: back to the per-world pool when poolable and there is room, otherwise destroyed. */
	void Finish();
	void EnterPool();
	void LeavePool(const FTransform& Placement);
	void DestroyGhosts();

	void SpawnParticle(FEmitterState& State);
	bool TraceGround(const FVector& Around, float& OutZ) const;
	void TickEmitter(FEmitterState& State, float DeltaSeconds, bool bEmitting, const FVector& CameraLocation);
	void TickMeshes(float DeltaSeconds);
	void TickLights();
	void SpawnDecals();
	void TickHeldDecals();
	void TickShakes();
	void TickAfterimages(float DeltaSeconds);
	float LocalTime() const { return Age; }
	float FadeFactor() const;

	/** Charge response: Lerp(1, AtFull, charge). */
	float ChargeLerp(float AtFull) const { return FMath::Lerp(1.f, AtFull, ChargeAlpha); }
	/** Effect scale including the charge size response. */
	float CurrentScale() const { return Scale * ChargeLerp(Desc.Charge.SizeScale); }
	/** 0..1 visibility of a layer gated at Threshold of charge. */
	float ChargeGate(float Threshold) const;
	/** An emissive colour blended toward Charge.TintAtFull by the charge. */
	FLinearColor ChargeColor(const FLinearColor& In) const;

	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UActorComponent>> Owned;

	FMTVFXDesc Desc;
	FName PresetName;
	float Scale = 1.f;
	float Age = 0.f;
	float StopTime = -1.f;
	bool bStopping = false;
	FLinearColor Tint = FLinearColor::White;
	float ChargeAlpha = 0.f;
	TArray<FEmitterState> Emitters;
	TArray<FMeshState> MeshLayers;
	TArray<FLightState> LightLayers;
	TArray<bool> DecalSpawned;
	TArray<FHeldDecal> HeldDecals;
	TArray<bool> ShakeDone;
	TArray<FAfterimage> Ghosts;
	float NextGhostTime = 0.f;
	int32 GhostsMade = 0;
	TWeakObjectPtr<USkeletalMeshComponent> GhostSource;
	FRandomStream Random;

	bool bBuilt = false;             // layers created for PresetName
	bool bPoolable = false;          // returns to the per-world pool instead of being destroyed
	bool bInPool = false;
	bool bCountedLive = false;       // counted in UMTVFXSubsystem's live effects
	bool bSpawnedAttached = false;   // attached at spawn: a loop that loses its parent stops itself
	bool bFormationPreset = false;   // "<Preset>.Formation...": a free-standing formation loop is stopped sooner
	bool bKeepAimRotation = false;   // socket attachment: keeps the aim, turning with the socket's owner
	FQuat AimRelativeToParent = FQuat::Identity;
	uint32 PlayCount = 0;
	int32 GroundTracesLeft = 0;      // per-frame budget of per-particle ground traces
};

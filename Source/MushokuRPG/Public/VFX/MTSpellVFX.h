// Runtime spell effects built from meshes, sprites, lights and decals: no Niagara assets needed.
// A preset (MTVFXLibrary.cpp) describes layers; AMTSpellVFX animates them over a lifetime.
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
	Stretched,    // camera-facing quad stretched along velocity (spark streaks, droplets, rain)
	Mesh          // the given mesh with its own material (rock chunks, shards)
};

struct MUSHOKURPG_API FMTVFXEmitter
{
	EMTVFXRender Render = EMTVFXRender::SpriteAdd;
	/** Texture for sprites (defaults: Dot for additive, Puff for smoke). */
	FString Texture;
	/** Mesh + material for EMTVFXRender::Mesh. */
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
	FVector Extent = FVector::ZeroVector;
	FVector Offset = FVector::ZeroVector;     // local offset of the spawn shape
	/** Initial velocity: along Direction (local) within ConeDeg, or radially out of the shape when bRadial. */
	FVector Direction = FVector::UpVector;
	float ConeDeg = 25.f;
	bool bRadial = false;
	float SpeedMin = 100.f, SpeedMax = 300.f;
	/** Swirl around local Z (deg/s) and pull toward the origin (cm/s^2) for gathering / vortex effects. */
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
	float Size = 200.f;          // radius-ish, cm
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
};

struct MUSHOKURPG_API FMTVFXShake
{
	float Strength = 0.f;        // 0..1
	float Duration = 0.4f;
	float Radius = 3000.f;       // players further away feel nothing
	float Delay = 0.f;
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

struct MUSHOKURPG_API FMTVFXDesc
{
	float Duration = 1.f;        // seconds (for loops: until Stop, then FadeOut)
	bool bLoop = false;
	float FadeOut = 0.4f;
	TArray<FMTVFXMeshLayer> Meshes;
	TArray<FMTVFXEmitter> Emitters;
	TArray<FMTVFXLight> Lights;
	TArray<FMTVFXDecal> Decals;
	TArray<FMTVFXShake> Shakes;
	FMTVFXAfterimages Afterimages;
};

UCLASS(NotBlueprintable)
class MUSHOKURPG_API AMTSpellVFX : public AActor
{
	GENERATED_BODY()

public:
	AMTSpellVFX();

	/**
	 * Spawns preset Name (e.g. "Fireball.Impact") at Transform. Scale multiplies sizes, speeds and radii.
	 * AttachTo/Socket makes the effect follow a component (formations in the hand, travel on a projectile).
	 * Returns null when the preset is unknown.
	 */
	static AMTSpellVFX* SpawnPreset(UObject* WorldContext, FName Name, const FTransform& Transform, float Scale = 1.f,
		USceneComponent* AttachTo = nullptr, FName Socket = NAME_None, AActor* Source = nullptr);

	/** True when the library has a preset of that name. */
	static bool HasPreset(FName Name);

	/** Loops: stop emitting and fade out (destroys itself afterwards). */
	void Stop();
	/** Recolours every layer (charge tint, awakening upgrades). */
	void SetTint(const FLinearColor& Tint);
	bool IsLooping() const { return Desc.bLoop; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FParticle
	{
		FVector Position;
		FVector Velocity;
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
		TArray<FParticle> Particles;
		TArray<FTransform> Transforms;
		float SpawnDebt = 0.f;
		bool bBurstDone = false;
		float GroundZ = 0.f;
		bool bHasGround = false;
		float MeshRadius = 50.f;
	};

	struct FMeshState
	{
		FMTVFXMeshLayer Def;
		TObjectPtr<UStaticMeshComponent> Comp = nullptr;
		TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
		FQuat BaseRotation = FQuat::Identity;
		FVector NativeScale = FVector(1.f);
	};

	struct FLightState
	{
		FMTVFXLight Def;
		TObjectPtr<UPointLightComponent> Comp = nullptr;
	};

	struct FAfterimage
	{
		TObjectPtr<UPoseableMeshComponent> Comp = nullptr;
		TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
		float Age = 0.f;
	};

	void Build(const FMTVFXDesc& InDesc, float InScale, AActor* InSource);
	void SpawnParticle(FEmitterState& State);
	void TickEmitter(FEmitterState& State, float DeltaSeconds, bool bEmitting, const FVector& CameraLocation);
	void TickMeshes(float Life01, float DeltaSeconds);
	void TickLights(float DeltaSeconds);
	void SpawnDecals(float DeltaSeconds);
	void TickShakes();
	void TickAfterimages(float DeltaSeconds);
	float LocalTime() const { return Age; }
	float FadeFactor() const;

	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UActorComponent>> Owned;

	FMTVFXDesc Desc;
	float Scale = 1.f;
	float Age = 0.f;
	float StopTime = -1.f;
	bool bStopping = false;
	FLinearColor Tint = FLinearColor::White;
	TArray<FEmitterState> Emitters;
	TArray<FMeshState> MeshLayers;
	TArray<FLightState> LightLayers;
	TArray<bool> DecalSpawned;
	TArray<bool> ShakeDone;
	TArray<FAfterimage> Ghosts;
	float NextGhostTime = 0.f;
	int32 GhostsMade = 0;
	TWeakObjectPtr<USkeletalMeshComponent> GhostSource;
	FRandomStream Random;
};

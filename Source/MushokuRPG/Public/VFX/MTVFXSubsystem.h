// Per-world bookkeeping for the runtime spell effects (AMTSpellVFX): the live-effect count behind the cap, the spell
// decals behind the decal cap, and the pool of finished actors reused by frequently spawned presets.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MTVFXSubsystem.generated.h"

class AMTSpellVFX;
class UDecalComponent;

UCLASS()
class MUSHOKURPG_API UMTVFXSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMTVFXSubsystem* Get(const UObject* WorldContext);

	/** Effects currently playing (pooled, idle actors are not counted). */
	int32 GetLiveEffects() const { return LiveEffects; }
	void NoteEffectStarted() { ++LiveEffects; }
	void NoteEffectEnded() { LiveEffects = FMath::Max(0, LiveEffects - 1); }

	/** An idle pooled actor that last played Preset (its layers are already built), or null. */
	AMTSpellVFX* TakeFromPool(FName Preset);
	/** Parks a finished actor for reuse; false when the pool is full (the caller destroys it). */
	bool ReturnToPool(AMTSpellVFX* Effect);
	int32 GetPooledCount() const { return Pool.Num(); }

	/**
	 * Tracks a spell decal. Past the decal cap the oldest fade out early: timed marks first, held (zone-length) marks only
	 * when nothing else is left.
	 */
	void RegisterDecal(UDecalComponent* Decal, bool bHeld);
	int32 GetLiveDecals() const;

	virtual void Deinitialize() override;

private:
	struct FLiveDecal
	{
		TWeakObjectPtr<UDecalComponent> Decal;
		bool bHeld = false;
	};

	UPROPERTY(Transient) TArray<TObjectPtr<AMTSpellVFX>> Pool;
	TArray<FLiveDecal> Decals;
	int32 LiveEffects = 0;
};

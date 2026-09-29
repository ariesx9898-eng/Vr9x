// Keeps every spell-effect asset (the effect meshes, materials, decal materials and the textures they sample) loaded
// for the whole game. The first effect of a session then no longer waits on a load, and a decal never draws before its
// texture is on the GPU: that drew the decal's whole box for a frame (a dark square under the first ground impact).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MTVFXPreloadSubsystem.generated.h"

UCLASS()
class MUSHOKURPG_API UMTVFXPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Draws every ground-decal material once, fully transparent, under Around for two seconds. The first draw of a
	 * decal material compiles its GPU pipeline, and until that finishes the engine draws the decal's whole box with
	 * the default decal material: a dark square under the first impact of a session. The player calls this on spawn.
	 */
	static void WarmUpDecals(AActor* Around);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Assets;
};

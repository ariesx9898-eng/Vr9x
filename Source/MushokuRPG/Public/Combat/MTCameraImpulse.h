// Camera feedback for impacts: tiered, distance-attenuated impulses and FOV kicks on the local player's camera,
// plus short hit-stop. Never constant: a new impulse replaces a weaker one instead of stacking without limit.
#pragma once

#include "CoreMinimal.h"

class AActor;

/** How hard an effect should hit the camera (Docs/Ability_Overhaul.md §1). */
enum class EMTCameraTier : uint8
{
	None,
	Minor,     // tiny impulse
	Heavy,     // short shake
	Ultimate   // larger shake + brief FOV kick
};

namespace MTCamera
{
	/** Parses "Minor" / "Heavy" / "Ultimate" (anything else = None). */
	MUSHOKURPG_API EMTCameraTier TierFromName(FName Name);

	/**
	 * Shakes the local player's camera for an event at Source. Scale multiplies the tier strength;
	 * the result fades with distance from the player (full within 8 m, gone beyond 60 m for Minor, 90 m for Ultimate).
	 */
	MUSHOKURPG_API void Impulse(const UObject* WorldContext, const FVector& Source, EMTCameraTier Tier, float Scale = 1.f);

	/** Freezes the given actors for Seconds (hit-stop). Only for very powerful close-range impacts. */
	MUSHOKURPG_API void HitStop(const TArray<AActor*>& Actors, float Seconds);
}

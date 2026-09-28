// Registry of announced attacks and live spells. Enemies publish telegraphs before
// important attacks; Demon Eye visualises them, AI uses them to dodge/counter, and
// Disturb Magic queries live spells near Orsted.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/MTTypes.h"
#include "MTTelegraphSubsystem.generated.h"

class AMTProjectile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnTelegraph, const FMTAttackTelegraph&, Telegraph);

UCLASS()
class MUSHOKURPG_API UMTTelegraphSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMTTelegraphSubsystem* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMTTelegraphSubsystem, STATGROUP_Tickables); }

	/** Announce an attack that will land at Telegraph.ImpactTime. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Combat")
	void PublishTelegraph(const FMTAttackTelegraph& Telegraph);

	/** Telegraphs whose impact is still in the future. */
	const TArray<FMTAttackTelegraph>& GetActiveTelegraphs() const { return Active; }

	/** Telegraphs threatening a location within Radius whose impact is within TimeHorizon. */
	TArray<FMTAttackTelegraph> GetThreatsTo(const FVector& Location, float Radius, float TimeHorizon, const AActor* IgnoreAttacker = nullptr) const;

	void RegisterSpell(AMTProjectile* Spell);
	void UnregisterSpell(AMTProjectile* Spell);
	/** Hostile magic projectiles within Radius of Location (Disturb Magic, AI dodges). */
	TArray<AMTProjectile*> GetSpellsNear(const FVector& Location, float Radius, const AActor* FriendlyTo = nullptr) const;

	UPROPERTY(BlueprintAssignable) FMTOnTelegraph OnTelegraphPublished;

private:
	TArray<FMTAttackTelegraph> Active;
	TArray<TWeakObjectPtr<AMTProjectile>> Spells;
};

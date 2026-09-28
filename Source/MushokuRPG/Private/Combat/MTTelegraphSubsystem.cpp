#include "Combat/MTTelegraphSubsystem.h"
#include "Combat/MTProjectile.h"
#include "Character/MTCharacterBase.h"
#include "Engine/World.h"

UMTTelegraphSubsystem* UMTTelegraphSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMTTelegraphSubsystem>() : nullptr;
}

void UMTTelegraphSubsystem::Tick(float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	Active.RemoveAll([Now](const FMTAttackTelegraph& T) { return T.ImpactTime < Now - 0.1f || !T.Attacker.IsValid(); });
	Spells.RemoveAll([](const TWeakObjectPtr<AMTProjectile>& P) { return !P.IsValid(); });
}

void UMTTelegraphSubsystem::PublishTelegraph(const FMTAttackTelegraph& Telegraph)
{
	Active.Add(Telegraph);
	OnTelegraphPublished.Broadcast(Telegraph);
}

TArray<FMTAttackTelegraph> UMTTelegraphSubsystem::GetThreatsTo(const FVector& Location, float Radius, float TimeHorizon, const AActor* IgnoreAttacker) const
{
	TArray<FMTAttackTelegraph> Result;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Result;
	}
	const float Now = World->GetTimeSeconds();
	for (const FMTAttackTelegraph& T : Active)
	{
		if (T.Attacker.Get() == IgnoreAttacker || T.ImpactTime < Now || T.ImpactTime - Now > TimeHorizon)
		{
			continue;
		}
		float Distance = FVector::Dist2D(T.ImpactLocation, Location);
		if (T.Length > 0.f)
		{
			// Line attack: distance to the segment.
			const FVector End = T.ImpactLocation + T.Direction.GetSafeNormal2D() * T.Length;
			Distance = FMath::PointDistToSegment(Location, T.ImpactLocation, End);
		}
		if (Distance <= T.Radius + Radius)
		{
			Result.Add(T);
		}
	}
	return Result;
}

void UMTTelegraphSubsystem::RegisterSpell(AMTProjectile* Spell)
{
	if (Spell)
	{
		Spells.AddUnique(Spell);
	}
}

void UMTTelegraphSubsystem::UnregisterSpell(AMTProjectile* Spell)
{
	Spells.Remove(Spell);
}

TArray<AMTProjectile*> UMTTelegraphSubsystem::GetSpellsNear(const FVector& Location, float Radius, const AActor* FriendlyTo) const
{
	TArray<AMTProjectile*> Result;
	const AMTCharacterBase* Friendly = Cast<AMTCharacterBase>(FriendlyTo);
	for (const TWeakObjectPtr<AMTProjectile>& Weak : Spells)
	{
		AMTProjectile* Spell = Weak.Get();
		if (!Spell || Spell->IsActorBeingDestroyed())
		{
			continue;
		}
		AActor* Caster = Spell->GetInstigatorActor();
		if (FriendlyTo && (Caster == FriendlyTo || (Friendly && !Friendly->IsHostileTo(Caster))))
		{
			continue;
		}
		if (FVector::DistSquared(Spell->GetActorLocation(), Location) <= Radius * Radius)
		{
			Result.Add(Spell);
		}
	}
	return Result;
}

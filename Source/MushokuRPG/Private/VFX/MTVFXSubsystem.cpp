#include "VFX/MTVFXSubsystem.h"
#include "VFX/MTSpellVFX.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 MaxLiveSpellDecals = 80;
	constexpr int32 MaxPooledEffects = 64;
	constexpr int32 MaxPooledPerPreset = 16;
	constexpr float EarlyDecalFade = 0.5f;
}

UMTVFXSubsystem* UMTVFXSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMTVFXSubsystem>() : nullptr;
}

AMTSpellVFX* UMTVFXSubsystem::TakeFromPool(FName Preset)
{
	for (int32 Index = Pool.Num() - 1; Index >= 0; --Index)
	{
		AMTSpellVFX* Candidate = Pool[Index].Get();
		if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed())
		{
			Pool.RemoveAtSwap(Index);
			continue;
		}
		if (Candidate->GetPresetName() == Preset)
		{
			Pool.RemoveAtSwap(Index);
			return Candidate;
		}
	}
	return nullptr;
}

bool UMTVFXSubsystem::ReturnToPool(AMTSpellVFX* Effect)
{
	if (!IsValid(Effect) || Effect->IsActorBeingDestroyed())
	{
		return false;
	}
	int32 SamePreset = 0;
	for (int32 Index = Pool.Num() - 1; Index >= 0; --Index)
	{
		const AMTSpellVFX* Entry = Pool[Index].Get();
		if (!IsValid(Entry))
		{
			Pool.RemoveAtSwap(Index);
			continue;
		}
		if (Entry == Effect)
		{
			return true;
		}
		if (Entry->GetPresetName() == Effect->GetPresetName())
		{
			++SamePreset;
		}
	}
	if (Pool.Num() >= MaxPooledEffects || SamePreset >= MaxPooledPerPreset)
	{
		return false;
	}
	Pool.Add(Effect);
	return true;
}

void UMTVFXSubsystem::RegisterDecal(UDecalComponent* Decal, bool bHeld)
{
	if (!Decal)
	{
		return;
	}
	for (int32 Index = Decals.Num() - 1; Index >= 0; --Index)
	{
		if (!Decals[Index].Decal.IsValid())
		{
			Decals.RemoveAt(Index);
		}
	}
	FLiveDecal& Added = Decals.AddDefaulted_GetRef();
	Added.Decal = Decal;
	Added.bHeld = bHeld;

	int32 Excess = Decals.Num() - MaxLiveSpellDecals;
	// Oldest timed marks first (scorches, craters, wet ground); zone-length marks only if nothing else is left.
	for (int32 Pass = 0; Pass < 2 && Excess > 0; ++Pass)
	{
		for (int32 Index = 0; Index < Decals.Num() - 1 && Excess > 0;)
		{
			if (Pass == 0 && Decals[Index].bHeld)
			{
				++Index;
				continue;
			}
			if (UDecalComponent* Oldest = Decals[Index].Decal.Get())
			{
				Oldest->SetFadeOut(0.f, EarlyDecalFade, false);
			}
			Decals.RemoveAt(Index);
			--Excess;
		}
	}
}

int32 UMTVFXSubsystem::GetLiveDecals() const
{
	int32 Count = 0;
	for (const FLiveDecal& Entry : Decals)
	{
		Count += Entry.Decal.IsValid() ? 1 : 0;
	}
	return Count;
}

void UMTVFXSubsystem::Deinitialize()
{
	Pool.Reset();
	Decals.Reset();
	LiveEffects = 0;
	Super::Deinitialize();
}

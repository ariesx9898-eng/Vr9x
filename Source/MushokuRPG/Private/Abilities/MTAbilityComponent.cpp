#include "Abilities/MTAbilityComponent.h"
#include "Abilities/MTAbility.h"
#include "Abilities/MTAbilityBehaviors.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameEvents.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Engine/World.h"

UMTAbilityComponent::UMTAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	for (FName& Slot : Slots)
	{
		Slot = NAME_None;
	}
}

AMTCharacterBase* UMTAbilityComponent::GetOwnerCharacter() const
{
	return Cast<AMTCharacterBase>(GetOwner());
}

FString UMTAbilityComponent::SlotToKeyLabel(EMTAbilitySlot Slot)
{
	switch (Slot)
	{
	case EMTAbilitySlot::Basic: return TEXT("LMB");
	case EMTAbilitySlot::Character1: return TEXT("1");
	case EMTAbilitySlot::Character2: return TEXT("2");
	case EMTAbilitySlot::Character3: return TEXT("3");
	case EMTAbilitySlot::Special: return TEXT("F");
	case EMTAbilitySlot::Awakening: return TEXT("G");
	case EMTAbilitySlot::ElementA1: return TEXT("4");
	case EMTAbilitySlot::ElementA2: return TEXT("5");
	case EMTAbilitySlot::ElementA3: return TEXT("6");
	case EMTAbilitySlot::ElementB1: return TEXT("7");
	case EMTAbilitySlot::ElementB2: return TEXT("8");
	case EMTAbilitySlot::ElementB3: return TEXT("9");
	case EMTAbilitySlot::RaceActive: return TEXT("R");
	case EMTAbilitySlot::RaceTransformation: return TEXT("T");
	default: return TEXT("");
	}
}

void UMTAbilityComponent::SetSlot(EMTAbilitySlot Slot, FName AbilityId)
{
	if (Slot == EMTAbilitySlot::MAX)
	{
		return;
	}
	Slots[(int32)Slot] = AbilityId;
	if (!AbilityId.IsNone())
	{
		FindOrCreateAbility(AbilityId);
	}
}

void UMTAbilityComponent::ClearAllSlots()
{
	CancelAll();
	for (FName& Slot : Slots)
	{
		Slot = NAME_None;
	}
}

FName UMTAbilityComponent::GetSlotAbilityId(EMTAbilitySlot Slot) const
{
	return Slot == EMTAbilitySlot::MAX ? NAME_None : ResolveOverride(Slots[(int32)Slot]);
}

UMTAbility* UMTAbilityComponent::FindOrCreateAbility(FName AbilityId)
{
	if (AbilityId.IsNone())
	{
		return nullptr;
	}
	if (TObjectPtr<UMTAbility>* Existing = Instances.Find(AbilityId))
	{
		return *Existing;
	}
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTAbilityData* Row = Registry ? Registry->FindAbility(AbilityId) : nullptr;
	if (!Row)
	{
		UE_LOG(LogMushoku, Warning, TEXT("Ability '%s' not found in data"), *AbilityId.ToString());
		return nullptr;
	}
	UMTAbility* Ability = NewObject<UMTAbility>(this, MTAbilityFactory::ClassForBehavior(Row->Behavior));
	Ability->Initialize(this, *Row);
	Instances.Add(AbilityId, Ability);
	return Ability;
}

UMTAbility* UMTAbilityComponent::GetActiveAbility() const
{
	return ActiveAbility.Get();
}

UMTAbility* UMTAbilityComponent::GetSlotAbility(EMTAbilitySlot Slot) const
{
	const FName Id = GetSlotAbilityId(Slot);
	const TObjectPtr<UMTAbility>* Found = Instances.Find(Id);
	return Found ? Found->Get() : nullptr;
}

const FMTAbilityData* UMTAbilityComponent::GetSlotData(EMTAbilitySlot Slot) const
{
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	return Registry ? Registry->FindAbility(GetSlotAbilityId(Slot)) : nullptr;
}

bool UMTAbilityComponent::IsCasting() const
{
	const UMTAbility* Active = ActiveAbility.Get();
	return Active && Active->IsBlockingOtherAbilities();
}

float UMTAbilityComponent::GetCooldownRemaining(FName AbilityId) const
{
	const float* End = CooldownEnd.Find(ResolveOverride(AbilityId));
	const UWorld* World = GetWorld();
	return (End && World) ? FMath::Max(0.f, *End - World->GetTimeSeconds()) : 0.f;
}

float UMTAbilityComponent::GetCooldownFraction(FName AbilityId) const
{
	const FName Id = ResolveOverride(AbilityId);
	const float* Duration = CooldownDuration.Find(Id);
	if (!Duration || *Duration <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(GetCooldownRemaining(Id) / *Duration, 0.f, 1.f);
}

void UMTAbilityComponent::StartCooldown(FName AbilityId, float Seconds)
{
	if (const UWorld* World = GetWorld())
	{
		CooldownEnd.Add(AbilityId, World->GetTimeSeconds() + Seconds);
		CooldownDuration.Add(AbilityId, Seconds);
	}
}

void UMTAbilityComponent::PressSlot(EMTAbilitySlot Slot)
{
	if (Slot == EMTAbilitySlot::MAX)
	{
		return;
	}
	UMTAbility* Ability = FindOrCreateAbility(GetSlotAbilityId(Slot));
	if (!Ability)
	{
		return;
	}
	if (!TryActivateInstance(Ability, Slot))
	{
		// Busy: remember the press briefly so fast inputs chain cleanly (ability queue).
		if (IsCasting() && GetCooldownRemaining(Ability->GetAbilityId()) <= InputBufferTime)
		{
			BufferedSlot = Slot;
			BufferedAt = GetWorld()->GetTimeSeconds();
			bBufferedReleased = false;
		}
	}
}

void UMTAbilityComponent::ReleaseSlot(EMTAbilitySlot Slot)
{
	if (Slot == BufferedSlot)
	{
		bBufferedReleased = true;
	}
	if (UMTAbility* Ability = GetSlotAbility(Slot))
	{
		Ability->InputReleased();
	}
}

bool UMTAbilityComponent::ActivateAbilityById(FName AbilityId)
{
	UMTAbility* Ability = FindOrCreateAbility(ResolveOverride(AbilityId));
	return Ability && TryActivateInstance(Ability, EMTAbilitySlot::MAX);
}

void UMTAbilityComponent::ReleaseAbilityById(FName AbilityId)
{
	if (TObjectPtr<UMTAbility>* Found = Instances.Find(ResolveOverride(AbilityId)))
	{
		(*Found)->InputReleased();
	}
}

bool UMTAbilityComponent::TryActivateInstance(UMTAbility* Ability, EMTAbilitySlot Slot)
{
	if (!Ability || Ability->IsActive())
	{
		return false;
	}
	if (IsCasting())
	{
		return false;
	}
	if (GetCooldownRemaining(Ability->GetAbilityId()) > 0.f)
	{
		return false;
	}
	// Progression gate (locked abilities show on the hotbar but cannot fire).
	if (const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		const AMTCharacterBase* Owner = GetOwnerCharacter();
		const bool bIsPlayer = Owner && Owner->IsPlayerControlled();
		if (bIsPlayer && !Progression->IsAbilityUnlocked(Ability->GetData()))
		{
			OnAbilityFailed.Broadcast(Ability->GetAbilityId(), NSLOCTEXT("MT", "Locked", "Locked"));
			return false;
		}
	}
	if (!Ability->TryActivate())
	{
		return false;
	}
	OnAbilityActivated.Broadcast(Ability->GetAbilityId(), Slot);
	return true;
}

void UMTAbilityComponent::NotifyAbilityStarted(UMTAbility* Ability)
{
	ActiveAbility = Ability;
	Running.AddUnique(Ability);
	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->OnAbilityUsed.Broadcast(Ability->GetAbilityId(), GetOwner());
	}
}

void UMTAbilityComponent::NotifyAbilityEnded(UMTAbility* Ability, bool bCancelled)
{
	if (ActiveAbility.Get() == Ability)
	{
		ActiveAbility.Reset();
	}
	Running.Remove(Ability);
	OnAbilityEnded.Broadcast(Ability ? Ability->GetAbilityId() : NAME_None, EMTAbilitySlot::MAX);
}

void UMTAbilityComponent::NotifyAbilityHit(FName AbilityId, float DamageDealt)
{
	AMTCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	// Awakening meter builds from landing abilities (not from spamming into the air).
	if (UMTAttributeComponent* Attr = Owner->GetAttributes())
	{
		Attr->AddAwakeningMeter(1.5f + DamageDealt * 0.02f);
	}
	if (!Owner->IsPlayerControlled())
	{
		return;
	}
	UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
	const FMTAbilityData* Row = Registry ? Registry->FindAbility(AbilityId) : nullptr;
	if (!Progression || !Row)
	{
		return;
	}
	// Mastery goes to the track that owns the ability.
	if (!Row->CharacterRequirement.IsNone())
	{
		Progression->AddMasteryXP(EMTMasteryTrack::Character, Row->CharacterRequirement, Row->MasteryXP);
	}
	else if (Row->bRequiresRace)
	{
		Progression->AddMasteryXP(EMTMasteryTrack::Race, FName(*StaticEnum<EMTRace>()->GetNameStringByValue((int64)Row->RaceRequirement)), Row->MasteryXP);
	}
	else if (Row->ElementRequirement != EMTElement::None)
	{
		Progression->AddMasteryXP(EMTMasteryTrack::Element, FName(*MTUtil::ElementToString(Row->ElementRequirement)), Row->MasteryXP);
	}
	if (Row->Element != EMTElement::None && Row->Element != EMTElement::Arcane)
	{
		Progression->AddMagicRankXP(Row->Element, Row->MasteryXP * 0.5f);
	}
}

void UMTAbilityComponent::CancelAll()
{
	TArray<TObjectPtr<UMTAbility>> Copy = Running;
	for (UMTAbility* Ability : Copy)
	{
		if (Ability && Ability->IsActive())
		{
			Ability->Cancel();
		}
	}
	ActiveAbility.Reset();
	BufferedSlot = EMTAbilitySlot::MAX;
}

void UMTAbilityComponent::PushAbilityOverrides(const TMap<FName, FName>& Overrides)
{
	for (const TPair<FName, FName>& Pair : Overrides)
	{
		ActiveOverrides.Add(Pair.Key, Pair.Value);
		FindOrCreateAbility(Pair.Value);
	}
}

void UMTAbilityComponent::PopAbilityOverrides(const TMap<FName, FName>& Overrides)
{
	for (const TPair<FName, FName>& Pair : Overrides)
	{
		ActiveOverrides.Remove(Pair.Key);
	}
}

FName UMTAbilityComponent::ResolveOverride(FName AbilityId) const
{
	const FName* Found = ActiveOverrides.Find(AbilityId);
	return Found ? *Found : AbilityId;
}

void UMTAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Tick copies: abilities may end (and leave Running) during their own tick.
	TArray<TObjectPtr<UMTAbility>> Copy = Running;
	for (UMTAbility* Ability : Copy)
	{
		if (Ability)
		{
			Ability->Tick(DeltaTime);
		}
	}

	if (BufferedSlot != EMTAbilitySlot::MAX)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - BufferedAt > InputBufferTime)
		{
			BufferedSlot = EMTAbilitySlot::MAX;
		}
		else if (!IsCasting())
		{
			const EMTAbilitySlot Slot = BufferedSlot;
			const bool bReleased = bBufferedReleased;
			BufferedSlot = EMTAbilitySlot::MAX;
			PressSlot(Slot);
			if (bReleased)
			{
				ReleaseSlot(Slot);
			}
		}
	}
}

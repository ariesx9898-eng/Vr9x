#include "Quests/MTInteractableComponent.h"

#include "Quests/MTQuestSubsystem.h"
#include "Core/MTGameEvents.h"
#include "Core/MTDataRegistry.h"
#include "Character/MTCharacterBase.h"
#include "AI/MTNPCScheduleComponent.h"
#include "UI/MTHUD.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MTInteraction"

namespace MTInteractPrivate
{
	static TArray<TWeakObjectPtr<UMTInteractableComponent>>& AllInteractables()
	{
		static TArray<TWeakObjectPtr<UMTInteractableComponent>> Instances;
		return Instances;
	}
}

// ---------------------------------------------------------------------------------------------
// UMTInteractableComponent
// ---------------------------------------------------------------------------------------------

UMTInteractableComponent::UMTInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PromptText = LOCTEXT("DefaultPrompt", "Interact");
}

void UMTInteractableComponent::BeginPlay()
{
	Super::BeginPlay();
	MTInteractPrivate::AllInteractables().AddUnique(this);
	RegisterWithQuests();
}

void UMTInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MTInteractPrivate::AllInteractables().RemoveAll([this](const TWeakObjectPtr<UMTInteractableComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == this;
	});
	UnregisterFromQuests();
	Super::EndPlay(EndPlayReason);
}

void UMTInteractableComponent::RegisterWithQuests()
{
	const FName Id = GetQuestRegistrationId();
	if (!bRegisterAsQuestActor || Id.IsNone() || !GetOwner())
	{
		return;
	}
	if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
	{
		Quests->RegisterQuestActor(Id, GetOwner());
		RegisteredId = Id;
	}
}

void UMTInteractableComponent::UnregisterFromQuests()
{
	if (RegisteredId.IsNone())
	{
		return;
	}
	if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
	{
		Quests->UnregisterQuestActor(RegisteredId, GetOwner());
	}
	RegisteredId = NAME_None;
}

FText UMTInteractableComponent::GetPromptText() const
{
	return PromptText.IsEmpty() ? LOCTEXT("DefaultPrompt", "Interact") : PromptText;
}

FText UMTInteractableComponent::GetDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return InteractionId.IsNone() ? FText::GetEmpty() : FText::FromName(InteractionId);
}

FVector UMTInteractableComponent::GetInteractionLocation() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}

bool UMTInteractableComponent::CanInteract(AMTCharacterBase* By) const
{
	if (!bEnabled || (bSingleUse && bUsed) || !IsValid(By) || !By->IsAlive())
	{
		return false;
	}
	const AActor* Owner = GetOwner();
	if (!Owner || Owner == By || Owner->IsHidden())
	{
		return false;
	}
	const float Allowed = InteractionRange + By->GetSimpleCollisionRadius() + 30.f;
	return FVector::DistSquared(By->GetActorLocation(), GetInteractionLocation()) <= FMath::Square(Allowed);
}

void UMTInteractableComponent::Interact(AMTCharacterBase* By)
{
	if (!CanInteract(By))
	{
		return;
	}
	if (!InteractionId.IsNone())
	{
		if (UMTGameEvents* Events = UMTGameEvents::Get(this))
		{
			Events->OnObjectInteracted.Broadcast(InteractionId);
		}
	}
	FinishInteraction(By);
}

void UMTInteractableComponent::FinishInteraction(AMTCharacterBase* By)
{
	bUsed = true;
	OnInteracted.Broadcast(By);
	OnInteractedNative.Broadcast(this, By);
	if (bSingleUse)
	{
		UnregisterFromQuests();
	}
}

void UMTInteractableComponent::ResetInteraction()
{
	bUsed = false;
	if (RegisteredId.IsNone())
	{
		RegisterWithQuests();
	}
}

UMTInteractableComponent* UMTInteractableComponent::FindBestInteractable(AMTCharacterBase* By, float ExtraRange)
{
	if (!IsValid(By))
	{
		return nullptr;
	}
	const FVector From = By->GetActorLocation();
	const FVector Facing = By->GetActorForwardVector();
	UMTInteractableComponent* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	TArray<TWeakObjectPtr<UMTInteractableComponent>>& All = MTInteractPrivate::AllInteractables();
	for (int32 Index = All.Num() - 1; Index >= 0; --Index)
	{
		UMTInteractableComponent* Candidate = All[Index].Get();
		if (!Candidate)
		{
			All.RemoveAtSwap(Index);
			continue;
		}
		if (Candidate->GetWorld() != By->GetWorld())
		{
			continue;
		}
		const float SavedRange = Candidate->InteractionRange;
		Candidate->InteractionRange = SavedRange + ExtraRange;
		const bool bUsable = Candidate->CanInteract(By);
		Candidate->InteractionRange = SavedRange;
		if (!bUsable)
		{
			continue;
		}
		const FVector ToTarget = Candidate->GetInteractionLocation() - From;
		// Prefer what the player faces: distance scaled up for things behind them.
		const float Facingness = FVector::DotProduct(ToTarget.GetSafeNormal2D(), Facing);
		const float Score = ToTarget.Size() * (Facingness > 0.f ? 1.f : 2.5f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------------------------
// UMTQuestGiverComponent
// ---------------------------------------------------------------------------------------------

UMTQuestGiverComponent::UMTQuestGiverComponent()
{
	PromptText = LOCTEXT("TalkPrompt", "Talk");
	InteractionRange = 250.f;
}

FText UMTQuestGiverComponent::GetDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	if (!NpcId.IsNone())
	{
		if (const UMTDataRegistry* Registry = UMTDataRegistry::Get(this))
		{
			if (const FMTCharacterData* Character = Registry->FindCharacter(NpcId))
			{
				if (!Character->DisplayName.IsEmpty())
				{
					return Character->DisplayName;
				}
			}
		}
		return FText::FromName(NpcId);
	}
	return Super::GetDisplayName();
}

void UMTQuestGiverComponent::Interact(AMTCharacterBase* By)
{
	if (!CanInteract(By))
	{
		return;
	}

	// Event bus first: TalkTo / Deliver objectives resolve before the dialog lists turn-ins.
	if (!NpcId.IsNone())
	{
		if (UMTGameEvents* Events = UMTGameEvents::Get(this))
		{
			Events->OnNPCInteracted.Broadcast(NpcId);
		}
	}

	if (AActor* Owner = GetOwner())
	{
		if (UMTNPCScheduleComponent* Schedule = Owner->FindComponentByClass<UMTNPCScheduleComponent>())
		{
			Schedule->PauseForConversation(By, ConversationPause);
		}
	}

	if (APlayerController* PC = Cast<APlayerController>(By->GetController()))
	{
		if (AMTHUD* HUD = Cast<AMTHUD>(PC->GetHUD()))
		{
			HUD->OpenNPCDialog(NpcId, GetDisplayName());
		}
	}

	IndicatorCacheTime = -1000.f;
	FinishInteraction(By);
}

bool UMTQuestGiverComponent::HasQuestIndicator(bool& bTurnIn) const
{
	bTurnIn = false;
	if (NpcId.IsNone() || !bEnabled)
	{
		return false;
	}

	// The HUD may ask every frame for every visible NPC: cache for half a second.
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now >= IndicatorCacheTime && Now - IndicatorCacheTime < 0.5f)
	{
		bTurnIn = bCachedTurnIn;
		return bCachedHasIndicator;
	}
	IndicatorCacheTime = Now;
	bCachedHasIndicator = false;
	bCachedTurnIn = false;

	if (const UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
	{
		if (Quests->GetTurnInQuestsForNPC(NpcId).Num() > 0 || Quests->HasPendingObjectiveForNPC(NpcId))
		{
			bCachedHasIndicator = true;
			bCachedTurnIn = true;
		}
		else if (Quests->GetAvailableQuestsForNPC(NpcId).Num() > 0)
		{
			bCachedHasIndicator = true;
		}
	}
	bTurnIn = bCachedTurnIn;
	return bCachedHasIndicator;
}

#undef LOCTEXT_NAMESPACE

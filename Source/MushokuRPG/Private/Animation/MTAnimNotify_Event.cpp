#include "Animation/MTAnimNotify_Event.h"
#include "Abilities/MTAbilityComponent.h"
#include "Character/MTCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

void UMTAnimNotify_Event::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	// Editor previews have no game character: nothing to forward to.
	AMTCharacterBase* Character = MeshComp ? Cast<AMTCharacterBase>(MeshComp->GetOwner()) : nullptr;
	if (Character && Character->GetAbilities())
	{
		Character->GetAbilities()->HandleAnimEvent(EventName, Animation);
	}
}

FString UMTAnimNotify_Event::GetNotifyName_Implementation() const
{
	return EventName.IsNone() ? FString(TEXT("MT Event")) : EventName.ToString();
}

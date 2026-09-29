// A named gameplay event on an animation frame ("Release", "Finale"). The ability playing the clip reacts on that
// exact frame, per character: Orsted's clips release earlier than Rudeus's. Content/Python/mt_setup_rudeus.py adds
// these from the "events" of the clip sidecar (SourceArt/Characters/<C>/<C>_Animated.anim.json), on a track named MT.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "MTAnimNotify_Event.generated.h"

UCLASS(meta = (DisplayName = "MT Event"))
class MUSHOKURPG_API UMTAnimNotify_Event : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** "Release" fires the casting ability on this frame; other names reach the ability's OnAnimEvent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku")
	FName EventName = TEXT("Release");

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

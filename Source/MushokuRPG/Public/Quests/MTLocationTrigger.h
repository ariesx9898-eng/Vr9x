// Named world location volume: player entry fires OnLocationReached (Reach objectives) and
// discovers the location; escorted NPCs entering it complete Escort objectives.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTLocationTrigger.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class MUSHOKURPG_API AMTLocationTrigger : public AActor
{
	GENERATED_BODY()

public:
	AMTLocationTrigger();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Location") FName LocationId;
	/** Calls UMTProgressionSubsystem::DiscoverLocation on first player entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Location") bool bDiscoverLocation = true;
	/** Minimum seconds between two OnLocationReached broadcasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Location") float RetriggerCooldown = 3.f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Location") void ReceivePlayerEntered(AActor* Player);

	UBoxComponent* GetBox() const { return Box; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION() void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	void HandleActorEntered(AActor* OtherActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Location") TObjectPtr<UBoxComponent> Box;

	float LastTriggerTime = -1000.f;
	bool bDiscovered = false;
};

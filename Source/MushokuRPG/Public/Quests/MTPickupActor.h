// Gatherable / dropped item. Collected on overlap (or via interact prompt), grants the item
// through progression and broadcasts OnItemCollected for Gather objectives.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTPickupActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMTInteractableComponent;
class UPrimitiveComponent;
class AMTCharacterBase;

UCLASS(Blueprintable)
class MUSHOKURPG_API AMTPickupActor : public AActor
{
	GENERATED_BODY()

public:
	AMTPickupActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Pickup") FName ItemId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Pickup", meta = (ClampMin = "1")) int32 Count = 1;
	/** Collect on overlap; when false the player must use the interact prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Pickup") bool bAutoPickupOnOverlap = true;
	/** Seconds until the pickup reappears; <= 0 destroys it after collection (enemy drops). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Pickup") float RespawnTime = 0.f;
	/** Short delay after spawning before it can be collected (drops pop out first). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Pickup") float PickupDelay = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Pickup") bool bNotifyOnPickup = true;

	/** Grants the item to the player. Returns false if unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Pickup") bool Collect(AMTCharacterBase* By);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Pickup") void Respawn();
	UFUNCTION(BlueprintPure, Category = "Mushoku|Pickup") bool IsAvailable() const { return bAvailable; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Pickup") void ReceiveCollected(AMTCharacterBase* By);
	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Pickup") void ReceiveRespawned();

	USphereComponent* GetCollision() const { return Collision; }
	UMTInteractableComponent* GetInteractable() const { return Interactable; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION() void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void HandleInteracted(AMTCharacterBase* By);

	void SetAvailable(bool bInAvailable);
	void TryCollectOverlapping();
	void RegisterForMarkers(bool bRegister);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Pickup") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Pickup") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Pickup") TObjectPtr<UMTInteractableComponent> Interactable;

	bool bAvailable = true;
	float AvailableFromTime = 0.f;
	FTimerHandle RespawnTimer;
};

// Rune-activation puzzle: the referenced rune actors must be activated (interacted with) in
// the configured order. A wrong rune resets the sequence. Success broadcasts OnPuzzleSolved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTPuzzleActor.generated.h"

class UMTInteractableComponent;
class AMTCharacterBase;

UCLASS(Blueprintable)
class MUSHOKURPG_API AMTPuzzleActor : public AActor
{
	GENERATED_BODY()

public:
	AMTPuzzleActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Puzzle") FName PuzzleId;
	/** Rune actors placed in the level. An interactable component is added at runtime if missing. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Mushoku|Puzzle") TArray<TObjectPtr<AActor>> Runes;
	/** Indices into Runes in the required activation order. Empty = 0,1,2... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Puzzle") TArray<int32> RequiredOrder;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Puzzle") FText RunePrompt;
	/** Toast shown the first time a rune is touched (optional riddle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Puzzle") FText HintText;
	/** Toggle light components found on rune actors to show their state (no assets required). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Puzzle") bool bToggleRuneLights = true;

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Puzzle") void ResetPuzzle();
	UFUNCTION(BlueprintPure, Category = "Mushoku|Puzzle") bool IsSolved() const { return bSolved; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Puzzle") int32 GetCurrentStep() const { return CurrentStep; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Puzzle") void ReceiveRuneActivated(AActor* Rune, int32 RuneIndex, bool bCorrect);
	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Puzzle") void ReceivePuzzleReset();
	UFUNCTION(BlueprintImplementableEvent, Category = "Mushoku|Puzzle") void ReceivePuzzleSolved();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleRuneInteracted(UMTInteractableComponent* Component, AMTCharacterBase* By);
	void SetRuneVisualState(int32 RuneIndex, bool bActive);
	void Solve();
	int32 GetExpectedRune() const;

	UPROPERTY(Transient) TArray<TObjectPtr<UMTInteractableComponent>> RuneComponents;
	TArray<bool> RuneActive;
	TArray<FDelegateHandle> RuneHandles;
	int32 CurrentStep = 0;
	bool bSolved = false;
	bool bHintShown = false;
};

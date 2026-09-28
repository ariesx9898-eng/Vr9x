#include "Quests/MTPuzzleActor.h"

#include "Quests/MTInteractableComponent.h"
#include "Quests/MTQuestSubsystem.h"
#include "Core/MTGameEvents.h"
#include "Character/MTCharacterBase.h"
#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"

#define LOCTEXT_NAMESPACE "MTPuzzle"

AMTPuzzleActor::AMTPuzzleActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RunePrompt = LOCTEXT("RunePrompt", "Touch the rune");
}

void AMTPuzzleActor::BeginPlay()
{
	Super::BeginPlay();

	RuneComponents.SetNum(Runes.Num());
	RuneActive.Init(false, Runes.Num());
	RuneHandles.SetNum(Runes.Num());

	for (int32 Index = 0; Index < Runes.Num(); ++Index)
	{
		AActor* Rune = Runes[Index];
		if (!IsValid(Rune))
		{
			UE_LOG(LogMushoku, Warning, TEXT("Puzzle %s: rune %d is not set."), *PuzzleId.ToString(), Index);
			continue;
		}
		UMTInteractableComponent* Interactable = Rune->FindComponentByClass<UMTInteractableComponent>();
		if (!Interactable)
		{
			Interactable = NewObject<UMTInteractableComponent>(Rune, TEXT("PuzzleRuneInteractable"));
			Interactable->bRegisterAsQuestActor = false;
			Interactable->RegisterComponent();
		}
		if (Interactable->PromptText.IsEmpty() || Interactable->PromptText.EqualTo(LOCTEXT("DefaultInteract", "Interact")))
		{
			Interactable->PromptText = RunePrompt;
		}
		if (Interactable->DisplayName.IsEmpty())
		{
			Interactable->DisplayName = FText::Format(LOCTEXT("RuneName", "Rune {0}"), FText::AsNumber(Index + 1));
		}
		RuneComponents[Index] = Interactable;
		RuneHandles[Index] = Interactable->OnInteractedNative.AddUObject(this, &AMTPuzzleActor::HandleRuneInteracted);
		SetRuneVisualState(Index, false);
	}

	if (RequiredOrder.Num() == 0)
	{
		for (int32 Index = 0; Index < Runes.Num(); ++Index)
		{
			RequiredOrder.Add(Index);
		}
	}

	if (!PuzzleId.IsNone())
	{
		if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
		{
			Quests->RegisterQuestActor(PuzzleId, this);
		}
	}
}

void AMTPuzzleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int32 Index = 0; Index < RuneComponents.Num(); ++Index)
	{
		if (UMTInteractableComponent* Interactable = RuneComponents[Index])
		{
			if (RuneHandles.IsValidIndex(Index))
			{
				Interactable->OnInteractedNative.Remove(RuneHandles[Index]);
			}
		}
	}
	if (!PuzzleId.IsNone())
	{
		if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
		{
			Quests->UnregisterQuestActor(PuzzleId, this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

int32 AMTPuzzleActor::GetExpectedRune() const
{
	return RequiredOrder.IsValidIndex(CurrentStep) ? RequiredOrder[CurrentStep] : INDEX_NONE;
}

void AMTPuzzleActor::HandleRuneInteracted(UMTInteractableComponent* Component, AMTCharacterBase* By)
{
	if (bSolved || !Component)
	{
		return;
	}
	const int32 RuneIndex = RuneComponents.IndexOfByKey(Component);
	if (RuneIndex == INDEX_NONE || (RuneActive.IsValidIndex(RuneIndex) && RuneActive[RuneIndex]))
	{
		return; // already lit this attempt
	}

	UMTGameEvents* Events = UMTGameEvents::Get(this);
	if (!bHintShown && !HintText.IsEmpty() && Events)
	{
		bHintShown = true;
		Events->Notify(HintText, FLinearColor(0.7f, 0.6f, 1.f));
	}

	const bool bCorrect = RuneIndex == GetExpectedRune();
	ReceiveRuneActivated(Runes.IsValidIndex(RuneIndex) ? Runes[RuneIndex].Get() : nullptr, RuneIndex, bCorrect);

	if (!bCorrect)
	{
		if (Events)
		{
			Events->Notify(LOCTEXT("WrongRune", "The runes flicker and fade..."), FLinearColor(0.75f, 0.55f, 0.95f));
		}
		ResetPuzzle();
		return;
	}

	RuneActive[RuneIndex] = true;
	SetRuneVisualState(RuneIndex, true);
	++CurrentStep;
	if (CurrentStep >= RequiredOrder.Num())
	{
		Solve();
	}
}

void AMTPuzzleActor::ResetPuzzle()
{
	if (bSolved)
	{
		return;
	}
	CurrentStep = 0;
	for (int32 Index = 0; Index < RuneActive.Num(); ++Index)
	{
		RuneActive[Index] = false;
		SetRuneVisualState(Index, false);
		if (RuneComponents.IsValidIndex(Index) && RuneComponents[Index])
		{
			RuneComponents[Index]->ResetInteraction();
		}
	}
	ReceivePuzzleReset();
}

void AMTPuzzleActor::Solve()
{
	bSolved = true;
	for (UMTInteractableComponent* Interactable : RuneComponents)
	{
		if (Interactable)
		{
			Interactable->bEnabled = false;
		}
	}
	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->Notify(LOCTEXT("Solved", "The seal breaks - the runes answer your mana."), FLinearColor(0.7f, 0.6f, 1.f));
		Events->OnPuzzleSolved.Broadcast(PuzzleId);
	}
	UE_LOG(LogMushoku, Log, TEXT("Puzzle solved: %s"), *PuzzleId.ToString());
	ReceivePuzzleSolved();
}

void AMTPuzzleActor::SetRuneVisualState(int32 RuneIndex, bool bActive)
{
	if (!bToggleRuneLights || !Runes.IsValidIndex(RuneIndex) || !IsValid(Runes[RuneIndex]))
	{
		return;
	}
	TArray<ULightComponent*> Lights;
	Runes[RuneIndex]->GetComponents<ULightComponent>(Lights);
	for (ULightComponent* Light : Lights)
	{
		Light->SetVisibility(bActive);
	}
}

#undef LOCTEXT_NAMESPACE

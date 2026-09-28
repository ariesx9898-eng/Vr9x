#include "Quests/MTPickupActor.h"

#include "Quests/MTInteractableComponent.h"
#include "Quests/MTQuestSubsystem.h"
#include "Core/MTGameEvents.h"
#include "Core/MTDataRegistry.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Character/MTCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "MTPickup"

AMTPickupActor::AMTPickupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(90.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetRelativeScale3D(FVector(0.25f));
	Mesh->SetCastShadow(false);
	// Engine basic shape as a visible fallback; Blueprint subclasses swap in the real mesh.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (FallbackMesh.Succeeded())
	{
		Mesh->SetStaticMesh(FallbackMesh.Object);
	}

	Interactable = CreateDefaultSubobject<UMTInteractableComponent>(TEXT("Interactable"));
	Interactable->PromptText = LOCTEXT("GatherPrompt", "Gather");
	Interactable->InteractionRange = 180.f;
	Interactable->bRegisterAsQuestActor = false; // the pickup registers itself under ItemId
}

void AMTPickupActor::BeginPlay()
{
	Super::BeginPlay();

	AvailableFromTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, PickupDelay);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AMTPickupActor::HandleOverlap);
	Interactable->OnInteracted.AddDynamic(this, &AMTPickupActor::HandleInteracted);
	Interactable->bEnabled = !bAutoPickupOnOverlap;
	if (Interactable->DisplayName.IsEmpty() && !ItemId.IsNone())
	{
		const UMTDataRegistry* Registry = UMTDataRegistry::Get(this);
		const FMTItemData* Item = Registry ? Registry->FindItem(ItemId) : nullptr;
		Interactable->DisplayName = (Item && !Item->DisplayName.IsEmpty()) ? Item->DisplayName : FText::FromName(ItemId);
	}

	SetAvailable(true);

	if (bAutoPickupOnOverlap)
	{
		// Spawned on top of the player (drops): initial overlaps may predate the binding.
		if (PickupDelay > 0.f)
		{
			FTimerHandle DelayHandle;
			GetWorldTimerManager().SetTimer(DelayHandle, this, &AMTPickupActor::TryCollectOverlapping, PickupDelay + 0.05f, false);
		}
		else
		{
			TryCollectOverlapping();
		}
	}
}

void AMTPickupActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RegisterForMarkers(false);
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void AMTPickupActor::RegisterForMarkers(bool bRegister)
{
	if (ItemId.IsNone())
	{
		return;
	}
	if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
	{
		if (bRegister)
		{
			Quests->RegisterQuestActor(ItemId, this);
		}
		else
		{
			Quests->UnregisterQuestActor(ItemId, this);
		}
	}
}

void AMTPickupActor::SetAvailable(bool bInAvailable)
{
	bAvailable = bInAvailable;
	SetActorHiddenInGame(!bInAvailable);
	Collision->SetCollisionEnabled(bInAvailable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Interactable->bEnabled = bInAvailable && !bAutoPickupOnOverlap;
	RegisterForMarkers(bInAvailable);
}

void AMTPickupActor::TryCollectOverlapping()
{
	if (!bAvailable || !bAutoPickupOnOverlap)
	{
		return;
	}
	TArray<AActor*> Overlapping;
	Collision->GetOverlappingActors(Overlapping, AMTCharacterBase::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		AMTCharacterBase* Character = Cast<AMTCharacterBase>(Actor);
		if (Character && Character->IsPlayerControlled() && Collect(Character))
		{
			return;
		}
	}
}

void AMTPickupActor::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bAutoPickupOnOverlap)
	{
		return;
	}
	AMTCharacterBase* Character = Cast<AMTCharacterBase>(OtherActor);
	if (Character && Character->IsPlayerControlled())
	{
		Collect(Character);
	}
}

void AMTPickupActor::HandleInteracted(AMTCharacterBase* By)
{
	Collect(By);
}

bool AMTPickupActor::Collect(AMTCharacterBase* By)
{
	if (!bAvailable || ItemId.IsNone() || Count <= 0)
	{
		return false;
	}
	if (By && !By->IsAlive())
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < AvailableFromTime)
	{
		return false;
	}

	if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Progression->AddItem(ItemId, Count);
	}
	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->OnItemCollected.Broadcast(ItemId, Count);
		if (bNotifyOnPickup)
		{
			Events->Notify(FText::Format(LOCTEXT("Collected", "+{0} {1}"), FText::AsNumber(Count), Interactable->GetDisplayName()),
				FLinearColor(0.75f, 0.92f, 0.7f));
		}
	}
	UE_LOG(LogMushoku, Verbose, TEXT("Pickup %s x%d collected by %s"), *ItemId.ToString(), Count, *GetNameSafe(By));

	SetAvailable(false);
	ReceiveCollected(By);

	if (RespawnTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMTPickupActor::Respawn, RespawnTime, false);
	}
	else
	{
		SetLifeSpan(0.1f); // let BP presentation run this frame, then go away
	}
	return true;
}

void AMTPickupActor::Respawn()
{
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	SetAvailable(true);
	Interactable->ResetInteraction();
	ReceiveRespawned();
	TryCollectOverlapping();
}

#undef LOCTEXT_NAMESPACE

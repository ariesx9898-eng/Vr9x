#include "Quests/MTLocationTrigger.h"

#include "Quests/MTQuestSubsystem.h"
#include "Core/MTGameEvents.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Character/MTCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

AMTLocationTrigger::AMTLocationTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->InitBoxExtent(FVector(400.f, 400.f, 250.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
	Box->ShapeColor = FColor(80, 200, 255);
	RootComponent = Box;
}

void AMTLocationTrigger::BeginPlay()
{
	Super::BeginPlay();

	Box->OnComponentBeginOverlap.AddDynamic(this, &AMTLocationTrigger::HandleOverlap);
	if (!LocationId.IsNone())
	{
		if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
		{
			Quests->RegisterQuestActor(LocationId, this);
		}
	}

	// The player may start inside the volume.
	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		HandleActorEntered(Actor);
	}
}

void AMTLocationTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!LocationId.IsNone())
	{
		if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
		{
			Quests->UnregisterQuestActor(LocationId, this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AMTLocationTrigger::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	HandleActorEntered(OtherActor);
}

void AMTLocationTrigger::HandleActorEntered(AActor* OtherActor)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || LocationId.IsNone())
	{
		return;
	}

	if (!Pawn->IsPlayerControlled())
	{
		// Escorted NPCs arriving at their destination.
		if (const AMTCharacterBase* Character = Cast<AMTCharacterBase>(OtherActor))
		{
			if (!Character->GameplayId.IsNone() && Character->IsAlive())
			{
				if (UMTQuestSubsystem* Quests = UMTQuestSubsystem::Get(this))
				{
					Quests->NotifyActorReachedLocation(Character->GameplayId, LocationId);
				}
			}
		}
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastTriggerTime < RetriggerCooldown)
	{
		return;
	}
	LastTriggerTime = Now;

	if (bDiscoverLocation && !bDiscovered)
	{
		bDiscovered = true;
		if (UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
		{
			Progression->DiscoverLocation(LocationId);
		}
	}
	if (UMTGameEvents* Events = UMTGameEvents::Get(this))
	{
		Events->OnLocationReached.Broadcast(LocationId);
	}
	UE_LOG(LogMushoku, Verbose, TEXT("Location reached: %s"), *LocationId.ToString());
	ReceivePlayerEntered(OtherActor);
}

#include "AI/MTNPCScheduleComponent.h"

#include "World/MTTimeOfDaySubsystem.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UMTNPCScheduleComponent::UMTNPCScheduleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

TArray<TWeakObjectPtr<UMTNPCScheduleComponent>>& UMTNPCScheduleComponent::AllSchedules()
{
	static TArray<TWeakObjectPtr<UMTNPCScheduleComponent>> Instances;
	return Instances;
}

void UMTNPCScheduleComponent::BeginPlay()
{
	Super::BeginPlay();

	AllSchedules().AddUnique(this);
	HomeLocation = GetOwner()->GetActorLocation();

	// Resolve every location tag once (GetAllActorsWithTag walks the whole level).
	for (const FMTScheduleEntry& Entry : Schedule)
	{
		if (!Entry.LocationTag.IsNone() && !TagActorCache.Contains(Entry.LocationTag))
		{
			CacheTag(Entry.LocationTag);
		}
	}

	if (UMTTimeOfDaySubsystem* TimeOfDay = UMTTimeOfDaySubsystem::Get(this))
	{
		TimeOfDay->OnHourChanged.AddUniqueDynamic(this, &UMTNPCScheduleComponent::HandleHourChanged);
		bHourDelegateBound = true;
	}

	// De-synchronise NPC ticks.
	SetComponentTickInterval(NearTickInterval + FMath::FRandRange(0.f, 0.1f));
	NextIdleVariationTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f, 6.f);

	UpdateLOD();
	LastHourSeen = FMath::FloorToInt(GetCurrentHour());
	RefreshSchedule(true);

	// Nobody should march across town at load: start in place unless the player is watching.
	if (bTravelling && SimLOD != EMTNPCSimLOD::Near)
	{
		SnapToDestination();
	}
}

void UMTNPCScheduleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AllSchedules().RemoveAll([this](const TWeakObjectPtr<UMTNPCScheduleComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == this;
	});
	if (bHourDelegateBound)
	{
		if (UMTTimeOfDaySubsystem* TimeOfDay = UMTTimeOfDaySubsystem::Get(this))
		{
			TimeOfDay->OnHourChanged.RemoveDynamic(this, &UMTNPCScheduleComponent::HandleHourChanged);
		}
		bHourDelegateBound = false;
	}
	Super::EndPlay(EndPlayReason);
}

void UMTNPCScheduleComponent::CacheTag(FName Tag)
{
	TArray<TWeakObjectPtr<AActor>>& Cached = TagActorCache.FindOrAdd(Tag);
	Cached.Reset();
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, Tag, Found);
	for (AActor* Actor : Found)
	{
		if (Actor && Actor != GetOwner())
		{
			Cached.Add(Actor);
		}
	}
}

float UMTNPCScheduleComponent::GetCurrentHour() const
{
	if (const UMTTimeOfDaySubsystem* TimeOfDay = UMTTimeOfDaySubsystem::Get(this))
	{
		return TimeOfDay->GetTimeOfDayHours();
	}
	return FallbackHour;
}

void UMTNPCScheduleComponent::HandleHourChanged(int32 Hour)
{
	LastHourSeen = Hour;
	RefreshSchedule(false);
}

int32 UMTNPCScheduleComponent::FindEntryForHour(float Hour) const
{
	Hour = FMath::Fmod(Hour, 24.f);
	if (Hour < 0.f)
	{
		Hour += 24.f;
	}
	for (int32 Index = 0; Index < Schedule.Num(); ++Index)
	{
		if (Schedule[Index].ContainsHour(Hour))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UMTNPCScheduleComponent::RefreshSchedule(bool bForce)
{
	const int32 Index = FindEntryForHour(GetCurrentHour());
	if (!bForce && Index == ActiveEntryIndex)
	{
		return;
	}
	EnterEntry(Index);
}

void UMTNPCScheduleComponent::EnterEntry(int32 EntryIndex)
{
	ActiveEntryIndex = EntryIndex;
	SetIndoorsHidden(false);
	TalkPartner = nullptr;
	CarryLeg = 0;
	bCarrying = false;

	if (!Schedule.IsValidIndex(EntryIndex))
	{
		// Free time: hang around home.
		CurrentLocationActor = nullptr;
		BeginTravel(HomeLocation);
		return;
	}

	const FMTScheduleEntry& Entry = Schedule[EntryIndex];
	if (!Entry.LocationTag.IsNone())
	{
		TArray<TWeakObjectPtr<AActor>>& Cached = TagActorCache.FindOrAdd(Entry.LocationTag);
		Cached.RemoveAll([](const TWeakObjectPtr<AActor>& Actor) { return !Actor.IsValid(); });
		if (Cached.Num() == 0)
		{
			CacheTag(Entry.LocationTag); // streamed in after BeginPlay (hourly at most)
		}
	}
	AActor* LocationActor = PickLocationActor(Entry, EntryIndex);
	CurrentLocationActor = LocationActor;
	BeginTravel(LocationActor ? ComputeDestination(LocationActor, Entry.Activity) : HomeLocation);
}

AActor* UMTNPCScheduleComponent::PickLocationActor(const FMTScheduleEntry& Entry, int32 EntryIndex, int32 Offset) const
{
	if (Entry.LocationTag.IsNone())
	{
		return nullptr;
	}
	const TArray<TWeakObjectPtr<AActor>>* Cached = TagActorCache.Find(Entry.LocationTag);
	if (!Cached || Cached->Num() == 0)
	{
		return nullptr;
	}
	// Stable per-NPC choice (each villager has "their" bench); Offset walks the list (carry legs).
	const uint32 Seed = GetTypeHash(GetOwner()->GetFName()) + static_cast<uint32>(EntryIndex) * 7919u + static_cast<uint32>(Offset);
	const uint32 Num = static_cast<uint32>(Cached->Num());
	for (uint32 Step = 0; Step < Num; ++Step)
	{
		if (AActor* Actor = (*Cached)[static_cast<int32>((Seed + Step) % Num)].Get())
		{
			return Actor;
		}
	}
	return nullptr;
}

FVector UMTNPCScheduleComponent::ComputeDestination(const AActor* LocationActor, EMTNPCActivity Activity) const
{
	if (!LocationActor)
	{
		return HomeLocation;
	}
	// NPCs sharing one spot (talk circles, market stalls) spread around it.
	const uint32 Hash = GetTypeHash(GetOwner()->GetFName());
	const float Angle = FMath::DegreesToRadians(static_cast<float>(Hash % 360u));
	const float Spread = Activity == EMTNPCActivity::Talk ? 110.f : (Activity == EMTNPCActivity::Wander ? 150.f : 30.f);
	return LocationActor->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Spread;
}

AAIController* UMTNPCScheduleComponent::GetAIController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
}

bool UMTNPCScheduleComponent::CanMove() const
{
	return SimLOD != EMTNPCSimLOD::Far && !bHiddenIndoors && GetAIController() != nullptr;
}

void UMTNPCScheduleComponent::SetActivity(EMTNPCActivity NewActivity)
{
	if (CurrentActivity != NewActivity)
	{
		CurrentActivity = NewActivity;
		OnActivityChanged.Broadcast(NewActivity);
	}
}

void UMTNPCScheduleComponent::BeginTravel(const FVector& InDestination)
{
	Destination = InDestination;
	bTravelling = true;
	TravelStartTime = GetWorld()->GetTimeSeconds();
	SetActivity(EMTNPCActivity::Wander); // walking between activities

	AActor* Owner = GetOwner();
	if (FVector::DistSquared2D(Owner->GetActorLocation(), Destination) <= FMath::Square(ArrivalRadius))
	{
		Arrive();
		return;
	}
	if (SimLOD == EMTNPCSimLOD::Far)
	{
		// Frozen at distance: teleport when nobody can see it, otherwise wait until unseen.
		if (!Owner->WasRecentlyRendered(0.5f))
		{
			SnapToDestination();
		}
		return;
	}
	if (AAIController* AIController = GetAIController())
	{
		AIController->MoveToLocation(Destination, ArrivalRadius * 0.5f);
	}
	else
	{
		SnapToDestination();
	}
}

void UMTNPCScheduleComponent::SnapToDestination()
{
	AActor* Owner = GetOwner();
	FVector Floor = Destination;
	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation NavLocation;
		if (NavSystem->ProjectPointToNavigation(Destination, NavLocation, FVector(200.f, 200.f, 400.f)))
		{
			Floor = NavLocation.Location;
		}
	}
	const ACharacter* Character = Cast<ACharacter>(Owner);
	const float HalfHeight = (Character && Character->GetCapsuleComponent()) ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	if (AAIController* AIController = GetAIController())
	{
		AIController->StopMovement();
	}
	Owner->SetActorLocation(Floor + FVector(0.f, 0.f, HalfHeight + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
	Arrive();
}

void UMTNPCScheduleComponent::Arrive()
{
	bTravelling = false;
	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const FMTScheduleEntry* Entry = Schedule.IsValidIndex(ActiveEntryIndex) ? &Schedule[ActiveEntryIndex] : nullptr;
	const EMTNPCActivity Activity = Entry ? Entry->Activity : DefaultActivity;

	// Stationary activities adopt the spot's facing (benches, altars, stalls, posts).
	if (const AActor* LocationActor = CurrentLocationActor.Get())
	{
		if (Activity != EMTNPCActivity::Wander && Activity != EMTNPCActivity::Talk && Activity != EMTNPCActivity::Carry)
		{
			GetOwner()->SetActorRotation(FRotator(0.f, LocationActor->GetActorRotation().Yaw, 0.f));
		}
	}

	if (Activity == EMTNPCActivity::Carry)
	{
		bCarrying = !bCarrying; // picked up / put down
		NextCarryToggleTime = Now + FMath::FRandRange(3.f, 5.f);
	}
	SetActivity(Activity);

	if (Entry && Entry->bIndoors && bHideWhenIndoors)
	{
		SetIndoorsHidden(true);
	}
	NextWanderTime = Now + FMath::FRandRange(6.f, 12.f);
	NextTalkSearchTime = Now;
}

void UMTNPCScheduleComponent::PauseForConversation(AActor* With, float Seconds)
{
	const UWorld* World = GetWorld();
	PausedUntil = (World ? World->GetTimeSeconds() : 0.f) + FMath::Max(0.5f, Seconds);
	if (AAIController* AIController = GetAIController())
	{
		AIController->StopMovement();
	}
	FaceActor(With);
}

void UMTNPCScheduleComponent::FaceActor(const AActor* Other)
{
	AActor* Owner = GetOwner();
	if (!Other || !Owner)
	{
		return;
	}
	FVector ToOther = Other->GetActorLocation() - Owner->GetActorLocation();
	ToOther.Z = 0.f;
	if (!ToOther.IsNearlyZero())
	{
		Owner->SetActorRotation(FRotator(0.f, ToOther.Rotation().Yaw, 0.f));
	}
}

void UMTNPCScheduleComponent::SetIndoorsHidden(bool bHidden)
{
	if (bHiddenIndoors == bHidden)
	{
		return;
	}
	bHiddenIndoors = bHidden;
	AActor* Owner = GetOwner();
	Owner->SetActorHiddenInGame(bHidden);
	Owner->SetActorEnableCollision(!bHidden);
	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			if (bHidden)
			{
				Movement->StopMovementImmediately();
			}
			// No collision -> no walking (would fall through the floor).
			Movement->SetComponentTickEnabled(!bHidden && SimLOD != EMTNPCSimLOD::Far);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// LOD
// ---------------------------------------------------------------------------------------------

void UMTNPCScheduleComponent::UpdateLOD()
{
	FVector ViewLocation;
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		ViewLocation = Camera->GetCameraLocation();
	}
	else if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		ViewLocation = Player->GetActorLocation();
	}
	else
	{
		return;
	}
	const float DistSq = FVector::DistSquared(ViewLocation, GetOwner()->GetActorLocation());
	const EMTNPCSimLOD NewLOD = DistSq < FMath::Square(NearDistance) ? EMTNPCSimLOD::Near
		: (DistSq < FMath::Square(FarDistance) ? EMTNPCSimLOD::Mid : EMTNPCSimLOD::Far);
	if (NewLOD != SimLOD || !bLODApplied)
	{
		ApplyLOD(NewLOD);
	}
}

void UMTNPCScheduleComponent::ApplyLOD(EMTNPCSimLOD NewLOD)
{
	const EMTNPCSimLOD OldLOD = SimLOD;
	SimLOD = NewLOD;
	bLODApplied = true;

	switch (NewLOD)
	{
	case EMTNPCSimLOD::Near: SetComponentTickInterval(NearTickInterval); break;
	case EMTNPCSimLOD::Mid:  SetComponentTickInterval(MidTickInterval); break;
	case EMTNPCSimLOD::Far:  SetComponentTickInterval(FarTickInterval); break;
	}

	const bool bFar = NewLOD == EMTNPCSimLOD::Far;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			Mesh->SetComponentTickEnabled(!bFar);
		}
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			if (bFar)
			{
				Movement->StopMovementImmediately();
			}
			Movement->SetComponentTickEnabled(!bFar && !bHiddenIndoors);
		}
	}

	if (bFar)
	{
		if (AAIController* AIController = GetAIController())
		{
			AIController->StopMovement();
		}
	}
	else if (OldLOD == EMTNPCSimLOD::Far && bTravelling && bLODApplied)
	{
		BeginTravel(Destination); // resume walking now that the player is closer
	}
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void UMTNPCScheduleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	UpdateLOD();

	// Hour polling backs up the OnHourChanged delegate (or replaces it without a TOD subsystem).
	const int32 Hour = FMath::FloorToInt(GetCurrentHour());
	if (Hour != LastHourSeen)
	{
		LastHourSeen = Hour;
		RefreshSchedule(false);
	}

	if (PausedUntil > 0.f)
	{
		if (Now < PausedUntil)
		{
			return;
		}
		PausedUntil = -1.f;
		if (bTravelling)
		{
			BeginTravel(Destination);
			return;
		}
	}

	if (bTravelling)
	{
		const float DistSq = FVector::DistSquared2D(Owner->GetActorLocation(), Destination);
		if (DistSq <= FMath::Square(ArrivalRadius))
		{
			Arrive();
			return;
		}
		if (SimLOD == EMTNPCSimLOD::Far)
		{
			if (!Owner->WasRecentlyRendered(0.5f))
			{
				SnapToDestination();
			}
			return;
		}
		AAIController* AIController = GetAIController();
		if (!AIController)
		{
			SnapToDestination();
			return;
		}
		if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			// Move ended (blocked, partial path): close enough counts; otherwise retry or snap unseen.
			if (DistSq <= FMath::Square(ArrivalRadius * 4.f))
			{
				Arrive();
			}
			else if (!Owner->WasRecentlyRendered(1.f) || Now - TravelStartTime > 60.f)
			{
				SnapToDestination();
			}
			else
			{
				AIController->MoveToLocation(Destination, ArrivalRadius * 0.5f);
			}
		}
		return;
	}

	TickActivity(Now);
}

void UMTNPCScheduleComponent::TickActivity(float Now)
{
	if (Now >= NextIdleVariationTime)
	{
		IdleVariation = FMath::RandRange(0, 3);
		NextIdleVariationTime = Now + FMath::FRandRange(6.f, 12.f);
	}
	if (SimLOD == EMTNPCSimLOD::Far || bHiddenIndoors)
	{
		return;
	}

	switch (CurrentActivity)
	{
	case EMTNPCActivity::Wander:
		if (Now >= NextWanderTime && CanMove())
		{
			NextWanderTime = Now + FMath::FRandRange(8.f, 14.f);
			if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
			{
				FNavLocation Point;
				if (NavSystem->GetRandomReachablePointInRadius(Destination, WanderRadius, Point))
				{
					GetAIController()->MoveToLocation(Point.Location, 50.f);
				}
			}
		}
		break;

	case EMTNPCActivity::Talk:
	{
		UMTNPCScheduleComponent* Partner = nullptr;
		if (AActor* PartnerActor = TalkPartner.Get())
		{
			Partner = PartnerActor->FindComponentByClass<UMTNPCScheduleComponent>();
			const bool bStillTalking = Partner && Partner->CurrentActivity == EMTNPCActivity::Talk && !Partner->bTravelling
				&& FVector::DistSquared(PartnerActor->GetActorLocation(), GetOwner()->GetActorLocation()) <= FMath::Square(TalkSearchRadius * 1.5f);
			if (!bStillTalking)
			{
				TalkPartner = nullptr;
				Partner = nullptr;
			}
		}
		if (!Partner && Now >= NextTalkSearchTime)
		{
			NextTalkSearchTime = Now + 3.f;
			float BestDistSq = FMath::Square(TalkSearchRadius);
			for (const TWeakObjectPtr<UMTNPCScheduleComponent>& Weak : AllSchedules())
			{
				UMTNPCScheduleComponent* Other = Weak.Get();
				if (!Other || Other == this || Other->GetWorld() != GetWorld() || Other->CurrentActivity != EMTNPCActivity::Talk || Other->bTravelling)
				{
					continue;
				}
				const float DistSq = FVector::DistSquared(Other->GetOwner()->GetActorLocation(), GetOwner()->GetActorLocation());
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Partner = Other;
				}
			}
			if (Partner)
			{
				TalkPartner = Partner->GetOwner();
				FaceActor(Partner->GetOwner());
				if (!Partner->TalkPartner.IsValid())
				{
					Partner->TalkPartner = GetOwner();
					Partner->FaceActor(GetOwner());
				}
			}
		}
		break;
	}

	case EMTNPCActivity::Carry:
		if (Now >= NextCarryToggleTime && CanMove() && Schedule.IsValidIndex(ActiveEntryIndex))
		{
			// Shuttle between the tagged spots (or back home), toggling the load on arrival.
			++CarryLeg;
			NextCarryToggleTime = TNumericLimits<float>::Max();
			const FMTScheduleEntry& Entry = Schedule[ActiveEntryIndex];
			AActor* Next = PickLocationActor(Entry, ActiveEntryIndex, CarryLeg);
			if (Next && Next == CurrentLocationActor.Get())
			{
				Next = nullptr; // single spot: alternate with home
			}
			CurrentLocationActor = Next;
			BeginTravel(Next ? ComputeDestination(Next, Entry.Activity) : HomeLocation);
		}
		break;

	default:
		break;
	}
}

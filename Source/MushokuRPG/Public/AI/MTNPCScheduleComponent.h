// Daily routine for town NPCs, driven by UMTTimeOfDaySubsystem, with distance-based
// simulation LOD (Near: full nav + anim, Mid: 1 s ticks, Far: frozen, snaps to destination).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MTNPCScheduleComponent.generated.h"

class AAIController;

UENUM(BlueprintType)
enum class EMTNPCActivity : uint8
{
	Sleep,
	Work,
	Farm,
	Carry,
	Talk,
	Sit,
	Eat,
	Wander,
	Pray,
	Guard
};

UENUM(BlueprintType)
enum class EMTNPCSimLOD : uint8
{
	Near,
	Mid,
	Far
};

USTRUCT(BlueprintType)
struct FMTScheduleEntry
{
	GENERATED_BODY()

	/** Hours in [0, 24). EndHour < StartHour wraps past midnight (22 -> 6). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") float StartHour = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") float EndHour = 18.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") EMTNPCActivity Activity = EMTNPCActivity::Work;
	/** Actors with this tag are the places for the activity (bench, field, shrine...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") FName LocationTag;
	/** The NPC goes inside (hidden while there when the component hides indoor NPCs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") bool bIndoors = false;

	bool ContainsHour(float Hour) const
	{
		if (FMath::IsNearlyEqual(StartHour, EndHour))
		{
			return true; // all day
		}
		return StartHour < EndHour ? (Hour >= StartHour && Hour < EndHour) : (Hour >= StartHour || Hour < EndHour);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnNPCActivitySignature, EMTNPCActivity, NewActivity);

UCLASS(ClassGroup = (Mushoku), Blueprintable, meta = (BlueprintSpawnableComponent))
class MUSHOKURPG_API UMTNPCScheduleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMTNPCScheduleComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule") TArray<FMTScheduleEntry> Schedule;
	/** Activity when no entry matches the hour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule") EMTNPCActivity DefaultActivity = EMTNPCActivity::Wander;
	/** Hour used when the time-of-day subsystem is missing (editor / test maps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule") float FallbackHour = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule|LOD") float NearDistance = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule|LOD") float FarDistance = 12000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule|LOD") float NearTickInterval = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule|LOD") float MidTickInterval = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule|LOD") float FarTickInterval = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule") float ArrivalRadius = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule") float WanderRadius = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule") float TalkSearchRadius = 700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Schedule") bool bHideWhenIndoors = true;

	// ---- State for the anim BP ----
	UPROPERTY(BlueprintReadOnly, Category = "Mushoku|Schedule") EMTNPCActivity CurrentActivity = EMTNPCActivity::Wander;
	UPROPERTY(BlueprintReadOnly, Category = "Mushoku|Schedule") bool bCarrying = false;
	UPROPERTY(BlueprintReadOnly, Category = "Mushoku|Schedule") bool bTravelling = false;
	/** Changes every few seconds so idles do not loop in sync. */
	UPROPERTY(BlueprintReadOnly, Category = "Mushoku|Schedule") int32 IdleVariation = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Mushoku|Schedule") EMTNPCSimLOD SimLOD = EMTNPCSimLOD::Near;
	UPROPERTY(BlueprintReadOnly, Category = "Mushoku|Schedule") int32 ActiveEntryIndex = INDEX_NONE;

	UPROPERTY(BlueprintAssignable) FMTOnNPCActivitySignature OnActivityChanged;

	/** Re-evaluates the schedule for the current hour (bForce restarts the current entry). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Schedule") void RefreshSchedule(bool bForce = false);
	/** Stops the routine and faces someone (talked to by the player). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Schedule") void PauseForConversation(AActor* With, float Seconds = 8.f);
	UFUNCTION(BlueprintPure, Category = "Mushoku|Schedule") AActor* GetTalkPartner() const { return TalkPartner.Get(); }
	UFUNCTION(BlueprintPure, Category = "Mushoku|Schedule") float GetCurrentHour() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|Schedule") bool IsIdleAtDestination() const { return !bTravelling && ActiveEntryIndex != INDEX_NONE; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION() void HandleHourChanged(int32 Hour);

	int32 FindEntryForHour(float Hour) const;
	void EnterEntry(int32 EntryIndex);
	void BeginTravel(const FVector& Destination);
	void Arrive();
	void UpdateLOD();
	void ApplyLOD(EMTNPCSimLOD NewLOD);
	void TickActivity(float Now);
	void SetActivity(EMTNPCActivity NewActivity);
	void SetIndoorsHidden(bool bHidden);
	void SnapToDestination();
	AActor* PickLocationActor(const FMTScheduleEntry& Entry, int32 EntryIndex, int32 Offset = 0) const;
	FVector ComputeDestination(const AActor* LocationActor, EMTNPCActivity Activity) const;
	void CacheTag(FName Tag);
	void FaceActor(const AActor* Other);
	AAIController* GetAIController() const;
	bool CanMove() const;

	TMap<FName, TArray<TWeakObjectPtr<AActor>>> TagActorCache;
	TWeakObjectPtr<AActor> CurrentLocationActor;
	TWeakObjectPtr<AActor> TalkPartner;
	FVector Destination = FVector::ZeroVector;
	FVector HomeLocation = FVector::ZeroVector;
	bool bHiddenIndoors = false;
	bool bLODApplied = false;
	bool bHourDelegateBound = false;
	float PausedUntil = -1.f;
	float NextWanderTime = 0.f;
	float NextIdleVariationTime = 0.f;
	float NextTalkSearchTime = 0.f;
	float NextCarryToggleTime = 0.f;
	float TravelStartTime = 0.f;
	int32 CarryLeg = 0;
	int32 LastHourSeen = -1;

	static TArray<TWeakObjectPtr<UMTNPCScheduleComponent>>& AllSchedules();
};

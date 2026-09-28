// World clock: in-game hours advanced by real time. Only ticks in game worlds (Game / PIE).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MTTimeOfDaySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnHourChanged, int32, Hour);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnClockNightChanged, bool, bIsNight);

/**
 * Owns the time of day for one world. Default: starts at 08:00, one full day lasts 48 real minutes.
 * Other systems (save game, quests, NPC schedules, AMTDayNightController) only read/write through this API.
 */
UCLASS()
class MUSHOKURPG_API UMTTimeOfDaySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Null-safe accessor. Returns nullptr in editor (non-game) worlds, where the subsystem is not created. */
	static UMTTimeOfDaySubsystem* Get(const UObject* ctx);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Time")
	float GetTimeOfDayHours() const { return TimeOfDayHours; }

	/** Jumps the clock (wrapped into [0,24)). Broadcasts OnHourChanged / OnNightChanged when they change. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Time")
	void SetTimeOfDayHours(float Hours);

	/** Night = before NightEndHour (05:30) or from NightStartHour (18:30). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Time")
	bool IsNight() const;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Time")
	float GetDayLengthMinutes() const { return DayLengthMinutes; }

	/** Real-time minutes for a full 24 h cycle (clamped to >= 1 minute). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Time")
	void SetDayLengthMinutes(float M);

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Time")
	void SetClockPaused(bool bPaused) { bClockPaused = bPaused; }

	UFUNCTION(BlueprintPure, Category = "Mushoku|Time")
	bool IsClockPaused() const { return bClockPaused; }

	/** Whole days elapsed since the subsystem started (wraps of the clock). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|Time")
	int32 GetDayCount() const { return DayCount; }

	UPROPERTY(BlueprintAssignable, Category = "Mushoku|Time")
	FMTOnHourChanged OnHourChanged;

	UPROPERTY(BlueprintAssignable, Category = "Mushoku|Time")
	FMTOnClockNightChanged OnNightChanged;

	static constexpr float DefaultStartHour = 8.f;
	static constexpr float DefaultDayLengthMinutes = 48.f;
	static constexpr float NightStartHour = 18.5f;
	static constexpr float NightEndHour = 5.5f;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void AdvanceHours(float DeltaHours);
	void BroadcastChanges(int32 PreviousHour, bool bPreviousNight);

	float TimeOfDayHours = DefaultStartHour;
	float DayLengthMinutes = DefaultDayLengthMinutes;
	int32 DayCount = 0;
	bool bClockPaused = false;
};

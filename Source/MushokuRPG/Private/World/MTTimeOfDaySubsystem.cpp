#include "World/MTTimeOfDaySubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Stats/Stats.h"

UMTTimeOfDaySubsystem* UMTTimeOfDaySubsystem::Get(const UObject* ctx)
{
	if (!ctx || !GEngine)
	{
		return nullptr;
	}
	const UWorld* World = GEngine->GetWorldFromContextObject(ctx, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UMTTimeOfDaySubsystem>() : nullptr;
}

bool UMTTimeOfDaySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Never created for editor, editor-preview or inactive worlds: the clock only runs with a game.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UMTTimeOfDaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TimeOfDayHours = DefaultStartHour;
	DayLengthMinutes = DefaultDayLengthMinutes;
	DayCount = 0;
}

TStatId UMTTimeOfDaySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMTTimeOfDaySubsystem, STATGROUP_Tickables);
}

void UMTTimeOfDaySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || bClockPaused || DeltaTime <= 0.f)
	{
		return;
	}
	const float SecondsPerDay = FMath::Max(DayLengthMinutes, 1.f) * 60.f;
	AdvanceHours(DeltaTime * (24.f / SecondsPerDay));
}

void UMTTimeOfDaySubsystem::AdvanceHours(float DeltaHours)
{
	const int32 PreviousHour = FMath::FloorToInt(TimeOfDayHours);
	const bool bPreviousNight = IsNight();
	float NewHours = TimeOfDayHours + DeltaHours;
	while (NewHours >= 24.f)
	{
		NewHours -= 24.f;
		++DayCount;
	}
	TimeOfDayHours = FMath::Clamp(NewHours, 0.f, 23.9999f);
	BroadcastChanges(PreviousHour, bPreviousNight);
}

void UMTTimeOfDaySubsystem::SetTimeOfDayHours(float Hours)
{
	const int32 PreviousHour = FMath::FloorToInt(TimeOfDayHours);
	const bool bPreviousNight = IsNight();
	float Wrapped = FMath::Fmod(Hours, 24.f);
	if (Wrapped < 0.f)
	{
		Wrapped += 24.f;
	}
	TimeOfDayHours = FMath::Clamp(Wrapped, 0.f, 23.9999f);
	BroadcastChanges(PreviousHour, bPreviousNight);
}

bool UMTTimeOfDaySubsystem::IsNight() const
{
	return TimeOfDayHours >= NightStartHour || TimeOfDayHours < NightEndHour;
}

void UMTTimeOfDaySubsystem::SetDayLengthMinutes(float M)
{
	DayLengthMinutes = FMath::Max(M, 1.f);
}

void UMTTimeOfDaySubsystem::BroadcastChanges(int32 PreviousHour, bool bPreviousNight)
{
	const int32 NewHour = FMath::FloorToInt(TimeOfDayHours);
	if (NewHour != PreviousHour)
	{
		OnHourChanged.Broadcast(NewHour);
	}
	const bool bNight = IsNight();
	if (bNight != bPreviousNight)
	{
		OnNightChanged.Broadcast(bNight);
	}
}

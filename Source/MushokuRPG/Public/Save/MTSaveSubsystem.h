// Save / load / autosave. Gathers the payload from UMTProgressionSubsystem plus any system bound to
// OnGatherSaveData (quests, world time), stores the player transform + map, migrates old versions and
// re-applies the build and settings after loading.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Save/MTSaveTypes.h"
#include "MTSaveSubsystem.generated.h"

class APawn;
class UMTProgressionSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnSaveSlotEvent, const FString&, SlotName, bool, bSuccess);

UCLASS()
class MUSHOKURPG_API UMTSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UMTSaveSubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Save") bool SaveGame(const FString& Slot = TEXT("Slot0"));
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Save") bool LoadGame(const FString& Slot = TEXT("Slot0"));
	UFUNCTION(BlueprintPure, Category = "Mushoku|Save") bool HasSave(const FString& Slot = TEXT("Slot0")) const;
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Save") void DeleteSave(const FString& Slot = TEXT("Slot0"));

	/**
	 * Applies the progression settings to the running game: FOV via APlayerCameraManager::SetFOV.
	 * Graphics quality (Scalability) is only applied when bIncludeGraphics (explicit settings-menu change),
	 * so loading a save never overrides the engine's auto-detected quality silently.
	 * Mouse sensitivity / invert Y / camera shake / toggle sprint are read live by the player code from
	 * UMTProgressionSubsystem::GetSettings().
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Save") void ApplySettings(bool bIncludeGraphics = false) const;

	/** Upgrades an older payload to MT_SAVE_VERSION. Returns true when anything changed. */
	static bool MigrateSave(FMTSaveData& Data);

	UFUNCTION(BlueprintPure, Category = "Mushoku|Save") FString GetCurrentSlot() const { return CurrentSlot; }

	/** Systems add their state to the payload (bind with AddUObject / AddRaw; native, not Blueprint). */
	TMulticastDelegate<void(FMTSaveData&)> OnGatherSaveData;
	/** Systems restore their state after a load (after progression has been restored). */
	TMulticastDelegate<void(const FMTSaveData&)> OnApplySaveData;

	UPROPERTY(BlueprintAssignable) FMTOnSaveSlotEvent OnSaved;
	UPROPERTY(BlueprintAssignable) FMTOnSaveSlotEvent OnLoaded;

	/** Seconds between autosaves (0 disables). Only runs in game worlds with a player pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Save") float AutosaveInterval = 120.f;
	/** Load CurrentSlot automatically the first time a player pawn exists (new game when there is no save). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Save") bool bAutoLoadOnStart = true;

private:
	bool HandleTick(float DeltaTime);
	UWorld* GetGameWorld() const;
	APawn* GetPlayerPawn() const;
	UMTProgressionSubsystem* GetProgression() const;
	void Notify(const FText& Message, const FLinearColor& Color) const;
	/** Applies build (+ pending transform after a load) to a newly seen player pawn. */
	void ApplyToPawn(APawn* Pawn);

	FTSTicker::FDelegateHandle TickerHandle;
	FString CurrentSlot = TEXT("Slot0");
	float AutosaveAccumulator = 0.f;
	bool bAutoLoadDone = false;
	bool bSaving = false;

	/** Pawn the build was last applied to (re-apply on respawn / possession change). */
	TWeakObjectPtr<APawn> LastAppliedPawn;

	/** Transform restored from the last load, applied once the pawn exists on the saved map. */
	bool bPendingTransform = false;
	FName PendingMapName;
	FVector PendingLocation = FVector::ZeroVector;
	FRotator PendingRotation = FRotator::ZeroRotator;
};

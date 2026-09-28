// Game flow for the LA PLACE front end: title screen at start, spawning at the chosen location, the pause menu (ESC),
// return to title and quit. Owns the SMTFrontEnd Slate widget on the game viewport.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "UI/LaPlace/SMTFrontEnd.h"
#include "MTFrontEndSubsystem.generated.h"

class APlayerController;

UCLASS()
class MUSHOKURPG_API UMTFrontEndSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UMTFrontEndSubsystem* Get(const UObject* WorldContext);

	/** Title screen: the pawn waits hidden until the player spawns from the world map. */
	void OpenTitle(APlayerController* PC);
	/** In-game pause menu (ESC). */
	void OpenPause(APlayerController* PC);
	/** Directly to a page (tests, tools). */
	void OpenPage(APlayerController* PC, EMTFrontPage Page);
	void Close();
	bool IsOpen() const { return Widget.IsValid(); }
	/** Places the player at a spawn location (Locations.json) and hands control back. */
	bool SpawnAt(FName LocationId);
	/** The player the front end acts on (set by the pages; tools that skip the menus set it directly). */
	void SetController(APlayerController* PC) { Controller = PC; }

	virtual void Deinitialize() override;

	/** Development: screenshots every front-end page and the in-game HUD, then quits (?UITour=1). */
	void StartTour(APlayerController* PC);

private:
	void Show(APlayerController* PC, EMTFrontPage Page, bool bHidePawn);
	void HandleSpawn(FName LocationId);
	void HandleResume();
	void HandleReturnToTitle();
	void HandleQuit();
	void SetPawnParked(bool bParked);

	void PlayTitleMusic(bool bPlay);
	bool TickTour(float DeltaTime);

	TWeakObjectPtr<APlayerController> Controller;
	TSharedPtr<SMTFrontEnd> Widget;
	bool bPawnParked = false;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Music;
	FTSTicker::FDelegateHandle TourHandle;
	float TourClock = 0.f;
	int32 TourStep = 0;
};

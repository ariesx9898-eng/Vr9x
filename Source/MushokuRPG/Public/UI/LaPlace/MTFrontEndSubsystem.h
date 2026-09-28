// Game flow for the LA PLACE front end: title screen at start, spawning at the chosen location, the pause menu (ESC),
// return to title and quit. Owns the SMTFrontEnd Slate widget on the game viewport.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Styling/SlateBrush.h"
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
	/** Admin / debug teleport: spawns there like the map's SPAWN button, closing the menus first if they are open. */
	void TeleportTo(FName LocationId);
	/** The player the front end acts on (set by the pages; tools that skip the menus set it directly). */
	void SetController(APlayerController* PC) { Controller = PC; }

	virtual void Deinitialize() override;

	/**
	 * Live preview of a spawn location for the map page: a scene capture drifting slowly along the location's
	 * PreviewCamera (Locations.json). None stops it. GetPreviewBrush is null until the first frames are captured.
	 */
	void SetPreviewLocation(FName LocationId);
	const FSlateBrush* GetPreviewBrush() const;
	/** Seconds since the current preview started (for the cross-fade from the painted art). */
	float GetPreviewAge() const;

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

	bool TickPreview(float DeltaTime);
	UPROPERTY(Transient) TObjectPtr<class ASceneCapture2D> PreviewCapture;
	UPROPERTY(Transient) TObjectPtr<class UTextureRenderTarget2D> PreviewTarget;
	TSharedPtr<FSlateBrush> PreviewBrush;
	FName PreviewId;
	FVector PreviewStart = FVector::ZeroVector;
	FRotator PreviewRotation = FRotator::ZeroRotator;
	double PreviewSince = 0.0;
	FTSTicker::FDelegateHandle PreviewTicker;
	float TourClock = 0.f;
	int32 TourStep = 0;
};

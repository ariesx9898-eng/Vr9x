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
struct FMTLocationData;

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
	/** Places the player at a spawn location (Locations.json) and hands control back. On a map that has none of the
	 *  world's places (the 2 km Fittoa test map) the place is this map's own player start. False when the current map
	 *  has no ground there (the player then stays where it is). */
	bool SpawnAt(FName LocationId);
	/** Admin / debug teleport: spawns there like the map's SPAWN button, closing the menus first if they are open.
	 *  False (and nothing moves) when the location is not part of the current map. */
	bool TeleportTo(FName LocationId);
	/** Standing position for a capsule of HalfHeight on the ground under Location (static geometry, searched from 200 m
	 *  above to 600 m below). False when the map has nothing there: a location from another world. */
	static bool FindSpawnGround(const UWorld* World, const FVector& Location, float HalfHeight, FVector& OutStandAt);
	/** True when this map's terrain covers Location. Locations.json holds LA PLACE world coordinates, which the 2 km
	 *  Fittoa test map (the editor's start-up map) does not contain. A map without a landscape counts as containing all. */
	static bool IsInThisMap(const UWorld* World, const FVector& Location);
	/** Whether any spawn point of Locations.json lies in this map (false on the Fittoa test map). */
	static bool HasWorldPlaces(const UWorld* World);
	/** Where a place is in this map: its own spot, or on a map with none of the world's places, the map's player start
	 *  (the Fittoa test map is Buena's surroundings in its own coordinates). False when neither applies. */
	static bool ResolvePlace(const UWorld* World, const FMTLocationData& Loc, FVector& OutSpot, float& OutYaw);
	/** The player the front end acts on (set by the pages; tools that skip the menus set it directly). */
	void SetController(APlayerController* PC) { Controller = PC; }

	virtual void Deinitialize() override;

	/**
	 * Live preview of a spawn location for the map page: a scene capture drifting slowly along the location's
	 * PreviewCamera (Locations.json). None stops it. GetPreviewBrush is null until the first frames are captured.
	 */
	void SetPreviewLocation(FName LocationId);
	const FSlateBrush* GetPreviewBrush() const;
	/** Seconds since the current preview started. */
	float GetPreviewAge() const;
	/** 0..1 cross-fade from the painted art to the live view. It starts once the place has streamed in around the
	 *  preview camera (World Partition), so the card never shows an empty or half-loaded world. */
	float GetPreviewFade() const;

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
	/** Stops loading the world around the preview camera (after the spawn, or when the menus close). */
	void StopPreviewStreaming();
	UPROPERTY(Transient) TObjectPtr<class ASceneCapture2D> PreviewCapture;
	/** Makes the preview camera a World Partition streaming source: the previewed town streams in (instead of only its
	 *  distant low-detail proxies), and is already loaded when the player spawns there. */
	UPROPERTY(Transient) TObjectPtr<class UWorldPartitionStreamingSourceComponent> PreviewStreaming;
	UPROPERTY(Transient) TObjectPtr<class UTextureRenderTarget2D> PreviewTarget;
	TSharedPtr<FSlateBrush> PreviewBrush;
	FName PreviewId;
	FVector PreviewStart = FVector::ZeroVector;
	FRotator PreviewRotation = FRotator::ZeroRotator;
	double PreviewSince = 0.0;
	/** Preview age at which the place counted as streamed in (negative: not yet). */
	float PreviewReadyAt = -1.f;
	float PreviewSettled = 0.f;
	FTSTicker::FDelegateHandle PreviewTicker;
	float TourClock = 0.f;
	int32 TourStep = 0;
};

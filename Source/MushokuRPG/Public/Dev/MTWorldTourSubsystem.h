// Development tool: photographs the world and quits (?WorldTour=1, usually with ?Menu=0). For every spawn location in
// Locations.json it takes the menu's preview view (PreviewCamera) and an on-foot view after spawning there; then a few
// aerial views over the landscape. Screenshots: Saved/Screenshots/WorldTour/<n>_<Name>.png. Locations outside the
// loaded landscape are skipped, so test maps get the aerial views only.
//   UnrealEditor <project> "/Game/Maps/L_LaPlace?Menu=0?WorldTour=1" -game -windowed -ResX=1920 -ResY=1080
// ?TourOnly=Buena,Roa limits the locations; ?TourAerial=0 skips the aerial views.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "MTWorldTourSubsystem.generated.h"

class ACameraActor;
class APlayerController;

UCLASS()
class MUSHOKURPG_API UMTWorldTourSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void Start(APlayerController* PC, const FString& Options);
	virtual void Deinitialize() override;

private:
	struct FShot
	{
		FString Name;
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FName SpawnAt;        // on-foot shot: spawn the pawn here and use its camera
		float Wait = 5.f;
	};

	bool Tick(float DeltaTime);
	void BuildShots(const FString& Options);
	bool IsWorldReady() const;

	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<ACameraActor> Camera;
	TArray<FShot> Shots;
	int32 Index = 0;
	float Clock = 0.f;
	float ReadyClock = 0.f;
	bool bCaptured = false;
	FTSTicker::FDelegateHandle Handle;
};

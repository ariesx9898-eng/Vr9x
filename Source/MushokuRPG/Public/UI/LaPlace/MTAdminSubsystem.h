// Admin panel: pressing 1 and 0 together opens an "ADMIN" popup that asks for the admin code. The right code unlocks
// everything in the game (characters, elements, races, max level / mastery / ranks, spins, gold, items, every location)
// and opens a panel to heal, fill awakening, toggle god mode / infinite mana / no cooldowns, switch character, set the
// time of day, smite nearby enemies and teleport to any location. Unlocked for the rest of the session.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MTAdminSubsystem.generated.h"

class APlayerController;
class SMTAdminPanel;

UCLASS()
class MUSHOKURPG_API UMTAdminSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UMTAdminSubsystem* Get(const UObject* WorldContext);
	virtual void Deinitialize() override;

	/** Opens the admin popup (the code prompt, or the panel once unlocked); closes it when open. */
	void Toggle(APlayerController* PC);
	void Close();
	bool IsOpen() const { return Widget.IsValid(); }
	bool IsUnlocked() const { return bUnlocked; }

	/** Checks the admin code (case-insensitive); the right one grants everything and unlocks the panel. */
	bool TryUnlock(const FString& Code);
	/** Runs a panel action (see SMTAdminPanel) and returns the line of feedback to show. */
	FString DoAction(FName Action, FName Param);
	bool IsToggleOn(FName Toggle) const;
	APlayerController* GetController() const { return Controller.Get(); }

private:
	void GrantEverything();
	/** Re-applies the toggles to the current pawn (after a respawn or character switch). */
	bool TickCheats(float DeltaTime);

	TWeakObjectPtr<APlayerController> Controller;
	TSharedPtr<SMTAdminPanel> Widget;
	FTSTicker::FDelegateHandle TickHandle;
	bool bUnlocked = false;
	bool bGodMode = false;
	bool bInfiniteMana = false;
	bool bNoCooldowns = false;
	bool bPausedByPanel = false;
};

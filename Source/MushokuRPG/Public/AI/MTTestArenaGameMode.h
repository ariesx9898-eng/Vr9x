// Rudeus-vs-Orsted AI test arena: spawns the opposite lineage as an AI opponent in front of
// the player start and logs a combat report (hits, disruptions, dodges) every 30 s.
#pragma once

#include "CoreMinimal.h"
#include "Core/MTGameMode.h"
#include "Core/MTTypes.h"
#include "MTTestArenaGameMode.generated.h"

class AMTCharacterBase;
class AMTEnemyCharacter;

/** Per-combatant counters for the arena report. */
struct FMTArenaStats
{
	int32 HitsLanded = 0;
	float DamageDealt = 0.f;
	int32 DisturbCasts = 0;
	int32 SpellsDisrupted = 0;
	int32 Dodges = 0;
	int32 HitsEvaded = 0;
	int32 HitsBlocked = 0;
};

UCLASS()
class MUSHOKURPG_API AMTTestArenaGameMode : public AMTGameMode
{
	GENERATED_BODY()

public:
	AMTTestArenaGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") FName OrstedOpponentId = TEXT("Arena_Orsted");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") FName RudeusOpponentId = TEXT("Arena_Rudeus");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") FName OrstedLineageId = TEXT("Orsted");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") FName RudeusLineageId = TEXT("Rudeus");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") float OpponentDistance = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") float SpawnDelay = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") float ReportInterval = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") bool bAutoRespawnOpponent = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") float OpponentRespawnDelay = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|Arena") TSubclassOf<AMTEnemyCharacter> OpponentClass;

	/** Respawns/heals the player at the start, respawns the opponent and clears the report. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Arena") void ResetArena();
	/** Player takes the other lineage; the opponent becomes the one the player had. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Arena") void SwapCharacters();
	/** Logs the combat report to LogMushoku (also runs every ReportInterval seconds). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Arena") void PrintCombatReport();
	UFUNCTION(BlueprintPure, Category = "Mushoku|Arena") AMTEnemyCharacter* GetOpponent() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SpawnOpponent();
	void DestroyOpponent();
	FName GetPlayerLineage() const;
	FTransform GetArenaStartTransform() const;
	AMTCharacterBase* GetPlayerCharacter() const;
	void BindPlayer(AMTCharacterBase* Player);
	void PollCombatants();
	void HandleOpponentPerfectDefense(AMTEnemyCharacter* Enemy, FName Kind);

	UFUNCTION() void HandlePlayerDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result);
	UFUNCTION() void HandleOpponentDamaged(const FMTDamageSpec& Spec, const FMTDamageResult& Result);
	UFUNCTION() void HandleOpponentDeath(AActor* Killer);
	UFUNCTION() void HandleAbilityUsed(FName AbilityId, AActor* User);

	FMTArenaStats PlayerStats;
	FMTArenaStats OpponentStats;
	TWeakObjectPtr<AMTEnemyCharacter> Opponent;
	TWeakObjectPtr<AMTCharacterBase> BoundPlayer;
	FName PlayerLineageOverride;
	FDelegateHandle PerfectDefenseHandle;
	bool bPlayerWasDodging = false;
	bool bOpponentWasDodging = false;
	int32 OpponentsDefeated = 0;
	int32 PlayerDeaths = 0;
	bool bPlayerWasAlive = true;
	float ArenaStartTime = 0.f;
	FTimerHandle SpawnTimer;
	FTimerHandle ReportTimer;
	FTimerHandle PollTimer;
};

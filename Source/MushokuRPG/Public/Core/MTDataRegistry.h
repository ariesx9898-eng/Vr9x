// Loads all gameplay data from Content/Data/*.json (and optional DataTables) at startup.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/MTDataTypes.h"
#include "MTDataRegistry.generated.h"

UCLASS()
class MUSHOKURPG_API UMTDataRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UMTDataRegistry* Get(const UObject* WorldContext);

	/** Re-reads every JSON file (hot reload while tuning). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Data")
	void Reload();

	const FMTAbilityData* FindAbility(FName Id) const { return Abilities.Find(Id); }
	const FMTCharacterData* FindCharacter(FName Id) const { return Characters.Find(Id); }
	const FMTElementData* FindElement(EMTElement Element) const { return Elements.Find(Element); }
	const FMTRaceData* FindRace(EMTRace Race) const { return Races.Find(Race); }
	const FMTQuestData* FindQuest(FName Id) const { return Quests.Find(Id); }
	const FMTEnemyData* FindEnemy(FName Id) const { return Enemies.Find(Id); }
	const FMTItemData* FindItem(FName Id) const { return Items.Find(Id); }
	const FMTLocationData* FindLocation(FName Id) const { return Locations.Find(Id); }
	const FMTRollConfig* FindRollConfig(EMTRollCategory Category) const { return RollConfigs.Find(Category); }
	/** Animation set of a character lineage (Content/Data/AnimSets.json), keyed by CharacterID. */
	const FMTAnimSetData* FindAnimSet(FName CharacterId) const { return AnimSets.Find(CharacterId); }

	const TMap<FName, FMTAbilityData>& GetAbilities() const { return Abilities; }
	const TMap<FName, FMTCharacterData>& GetCharacters() const { return Characters; }
	const TMap<EMTElement, FMTElementData>& GetElements() const { return Elements; }
	const TMap<EMTRace, FMTRaceData>& GetRaces() const { return Races; }
	const TMap<FName, FMTQuestData>& GetQuests() const { return Quests; }
	const TMap<FName, FMTEnemyData>& GetEnemies() const { return Enemies; }
	const TMap<FName, FMTItemData>& GetItems() const { return Items; }
	const TMap<FName, FMTLocationData>& GetLocations() const { return Locations; }
	const TMap<FName, FMTAnimSetData>& GetAnimSets() const { return AnimSets; }

	/** Directory the JSON files are read from. */
	static FString GetDataDirectory();

private:
	template <typename RowType>
	bool LoadArray(const FString& FileName, TArray<RowType>& OutRows) const;

	void ValidateData() const;

	TMap<FName, FMTAbilityData> Abilities;
	TMap<FName, FMTCharacterData> Characters;
	TMap<EMTElement, FMTElementData> Elements;
	TMap<EMTRace, FMTRaceData> Races;
	TMap<FName, FMTQuestData> Quests;
	TMap<FName, FMTEnemyData> Enemies;
	TMap<FName, FMTItemData> Items;
	TMap<FName, FMTLocationData> Locations;
	TMap<EMTRollCategory, FMTRollConfig> RollConfigs;
	TMap<FName, FMTAnimSetData> AnimSets;
};

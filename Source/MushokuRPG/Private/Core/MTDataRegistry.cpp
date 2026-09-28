#include "Core/MTDataRegistry.h"
#include "Core/MTTypes.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"

namespace
{
	/** Rebuilds a container so parent tags are filled (JSON import only sets the explicit list). */
	FGameplayTagContainer SanitizeTags(const FGameplayTagContainer& In)
	{
		FGameplayTagContainer Out;
		for (const FGameplayTag& Tag : In)
		{
			const FGameplayTag Valid = FGameplayTag::RequestGameplayTag(Tag.GetTagName(), false);
			if (Valid.IsValid())
			{
				Out.AddTag(Valid);
			}
			else
			{
				UE_LOG(LogMushoku, Warning, TEXT("Data: unknown gameplay tag '%s'"), *Tag.GetTagName().ToString());
			}
		}
		return Out;
	}
}

UMTDataRegistry* UMTDataRegistry::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMTDataRegistry>() : nullptr;
}

void UMTDataRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reload();
}

FString UMTDataRegistry::GetDataDirectory()
{
	return FPaths::ProjectContentDir() / TEXT("Data");
}

template <typename RowType>
bool UMTDataRegistry::LoadArray(const FString& FileName, TArray<RowType>& OutRows) const
{
	const FString Path = GetDataDirectory() / FileName;
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogMushoku, Warning, TEXT("Data: missing %s"), *Path);
		return false;
	}
	if (!FJsonObjectConverter::JsonArrayStringToUStruct<RowType>(Json, &OutRows, 0, 0))
	{
		UE_LOG(LogMushoku, Error, TEXT("Data: failed to parse %s"), *Path);
		return false;
	}
	UE_LOG(LogMushoku, Log, TEXT("Data: loaded %d rows from %s"), OutRows.Num(), *FileName);
	return true;
}

void UMTDataRegistry::Reload()
{
	Abilities.Reset();
	Characters.Reset();
	Elements.Reset();
	Races.Reset();
	Quests.Reset();
	Enemies.Reset();
	Items.Reset();
	Locations.Reset();
	RollConfigs.Reset();

	TArray<FMTAbilityData> AbilityRows;
	if (LoadArray(TEXT("Abilities.json"), AbilityRows))
	{
		for (FMTAbilityData& Row : AbilityRows)
		{
			Row.ActivationTags = SanitizeTags(Row.ActivationTags);
			Abilities.Add(Row.AbilityID, Row);
		}
	}

	TArray<FMTCharacterData> CharacterRows;
	if (LoadArray(TEXT("Characters.json"), CharacterRows))
	{
		for (const FMTCharacterData& Row : CharacterRows) { Characters.Add(Row.CharacterID, Row); }
	}

	TArray<FMTElementData> ElementRows;
	if (LoadArray(TEXT("Elements.json"), ElementRows))
	{
		for (const FMTElementData& Row : ElementRows) { Elements.Add(Row.Element, Row); }
	}

	TArray<FMTRaceData> RaceRows;
	if (LoadArray(TEXT("Races.json"), RaceRows))
	{
		for (const FMTRaceData& Row : RaceRows) { Races.Add(Row.Race, Row); }
	}

	TArray<FMTQuestData> QuestRows;
	if (LoadArray(TEXT("Quests.json"), QuestRows))
	{
		for (const FMTQuestData& Row : QuestRows) { Quests.Add(Row.QuestID, Row); }
	}

	TArray<FMTEnemyData> EnemyRows;
	if (LoadArray(TEXT("Enemies.json"), EnemyRows))
	{
		for (FMTEnemyData& Row : EnemyRows)
		{
			Row.Tags = SanitizeTags(Row.Tags);
			Enemies.Add(Row.EnemyID, Row);
		}
	}

	TArray<FMTItemData> ItemRows;
	if (LoadArray(TEXT("Items.json"), ItemRows))
	{
		for (const FMTItemData& Row : ItemRows) { Items.Add(Row.ItemID, Row); }
	}

	TArray<FMTLocationData> LocationRows;
	if (LoadArray(TEXT("Locations.json"), LocationRows))
	{
		for (const FMTLocationData& Row : LocationRows) { Locations.Add(Row.LocationID, Row); }
	}

	TArray<FMTRollConfig> RollRows;
	if (LoadArray(TEXT("RollConfigs.json"), RollRows))
	{
		for (const FMTRollConfig& Row : RollRows) { RollConfigs.Add(Row.Category, Row); }
	}

	ValidateData();
}

void UMTDataRegistry::ValidateData() const
{
	int32 Problems = 0;
	auto CheckAbility = [&](FName Id, const TCHAR* Context)
	{
		if (!Id.IsNone() && !Abilities.Contains(Id))
		{
			UE_LOG(LogMushoku, Error, TEXT("Data: %s references missing ability '%s'"), Context, *Id.ToString());
			++Problems;
		}
	};

	for (const TPair<FName, FMTCharacterData>& Pair : Characters)
	{
		const FMTCharacterData& C = Pair.Value;
		CheckAbility(C.BasicAbility, *C.CharacterID.ToString());
		CheckAbility(C.SpecialAbility, *C.CharacterID.ToString());
		CheckAbility(C.AwakeningAbility, *C.CharacterID.ToString());
		for (FName Id : C.Abilities) { CheckAbility(Id, *C.CharacterID.ToString()); }
	}
	for (const TPair<EMTElement, FMTElementData>& Pair : Elements)
	{
		if (Pair.Value.Abilities.Num() != 3)
		{
			UE_LOG(LogMushoku, Error, TEXT("Data: element %s must have exactly 3 abilities (has %d)"),
				*MTUtil::ElementToString(Pair.Key), Pair.Value.Abilities.Num());
			++Problems;
		}
		for (FName Id : Pair.Value.Abilities) { CheckAbility(Id, TEXT("Element")); }
	}
	for (const TPair<EMTRace, FMTRaceData>& Pair : Races)
	{
		CheckAbility(Pair.Value.ActiveAbility, *Pair.Value.RaceID.ToString());
		CheckAbility(Pair.Value.TransformationAbility, *Pair.Value.RaceID.ToString());
	}
	for (const TPair<FName, FMTAbilityData>& Pair : Abilities)
	{
		for (const FMTSequenceStep& Step : Pair.Value.Sequence) { CheckAbility(Step.AbilityId, *Pair.Key.ToString()); }
		for (const TPair<FName, FName>& Override : Pair.Value.AbilityOverrides) { CheckAbility(Override.Value, *Pair.Key.ToString()); }
	}
	for (const TPair<FName, FMTEnemyData>& Pair : Enemies)
	{
		for (const FMTEnemyAttack& Attack : Pair.Value.Attacks) { CheckAbility(Attack.AbilityId, *Pair.Key.ToString()); }
	}

	if (Problems == 0)
	{
		UE_LOG(LogMushoku, Log, TEXT("Data: validation passed (%d abilities, %d characters, %d elements, %d races, %d quests, %d enemies)"),
			Abilities.Num(), Characters.Num(), Elements.Num(), Races.Num(), Quests.Num(), Enemies.Num());
	}
}

#include "World/MTRegionSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Core/MTTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "World/MTWeatherActor.h"

namespace MTRegion
{
	FLinearColor ReadColor(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FLinearColor& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Obj->TryGetArrayField(Field, Values) && Values->Num() >= 3)
		{
			return FLinearColor((float)(*Values)[0]->AsNumber(), (float)(*Values)[1]->AsNumber(), (float)(*Values)[2]->AsNumber(), 1.f);
		}
		return Default;
	}

	float ReadNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Default)
	{
		double Value = Default;
		return Obj.IsValid() && Obj->TryGetNumberField(Field, Value) ? (float)Value : Default;
	}
}

void FMTRegionAtmosphere::BlendToward(const FMTRegionAtmosphere& Target, float Alpha)
{
	FogDensityScale = FMath::Lerp(FogDensityScale, Target.FogDensityScale, Alpha);
	FogTint = FMath::Lerp(FogTint, Target.FogTint, Alpha);
	Saturation = FMath::Lerp(Saturation, Target.Saturation, Alpha);
	Contrast = FMath::Lerp(Contrast, Target.Contrast, Alpha);
	Gain = FMath::Lerp(Gain, Target.Gain, Alpha);
	Wind = FMath::Lerp(Wind, Target.Wind, Alpha);
	Snow = FMath::Lerp(Snow, Target.Snow, Alpha);
	Ash = FMath::Lerp(Ash, Target.Ash, Alpha);
	Embers = FMath::Lerp(Embers, Target.Embers, Alpha);
	Dust = FMath::Lerp(Dust, Target.Dust, Alpha);
	Pollen = FMath::Lerp(Pollen, Target.Pollen, Alpha);
	Fireflies = FMath::Lerp(Fireflies, Target.Fireflies, Alpha);
	Mist = FMath::Lerp(Mist, Target.Mist, Alpha);
}

UMTRegionSubsystem* UMTRegionSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext || !GEngine)
	{
		return nullptr;
	}
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UMTRegionSubsystem>() : nullptr;
}

bool UMTRegionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UMTRegionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMTRegionSubsystem, STATGROUP_Tickables);
}

bool UMTRegionSubsystem::LoadData()
{
	const FString Dir = FPaths::ProjectContentDir() / TEXT("Data");

	// World rectangle and region names from the world generator's World.json.
	FString Text;
	TSharedPtr<FJsonObject> World;
	if (!FFileHelper::LoadFileToString(Text, *(Dir / TEXT("World.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), World) || !World.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* Landscape = nullptr;
	if (!World->TryGetObjectField(TEXT("Landscape"), Landscape))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Loc = (*Landscape)->GetObjectField(TEXT("LocationCm"));
	const double Quad = (*Landscape)->GetNumberField(TEXT("QuadSizeCm"));
	MapMin = FVector2D(Loc->GetNumberField(TEXT("X")), Loc->GetNumberField(TEXT("Y")));
	MapSize = FVector2D(((*Landscape)->GetNumberField(TEXT("VerticesX")) - 1) * Quad, ((*Landscape)->GetNumberField(TEXT("VerticesY")) - 1) * Quad);
	for (const TSharedPtr<FJsonValue>& Value : World->GetArrayField(TEXT("Regions")))
	{
		const TSharedPtr<FJsonObject> Row = Value->AsObject();
		FMTRegionInfo& Info = Regions.FindOrAdd((int32)Row->GetNumberField(TEXT("Id")));
		Info.Name = FText::FromString(Row->GetStringField(TEXT("Name")));
		FString Continent;
		Row->TryGetStringField(TEXT("Continent"), Continent);
		Info.Continent = FText::FromString(Continent.IsEmpty() ? FString() : Continent + TEXT(" Continent"));
	}

	// Atmosphere per region.
	TSharedPtr<FJsonObject> Atmos;
	if (FFileHelper::LoadFileToString(Text, *(Dir / TEXT("RegionAtmosphere.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Atmos) && Atmos.IsValid())
	{
		SnowAboveCm = MTRegion::ReadNumber(Atmos, TEXT("SnowAboveM"), 600.f) * 100.f;
		for (const TSharedPtr<FJsonValue>& Value : Atmos->GetArrayField(TEXT("Regions")))
		{
			const TSharedPtr<FJsonObject> Row = Value->AsObject();
			FMTRegionInfo& Info = Regions.FindOrAdd((int32)Row->GetNumberField(TEXT("Id")));
			FMTRegionAtmosphere& A = Info.Atmosphere;
			Row->TryGetBoolField(TEXT("Banner"), Info.bBanner);
			A.FogDensityScale = MTRegion::ReadNumber(Row, TEXT("FogDensity"), 1.f);
			A.FogTint = MTRegion::ReadColor(Row, TEXT("FogTint"), FLinearColor::White);
			A.Saturation = MTRegion::ReadNumber(Row, TEXT("Saturation"), 1.f);
			A.Contrast = MTRegion::ReadNumber(Row, TEXT("Contrast"), 1.f);
			A.Gain = MTRegion::ReadColor(Row, TEXT("Gain"), FLinearColor::White);
			A.Wind = MTRegion::ReadNumber(Row, TEXT("Wind"), 1.f);
			const TSharedPtr<FJsonObject>* Kinds = nullptr;
			if (Row->TryGetObjectField(TEXT("Weather"), Kinds))
			{
				A.Snow = MTRegion::ReadNumber(*Kinds, TEXT("Snow"), 0.f);
				A.Ash = MTRegion::ReadNumber(*Kinds, TEXT("Ash"), 0.f);
				A.Embers = MTRegion::ReadNumber(*Kinds, TEXT("Embers"), 0.f);
				A.Dust = MTRegion::ReadNumber(*Kinds, TEXT("Dust"), 0.f);
				A.Pollen = MTRegion::ReadNumber(*Kinds, TEXT("Pollen"), 0.f);
				A.Fireflies = MTRegion::ReadNumber(*Kinds, TEXT("Fireflies"), 0.f);
				A.Mist = MTRegion::ReadNumber(*Kinds, TEXT("Mist"), 0.f);
			}
		}
	}

	// Region ids, one byte per pixel over the landscape rectangle.
	TArray<uint8> Png;
	if (!FFileHelper::LoadFileToArray(Png, *(Dir / TEXT("WorldRegions.png"))))
	{
		return false;
	}
	IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = Images.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Png.GetData(), Png.Num()) || !Wrapper->GetRaw(ERGBFormat::Gray, 8, RegionMap))
	{
		return false;
	}
	MapWidth = (int32)Wrapper->GetWidth();
	MapHeight = (int32)Wrapper->GetHeight();
	return RegionMap.Num() == MapWidth * MapHeight && MapWidth > 0;
}

void UMTRegionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bLoaded = LoadData();
	UE_LOG(LogMushoku, Log, TEXT("[Regions] %s (%d regions, map %d x %d)"), bLoaded ? TEXT("active") : TEXT("inactive: no generated world data"),
		Regions.Num(), MapWidth, MapHeight);
}

int32 UMTRegionSubsystem::GetRegionAt(const FVector& Location) const
{
	if (!bLoaded)
	{
		return 0;
	}
	const double U = (Location.X - MapMin.X) / MapSize.X;
	const double V = (Location.Y - MapMin.Y) / MapSize.Y;
	if (U < 0.0 || V < 0.0 || U >= 1.0 || V >= 1.0)
	{
		return 0;
	}
	const int32 X = FMath::Clamp((int32)(U * MapWidth), 0, MapWidth - 1);
	const int32 Y = FMath::Clamp((int32)(V * MapHeight), 0, MapHeight - 1);
	return RegionMap[Y * MapWidth + X];
}

FVector UMTRegionSubsystem::ViewLocation(bool& bOutValid) const
{
	bOutValid = false;
	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return FVector::ZeroVector;
	}
	bOutValid = true;
	if (const APawn* Pawn = PC->GetPawn())
	{
		return Pawn->GetActorLocation();
	}
	return PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
}

void UMTRegionSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!bLoaded || !World)
	{
		return;
	}
	bool bValid = false;
	const FVector Location = ViewLocation(bValid);
	if (!bValid)
	{
		return;
	}

	// Region changes settle for a moment first so crossing a border back and forth does not flicker.
	const int32 Id = GetRegionAt(Location);
	if (Id != CurrentRegion)
	{
		CandidateTime = (Id == CandidateRegion) ? CandidateTime + DeltaTime : 0.f;
		CandidateRegion = Id;
		if (CandidateTime > 0.6f || CurrentRegion == INDEX_NONE)
		{
			CurrentRegion = Id;
			BannerRegion = Id;
			BannerSince = World->GetTimeSeconds();
		}
	}
	else
	{
		CandidateRegion = INDEX_NONE;
	}

	FMTRegionAtmosphere Target;
	if (const FMTRegionInfo* Info = Regions.Find(CurrentRegion))
	{
		Target = Info->Atmosphere;
	}
	Target.Snow = FMath::Max(Target.Snow, FMath::Clamp((float)(Location.Z - SnowAboveCm) / 20000.f, 0.f, 1.f));
	if (bFirstBlend)
	{
		Blended = Target;
		bFirstBlend = false;
	}
	else
	{
		Blended.BlendToward(Target, 1.f - FMath::Exp(-DeltaTime / 3.f));
	}

	if (!Weather)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Weather = World->SpawnActor<AMTWeatherActor>(Params);
	}
	if (Weather)
	{
		Weather->ApplyAtmosphere(Blended, DeltaTime);
	}
}

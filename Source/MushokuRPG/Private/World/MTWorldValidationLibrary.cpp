#include "World/MTWorldValidationLibrary.h"

#include "World/MTPlacementValidator.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "LandscapeProxy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace MTValidation
{
	static const FName TagBuilding(TEXT("MTBuilding"));
	static const FName TagSpawner(TEXT("MTSpawner"));
	static const FName TagQuest(TEXT("MTQuest"));
	static const FName TagSpawnPoint(TEXT("MTSpawnPoint"));
	static const FName TagAllowFloating(TEXT("MTAllowFloating"));

	static UWorld* ResolveWorld(const UObject* Ctx)
	{
		return (Ctx && GEngine) ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
	}

	static FString NameOf(const AActor* Actor)
	{
		return Actor ? Actor->GetActorNameOrLabel() : FString(TEXT("<none>"));
	}

	static FMTValidationIssue MakeIssue(EMTValidationSeverity Severity, FName Category, const FString& Message,
		AActor* A, AActor* B, const FVector& Location)
	{
		FMTValidationIssue Issue;
		Issue.Severity = Severity;
		Issue.Category = Category;
		Issue.Message = Message;
		Issue.ActorA = A;
		Issue.ActorB = B;
		Issue.ActorAName = NameOf(A);
		Issue.ActorBName = B ? NameOf(B) : FString();
		Issue.Location = Location;
		return Issue;
	}

	static bool IsLandscape(const AActor* Actor) { return Actor && Actor->IsA<ALandscapeProxy>(); }

	/** Landscape height under XY (downward trace, non-landscape hits skipped). */
	static bool LandscapeZ(UWorld* World, const FVector& Location, float& OutZ)
	{
		TArray<const AActor*> Ignore;
		return UMTPlacementValidator::TraceGroundZ(World, FVector2D(Location.X, Location.Y), OutZ, Ignore,
			float(Location.Z) + 100000.f, float(Location.Z) - 100000.f);
	}

	struct FMeshEntry
	{
		const UStaticMesh* Mesh = nullptr;
		FTransform Transform;
		AActor* Owner = nullptr;
		int32 InstanceIndex = INDEX_NONE;
	};

	static FIntVector CellOf(const FVector& L, double CellSize)
	{
		return FIntVector(FMath::FloorToInt(L.X / CellSize), FMath::FloorToInt(L.Y / CellSize), FMath::FloorToInt(L.Z / CellSize));
	}

	static bool SameTransform(const FTransform& A, const FTransform& B)
	{
		if (FVector::DistSquared(A.GetLocation(), B.GetLocation()) > 1.0) // 1 cm
		{
			return false;
		}
		if (FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())) > 1.0) // 1 degree
		{
			return false;
		}
		const FVector SA = A.GetScale3D();
		const FVector SB = B.GetScale3D();
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const double Ref = FMath::Max(FMath::Abs(SA[Axis]), UE_KINDA_SMALL_NUMBER);
			if (FMath::Abs(SA[Axis] - SB[Axis]) / Ref > 0.01) // 1 %
			{
				return false;
			}
		}
		return true;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindDuplicateMeshesInWorld(UWorld* World)
{
	TArray<FMTValidationIssue> Issues;
	if (!World)
	{
		return Issues;
	}
	using namespace MTValidation;
	TArray<FMeshEntry> Entries;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		TArray<UStaticMeshComponent*> Comps;
		Actor->GetComponents<UStaticMeshComponent>(Comps);
		for (UStaticMeshComponent* Comp : Comps)
		{
			const UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
			if (!Mesh || !Comp->IsVisible())
			{
				continue;
			}
			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Comp))
			{
				const int32 Count = ISM->GetInstanceCount();
				for (int32 i = 0; i < Count; ++i)
				{
					FTransform T;
					if (ISM->GetInstanceTransform(i, T, /*bWorldSpace*/ true))
					{
						Entries.Add({ Mesh, T, Actor, i });
					}
				}
			}
			else
			{
				Entries.Add({ Mesh, Comp->GetComponentTransform(), Actor, INDEX_NONE });
			}
		}
	}

	// Spatial hash (2 cm cells, 27-neighbourhood) keyed by mesh: O(n) for typical worlds.
	const double CellSize = 2.0;
	TMap<const UStaticMesh*, TMap<FIntVector, TArray<int32>>> Hash;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FMeshEntry& E = Entries[i];
		const FIntVector C = CellOf(E.Transform.GetLocation(), CellSize);
		TMap<FIntVector, TArray<int32>>& MeshCells = Hash.FindOrAdd(E.Mesh);
		for (int32 dz = -1; dz <= 1; ++dz)
		{
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				for (int32 dx = -1; dx <= 1; ++dx)
				{
					const TArray<int32>* Cell = MeshCells.Find(C + FIntVector(dx, dy, dz));
					if (!Cell)
					{
						continue;
					}
					for (const int32 j : *Cell)
					{
						const FMeshEntry& O = Entries[j];
						if (SameTransform(E.Transform, O.Transform))
						{
							const FString Msg = FString::Printf(TEXT("Duplicate mesh %s at the same transform (z-fighting): %s%s vs %s%s"),
								*E.Mesh->GetName(), *NameOf(E.Owner),
								E.InstanceIndex != INDEX_NONE ? *FString::Printf(TEXT(" [instance %d]"), E.InstanceIndex) : TEXT(""),
								*NameOf(O.Owner),
								O.InstanceIndex != INDEX_NONE ? *FString::Printf(TEXT(" [instance %d]"), O.InstanceIndex) : TEXT(""));
							Issues.Add(MakeIssue(EMTValidationSeverity::Error, TEXT("DuplicateMesh"), Msg, E.Owner, O.Owner,
								E.Transform.GetLocation()));
						}
					}
				}
			}
		}
		MeshCells.FindOrAdd(C).Add(i);
	}
	return Issues;
}

TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindCoplanarOverlapsInWorld(UWorld* World)
{
	TArray<FMTValidationIssue> Issues;
	if (!World)
	{
		return Issues;
	}
	using namespace MTValidation;
	struct FFace
	{
		int32 Axis = 0;
		double Plane = 0.0;
		FBox Box;
		bool bThin = false;
		UPrimitiveComponent* Comp = nullptr;
		AActor* Owner = nullptr;
	};
	TArray<FFace> Faces;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || IsLandscape(Actor))
		{
			continue;
		}
		TArray<UStaticMeshComponent*> Comps;
		Actor->GetComponents<UStaticMeshComponent>(Comps);
		for (UStaticMeshComponent* Comp : Comps)
		{
			if (!Comp || !Comp->GetStaticMesh() || !Comp->IsVisible() || Comp->IsA<UInstancedStaticMeshComponent>())
			{
				continue;
			}
			const FBox Box = Comp->Bounds.GetBox();
			const FVector Ext = Box.GetExtent();
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const int32 U = (Axis + 1) % 3;
				const int32 V = (Axis + 2) % 3;
				if (Ext[U] < 5.0 || Ext[V] < 5.0)
				{
					continue; // not a "large flat face" along this axis
				}
				if (Ext[Axis] * 2.0 < 2.0)
				{
					Faces.Add({ Axis, Box.GetCenter()[Axis], Box, true, Comp, Actor });
				}
				else
				{
					Faces.Add({ Axis, Box.Min[Axis], Box, false, Comp, Actor });
					Faces.Add({ Axis, Box.Max[Axis], Box, false, Comp, Actor });
				}
			}
		}
	}

	// Bucket by (axis, plane rounded to 1 cm); compare neighbouring buckets.
	TMap<FIntVector, TArray<int32>> Buckets; // X = axis, Y = plane cm
	for (int32 i = 0; i < Faces.Num(); ++i)
	{
		Buckets.FindOrAdd(FIntVector(Faces[i].Axis, FMath::RoundToInt(Faces[i].Plane), 0)).Add(i);
	}
	TSet<TPair<const UPrimitiveComponent*, const UPrimitiveComponent*>> Reported;
	for (int32 i = 0; i < Faces.Num(); ++i)
	{
		const FFace& A = Faces[i];
		if (!A.bThin)
		{
			continue; // at least one side must be a thin plane (decal-like card, road plane, water plane)
		}
		const int32 Key = FMath::RoundToInt(A.Plane);
		for (int32 dk = -1; dk <= 1; ++dk)
		{
			const TArray<int32>* Bucket = Buckets.Find(FIntVector(A.Axis, Key + dk, 0));
			if (!Bucket)
			{
				continue;
			}
			for (const int32 j : *Bucket)
			{
				const FFace& B = Faces[j];
				if (j == i || B.Comp == A.Comp || FMath::Abs(A.Plane - B.Plane) > 0.5)
				{
					continue;
				}
				const int32 U = (A.Axis + 1) % 3;
				const int32 V = (A.Axis + 2) % 3;
				const double OverlapU = FMath::Min(A.Box.Max[U], B.Box.Max[U]) - FMath::Max(A.Box.Min[U], B.Box.Min[U]);
				const double OverlapV = FMath::Min(A.Box.Max[V], B.Box.Max[V]) - FMath::Max(A.Box.Min[V], B.Box.Min[V]);
				if (OverlapU <= 1.0 || OverlapV <= 1.0)
				{
					continue;
				}
				const UPrimitiveComponent* First = A.Comp < B.Comp ? A.Comp : B.Comp;
				const UPrimitiveComponent* Second = A.Comp < B.Comp ? B.Comp : A.Comp;
				if (Reported.Contains(TPair<const UPrimitiveComponent*, const UPrimitiveComponent*>(First, Second)))
				{
					continue;
				}
				Reported.Add(TPair<const UPrimitiveComponent*, const UPrimitiveComponent*>(First, Second));
				static const TCHAR* AxisNames[] = { TEXT("X"), TEXT("Y"), TEXT("Z") };
				const FString Msg = FString::Printf(TEXT("Coplanar overlap on %s=%.1f (%.0f x %.0f cm): %s.%s vs %s.%s - offset one or use a decal"),
					AxisNames[A.Axis], A.Plane, OverlapU, OverlapV, *NameOf(A.Owner), *A.Comp->GetName(), *NameOf(B.Owner), *B.Comp->GetName());
				Issues.Add(MakeIssue(EMTValidationSeverity::Error, TEXT("CoplanarOverlap"), Msg, A.Owner, B.Owner, A.Box.GetCenter()));
			}
		}
	}
	return Issues;
}

TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindInterpenetratingBuildingsInWorld(UWorld* World)
{
	TArray<FMTValidationIssue> Issues;
	if (!World)
	{
		return Issues;
	}
	using namespace MTValidation;
	TArray<AActor*> Buildings;
	TArray<FMTFootprint> Footprints;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->ActorHasTag(TagBuilding))
		{
			continue;
		}
		const FBox Local = Actor->CalculateComponentsBoundingBoxInLocalSpace(/*bNonColliding*/ true);
		if (!Local.IsValid)
		{
			continue;
		}
		const FTransform T = Actor->GetActorTransform();
		const FVector Scale = T.GetScale3D().GetAbs();
		const FVector LocalCenter = Local.GetCenter();
		const FVector WorldCenter = T.TransformPosition(LocalCenter);
		// Shrink by 1 cm so buildings that merely touch are not reported.
		const FVector2D Half(FMath::Max(Local.GetExtent().X * Scale.X - 1.0, 1.0), FMath::Max(Local.GetExtent().Y * Scale.Y - 1.0, 1.0));
		Buildings.Add(Actor);
		Footprints.Add(FMTFootprint(FVector2D(WorldCenter.X, WorldCenter.Y), Half, float(T.Rotator().Yaw), 0.f));
	}
	for (int32 i = 0; i < Buildings.Num(); ++i)
	{
		for (int32 j = i + 1; j < Buildings.Num(); ++j)
		{
			if (!UMTPlacementValidator::FootprintsOverlap(Footprints[i], Footprints[j]))
			{
				continue;
			}
			// Vertical separation (e.g. a building on a cliff above another) is not an interpenetration.
			FVector OA, EA, OB, EB;
			Buildings[i]->GetActorBounds(false, OA, EA);
			Buildings[j]->GetActorBounds(false, OB, EB);
			if (OA.Z + EA.Z <= OB.Z - EB.Z || OB.Z + EB.Z <= OA.Z - EA.Z)
			{
				continue;
			}
			const FVector2D Mid = (Footprints[i].Center + Footprints[j].Center) * 0.5;
			Issues.Add(MakeIssue(EMTValidationSeverity::Error, TEXT("BuildingOverlap"),
				FString::Printf(TEXT("Buildings %s and %s have overlapping footprints"), *NameOf(Buildings[i]), *NameOf(Buildings[j])),
				Buildings[i], Buildings[j], FVector(Mid.X, Mid.Y, OA.Z)));
		}
	}
	return Issues;
}

TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindActorsInsideGeometryInWorld(UWorld* World)
{
	TArray<FMTValidationIssue> Issues;
	if (!World)
	{
		return Issues;
	}
	using namespace MTValidation;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		const bool bRelevant = Actor->IsA<APawn>() || Actor->IsA<APlayerStart>() || Actor->ActorHasTag(TagSpawner) ||
			Actor->ActorHasTag(TagQuest) || Actor->ActorHasTag(TagSpawnPoint);
		if (!bRelevant)
		{
			continue;
		}
		float Radius = 20.f;
		float HalfHeight = 20.f;
		FVector Feet = Actor->GetActorLocation();
		if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Actor->GetRootComponent()))
		{
			Radius = Capsule->GetScaledCapsuleRadius();
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			Feet.Z -= HalfHeight; // capsule root sits at the capsule centre
		}
		else if (Actor->IsA<APlayerStart>())
		{
			Radius = 40.f;
			HalfHeight = 92.f;
			Feet.Z -= HalfHeight;
		}
		TArray<const AActor*> Ignore;
		Ignore.Add(Actor);
		FString Blocker;
		// Shrink slightly: touching the floor is fine, being embedded is not.
		if (UMTPlacementValidator::IsCapsuleBlocked(World, Feet, FMath::Max(Radius - 2.f, 1.f), FMath::Max(HalfHeight - 2.f, 1.f), Ignore, &Blocker))
		{
			Issues.Add(MakeIssue(EMTValidationSeverity::Error, TEXT("InsideGeometry"),
				FString::Printf(TEXT("%s starts inside blocking geometry (%s)"), *NameOf(Actor), *Blocker), Actor, nullptr,
				Actor->GetActorLocation()));
		}
	}
	return Issues;
}

TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindBelowLandscapeInWorld(UWorld* World)
{
	TArray<FMTValidationIssue> Issues;
	if (!World)
	{
		return Issues;
	}
	using namespace MTValidation;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || IsLandscape(Actor) || !Actor->GetRootComponent())
		{
			continue;
		}
		const bool bRelevant = Actor->IsA<AStaticMeshActor>() || Actor->IsA<APawn>() || Actor->IsA<APlayerStart>() ||
			Actor->ActorHasTag(TagSpawner) || Actor->ActorHasTag(TagQuest) || Actor->ActorHasTag(TagSpawnPoint) ||
			Actor->ActorHasTag(TagBuilding);
		if (!bRelevant)
		{
			continue;
		}
		const FVector Loc = Actor->GetActorLocation();
		float GroundZ = 0.f;
		if (!LandscapeZ(World, Loc, GroundZ))
		{
			continue; // no landscape under this actor
		}
		if (Loc.Z < GroundZ - 50.f)
		{
			Issues.Add(MakeIssue(EMTValidationSeverity::Error, TEXT("BelowLandscape"),
				FString::Printf(TEXT("%s is %.0f cm below the landscape"), *NameOf(Actor), GroundZ - Loc.Z), Actor, nullptr, Loc));
		}
	}
	return Issues;
}

TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindFloatingPropsInWorld(UWorld* World)
{
	TArray<FMTValidationIssue> Issues;
	if (!World)
	{
		return Issues;
	}
	using namespace MTValidation;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Actor = *It;
		if (!Actor || Actor->ActorHasTag(TagAllowFloating))
		{
			continue;
		}
		const UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		if (!Comp || !Comp->GetStaticMesh() || !Comp->IsVisible())
		{
			continue;
		}
		const FBox Box = Comp->Bounds.GetBox();
		if (Box.GetSize().Z > 1000.0)
		{
			continue; // cliffs / large architecture are not props
		}
		const FVector BottomCenter(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTFloatingProps), false);
		Params.AddIgnoredActor(Actor);
		FHitResult Hit;
		const FVector Start = BottomCenter + FVector(0.f, 0.f, 5.f);
		const FVector End = BottomCenter - FVector(0.f, 0.f, 10000.f);
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			continue; // nothing below (or the prop is sunk below the surface): not "floating"
		}
		const double Gap = BottomCenter.Z - Hit.ImpactPoint.Z;
		if (Gap > 10.0)
		{
			Issues.Add(MakeIssue(EMTValidationSeverity::Warning, TEXT("FloatingProp"),
				FString::Printf(TEXT("%s floats %.0f cm above %s"), *NameOf(Actor), Gap, *NameOf(Hit.GetActor())),
				Actor, Hit.GetActor(), BottomCenter));
		}
	}
	return Issues;
}

TArray<FMTValidationIssue> UMTWorldValidationLibrary::RunAllChecksForWorld(UWorld* World)
{
	TArray<FMTValidationIssue> All;
	All.Append(FindDuplicateMeshesInWorld(World));
	All.Append(FindCoplanarOverlapsInWorld(World));
	All.Append(FindInterpenetratingBuildingsInWorld(World));
	All.Append(FindActorsInsideGeometryInWorld(World));
	All.Append(FindBelowLandscapeInWorld(World));
	All.Append(FindFloatingPropsInWorld(World));
	return All;
}

// ---------------------------------------------------------------------------------------------------------------------
TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindDuplicateMeshes(const UObject* Ctx) { return FindDuplicateMeshesInWorld(MTValidation::ResolveWorld(Ctx)); }
TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindCoplanarOverlaps(const UObject* Ctx) { return FindCoplanarOverlapsInWorld(MTValidation::ResolveWorld(Ctx)); }
TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindInterpenetratingBuildings(const UObject* Ctx) { return FindInterpenetratingBuildingsInWorld(MTValidation::ResolveWorld(Ctx)); }
TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindActorsInsideGeometry(const UObject* Ctx) { return FindActorsInsideGeometryInWorld(MTValidation::ResolveWorld(Ctx)); }
TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindBelowLandscape(const UObject* Ctx) { return FindBelowLandscapeInWorld(MTValidation::ResolveWorld(Ctx)); }
TArray<FMTValidationIssue> UMTWorldValidationLibrary::FindFloatingProps(const UObject* Ctx) { return FindFloatingPropsInWorld(MTValidation::ResolveWorld(Ctx)); }
TArray<FMTValidationIssue> UMTWorldValidationLibrary::RunAllChecks(const UObject* Ctx) { return RunAllChecksForWorld(MTValidation::ResolveWorld(Ctx)); }

int32 UMTWorldValidationLibrary::CountIssuesOfSeverity(const TArray<FMTValidationIssue>& Issues, EMTValidationSeverity Severity)
{
	int32 Count = 0;
	for (const FMTValidationIssue& Issue : Issues)
	{
		Count += Issue.Severity == Severity ? 1 : 0;
	}
	return Count;
}

FString UMTWorldValidationLibrary::SeverityToString(EMTValidationSeverity Severity)
{
	switch (Severity)
	{
	case EMTValidationSeverity::Info: return TEXT("Info");
	case EMTValidationSeverity::Warning: return TEXT("Warning");
	default: return TEXT("Error");
	}
}

FString UMTWorldValidationLibrary::IssuesToJson(const TArray<FMTValidationIssue>& Issues, const FString& MapName)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Map"), MapName);
	Root->SetNumberField(TEXT("Errors"), CountIssuesOfSeverity(Issues, EMTValidationSeverity::Error));
	Root->SetNumberField(TEXT("Warnings"), CountIssuesOfSeverity(Issues, EMTValidationSeverity::Warning));
	TArray<TSharedPtr<FJsonValue>> List;
	for (const FMTValidationIssue& Issue : Issues)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("Severity"), SeverityToString(Issue.Severity));
		Obj->SetStringField(TEXT("Category"), Issue.Category.ToString());
		Obj->SetStringField(TEXT("Message"), Issue.Message);
		Obj->SetStringField(TEXT("ActorA"), Issue.ActorAName);
		Obj->SetStringField(TEXT("ActorB"), Issue.ActorBName);
		TArray<TSharedPtr<FJsonValue>> Loc;
		Loc.Add(MakeShared<FJsonValueNumber>(Issue.Location.X));
		Loc.Add(MakeShared<FJsonValueNumber>(Issue.Location.Y));
		Loc.Add(MakeShared<FJsonValueNumber>(Issue.Location.Z));
		Obj->SetArrayField(TEXT("Location"), Loc);
		List.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Root->SetArrayField(TEXT("Issues"), List);
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

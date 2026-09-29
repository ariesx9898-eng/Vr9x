#include "Combat/MTEarthWall.h"
#include "Combat/MTCombatStatics.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"

AMTEarthWall::AMTEarthWall()
{
	PrimaryActorTick.bCanEverTick = true;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionObjectType(ECC_WorldDynamic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); // never pops the camera
	Box->SetCanEverAffectNavigation(true);
	RootComponent = Box;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Box);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMTEarthWall::InitWall(const FMTAbilityData& InData, AActor* InOwner, float Health, float InLifetime, float InRiseDelay, int32 Variant, float HeightScale)
{
	Data = InData;
	SetOwner(InOwner);
	MaxHealth = CurrentHealth = FMath::Max(1.f, Health);
	Lifetime = InLifetime > 0.f ? InLifetime : 15.f;
	Extent = InData.StructureExtent * FVector(1.f, 1.f, FMath::Clamp(HeightScale, 0.5f, 1.5f));
	Box->SetBoxExtent(Extent);
	RiseDelay = FMath::Max(0.f, InRiseDelay);
	// The first segment speaks loudest; the rest of the wall follows under it instead of stacking seven times.
	RiseVolume = RiseDelay <= 0.f ? 1.f : 0.4f;

	// The authored wall (one of its fractured variants) when it exists; until then (or if the named asset is missing)
	// the blockout, never an invisible wall that still blocks.
	UStaticMesh* WallMesh = nullptr;
	if (Variant > 0 && !Data.FX.BodyMesh.IsNull())
	{
		const FSoftObjectPath Base = Data.FX.BodyMesh.ToSoftObjectPath();
		const FString Suffix = Variant == 1 ? TEXT("_B") : TEXT("_C");
		const FString AssetName = Base.GetAssetName() + Suffix;
		const FSoftObjectPath VariantPath(FString::Printf(TEXT("%s/%s.%s"), *FPackageName::GetLongPackagePath(Base.GetLongPackageName()), *AssetName, *AssetName));
		WallMesh = Cast<UStaticMesh>(MTCombat::LoadOptionalAsset(VariantPath));
	}
	if (!WallMesh)
	{
		WallMesh = MTCombat::LoadOptional(Data.FX.BodyMesh);
	}
	if (WallMesh)
	{
		Mesh->SetStaticMesh(WallMesh);
		if (UMaterialInterface* WallMaterial = MTCombat::LoadOptional(Data.FX.BodyMaterial))
		{
			// Dynamic so the crack stages can show on it (the rock material's Crack parameter).
			RockMID = UMaterialInstanceDynamic::Create(WallMaterial, this);
			Mesh->SetMaterial(0, RockMID);
		}
		// Fit the mesh's bounds to the collision box: what you see is exactly what blocks.
		const FBox Bounds = WallMesh->GetBoundingBox();
		const FVector MeshExtent = Bounds.GetExtent().ComponentMax(FVector(1.f));
		const bool bSwap = (MeshExtent.X > MeshExtent.Y) != (Extent.X > Extent.Y);
		const FRotator Turn(0.f, bSwap ? 90.f : 0.f, 0.f);
		const FVector LocalScale = bSwap ? FVector(Extent.Y / MeshExtent.X, Extent.X / MeshExtent.Y, Extent.Z / MeshExtent.Z) : Extent / MeshExtent;
		Mesh->SetRelativeRotation(Turn);
		Mesh->SetRelativeScale3D(LocalScale);
		MeshBaseOffset = -Turn.RotateVector(Bounds.GetCenter() * LocalScale);
	}
	else if (UStaticMesh* Cube = MTCombat::LoadEngineShape(TEXT("Cube")))
	{
		// BLOCKOUT: engine cube is 100 cm; scale to the collision extent exactly so the
		// visual and collision match (no invisible walls, no see-through gaps).
		Mesh->SetStaticMesh(Cube);
		Mesh->SetRelativeScale3D(Extent / 50.f);
		if (UMaterialInterface* BaseMaterial = MTCombat::LoadMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			RockMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			RockMID->SetVectorParameterValue(TEXT("Color"), MTUtil::ElementColor(EMTElement::Earth));
			Mesh->SetMaterial(0, RockMID);
		}
	}

	// Start buried; rise over RiseTime once the delay has passed. Collision comes on with the rise, at the final
	// location, so nothing can clip into the rising wall and nothing is blocked by a wall that is not there yet.
	Mesh->SetRelativeLocation(MeshBaseOffset + FVector(0.f, 0.f, -Extent.Z * 2.f));
	Mesh->SetVisibility(false);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (RiseDelay <= 0.f)
	{
		BeginRise();
	}
}

void AMTEarthWall::BeginRise()
{
	if (bRisen)
	{
		return;
	}
	bRisen = true;
	Age = 0.f;
	Mesh->SetVisibility(true);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	const FVector Ground = GetActorLocation() - FVector(0.f, 0.f, Extent.Z);
	MTCombat::SpawnFX(this, Data.FX.Formation, Ground, GetActorRotation());
	MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Rise"), FTransform(GetActorRotation(), Ground), 1.f, nullptr, NAME_None, GetOwner());
	MTCombat::PlaySound(this, Data.FX.ImpactSound.IsNull() ? Data.FX.CastSound : Data.FX.ImpactSound, GetActorLocation(), RiseVolume);
}

void AMTEarthWall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bRisen)
	{
		RiseDelay -= DeltaSeconds;
		if (RiseDelay <= 0.f)
		{
			BeginRise();
		}
		return;
	}
	Age += DeltaSeconds;

	if (bCrumbling)
	{
		Mesh->AddRelativeLocation(FVector(0.f, 0.f, -Extent.Z * 4.f * DeltaSeconds));
		return;
	}

	const float Alpha = FMath::Clamp(Age / RiseTime, 0.f, 1.f);
	const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f);
	Mesh->SetRelativeLocation(MeshBaseOffset + FVector(0.f, 0.f, FMath::Lerp(-Extent.Z * 2.f, 0.f, Eased)));

	if (Age >= Lifetime)
	{
		Crumble(true);
	}
}

void AMTEarthWall::TakeStructureDamage(float Amount)
{
	if (bCrumbling || !bRisen)
	{
		return;
	}
	CurrentHealth -= Amount;
	MTCombat::SpawnFX(this, Data.FX.Impact, GetActorLocation(), GetActorRotation(), 0.6f);
	if (CurrentHealth <= 0.f)
	{
		Crumble();
		return;
	}
	UpdateCracks();
}

void AMTEarthWall::UpdateCracks()
{
	const float Fraction = GetHealthFraction();
	const int32 Stage = Fraction <= 0.33f ? 2 : (Fraction <= 0.66f ? 1 : 0);
	if (Stage <= CrackStage)
	{
		return;
	}
	CrackStage = Stage;
	// Strong hits visibly split the stone before it gives way.
	if (RockMID)
	{
		RockMID->SetScalarParameterValue(TEXT("Crack"), CrackStage >= 2 ? 1.f : 0.5f);
	}
	MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Crack"), GetActorTransform(), CrackStage >= 2 ? 1.25f : 1.f, nullptr, NAME_None, GetOwner());
	MTCombat::PlaySound(this, Data.FX.TravelSound, GetActorLocation());
}

void AMTEarthWall::Crumble(bool bTogether)
{
	if (bCrumbling)
	{
		return;
	}
	bCrumbling = true;
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bRisen)
	{
		MTCombat::SpawnFX(this, Data.FX.Dissipation, GetActorLocation(), GetActorRotation());
		MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Crumble"), FTransform(GetActorRotation(), GetActorLocation() - FVector(0.f, 0.f, Extent.Z)), 1.f,
			nullptr, NAME_None, GetOwner());
		// Seven segments expiring within 0.2 s would stack into one deafening crash: full volume for the first only.
		MTCombat::PlaySound(this, Data.FX.AccentSound, GetActorLocation(), bTogether ? RiseVolume : 1.f);
	}
	SetLifeSpan(0.6f);
}

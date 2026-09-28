#include "Combat/MTEarthWall.h"
#include "Combat/MTCombatStatics.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

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

void AMTEarthWall::InitWall(const FMTAbilityData& InData, AActor* InOwner, float Health, float InLifetime)
{
	Data = InData;
	SetOwner(InOwner);
	MaxHealth = CurrentHealth = FMath::Max(1.f, Health);
	Lifetime = InLifetime > 0.f ? InLifetime : 15.f;
	Extent = InData.StructureExtent;
	Box->SetBoxExtent(Extent);

	if (!Data.FX.BodyMesh.IsNull())
	{
		Mesh->SetStaticMesh(Data.FX.BodyMesh.LoadSynchronous());
		if (!Data.FX.BodyMaterial.IsNull())
		{
			Mesh->SetMaterial(0, Data.FX.BodyMaterial.LoadSynchronous());
		}
	}
	else if (UStaticMesh* Cube = MTCombat::LoadEngineShape(TEXT("Cube")))
	{
		// BLOCKOUT: engine cube is 100 cm; scale to the collision extent exactly so the
		// visual and collision match (no invisible walls, no see-through gaps).
		Mesh->SetStaticMesh(Cube);
		Mesh->SetRelativeScale3D(Extent / 50.f);
		if (UMaterialInterface* Base = MTCombat::LoadMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), MTUtil::ElementColor(EMTElement::Earth));
			Mesh->SetMaterial(0, MID);
		}
	}

	// Start buried; rise over RiseTime. Collision is on from the start at its final
	// location so nothing can clip into the rising wall.
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -Extent.Z * 2.f));
	MTCombat::SpawnFX(this, Data.FX.Formation, GetActorLocation() - FVector(0.f, 0.f, Extent.Z), GetActorRotation());
	MTCombat::PlaySound(this, Data.FX.CastSound, GetActorLocation());
}

void AMTEarthWall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (bCrumbling)
	{
		Mesh->AddRelativeLocation(FVector(0.f, 0.f, -Extent.Z * 4.f * DeltaSeconds));
		return;
	}

	const float Alpha = FMath::Clamp(Age / RiseTime, 0.f, 1.f);
	const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, FMath::Lerp(-Extent.Z * 2.f, 0.f, Eased)));

	if (Age >= Lifetime)
	{
		Crumble();
	}
}

void AMTEarthWall::TakeStructureDamage(float Amount)
{
	if (bCrumbling)
	{
		return;
	}
	CurrentHealth -= Amount;
	MTCombat::SpawnFX(this, Data.FX.Impact, GetActorLocation(), GetActorRotation(), 0.6f);
	if (CurrentHealth <= 0.f)
	{
		Crumble();
	}
}

void AMTEarthWall::Crumble()
{
	if (bCrumbling)
	{
		return;
	}
	bCrumbling = true;
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MTCombat::SpawnFX(this, Data.FX.Dissipation, GetActorLocation(), GetActorRotation());
	SetLifeSpan(0.6f);
}

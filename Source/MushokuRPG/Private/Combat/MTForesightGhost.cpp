#include "Combat/MTForesightGhost.h"
#include "Combat/MTCombatStatics.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

AMTForesightGhost::AMTForesightGhost()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Silhouette = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Silhouette"));
	Silhouette->SetupAttachment(Root);
	Silhouette->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Silhouette->SetCastShadow(false);
	Silhouette->SetVisibility(false);

	Marker = CreateDefaultSubobject<UDecalComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Root);
	Marker->SetUsingAbsoluteLocation(true);
	Marker->SetUsingAbsoluteRotation(true);
	Marker->DecalSize = FVector(200.f, 150.f, 150.f);
	Marker->SetFadeScreenSize(0.f);
}

void AMTForesightGhost::InitFromTelegraph(const FMTAttackTelegraph& Telegraph, float LeadBonus)
{
	const UWorld* World = GetWorld();
	SpawnTime = World->GetTimeSeconds();
	// Extra foresight (awakening) keeps the prediction on screen longer after impact.
	ImpactTime = FMath::Max(SpawnTime + 0.2f, Telegraph.ImpactTime) + LeadBonus;

	AActor* Attacker = Telegraph.Attacker.Get();
	StartLocation = Attacker ? Attacker->GetActorLocation() : Telegraph.PredictedAttackerLocation;
	EndLocation = Telegraph.PredictedAttackerLocation;
	SetActorLocation(StartLocation);
	SetActorRotation(Telegraph.PredictedAttackerRotation);

	// Silhouette: copy the attacker's current pose, rendered with the foresight material.
	const ACharacter* AttackerCharacter = Cast<ACharacter>(Attacker);
	USkeletalMeshComponent* SourceMesh = AttackerCharacter ? AttackerCharacter->GetMesh() : nullptr;
	UMaterialInterface* GhostMaterial = MTCombat::LoadMaterial(MTCombat::ForesightMaterialPath);
	if (SourceMesh && SourceMesh->GetSkinnedAsset() && GhostMaterial)
	{
		Silhouette->SetSkinnedAssetAndUpdate(SourceMesh->GetSkinnedAsset());
		Silhouette->SetRelativeTransform(SourceMesh->GetRelativeTransform());
		Silhouette->CopyPoseFromSkeletalComponent(SourceMesh);
		GhostMID = UMaterialInstanceDynamic::Create(GhostMaterial, this);
		for (int32 i = 0; i < Silhouette->GetNumMaterials(); ++i)
		{
			Silhouette->SetMaterial(i, GhostMID);
		}
		Silhouette->SetVisibility(true);
		bHasSilhouette = true;
	}

	// Danger marker projected onto the ground (decal - never a coplanar plane).
	if (UMaterialInterface* DecalMaterial = MTCombat::LoadMaterial(MTCombat::ZoneDecalMaterialPath))
	{
		MarkerMID = UMaterialInstanceDynamic::Create(DecalMaterial, this);
		MarkerMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.15f, 0.2f));
		MarkerMID->SetScalarParameterValue(TEXT("RingOnly"), 1.f);
		MarkerMID->SetScalarParameterValue(TEXT("Opacity"), 0.f);
		Marker->SetDecalMaterial(MarkerMID);
	}
	const float Radius = FMath::Max(80.f, Telegraph.Radius);
	Marker->DecalSize = FVector(250.f, Telegraph.Length > 0.f ? Telegraph.Length * 0.5f + Radius : Radius, Radius);
	const FVector MarkerCenter = Telegraph.Length > 0.f ? Telegraph.ImpactLocation + Telegraph.Direction.GetSafeNormal2D() * Telegraph.Length * 0.5f : Telegraph.ImpactLocation;
	Marker->SetWorldLocation(MarkerCenter);
	Marker->SetWorldRotation(FRotator(-90.f, Telegraph.Direction.Rotation().Yaw, 0.f));
}

void AMTForesightGhost::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	const float Span = FMath::Max(0.05f, ImpactTime - SpawnTime);
	const float Alpha = FMath::Clamp((Now - SpawnTime) / Span, 0.f, 1.f);

	// The silhouette arrives slightly ahead of the real attacker: that lead IS the foresight.
	const float Lead = FMath::Clamp(Alpha * 1.6f, 0.f, 1.f);
	SetActorLocation(FMath::Lerp(StartLocation, EndLocation, 1.f - FMath::Pow(1.f - Lead, 2.f)));

	const float Fade = Alpha < 0.15f ? Alpha / 0.15f : (Alpha > 0.85f ? (1.f - Alpha) / 0.15f : 1.f);
	if (GhostMID)
	{
		GhostMID->SetScalarParameterValue(TEXT("Opacity"), 0.45f * Fade);
	}
	if (MarkerMID)
	{
		MarkerMID->SetScalarParameterValue(TEXT("Opacity"), 0.8f * Fade);
		MarkerMID->SetScalarParameterValue(TEXT("Age"), Alpha);
	}
	if (Now >= ImpactTime + 0.1f)
	{
		Destroy();
	}
}

#include "Combat/MTWaterSerpent.h"
#include "Combat/MTCombatStatics.h"
#include "Character/MTCharacterBase.h"
#include "Abilities/MTAbilityComponent.h"
#include "VFX/MTSpellVFX.h"
#include "VFX/MTVFXLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

namespace
{
	const FLinearColor MTSerpent_WaterColor(0.18f, 0.45f, 0.75f);
	constexpr float MTSerpent_PathStep = 25.f;
	constexpr float MTSerpent_Opacity = 0.9f;
}

AMTWaterSerpent::AMTWaterSerpent()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetCastShadow(false);
	Head->SetGenerateOverlapEvents(false);
	Head->bReceivesDecals = false;
	SetCanBeDamaged(false);
}

void AMTWaterSerpent::InitSerpent(const FMTAbilityData& InData, AMTCharacterBase* InCaster, AActor* InPrey, const FVector& InAimPoint, float InDamageScale)
{
	Data = InData;
	Caster = InCaster;
	Prey = InPrey;
	AimPoint = InAimPoint;
	DamageScale = FMath::Max(0.f, InDamageScale);
	SetOwner(InCaster);
	SetInstigator(InCaster);
	CastOrigin = InCaster ? InCaster->GetActorLocation() : GetActorLocation();
	CastForward = InCaster ? InCaster->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	if (CastForward.IsNearlyZero())
	{
		CastForward = FVector::ForwardVector;
	}
	HeadRadius = FMath::Max(30.f, Data.GetParam(TEXT("HeadRadius"), 110.f));
	const int32 SegmentCount = FMath::Clamp(FMath::RoundToInt(Data.GetParam(TEXT("Segments"), 16.f)), 4, 32);
	SegmentSpacing = FMath::Max(30.f, Data.GetParam(TEXT("Length"), 1700.f) / SegmentCount);

	// One water material instance for the whole body, fading in as the streams take shape.
	if (UMaterialInterface* Water = MTVFX::LoadMaterial(MTVFX::Paths::MatWater))
	{
		WaterMID = UMaterialInstanceDynamic::Create(Water, this);
		WaterMID->SetVectorParameterValue(TEXT("Color"), MTSerpent_WaterColor);
		WaterMID->SetScalarParameterValue(TEXT("Intensity"), 1.f);
	}
	// Kit meshes (SM_VFX_DragonHead / SM_VFX_DragonSegment) run along +Y: a -90 degree yaw turns them to +X.
	MeshToForward = FQuat(FRotator(0.f, -90.f, 0.f));
	if (UStaticMesh* HeadMesh = MTVFX::LoadMesh(MTVFX::Paths::DragonHead))
	{
		Head->SetStaticMesh(HeadMesh);
		const FVector Native = FVector(HeadMesh->GetBounds().BoxExtent).ComponentMax(FVector(1.f));
		// The head is about three times as long as its radius; fit its length (native Y) to that.
		const float Uniform = (HeadRadius * 1.5f) / Native.Y;
		Head->SetRelativeScale3D(FVector(Uniform));
		Head->SetRelativeRotation(MeshToForward);
	}
	if (WaterMID)
	{
		for (int32 i = 0; i < Head->GetNumMaterials(); ++i)
		{
			Head->SetMaterial(i, WaterMID);
		}
	}
	UStaticMesh* SegmentMesh = MTVFX::LoadMesh(MTVFX::Paths::DragonSegment);
	SegmentNativeExtent = SegmentMesh ? FVector(SegmentMesh->GetBounds().BoxExtent).ComponentMax(FVector(1.f)) : FVector(50.f);
	for (int32 i = 0; i < SegmentCount; ++i)
	{
		UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
		Segment->SetStaticMesh(SegmentMesh);
		Segment->SetUsingAbsoluteLocation(true);
		Segment->SetUsingAbsoluteRotation(true);
		Segment->SetUsingAbsoluteScale(true);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCastShadow(false);
		Segment->SetGenerateOverlapEvents(false);
		Segment->bReceivesDecals = false;
		Segment->SetupAttachment(Root);
		Segment->RegisterComponent();
		Segment->SetVisibility(false);
		if (WaterMID)
		{
			for (int32 m = 0; m < Segment->GetNumMaterials(); ++m)
			{
				Segment->SetMaterial(m, WaterMID);
			}
		}
		Segments.Add(Segment);
	}
	SetWaterOpacity(0.f);

	// It rises out of the streams behind the caster's shoulder.
	const FVector Start = CastOrigin - CastForward * 120.f + FVector(0.f, 0.f, 60.f);
	Heading = (FVector::UpVector - CastForward * 0.3f).GetSafeNormal();
	SetActorLocationAndRotation(Start, Heading.Rotation());
	Path.Add(Start);
	TravelVFX = MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Travel"), GetActorTransform(), FMath::Max(0.05f, Data.FX.PresetScale), Root, NAME_None, InCaster);
	MTCombat::PlaySound(this, Data.FX.TravelSound, Start);
}

void AMTWaterSerpent::SetWaterOpacity(float Opacity)
{
	if (WaterMID)
	{
		WaterMID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}
}

void AMTWaterSerpent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (bFinished)
	{
		// The body loses its shape and falls away.
		const float Fade = 1.f - FMath::Clamp((Age - FinishedAt) / 0.3f, 0.f, 1.f);
		SetWaterOpacity(MTSerpent_Opacity * Fade);
		if (Fade <= 0.f)
		{
			Destroy();
		}
		return;
	}
	SetWaterOpacity(MTSerpent_Opacity * FMath::Clamp(Age / 0.35f, 0.f, 1.f));

	const float CircleTime = FMath::Max(0.1f, Data.GetParam(TEXT("CircleTime"), 0.5f));
	const float HuntTime = Data.GetParam(TEXT("HuntTime"), 2.4f);
	const FVector Previous = GetActorLocation();
	FVector Next = Previous;
	if (Age < CircleTime)
	{
		// One climbing turn around the caster, behind and above them, before it attacks.
		const float T = Age / CircleTime;
		const FVector Right = FVector::CrossProduct(FVector::UpVector, CastForward);
		const float Angle = PI + T * 1.5f * PI; // from behind, three quarters of a turn
		const FVector Around = CastForward * FMath::Cos(Angle) + Right * FMath::Sin(Angle);
		Next = CastOrigin + Around * 320.f + FVector(0.f, 0.f, FMath::Lerp(150.f, 620.f, FMath::SmoothStep(0.f, 1.f, T)));
		const FVector Step = Next - Previous;
		if (!Step.IsNearlyZero())
		{
			Heading = Step.GetSafeNormal();
		}
	}
	else
	{
		// Hunt: steer at the target (or the aim point) with a limited turn rate.
		const AActor* Target = Prey.Get();
		const FVector Goal = Target ? Target->GetActorLocation() + FVector(0.f, 0.f, 20.f) : AimPoint;
		const FVector Wanted = (Goal - Previous).GetSafeNormal();
		if (!Wanted.IsNearlyZero())
		{
			const float MaxTurn = FMath::DegreesToRadians(Data.GetParam(TEXT("TurnRate"), 160.f)) * DeltaSeconds;
			const float Between = FMath::Acos(FMath::Clamp(FVector::DotProduct(Heading, Wanted), -1.f, 1.f));
			Heading = Between <= MaxTurn ? Wanted : FMath::Lerp(Heading, Wanted, MaxTurn / Between).GetSafeNormal();
		}
		Next = Previous + Heading * Data.GetParam(TEXT("Speed"), 2800.f) * DeltaSeconds;

		// Contact: terrain and walls, any hostile at the head, or the aim point when there is no target.
		FHitResult Hit;
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTSerpentMove), false, this);
		if (AMTCharacterBase* Owner = Caster.Get())
		{
			Params.AddIgnoredActor(Owner);
		}
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Previous, Next, Objects, Params))
		{
			Finish(true, Hit.ImpactPoint);
			return;
		}
		if (AMTCharacterBase* Owner = Caster.Get())
		{
			if (MTCombat::GetHostilesInRadius(Owner, Next, Data.HitRadius(HeadRadius)).Num() > 0)
			{
				Finish(true, Next);
				return;
			}
		}
		if ((!Target && FVector::Dist(Next, AimPoint) <= HeadRadius) || Age >= CircleTime + HuntTime)
		{
			Finish(true, Next);
			return;
		}
	}
	SetActorLocationAndRotation(Next, Heading.Rotation());

	// Remember the path (newest first) so the body follows exactly where the head went.
	if (Path.Num() == 0 || FVector::DistSquared(Next, Path[0]) >= FMath::Square(MTSerpent_PathStep))
	{
		Path.Insert(Next, 0);
		const int32 MaxPoints = FMath::CeilToInt((Segments.Num() + 2) * SegmentSpacing / MTSerpent_PathStep) + 4;
		if (Path.Num() > MaxPoints)
		{
			Path.SetNum(MaxPoints);
		}
	}
	UpdateBody();
}

void AMTWaterSerpent::UpdateBody()
{
	// Segment i sits (i + 1) spacings behind the head along the travelled path; the body only exists where the head has
	// already been, so it visibly pulls itself out of the streams. It tapers toward the tail.
	int32 Index = 0;
	float Walked = 0.f;
	FVector From = GetActorLocation();
	const int32 Count = Segments.Num();
	for (int32 p = 0; p < Path.Num() && Index < Count; ++p)
	{
		const FVector To = Path[p];
		const float Length = FVector::Dist(From, To);
		while (Index < Count && Length > KINDA_SMALL_NUMBER && Walked + Length >= (Index + 1) * SegmentSpacing)
		{
			const float Along = ((Index + 1) * SegmentSpacing - Walked) / Length;
			const FVector Where = FMath::Lerp(From, To, Along);
			const FVector Toward = (From - To).GetSafeNormal();
			const float Taper = FMath::Lerp(0.55f, 0.16f, Count > 1 ? Index / float(Count - 1) : 0.f) * HeadRadius;
			UStaticMeshComponent* Segment = Segments[Index];
			Segment->SetWorldLocationAndRotation(Where, FQuat(FRotationMatrix::MakeFromX(Toward).Rotator()) * MeshToForward);
			Segment->SetWorldScale3D(FVector(Taper / SegmentNativeExtent.X, SegmentSpacing * 0.65f / SegmentNativeExtent.Y, Taper / SegmentNativeExtent.Z));
			Segment->SetVisibility(true);
			++Index;
		}
		Walked += Length;
		From = To;
	}
	for (; Index < Count; ++Index)
	{
		Segments[Index]->SetVisibility(false);
	}
}

void AMTWaterSerpent::Collapse()
{
	if (!bFinished)
	{
		Finish(false, GetActorLocation());
	}
}

void AMTWaterSerpent::Finish(bool bExplode, const FVector& Where)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	FinishedAt = Age;
	MTCombat::StopSpellFX(TravelVFX.Get());
	TravelVFX.Reset();
	AMTCharacterBase* Owner = Caster.Get();
	if (!bExplode)
	{
		MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Dissipation, TEXT("Dissipation"), FTransform(Heading.Rotation(), Where), 1.f, nullptr, NAME_None, Owner);
		return;
	}
	// An enormous water explosion: everyone inside is hit and thrown outward, and the ground is left soaked.
	const FVector Ground = MTCombat::GroundBelow(this, Where);
	const FVector Blast = FVector::Dist(Ground, Where) < 250.f ? Ground : Where;
	if (Owner)
	{
		for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(Owner, Blast, Data.HitRadius(Data.AOERadius)))
		{
			FVector Out = (Target->GetActorLocation() - Blast).GetSafeNormal2D();
			if (Out.IsNearlyZero())
			{
				Out = Heading.GetSafeNormal2D();
			}
			FMTDamageSpec Spec = MTCombat::MakeAbilityHit(Data, Owner, Target->GetActorLocation(), Out, DamageScale);
			MTCombat::ApplyElementInteractions(Spec, Target);
			const FMTDamageResult Result = Target->ReceiveCombatHit(Spec);
			if (Owner->GetAbilities())
			{
				Owner->GetAbilities()->NotifyAbilityHit(Data.AbilityID, Result.DamageDealt);
			}
		}
	}
	MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Impact, TEXT("Impact"), FTransform(Heading.Rotation(), Blast), 1.f, nullptr, NAME_None, Owner);
	MTCombat::PlaySound(this, Data.FX.ImpactSound, Blast);
}

void AMTWaterSerpent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MTCombat::StopSpellFX(TravelVFX.Get());
	TravelVFX.Reset();
	Super::EndPlay(EndPlayReason);
}

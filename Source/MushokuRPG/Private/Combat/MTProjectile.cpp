#include "Combat/MTProjectile.h"
#include "Combat/MTCombatStatics.h"
#include "Combat/MTTelegraphSubsystem.h"
#include "Combat/MTEarthWall.h"
#include "Character/MTCharacterBase.h"
#include "Abilities/MTAbilityComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

AMTProjectile::AMTProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(20.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->bReturnMaterialOnMove = false;
	RootComponent = Collision;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);

	TravelFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TravelFX"));
	TravelFX->SetupAttachment(Collision);
	TravelFX->SetAutoActivate(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Collision);
	Light->SetIntensity(0.f);
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(400.f);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bShouldBounce = false;
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 20000.f;
	Movement->bAutoActivate = false;

	InitialLifeSpan = 0.f;
}

void AMTProjectile::BeginPlay()
{
	Super::BeginPlay();
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AMTProjectile::OnOverlap);
	Collision->OnComponentHit.AddDynamic(this, &AMTProjectile::OnHit);
	if (UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this))
	{
		Telegraphs->RegisterSpell(this);
	}
}

void AMTProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this))
	{
		Telegraphs->UnregisterSpell(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AMTProjectile::InitProjectile(const FMTAbilityData& InData, AMTCharacterBase* InOwner, float InChargeAlpha, const FVector& Direction, float SpeedMultiplier, float DamageMultiplier)
{
	Data = InData;
	OwnerCharacter = InOwner;
	SetInstigator(InOwner);
	SetOwner(InOwner);
	ChargeAlpha = FMath::Clamp(InChargeAlpha, 0.f, 1.f);

	const float SpeedScale = FMath::Lerp(1.f, Data.ChargeSpeedScale, ChargeAlpha) * SpeedMultiplier;
	DamageScale = FMath::Lerp(1.f, Data.ChargeDamageScale, ChargeAlpha) * DamageMultiplier;
	StaggerScale = FMath::Lerp(1.f, Data.ChargeStaggerScale, ChargeAlpha);

	Collision->SetSphereRadius(FMath::Max(4.f, Data.ProjectileRadius * (1.f + 0.35f * ChargeAlpha)));
	if (InOwner)
	{
		Collision->IgnoreActorWhenMoving(InOwner, true);
	}

	const FVector Dir = Direction.GetSafeNormal();
	Movement->ProjectileGravityScale = Data.Motion == EMTProjectileMotion::Arc ? FMath::Max(0.2f, Data.ProjectileGravity) : Data.ProjectileGravity;
	Movement->InitialSpeed = Data.ProjectileSpeed * SpeedScale;
	Movement->MaxSpeed = FMath::Max(Movement->InitialSpeed * 1.5f, 1000.f);
	Movement->Velocity = Dir * Movement->InitialSpeed;
	Movement->Activate(true);
	SetActorRotation(Dir.Rotation());

	WaveAxis = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	Lifetime = FMath::Clamp(Data.Range / FMath::Max(100.f, Movement->InitialSpeed) + 0.25f, 0.3f, 8.f);

	// Presentation: formation happens at the hand in the ability; here the travel phase.
	if (UNiagaraSystem* TravelSystem = MTCombat::LoadOptional(Data.FX.Travel))
	{
		TravelFX->SetAsset(TravelSystem);
		TravelFX->Activate(true);
	}
	// The authored body when it exists; until then (or if the named asset is missing) the blockout look, never an
	// invisible spell.
	if (UStaticMesh* BodyMesh = MTCombat::LoadOptional(Data.FX.BodyMesh))
	{
		Body->SetStaticMesh(BodyMesh);
		if (UMaterialInterface* BodyMaterial = MTCombat::LoadOptional(Data.FX.BodyMaterial))
		{
			Body->SetMaterial(0, BodyMaterial);
		}
	}
	else
	{
		ApplyPlaceholderLook();
	}
	MTCombat::PlaySound(this, Data.FX.TravelSound, GetActorLocation(), 0.7f);
}

void AMTProjectile::ApplyPlaceholderLook()
{
	// BLOCKOUT visuals until the Niagara/mesh assets exist. Each element still differs in
	// silhouette, motion and light so they read differently in playtests.
	UStaticMesh* Mesh = nullptr;
	FVector Scale(0.2f);
	float LightIntensity = 0.f;
	switch (Data.Element)
	{
	case EMTElement::Earth:
		Mesh = MTCombat::LoadEngineShape(TEXT("Cone"));
		Scale = FVector(0.35f, 0.25f, 0.25f) * (1.f + 0.6f * ChargeAlpha);
		Body->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // cone tip forward: dense slug
		break;
	case EMTElement::Fire:
		Mesh = MTCombat::LoadEngineShape(TEXT("Sphere"));
		Scale = FVector(0.18f);
		LightIntensity = 6000.f;
		break;
	case EMTElement::Water:
		Mesh = MTCombat::LoadEngineShape(TEXT("Sphere"));
		Scale = FVector(0.45f, 0.18f, 0.18f); // stretched pressure jet
		break;
	case EMTElement::Wind:
		Mesh = MTCombat::LoadEngineShape(TEXT("Cube"));
		Scale = FVector(0.04f, 0.9f, 0.02f); // thin crescent blade
		break;
	default:
		Mesh = MTCombat::LoadEngineShape(TEXT("Sphere"));
		Scale = FVector(0.12f);
		LightIntensity = 1500.f;
		break;
	}
	if (Mesh)
	{
		Body->SetStaticMesh(Mesh);
		Body->SetRelativeScale3D(Scale);
		UMaterialInterface* Base = MTCombat::LoadMaterial(MTCombat::SpellBodyMaterialPath);
		if (!Base)
		{
			Base = MTCombat::LoadMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		if (Base)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), MTUtil::ElementColor(Data.Element));
			Body->SetMaterial(0, MID);
		}
	}
	Light->SetLightColor(MTUtil::ElementColor(Data.Element));
	Light->SetIntensity(LightIntensity);
}

void AMTProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished)
	{
		return;
	}
	Age += DeltaSeconds;

	if (Data.Motion == EMTProjectileMotion::Wave && !WaveAxis.IsNearlyZero())
	{
		// Wind blades drift in a gentle S so they read as air, not bullets.
		// d/dt (A sin wt) with A = 8 cm, w = 14 rad/s.
		const float Drift = FMath::Cos(Age * 14.f) * 14.f * 8.f * DeltaSeconds;
		AddActorWorldOffset(WaveAxis * Drift, true);
	}
	if (Data.Element == EMTElement::Earth && Body)
	{
		Body->AddLocalRotation(FRotator(0.f, 0.f, 720.f * DeltaSeconds)); // rifling spin
	}
	if (Data.Element == EMTElement::Fire && Light)
	{
		Light->SetIntensity(5000.f + 1500.f * FMath::Sin(Age * 40.f)); // flicker
	}

	if (Age >= Lifetime)
	{
		// Out of range: fire/water burst harmlessly, stone drops, wind fades.
		Dissipate(true);
	}
}

AActor* AMTProjectile::GetInstigatorActor() const
{
	return OwnerCharacter.Get();
}

FMTDamageSpec AMTProjectile::GetDamageSpec() const
{
	FMTDamageSpec Spec;
	Spec.Damage = Data.Damage * DamageScale;
	Spec.Stagger = Data.Stagger * StaggerScale;
	Spec.Knockback = Data.Knockback * FMath::Lerp(1.f, 1.5f, ChargeAlpha);
	Spec.Element = Data.Element;
	Spec.bIsMagic = Data.Element != EMTElement::None;
	Spec.SourceAbility = Data.AbilityID;
	Spec.Instigator = OwnerCharacter.Get();
	Spec.HitDirection = GetProjectileVelocity().GetSafeNormal();
	return Spec;
}

FVector AMTProjectile::GetProjectileVelocity() const
{
	return Movement ? Movement->Velocity : FVector::ZeroVector;
}

float AMTProjectile::EstimateTimeToReach(const FVector& Location, float Tolerance) const
{
	const FVector Velocity = GetProjectileVelocity();
	const float Speed = Velocity.Size();
	if (Speed < 1.f)
	{
		return BIG_NUMBER;
	}
	const FVector ToTarget = Location - GetActorLocation();
	const float Along = FVector::DotProduct(ToTarget, Velocity / Speed);
	if (Along < 0.f)
	{
		return BIG_NUMBER;
	}
	const FVector Closest = GetActorLocation() + Velocity / Speed * Along;
	if (FVector::Dist(Closest, Location) > Tolerance)
	{
		return BIG_NUMBER;
	}
	return Along / Speed;
}

void AMTProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bFinished || !OtherActor || OtherActor == OwnerCharacter.Get())
	{
		return;
	}
	AMTCharacterBase* Target = Cast<AMTCharacterBase>(OtherActor);
	if (!Target || !Target->IsAlive() || AlreadyHit.Contains(Target))
	{
		return;
	}
	if (OwnerCharacter.IsValid() && !OwnerCharacter->IsHostileTo(Target))
	{
		return;
	}
	const FVector Location = bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation();
	HitCharacter(Target, Location);
}

void AMTProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bFinished)
	{
		return;
	}
	if (AMTEarthWall* Wall = Cast<AMTEarthWall>(OtherActor))
	{
		Wall->TakeStructureDamage(GetDamageSpec().Damage);
	}
	Explode(Hit.ImpactPoint, Hit.ImpactNormal);
}

void AMTProjectile::HitCharacter(AMTCharacterBase* Target, const FVector& Location)
{
	AlreadyHit.Add(Target);
	FMTDamageSpec Spec = GetDamageSpec();
	Spec.HitLocation = Location;
	MTCombat::ApplyElementInteractions(Spec, Target);
	const FMTDamageResult Result = Target->ReceiveCombatHit(Spec);
	if (OwnerCharacter.IsValid() && OwnerCharacter->GetAbilities() && Result.DamageDealt > 0.f)
	{
		OwnerCharacter->GetAbilities()->NotifyAbilityHit(Data.AbilityID, Result.DamageDealt);
	}

	const bool bPierce = Data.Motion == EMTProjectileMotion::Piercing || Data.Motion == EMTProjectileMotion::Wave;
	if (!bPierce || AlreadyHit.Num() >= 3)
	{
		Explode(Location, -GetProjectileVelocity().GetSafeNormal());
	}
}

void AMTProjectile::Explode(const FVector& Location, const FVector& Normal)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (Data.AOERadius > 0.f && OwnerCharacter.IsValid())
	{
		for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(OwnerCharacter.Get(), Location, Data.AOERadius))
		{
			if (AlreadyHit.Contains(Target))
			{
				continue;
			}
			FMTDamageSpec Spec = GetDamageSpec();
			Spec.Damage *= 0.6f; // splash
			Spec.Stagger *= 0.6f;
			Spec.HitLocation = Location;
			Spec.HitDirection = (Target->GetActorLocation() - Location).GetSafeNormal2D();
			MTCombat::ApplyElementInteractions(Spec, Target);
			const FMTDamageResult Result = Target->ReceiveCombatHit(Spec);
			if (OwnerCharacter->GetAbilities() && Result.DamageDealt > 0.f)
			{
				OwnerCharacter->GetAbilities()->NotifyAbilityHit(Data.AbilityID, Result.DamageDealt);
			}
		}
	}

	MTCombat::SpawnFX(this, Data.FX.Impact, Location, Normal.Rotation(), 1.f + ChargeAlpha * 0.5f);
	MTCombat::PlaySound(this, Data.FX.ImpactSound, Location);
	Dissipate(false);
}

void AMTProjectile::Dissipate(bool bSpawnFX)
{
	bFinished = true;
	if (bSpawnFX)
	{
		MTCombat::SpawnFX(this, Data.FX.Dissipation, GetActorLocation(), GetActorRotation());
	}
	if (TravelFX)
	{
		TravelFX->Deactivate();
	}
	Movement->StopMovementImmediately();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetVisibility(false);
	Light->SetIntensity(0.f);
	SetLifeSpan(0.5f); // let trailing particles finish
}

void AMTProjectile::Disrupt(AActor* By)
{
	if (bFinished)
	{
		return;
	}
	// The spell's structure destabilises and scatters: dissipation FX, no damage.
	MTCombat::SpawnFX(this, Data.FX.Dissipation, GetActorLocation(), GetActorRotation(), 1.3f);
	UE_LOG(LogMushoku, Verbose, TEXT("%s disrupted by %s"), *Data.AbilityID.ToString(), *GetNameSafe(By));
	Dissipate(false);
}

void AMTProjectile::Deflect(const FVector& NewDirection)
{
	if (bFinished || !Movement)
	{
		return;
	}
	const float Speed = Movement->Velocity.Size();
	Movement->Velocity = NewDirection.GetSafeNormal() * Speed;
	WaveAxis = FVector::CrossProduct(NewDirection.GetSafeNormal(), FVector::UpVector).GetSafeNormal();
}

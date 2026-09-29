#include "Combat/MTProjectile.h"
#include "VFX/MTSpellVFX.h"
#include "Combat/MTCombatStatics.h"
#include "Combat/MTTelegraphSubsystem.h"
#include "Combat/MTEarthWall.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbilityComponent.h"
#include "Core/MTGameplayTags.h"
#include "CollisionQueryParams.h"
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
	MTCombat::StopSpellFX(TravelVFX.Get());
	TravelVFX.Reset();
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

	// ProjectileRadius is the visible body; the hit sphere is that x HitForgiveness (forgiving, never invisible).
	Collision->SetSphereRadius(FMath::Max(4.f, Data.HitRadius(Data.ProjectileRadius * GetSizeScale())));
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

	// Homing spells (Water Dragon) steer toward the caster's lock target.
	if (Data.bHoming && InOwner)
	{
		if (const AActor* Target = InOwner->GetLockTarget())
		{
			Movement->bIsHomingProjectile = true;
			Movement->HomingTargetComponent = Target->GetRootComponent();
			Movement->HomingAccelerationMagnitude = Data.HomingStrength;
		}
	}

	// Presentation: formation happens at the hand in the ability; here the travel phase.
	if (UNiagaraSystem* TravelSystem = MTCombat::LoadOptional(Data.FX.Travel))
	{
		TravelFX->SetAsset(TravelSystem);
		TravelFX->Activate(true);
	}
	else
	{
		TravelVFX = MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Travel"), GetActorTransform(),
			FMath::Max(0.05f, Data.FX.PresetScale) * GetSizeScale(), Collision, NAME_None, InOwner);
	}
	// The authored body when it exists; the runtime travel effect draws the body itself; otherwise the blockout look,
	// never an invisible spell.
	if (TravelVFX.IsValid())
	{
		Body->SetVisibility(false);
		Light->SetIntensity(0.f);
	}
	else if (UStaticMesh* BodyMesh = MTCombat::LoadOptional(Data.FX.BodyMesh))
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

	// Hostiles already inside the sphere at spawn (point-blank casts) are hit now that the caster is known.
	bInitialized = true;
	LastSweepLocation = GetActorLocation();
	TArray<AActor*> Overlapping;
	Collision->GetOverlappingActors(Overlapping, AMTCharacterBase::StaticClass());
	for (AActor* Other : Overlapping)
	{
		TryHitActor(Other, GetActorLocation());
	}
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
	if (Data.ProjectileWidth > 0.f)
	{
		SweepCrescent();
		if (bFinished)
		{
			return;
		}
	}
	if (!TravelVFX.IsValid())
	{
		if (Data.Element == EMTElement::Earth && Body)
		{
			Body->AddLocalRotation(FRotator(0.f, 0.f, 720.f * DeltaSeconds)); // rifling spin
		}
		if (Data.Element == EMTElement::Fire && Light)
		{
			Light->SetIntensity(5000.f + 1500.f * FMath::Sin(Age * 40.f)); // flicker
		}
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
	Spec.Knockback = Data.Knockback * FMath::Lerp(1.f, Data.ChargeKnockbackScale, ChargeAlpha);
	Spec.Launch = Data.Launch;
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
	TryHitActor(OtherActor, bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation());
}

void AMTProjectile::TryHitActor(AActor* OtherActor, const FVector& Location)
{
	// The spawn overlap test runs inside SpawnActor, before InitProjectile names the caster: without these checks the
	// spell hit whoever cast it and burst in their hand.
	if (bFinished || !bInitialized || !OtherActor || OtherActor == OwnerCharacter.Get() || OtherActor == GetInstigator() || OtherActor == GetOwner())
	{
		return;
	}
	AMTCharacterBase* Target = Cast<AMTCharacterBase>(OtherActor);
	if (!Target || !Target->IsAlive() || AlreadyHit.Contains(Target))
	{
		return;
	}
	if (!OwnerCharacter.IsValid() || !OwnerCharacter->IsHostileTo(Target))
	{
		return;
	}
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
	// Decide before the hit lands: the knockback may move the target, and a kill must not change the rule.
	const bool bPassThrough = CanPierce(Target);
	FMTDamageSpec Spec = GetDamageSpec();
	Spec.HitLocation = Location;
	MTCombat::ApplyElementInteractions(Spec, Target);
	const FMTDamageResult Result = Target->ReceiveCombatHit(Spec);
	if (Result.DamageDealt > 0.f)
	{
		ApplyBurn(Target);
		if (OwnerCharacter.IsValid() && OwnerCharacter->GetAbilities())
		{
			OwnerCharacter->GetAbilities()->NotifyAbilityHit(Data.AbilityID, Result.DamageDealt);
		}
	}

	if (!bPassThrough)
	{
		Explode(Location, -GetProjectileVelocity().GetSafeNormal());
		return;
	}
	++Pierced;
	// Punching through: fragments burst out of the far side and the spell keeps its speed.
	const FVector Velocity = GetProjectileVelocity();
	MTCombat::SpawnPresetPhase(this, Data.FX.Preset, TEXT("Pierce"), FTransform(Velocity.IsNearlyZero() ? GetActorRotation() : Velocity.Rotation(), Location),
		FMath::Max(0.05f, Data.FX.PresetScale) * GetSizeScale(), nullptr, NAME_None, OwnerCharacter.Get());
	MTCombat::PlaySound(this, Data.FX.ImpactSound, Location, 0.6f);
}

bool AMTProjectile::CanPierce(const AMTCharacterBase* Target) const
{
	// Explicit budget from the row; the older Piercing / Wave rows pass through two and stop on the third.
	const bool bLegacyPierce = Data.Motion == EMTProjectileMotion::Piercing || Data.Motion == EMTProjectileMotion::Wave;
	const int32 Budget = Data.PierceCount > 0 ? Data.PierceCount : (bLegacyPierce ? 2 : 0);
	if (Pierced >= Budget || !Target)
	{
		return false;
	}
	if (Data.PierceMaxHealth > 0.f)
	{
		// Heavy targets (bosses, anything crowd-control immune or tougher than the limit) stop the spell.
		const UMTAttributeComponent* Attr = Target->GetAttributes();
		if (!Attr || Attr->bCrowdControlImmune || Attr->GetMaxHealth() > Data.PierceMaxHealth)
		{
			return false;
		}
	}
	return true;
}

void AMTProjectile::ApplyBurn(AMTCharacterBase* Target) const
{
	if (!Target || Data.Element != EMTElement::Fire || Data.BurnSeconds <= 0.f || !Target->IsAlive() || !Target->GetAttributes())
	{
		return;
	}
	FMTStatusEffect Burn;
	Burn.Id = TEXT("Burning");
	Burn.Duration = Data.BurnSeconds;
	Burn.HealthPerSecond = -FMath::Max(0.f, Data.BurnDamagePerSecond);
	Burn.GrantedTags.AddTag(MTTags::State_Burning);
	Burn.Instigator = OwnerCharacter.Get();
	Target->GetAttributes()->AddStatusEffect(Burn);
}

float AMTProjectile::GetSizeScale() const
{
	return FMath::Lerp(1.f, FMath::Max(0.1f, Data.ChargeSizeScale), ChargeAlpha);
}

float AMTProjectile::GetImpactRadius() const
{
	return Data.AOERadius * FMath::Lerp(1.f, FMath::Max(0.1f, Data.ChargeRadiusScale), ChargeAlpha);
}

void AMTProjectile::SweepCrescent()
{
	const FVector To = GetActorLocation();
	UWorld* World = GetWorld();
	if (!World || !bInitialized || bFinished)
	{
		LastSweepLocation = To;
		return;
	}
	const FVector Velocity = GetProjectileVelocity();
	const FQuat Facing = (Velocity.IsNearlyZero() ? GetActorForwardVector() : Velocity).Rotation().Quaternion();
	// The blade's hit box: its thickness, the forgiving half-width, and tall enough for a standing body.
	const FVector HalfExtent(FMath::Max(10.f, Data.ProjectileRadius), Data.HitRadius(Data.ProjectileWidth * 0.5f), 90.f);
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTCrescentSweep), false, this);
	if (AMTCharacterBase* Caster = OwnerCharacter.Get())
	{
		Params.AddIgnoredActor(Caster);
	}
	const FVector From = LastSweepLocation;
	LastSweepLocation = To;
	World->SweepMultiByObjectType(Hits, From, To, Facing, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(HalfExtent), Params);
	for (const FHitResult& Hit : Hits)
	{
		AActor* Other = Hit.GetActor();
		if (Other)
		{
			TryHitActor(Other, Other->GetActorLocation());
		}
		if (bFinished)
		{
			break;
		}
	}
}

void AMTProjectile::Explode(const FVector& Location, const FVector& Normal)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	const float ImpactRadius = GetImpactRadius();
	if (ImpactRadius > 0.f && OwnerCharacter.IsValid())
	{
		// Splash (60% unless the row says otherwise; the Barrage finale hits everyone for full damage).
		const float SplashScale = FMath::Clamp(Data.GetParam(TEXT("SplashScale"), 0.6f), 0.f, 1.f);
		for (AMTCharacterBase* Target : MTCombat::GetHostilesInRadius(OwnerCharacter.Get(), Location, Data.HitRadius(ImpactRadius)))
		{
			if (AlreadyHit.Contains(Target))
			{
				continue;
			}
			FMTDamageSpec Spec = GetDamageSpec();
			Spec.Damage *= SplashScale;
			Spec.Stagger *= SplashScale;
			Spec.HitLocation = Location;
			Spec.HitDirection = (Target->GetActorLocation() - Location).GetSafeNormal2D();
			MTCombat::ApplyElementInteractions(Spec, Target);
			const FMTDamageResult Result = Target->ReceiveCombatHit(Spec);
			if (Result.DamageDealt > 0.f)
			{
				ApplyBurn(Target);
				if (OwnerCharacter->GetAbilities())
				{
					OwnerCharacter->GetAbilities()->NotifyAbilityHit(Data.AbilityID, Result.DamageDealt);
				}
			}
		}
	}

	// The impact preset is authored at the row's AOERadius: it grows with the charged radius.
	const float ImpactScale = Data.AOERadius > 0.f ? ImpactRadius / Data.AOERadius : 1.f;
	MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Impact, TEXT("Impact"), FTransform(Normal.IsNearlyZero() ? FRotator::ZeroRotator : (-Normal).Rotation(), Location),
		ImpactScale, nullptr, NAME_None, OwnerCharacter.Get());
	MTCombat::PlaySound(this, Data.FX.ImpactSound, Location);
	Dissipate(false);
}

void AMTProjectile::Dissipate(bool bSpawnFX)
{
	bFinished = true;
	if (bSpawnFX)
	{
		MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Dissipation, TEXT("Dissipation"), GetActorTransform(), 1.f, nullptr, NAME_None, OwnerCharacter.Get());
	}
	if (TravelFX)
	{
		TravelFX->Deactivate();
	}
	MTCombat::StopSpellFX(TravelVFX.Get());
	TravelVFX.Reset();
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
	MTCombat::SpawnSpellFX(this, Data.FX, Data.FX.Dissipation, TEXT("Dissipation"), GetActorTransform(), 1.3f, nullptr, NAME_None, OwnerCharacter.Get());
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

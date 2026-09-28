#include "VFX/MTSpellVFX.h"
#include "VFX/MTVFXLibrary.h"
#include "Character/MTPlayerCharacter.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"

namespace
{
	const FName MTName_Color(TEXT("Color"));
	const FName MTName_Intensity(TEXT("Intensity"));
	const FName MTName_Opacity(TEXT("Opacity"));
	const FName MTName_Texture(TEXT("Texture"));
	constexpr float MaxLifetime = 30.f;

	FVector RandomInCone(FRandomStream& Random, const FVector& Dir, float ConeDeg)
	{
		if (ConeDeg >= 179.f)
		{
			return Random.GetUnitVector();
		}
		return Random.VRandCone(Dir.GetSafeNormal(), FMath::DegreesToRadians(FMath::Max(0.f, ConeDeg)));
	}
}

// ---------------------------------------------------------------------------------------------
// Curves
// ---------------------------------------------------------------------------------------------

float FMTVFXCurve::Eval(float T) const
{
	if (Keys.Num() == 0)
	{
		return 1.f;
	}
	if (T <= Keys[0].X)
	{
		return Keys[0].Y;
	}
	for (int32 i = 1; i < Keys.Num(); ++i)
	{
		if (T <= Keys[i].X)
		{
			const float Span = FMath::Max(KINDA_SMALL_NUMBER, Keys[i].X - Keys[i - 1].X);
			const float A = (T - Keys[i - 1].X) / Span;
			return FMath::Lerp(Keys[i - 1].Y, Keys[i].Y, FMath::SmoothStep(0.f, 1.f, A));
		}
	}
	return Keys.Last().Y;
}

FMTVFXCurve FMTVFXCurve::FadeInOut(float In, float Out)
{
	In = FMath::Clamp(In, 0.f, 0.95f);
	Out = FMath::Clamp(Out, 0.f, 1.f - In);
	return FMTVFXCurve({ { 0.f, 0.f }, { In, 1.f }, { 1.f - Out, 1.f }, { 1.f, 0.f } });
}

FMTVFXCurve FMTVFXCurve::Grow(float From, float At)
{
	return FMTVFXCurve({ { 0.f, From }, { FMath::Clamp(At, 0.01f, 1.f), 1.f }, { 1.f, 1.f } });
}

// ---------------------------------------------------------------------------------------------
// Actor
// ---------------------------------------------------------------------------------------------

AMTSpellVFX::AMTSpellVFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	SetCanBeDamaged(false);
	SetReplicates(false);
}

bool AMTSpellVFX::HasPreset(FName Name)
{
	return MTVFX::FindPreset(Name) != nullptr;
}

AMTSpellVFX* AMTSpellVFX::SpawnPreset(UObject* WorldContext, FName Name, const FTransform& Transform, float InScale,
	USceneComponent* AttachTo, FName Socket, AActor* Source)
{
	const FMTVFXDesc* Preset = MTVFX::FindPreset(Name);
	UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!Preset || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AMTSpellVFX* FX = World->SpawnActor<AMTSpellVFX>(AMTSpellVFX::StaticClass(), Transform, Params);
	if (!FX)
	{
		return nullptr;
	}
	if (AttachTo)
	{
		FX->AttachToComponent(AttachTo, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	}
	FX->Build(*Preset, FMath::Max(0.05f, InScale), Source);
	return FX;
}

void AMTSpellVFX::Build(const FMTVFXDesc& InDesc, float InScale, AActor* InSource)
{
	Desc = InDesc;
	Scale = InScale;
	Random.Initialize(static_cast<int32>(GetUniqueID() ^ 0x5A17u));

	for (const FMTVFXMeshLayer& Def : Desc.Meshes)
	{
		FMeshState& State = MeshLayers.AddDefaulted_GetRef();
		State.Def = Def;
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		UStaticMesh* LayerMesh = MTVFX::LoadMesh(Def.MeshPath);
		Comp->SetStaticMesh(LayerMesh);
		const FVector Native = LayerMesh ? FVector(LayerMesh->GetBounds().BoxExtent) : FVector(50.f);
		State.NativeScale = Def.Size / Native.ComponentMax(FVector(1.f));
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
		Comp->SetGenerateOverlapEvents(false);
		Comp->bReceivesDecals = false;
		Comp->SetupAttachment(Root);
		Comp->SetRelativeLocation(Def.Offset * Scale);
		State.BaseRotation = Def.Rotation.Quaternion();
		Comp->SetRelativeRotation(State.BaseRotation);
		Comp->SetRelativeScale3D(FVector(KINDA_SMALL_NUMBER));
		Comp->RegisterComponent();
		if (UMaterialInterface* Base = MTVFX::LoadMaterial(Def.MaterialPath))
		{
			State.MID = UMaterialInstanceDynamic::Create(Base, this);
			for (const TPair<FName, float>& Param : Def.Scalars)
			{
				State.MID->SetScalarParameterValue(Param.Key, Param.Value);
			}
			for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
			{
				Comp->SetMaterial(i, State.MID);
			}
		}
		State.Comp = Comp;
		Owned.Add(Comp);
	}

	UStaticMesh* PlaneMesh = MTVFX::LoadMesh(MTVFX::Paths::Plane);
	for (const FMTVFXEmitter& Def : Desc.Emitters)
	{
		FEmitterState& State = Emitters.AddDefaulted_GetRef();
		State.Def = Def;
		State.Def.MaxParticles = FMath::Clamp(Def.MaxParticles, 1, 512);
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		const bool bMesh = Def.Render == EMTVFXRender::Mesh;
		UStaticMesh* ParticleMesh = bMesh ? MTVFX::LoadMesh(Def.MeshPath) : PlaneMesh;
		ISM->SetStaticMesh(ParticleMesh);
		State.MeshRadius = (bMesh && ParticleMesh) ? FMath::Max(1.f, ParticleMesh->GetBounds().SphereRadius) : 50.f;
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCastShadow(bMesh);
		ISM->SetGenerateOverlapEvents(false);
		ISM->bReceivesDecals = false;
		ISM->SetupAttachment(Root);
		ISM->RegisterComponent();
		ISM->SetNumCustomDataFloats(4);
		const TCHAR* DefaultMaterial = Def.Render == EMTVFXRender::SpriteSmoke ? MTVFX::Paths::MatSmoke : MTVFX::Paths::MatSprite;
		UMaterialInterface* Base = MTVFX::LoadMaterial(bMesh ? (Def.MaterialPath.IsEmpty() ? FString(MTVFX::Paths::MatRock) : Def.MaterialPath)
			: (Def.MaterialPath.IsEmpty() ? FString(DefaultMaterial) : Def.MaterialPath));
		if (Base)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			const FString TexPath = !Def.Texture.IsEmpty() ? Def.Texture
				: FString(Def.Render == EMTVFXRender::SpriteSmoke ? MTVFX::Paths::TexPuff
					: Def.Render == EMTVFXRender::Stretched ? MTVFX::Paths::TexStreak : MTVFX::Paths::TexDot);
			if (UTexture* Tex = MTVFX::LoadTexture(TexPath))
			{
				MID->SetTextureParameterValue(MTName_Texture, Tex);
			}
			MID->SetScalarParameterValue(MTName_Intensity, Def.Intensity);
			MID->SetScalarParameterValue(MTName_Opacity, 1.f);
			MID->SetVectorParameterValue(MTName_Color, FLinearColor::White);
			ISM->SetMaterial(0, MID);
		}
		State.Particles.SetNum(State.Def.MaxParticles);
		State.Transforms.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector(KINDA_SMALL_NUMBER)), State.Def.MaxParticles);
		ISM->AddInstances(State.Transforms, false, true);
		State.ISM = ISM;
		Owned.Add(ISM);

		if (Def.bBounce || Def.bFollowGround)
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(MTVFXGround), false);
			Params.AddIgnoredActor(this);
			if (InSource)
			{
				Params.AddIgnoredActor(InSource);
			}
			const FVector From = GetActorLocation() + FVector(0.f, 0.f, 300.f);
			if (GetWorld()->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Params))
			{
				State.GroundZ = Hit.ImpactPoint.Z;
				State.bHasGround = true;
			}
		}
	}

	for (const FMTVFXLight& Def : Desc.Lights)
	{
		FLightState& State = LightLayers.AddDefaulted_GetRef();
		State.Def = Def;
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(Def.Offset * Scale);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(0.f);
		Light->SetAttenuationRadius(Def.Radius * Scale);
		Light->SetLightColor(Def.Color);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		State.Comp = Light;
		Owned.Add(Light);
	}

	DecalSpawned.Init(false, Desc.Decals.Num());
	ShakeDone.Init(false, Desc.Shakes.Num());
	if (Desc.Afterimages.Count > 0)
	{
		if (const ACharacter* Character = Cast<ACharacter>(InSource))
		{
			GhostSource = Character->GetMesh();
		}
	}
	// Show the first frame immediately (no pop on the frame the effect is born).
	Tick(0.f);
}

void AMTSpellVFX::Stop()
{
	if (!bStopping)
	{
		bStopping = true;
		StopTime = Age;
	}
}

void AMTSpellVFX::SetTint(const FLinearColor& InTint)
{
	Tint = InTint;
}

float AMTSpellVFX::FadeFactor() const
{
	if (!bStopping)
	{
		return 1.f;
	}
	return FMath::Clamp(1.f - (Age - StopTime) / FMath::Max(0.05f, Desc.FadeOut), 0.f, 1.f);
}

void AMTSpellVFX::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FAfterimage& Ghost : Ghosts)
	{
		if (Ghost.Comp)
		{
			Ghost.Comp->DestroyComponent();
		}
	}
	Ghosts.Reset();
	Super::EndPlay(EndPlayReason);
}

void AMTSpellVFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (!Desc.bLoop && !bStopping && Age >= Desc.Duration)
	{
		// One-shots stop emitting at their duration; particles finish their lives.
		bStopping = true;
		StopTime = Desc.Duration;
	}

	FVector CameraLocation = GetActorLocation() + FVector(-500.f, 0.f, 200.f);
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
		}
	}

	const float Life01 = FMath::Clamp(Age / FMath::Max(0.01f, Desc.Duration), 0.f, 1.f);
	TickMeshes(Life01, DeltaSeconds);
	TickLights(DeltaSeconds);
	SpawnDecals(DeltaSeconds);
	TickShakes();
	TickAfterimages(DeltaSeconds);

	bool bAnyParticle = false;
	for (FEmitterState& State : Emitters)
	{
		const float Start = State.Def.Delay;
		const bool bInWindow = Age >= Start && (State.Def.EmitDuration < 0.f || Age <= Start + State.Def.EmitDuration);
		TickEmitter(State, DeltaSeconds, bInWindow && !bStopping, CameraLocation);
		for (const FParticle& P : State.Particles)
		{
			bAnyParticle |= P.bAlive;
		}
	}

	const bool bFadedOut = bStopping && (Age - StopTime) >= Desc.FadeOut;
	const bool bGhostsDone = Ghosts.Num() == 0 && (Desc.Afterimages.Count == 0 || GhostsMade >= Desc.Afterimages.Count || bStopping);
	bool bDecalsPending = false;
	for (int32 i = 0; i < Desc.Decals.Num(); ++i)
	{
		bDecalsPending |= !DecalSpawned[i] && Desc.Decals[i].Delay > Age && !bStopping;
	}
	if ((bFadedOut && !bAnyParticle && bGhostsDone && !bDecalsPending) || Age > MaxLifetime + Desc.Duration)
	{
		Destroy();
	}
}

void AMTSpellVFX::TickMeshes(float Life01, float DeltaSeconds)
{
	const float Fade = FadeFactor();
	for (FMeshState& State : MeshLayers)
	{
		UStaticMeshComponent* Comp = State.Comp;
		if (!Comp)
		{
			continue;
		}
		const FMTVFXMeshLayer& Def = State.Def;
		const float LayerDuration = Def.Duration > 0.f ? Def.Duration : FMath::Max(0.01f, Desc.Duration - Def.Delay);
		const float Local = Age - Def.Delay;
		const bool bVisible = Local >= 0.f && (Def.Duration < 0.f || Local <= Def.Duration);
		if (!bVisible)
		{
			Comp->SetVisibility(false);
			continue;
		}
		Comp->SetVisibility(true);
		const float T = FMath::Clamp(Local / LayerDuration, 0.f, 1.f);
		FVector S = State.NativeScale * Scale * Def.ScaleOverLife.Eval(T);
		if (Def.bWobble)
		{
			const float W = Age * 9.f + Def.Offset.X;
			S *= FVector(1.f + 0.07f * FMath::Sin(W), 1.f + 0.07f * FMath::Sin(W * 1.3f + 1.f), 1.f + 0.07f * FMath::Sin(W * 0.8f + 2.f));
		}
		Comp->SetRelativeScale3D(S.ComponentMax(FVector(KINDA_SMALL_NUMBER)));
		if (Def.SpinSpeed != 0.f)
		{
			const FQuat Spin(Def.SpinAxis.GetSafeNormal(), FMath::DegreesToRadians(Def.SpinSpeed * Age));
			Comp->SetRelativeRotation(Spin * State.BaseRotation);
		}
		if (State.MID)
		{
			State.MID->SetVectorParameterValue(MTName_Color, Def.Color * Tint);
			State.MID->SetScalarParameterValue(MTName_Intensity, Def.Intensity);
			State.MID->SetScalarParameterValue(MTName_Opacity, Def.AlphaOverLife.Eval(T) * Fade);
		}
	}
}

void AMTSpellVFX::TickLights(float DeltaSeconds)
{
	const float Fade = FadeFactor();
	for (FLightState& State : LightLayers)
	{
		if (!State.Comp)
		{
			continue;
		}
		const FMTVFXLight& Def = State.Def;
		const float Duration = Def.Duration > 0.f ? Def.Duration : FMath::Max(0.01f, Desc.Duration - Def.Delay);
		const float Local = Age - Def.Delay;
		float Value = 0.f;
		if (Local >= 0.f)
		{
			const float T = FMath::Clamp(Local / Duration, 0.f, 1.f);
			Value = Def.Intensity * Def.IntensityOverLife.Eval(Desc.bLoop && Def.Duration < 0.f ? FMath::Min(T, 0.5f) : T) * Fade;
			if (Def.Flicker > 0.f)
			{
				Value *= 1.f - Def.Flicker * 0.5f * (1.f + FMath::PerlinNoise1D(Age * 11.f + static_cast<float>(GetUniqueID() % 997)));
			}
		}
		State.Comp->SetIntensity(FMath::Max(0.f, Value));
	}
}

void AMTSpellVFX::SpawnDecals(float DeltaSeconds)
{
	for (int32 i = 0; i < Desc.Decals.Num(); ++i)
	{
		if (DecalSpawned[i] || Age < Desc.Decals[i].Delay || (bStopping && Desc.bLoop))
		{
			continue;
		}
		DecalSpawned[i] = true;
		const FMTVFXDecal& Def = Desc.Decals[i];
		UMaterialInterface* Base = MTVFX::LoadMaterial(Def.MaterialPath);
		if (!Base)
		{
			continue;
		}
		// Project onto whatever is under the effect (ground, slopes, walls near the ground).
		const FVector Origin = GetActorTransform().TransformPosition(Def.Offset * Scale);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTVFXDecal), false);
		Params.AddIgnoredActor(this);
		if (AActor* ParentActor = GetAttachParentActor())
		{
			Params.AddIgnoredActor(ParentActor);
		}
		// Static ground only: a hit on a character must not orient the decal to their body (a stretched square).
		FCollisionObjectQueryParams Ground(ECC_WorldStatic);
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Origin + FVector(0.f, 0.f, 150.f), Origin - FVector(0.f, 0.f, 800.f), Ground, Params))
		{
			continue;
		}
		if (Hit.ImpactNormal.Z < 0.55f)
		{
			Hit.ImpactNormal = FVector::UpVector;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(MTName_Color, Def.Color * Tint);
		MID->SetScalarParameterValue(MTName_Intensity, Def.Intensity);
		MID->SetScalarParameterValue(TEXT("SpinSpeed"), Def.SpinSpeed);
		for (const TPair<FName, float>& Param : Def.Scalars)
		{
			MID->SetScalarParameterValue(Param.Key, Param.Value);
		}
		FRotator Rotation = FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator();
		if (Def.bRandomYaw)
		{
			Rotation.Roll = Random.FRandRange(-180.f, 180.f);
		}
		const float Size = Def.Size * Scale;
		const float LifeSpan = Def.Lifetime + Def.FadeOut;
		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(this, MID, FVector(Def.Depth * Scale, Size, Size), Hit.ImpactPoint, Rotation, LifeSpan);
		if (Decal)
		{
			Decal->SetFadeIn(0.f, FMath::Max(0.01f, Def.FadeIn));
			Decal->SetFadeOut(Def.Lifetime, FMath::Max(0.05f, Def.FadeOut), false);
			Decal->SetFadeScreenSize(0.002f);
			Decal->SortOrder = 5;
		}
	}
}

void AMTSpellVFX::TickShakes()
{
	for (int32 i = 0; i < Desc.Shakes.Num(); ++i)
	{
		if (ShakeDone[i] || Age < Desc.Shakes[i].Delay)
		{
			continue;
		}
		ShakeDone[i] = true;
		const FMTVFXShake& Def = Desc.Shakes[i];
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AMTPlayerCharacter* Player = It->IsValid() ? Cast<AMTPlayerCharacter>((*It)->GetPawn()) : nullptr;
			if (!Player)
			{
				continue;
			}
			const float Dist = FVector::Dist(Player->GetActorLocation(), GetActorLocation());
			const float Falloff = 1.f - FMath::Clamp(Dist / FMath::Max(1.f, Def.Radius * Scale), 0.f, 1.f);
			if (Falloff > 0.f)
			{
				Player->AddCameraShake(Def.Strength * Falloff, Def.Duration);
			}
		}
	}
}

void AMTSpellVFX::TickAfterimages(float DeltaSeconds)
{
	const FMTVFXAfterimages& Def = Desc.Afterimages;
	USkeletalMeshComponent* Source = GhostSource.Get();
	if (Source && !bStopping && GhostsMade < Def.Count && Age >= NextGhostTime)
	{
		NextGhostTime = Age + Def.Interval;
		++GhostsMade;
		UPoseableMeshComponent* Ghost = NewObject<UPoseableMeshComponent>(this);
		Ghost->SetUsingAbsoluteLocation(true);
		Ghost->SetUsingAbsoluteRotation(true);
		Ghost->SetUsingAbsoluteScale(true);
		Ghost->SetSkinnedAssetAndUpdate(Source->GetSkinnedAsset());
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ghost->SetCastShadow(false);
		Ghost->bReceivesDecals = false;
		Ghost->RegisterComponent();
		Ghost->SetWorldTransform(Source->GetComponentTransform());
		Ghost->CopyPoseFromSkeletalComponent(Source);
		FAfterimage& Entry = Ghosts.AddDefaulted_GetRef();
		Entry.Comp = Ghost;
		if (UMaterialInterface* Base = MTVFX::LoadMaterial(MTVFX::Paths::MatGhost))
		{
			for (int32 i = 0; i < Ghost->GetNumMaterials(); ++i)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
				MID->SetVectorParameterValue(MTName_Color, Def.Color * Tint);
				MID->SetScalarParameterValue(MTName_Intensity, Def.Intensity);
				Ghost->SetMaterial(i, MID);
				Entry.MIDs.Add(MID);
			}
		}
	}
	for (int32 i = Ghosts.Num() - 1; i >= 0; --i)
	{
		FAfterimage& Ghost = Ghosts[i];
		Ghost.Age += DeltaSeconds;
		const float Alpha = 1.f - FMath::Clamp(Ghost.Age / FMath::Max(0.05f, Def.Lifetime), 0.f, 1.f);
		for (UMaterialInstanceDynamic* MID : Ghost.MIDs)
		{
			MID->SetScalarParameterValue(MTName_Opacity, Alpha * Alpha);
		}
		if (Alpha <= 0.f)
		{
			if (Ghost.Comp)
			{
				Ghost.Comp->DestroyComponent();
			}
			Ghosts.RemoveAtSwap(i);
		}
	}
}

void AMTSpellVFX::SpawnParticle(FEmitterState& State)
{
	int32 Slot = INDEX_NONE;
	for (int32 i = 0; i < State.Particles.Num(); ++i)
	{
		if (!State.Particles[i].bAlive)
		{
			Slot = i;
			break;
		}
	}
	if (Slot == INDEX_NONE)
	{
		return;
	}
	const FMTVFXEmitter& Def = State.Def;
	FVector Local = Def.Offset;
	FVector Radial = FVector::ZeroVector;
	const float R = Def.Radius;
	switch (Def.Shape)
	{
	case EMTVFXShape::Sphere:
		Radial = Random.GetUnitVector();
		Local += Radial * R * FMath::Pow(Random.FRand(), 1.f / 3.f);
		break;
	case EMTVFXShape::SphereShell:
		Radial = Random.GetUnitVector();
		Local += Radial * R;
		break;
	case EMTVFXShape::Ring:
	{
		const float A = Random.FRandRange(0.f, 2.f * PI);
		Radial = FVector(FMath::Cos(A), FMath::Sin(A), 0.f);
		Local += Radial * R;
		break;
	}
	case EMTVFXShape::Disc:
	{
		const float A = Random.FRandRange(0.f, 2.f * PI);
		Radial = FVector(FMath::Cos(A), FMath::Sin(A), 0.f);
		Local += Radial * R * FMath::Sqrt(Random.FRand());
		break;
	}
	case EMTVFXShape::Line:
		Local += FVector(0.f, Random.FRandRange(-Def.Extent.Y, Def.Extent.Y), 0.f);
		Radial = FVector(1.f, 0.f, 0.f);
		break;
	case EMTVFXShape::Box:
		Local += FVector(Random.FRandRange(-Def.Extent.X, Def.Extent.X), Random.FRandRange(-Def.Extent.Y, Def.Extent.Y), Random.FRandRange(-Def.Extent.Z, Def.Extent.Z));
		Radial = Random.GetUnitVector();
		break;
	default:
		Radial = Random.GetUnitVector();
		break;
	}

	FParticle& P = State.Particles[Slot];
	const FTransform& T = GetActorTransform();
	const FVector LocalScaled = Local * Scale;
	const FVector Dir = Def.bRadial ? (Radial.IsNearlyZero() ? FVector::UpVector : Radial) : RandomInCone(Random, Def.Direction, Def.ConeDeg);
	const FVector Vel = Dir * Random.FRandRange(Def.SpeedMin, Def.SpeedMax) * Scale;
	if (Def.bWorldSpace)
	{
		P.Position = T.TransformPosition(LocalScaled);
		P.Velocity = T.TransformVectorNoScale(Vel);
		if (Def.bFollowGround && State.bHasGround)
		{
			P.Position.Z = State.GroundZ + LocalScaled.Z;
		}
	}
	else
	{
		P.Position = LocalScaled;
		P.Velocity = Vel;
	}
	P.Age = 0.f;
	P.Life = Random.FRandRange(Def.LifeMin, Def.LifeMax);
	P.Size = Random.FRandRange(Def.SizeMin, Def.SizeMax) * Scale;
	P.Spin = Random.FRandRange(0.f, 360.f);
	P.SpinSpeed = Random.FRandRange(-Def.SpinMax, Def.SpinMax);
	P.Seed = Random.FRand();
	P.bAlive = true;
	P.bResting = false;
}

void AMTSpellVFX::TickEmitter(FEmitterState& State, float DeltaSeconds, bool bEmitting, const FVector& CameraLocation)
{
	const FMTVFXEmitter& Def = State.Def;
	if (Age >= Def.Delay && !State.bBurstDone && !(bStopping && Age > StopTime + 0.01f && Desc.bLoop))
	{
		State.bBurstDone = true;
		for (int32 i = 0; i < Def.Burst; ++i)
		{
			SpawnParticle(State);
		}
	}
	if (bEmitting && Def.Rate > 0.f)
	{
		State.SpawnDebt += Def.Rate * DeltaSeconds;
		while (State.SpawnDebt >= 1.f)
		{
			State.SpawnDebt -= 1.f;
			SpawnParticle(State);
		}
	}

	UInstancedStaticMeshComponent* ISM = State.ISM;
	if (!ISM)
	{
		return;
	}
	const FTransform& Actor = GetActorTransform();
	const FVector Origin = Actor.GetLocation();
	const FVector Up = Actor.GetRotation().GetUpVector();
	const float Fade = Desc.bLoop ? FadeFactor() : 1.f;
	TArray<float> Custom;
	Custom.SetNum(4);
	bool bAny = false;
	for (int32 i = 0; i < State.Particles.Num(); ++i)
	{
		FParticle& P = State.Particles[i];
		FTransform& Out = State.Transforms[i];
		if (!P.bAlive)
		{
			Out.SetScale3D(FVector(KINDA_SMALL_NUMBER));
			continue;
		}
		P.Age += DeltaSeconds;
		if (P.Age >= P.Life)
		{
			P.bAlive = false;
			Out.SetScale3D(FVector(KINDA_SMALL_NUMBER));
			ISM->SetCustomData(i, TArray<float>({ 0.f, 0.f, 0.f, 0.f }), false);
			continue;
		}
		bAny = true;
		const float T = P.Age / P.Life;

		// Integrate.
		if (!P.bResting)
		{
			FVector Accel(0.f, 0.f, (Def.Buoyancy - Def.Gravity) * Scale);
			const FVector Centre = Def.bWorldSpace ? Origin : FVector::ZeroVector;
			const FVector Axis = Def.bWorldSpace ? Up : FVector::UpVector;
			if (Def.Attract != 0.f)
			{
				Accel += (Centre - P.Position).GetSafeNormal() * Def.Attract * Scale;
			}
			if (Def.Orbit != 0.f)
			{
				const FVector Rel = P.Position - Centre;
				const FVector Tangent = FVector::CrossProduct(Axis, Rel).GetSafeNormal();
				const float Speed = FMath::DegreesToRadians(Def.Orbit) * Rel.Size2D();
				P.Position += Tangent * Speed * DeltaSeconds;
			}
			P.Velocity += Accel * DeltaSeconds;
			if (Def.Drag > 0.f)
			{
				P.Velocity *= FMath::Exp(-Def.Drag * DeltaSeconds);
			}
			P.Position += P.Velocity * DeltaSeconds;
			if (Def.bBounce && State.bHasGround && Def.bWorldSpace && P.Position.Z < State.GroundZ + 3.f)
			{
				P.Position.Z = State.GroundZ + 3.f;
				P.Velocity.Z = -P.Velocity.Z * 0.28f;
				P.Velocity.X *= 0.55f;
				P.Velocity.Y *= 0.55f;
				if (FMath::Abs(P.Velocity.Z) < 40.f)
				{
					P.bResting = true;
					P.SpinSpeed = 0.f;
				}
			}
		}
		P.Spin += P.SpinSpeed * DeltaSeconds;

		const FVector World = Def.bWorldSpace ? P.Position : Actor.TransformPosition(P.Position);
		const float Size = P.Size * Def.SizeOverLife.Eval(T);
		FQuat Rotation;
		FVector S;
		if (Def.Render == EMTVFXRender::Mesh)
		{
			if (Def.bFlat)
			{
				Rotation = Actor.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(P.Spin));
			}
			else
			{
				Rotation = FQuat(FVector(0.3f, 0.8f, 0.5f).GetSafeNormal(), FMath::DegreesToRadians(P.Spin)) * FQuat(FRotator(P.Seed * 360.f, P.Seed * 720.f, 0.f));
			}
			S = FVector(Size / State.MeshRadius);
		}
		else
		{
			const FVector ToCamera = (CameraLocation - World).GetSafeNormal();
			if (Def.Render == EMTVFXRender::Stretched)
			{
				const FVector WorldVel = Def.bWorldSpace ? P.Velocity : Actor.TransformVectorNoScale(P.Velocity);
				FVector Along = WorldVel - ToCamera * FVector::DotProduct(WorldVel, ToCamera);
				if (Along.IsNearlyZero())
				{
					Along = FVector::CrossProduct(ToCamera, FVector::UpVector);
				}
				Rotation = FRotationMatrix::MakeFromZX(ToCamera, Along).ToQuat();
				const float SpeedStretch = FMath::Clamp(WorldVel.Size() / 600.f, 0.6f, 2.5f);
				S = FVector(Size * Def.Stretch * SpeedStretch / 100.f, Size / 100.f, 1.f);
			}
			else
			{
				Rotation = FQuat(ToCamera, FMath::DegreesToRadians(P.Spin)) * FRotationMatrix::MakeFromZ(ToCamera).ToQuat();
				S = FVector(Size / 100.f, Size / 100.f, 1.f);
			}
		}
		Out = FTransform(Rotation, World, S);

		const FLinearColor C = FLinearColor::LerpUsingHSV(Def.ColorStart, Def.ColorEnd, T) * Tint;
		Custom[0] = C.R;
		Custom[1] = C.G;
		Custom[2] = C.B;
		Custom[3] = Def.AlphaOverLife.Eval(T) * Fade;
		ISM->SetCustomData(i, Custom, false);
	}
	ISM->BatchUpdateInstancesTransforms(0, State.Transforms, true, true, true);
	if (bAny)
	{
		ISM->MarkRenderStateDirty();
	}
}

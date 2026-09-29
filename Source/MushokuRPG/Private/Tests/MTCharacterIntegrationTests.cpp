// Runtime integration tests for the two playable lineages (Rudeus, Orsted): data, body and pose, lock-on facing and
// Orsted's signature abilities, in a real game world ticked headless. Run them with:
//   UnrealEditor-Cmd MushokuRPG.uproject -ExecCmds="Automation RunTests MushokuRPG;Quit" -unattended -nullrhi -nosplash
// (Tools/mac/run_automation_tests.sh wraps that.) Tests that need imported assets (meshes, clips) report what is
// missing instead of passing silently; run Tools/mac/build_and_setup.sh first.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/MTAbility.h"
#include "Abilities/MTAbilityBehaviors.h"
#include "Abilities/MTAbilityComponent.h"
#include "AI/MTEnemyCharacter.h"
#include "Animation/AnimSequence.h"
#include "Animation/MTNativeAnimInstance.h"
#include "Character/MTAttributeComponent.h"
#include "Character/MTCharacterBase.h"
#include "Character/MTPlayerCharacter.h"
#include "Combat/MTEarthWall.h"
#include "Combat/MTProjectile.h"
#include "Combat/MTWaterSerpent.h"
#include "Combat/MTZoneActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/MTDataRegistry.h"
#include "Core/MTGameplayTags.h"
#include "Core/MTTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UObjectGlobals.h"
#include "VFX/MTSpellVFX.h"
#include "VFX/MTVFXSubsystem.h"

namespace MTTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** A standalone game world with the project's GameInstance subsystems (the data registry reads Content/Data),
	 *  a plain game mode, begun play and a 100 x 100 m floor at Z = 0. Ticked by hand like the engine's
	 *  FTestWorldWrapper: every tick advances GFrameCounter (tick functions run once per frame counter). */
	struct FGameWorld
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		uint64 CachedFrameCounter = 0;

		FGameWorld()
		{
			CachedFrameCounter = GFrameCounter;
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			if (!World)
			{
				return;
			}
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			World->SetGameMode(FURL());
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();

			if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator, Params);
				Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
				Floor->SetActorScale3D(FVector(100.f, 100.f, 1.f));
			}
		}

		~FGameWorld()
		{
			if (World && World->HasBegunPlay())
			{
				World->BeginTearingDown();
				World->EndPlay(EEndPlayReason::Quit);
			}
			GFrameCounter = CachedFrameCounter;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}

		bool IsValid() const { return World != nullptr && UMTDataRegistry::Get(World) != nullptr; }

		void Tick(float Seconds, float Step = 1.f / 60.f)
		{
			for (float T = 0.f; T < Seconds; T += Step)
			{
				World->Tick(LEVELTICK_All, Step);
				++GFrameCounter;
			}
		}

		/** A player character of the given lineage standing at Location, facing Yaw. No controller: movement runs
		 *  without one and the pose ticks although nothing is rendered (-nullrhi). */
		AMTPlayerCharacter* SpawnPlayer(FName Lineage, const FVector& Location, float Yaw = 0.f)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AMTPlayerCharacter* Player = World->SpawnActor<AMTPlayerCharacter>(AMTPlayerCharacter::StaticClass(),
				Location + FVector(0.f, 0.f, 110.f), FRotator(0.f, Yaw, 0.f), Params);
			if (Player)
			{
				Player->ApplyCharacterLineage(Lineage);
				Prepare(Player);
			}
			return Player;
		}

		/** A hostile lineage opponent (Content/Data/Enemies.json row) with its AI switched off. */
		AMTEnemyCharacter* SpawnOpponent(FName EnemyId, const FVector& Location, float Yaw = 180.f)
		{
			const FTransform Transform(FRotator(0.f, Yaw, 0.f), Location + FVector(0.f, 0.f, 110.f));
			AMTEnemyCharacter* Enemy = World->SpawnActorDeferred<AMTEnemyCharacter>(AMTEnemyCharacter::StaticClass(), Transform,
				nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Enemy)
			{
				Enemy->EnemyId = EnemyId;
				Enemy->AutoPossessAI = EAutoPossessAI::Disabled;
				Enemy->FinishSpawning(Transform);
				Prepare(Enemy);
			}
			return Enemy;
		}

		static void Prepare(AMTCharacterBase* Character)
		{
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			Movement->bRunPhysicsWithNoController = true;
			// What ACharacter::PostInitializeComponents does for a controller-less character spawned with that flag
			// (a possessed character gets its mode on possession); without it the movement mode stays None.
			Movement->SetDefaultMovementMode();
			Character->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	};

	float YawTo(const AActor* From, const AActor* To)
	{
		return (To->GetActorLocation() - From->GetActorLocation()).Rotation().Yaw;
	}

	float YawError(const AActor* From, const AActor* To)
	{
		return FMath::Abs(FMath::FindDeltaAngleDegrees(From->GetActorRotation().Yaw, YawTo(From, To)));
	}

	/** Lowest point of the character's feet (ankle and toe joints) above the capsule bottom, in cm. */
	float FeetAboveCapsuleBottom(const AMTCharacterBase* Character, FName AnkleL, FName AnkleR, FName ToeL, FName ToeR)
	{
		const USkeletalMeshComponent* Mesh = Character->GetMesh();
		const float Bottom = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		float Lowest = BIG_NUMBER;
		for (const FName Bone : { AnkleL, AnkleR, ToeL, ToeR })
		{
			Lowest = FMath::Min(Lowest, Mesh->GetBoneLocation(Bone).Z);
		}
		return Lowest - Bottom;
	}
}

// ---------------------------------------------------------------------------------------------------- data

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTOrstedDataTest, "MushokuRPG.Orsted.Data", MTTest::Flags)
bool FMTOrstedDataTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(Game.World);
	const FMTCharacterData* Orsted = Registry->FindCharacter(TEXT("Orsted"));
	const FMTAnimSetData* AnimSet = Registry->FindAnimSet(TEXT("Orsted"));
	if (!TestNotNull(TEXT("Orsted character row"), Orsted) || !TestNotNull(TEXT("Orsted AnimSet row"), AnimSet))
	{
		return false;
	}

	USkeletalMesh* Mesh = Orsted->Mesh.LoadSynchronous();
	TestNotNull(TEXT("SK_Orsted imported (Tools/mac/build_and_setup.sh)"), Mesh);
	const USkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
	if (Mesh)
	{
		const float Height = Mesh->GetBounds().BoxExtent.Z * 2.f;
		TestTrue(FString::Printf(TEXT("SK_Orsted is %.1f cm tall (185-205)"), Height), Height > 185.f && Height < 205.f);
		AddInfo(FString::Printf(TEXT("SK_Orsted: %.1f cm tall, %d bones"), Height, Mesh->GetRefSkeleton().GetNum()));
	}

	// Every clip the native instance samples and every signature ability plays is his own and loads on his skeleton.
	int32 Checked = 0;
	for (const TPair<FName, TSoftObjectPtr<UAnimSequenceBase>>& Pair : AnimSet->Anims)
	{
		const FString Path = Pair.Value.ToSoftObjectPath().ToString();
		TestTrue(FString::Printf(TEXT("%s uses an A_Orsted_ clip (%s)"), *Pair.Key.ToString(), *Path), Path.Contains(TEXT("/Orsted/Animations/A_Orsted_")));
		const UAnimSequence* Clip = Cast<UAnimSequence>(Pair.Value.LoadSynchronous());
		if (TestNotNull(FString::Printf(TEXT("clip %s loads"), *Path), Clip) && Skeleton)
		{
			TestTrue(FString::Printf(TEXT("clip %s is on SK_Orsted's skeleton"), *Path), Clip->GetSkeleton() == Skeleton);
		}
		++Checked;
	}
	TestTrue(FString::Printf(TEXT("AnimSet has the 40 authored keys (%d)"), Checked), Checked >= 40);
	AddInfo(FString::Printf(TEXT("%d AnimSet clips load on SK_Orsted's skeleton"), Checked));
	for (const FName AbilityId : { FName("Orsted_Basic"), FName("Orsted_DisturbMagic"), FName("Orsted_DragonStep"),
		FName("Orsted_DragonStep_Awakened"), FName("Orsted_SaintDragonAura"), FName("Orsted_Awakening_DragonGod") })
	{
		const FMTAbilityData* Row = Registry->FindAbility(AbilityId);
		if (TestNotNull(FString::Printf(TEXT("ability %s"), *AbilityId.ToString()), Row))
		{
			TestTrue(FString::Printf(TEXT("%s plays an Orsted clip"), *AbilityId.ToString()),
				Row->Montage.ToSoftObjectPath().ToString().Contains(TEXT("A_Orsted_")));
			TestNotNull(FString::Printf(TEXT("%s clip loads"), *AbilityId.ToString()), Row->Montage.LoadSynchronous());
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------- body + pose

namespace MTTest
{
	/** Applies the lineage, lets it settle and animate for 1.5 s, then checks the body and the pose. */
	void CheckLineageBodyAndPose(FAutomationTestBase& Test, FName Lineage, float CapsuleRadius, float CapsuleHalfHeight,
		float MinHeadAboveFeet, float MaxHeadAboveFeet)
	{
		FGameWorld Game;
		if (!Test.TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
		{
			return;
		}
		AMTPlayerCharacter* Character = Game.SpawnPlayer(Lineage, FVector::ZeroVector);
		if (!Test.TestNotNull(TEXT("character spawned"), Character))
		{
			return;
		}
		Game.Tick(1.5f);
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		Test.TestEqual(TEXT("capsule radius"), Capsule->GetUnscaledCapsuleRadius(), CapsuleRadius, 0.01f);
		Test.TestEqual(TEXT("capsule half height"), Capsule->GetUnscaledCapsuleHalfHeight(), CapsuleHalfHeight, 0.01f);
		Test.TestTrue(TEXT("standing on the floor"), Character->GetCharacterMovement()->IsMovingOnGround());
		USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (!Test.TestNotNull(FString::Printf(TEXT("SK_%s assigned"), *Lineage.ToString()), Mesh->GetSkeletalMeshAsset()))
		{
			return;
		}
		Test.TestTrue(TEXT("graph-free native anim instance"), Mesh->GetAnimInstance() && Mesh->GetAnimInstance()->IsA<UMTNativeAnimInstance>());

		// Feet on the floor: the ankles sit at ankle height above the capsule bottom (Rudeus 11.4 cm, Orsted 14.1 cm).
		const float Feet = FeetAboveCapsuleBottom(Character, TEXT("leg_L0_2_jnt_010"), TEXT("leg_R0_2_jnt_015"),
			TEXT("leg_L0_3_jnt_011"), TEXT("leg_R0_3_jnt_016"));
		Test.TestTrue(FString::Printf(TEXT("feet joints %.1f cm above the capsule bottom (0-16: planted, not floating or sunk)"), Feet),
			Feet > 0.f && Feet < 16.f);
		const float Head = Mesh->GetBoneLocation(TEXT("head_C0_0_jnt_067")).Z - (Character->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight());
		Test.TestTrue(FString::Printf(TEXT("head joint %.1f cm above the floor (%.0f-%.0f)"), Head, MinHeadAboveFeet, MaxHeadAboveFeet),
			Head > MinHeadAboveFeet && Head < MaxHeadAboveFeet);

		// Animated, not the reference T-pose: the idle's hands hang (or are clasped) well below the shoulders.
		const float Shoulder = Mesh->GetBoneLocation(TEXT("arm_L0_0_jnt_092")).Z;
		const float Hand = Mesh->GetBoneLocation(TEXT("arm_L0_2_jnt_094")).Z;
		Test.TestTrue(FString::Printf(TEXT("left hand %.1f cm below the shoulder (idle playing, no T-pose)"), Shoulder - Hand), Shoulder - Hand > 25.f);
		Test.AddInfo(FString::Printf(TEXT("%s: capsule %.0f / %.0f, on the ground %s, feet joints %.1f cm above the capsule bottom, head joint %.1f cm, left hand %.1f cm below the shoulder"),
			*Lineage.ToString(), CapsuleRadius, CapsuleHalfHeight, Character->GetCharacterMovement()->IsMovingOnGround() ? TEXT("yes") : TEXT("no"), Feet, Head, Shoulder - Hand));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTOrstedBodyPoseTest, "MushokuRPG.Orsted.BodyAndPose", MTTest::Flags)
bool FMTOrstedBodyPoseTest::RunTest(const FString& Parameters)
{
	MTTest::CheckLineageBodyAndPose(*this, TEXT("Orsted"), 38.f, 98.f, 165.f, 190.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTRudeusBodyPoseTest, "MushokuRPG.Rudeus.BodyAndPose", MTTest::Flags)
bool FMTRudeusBodyPoseTest::RunTest(const FString& Parameters)
{
	MTTest::CheckLineageBodyAndPose(*this, TEXT("Rudeus"), 32.f, 81.f, 125.f, 150.f);
	return true;
}

// ---------------------------------------------------------------------------------------------------- lock-on

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTOrstedLockOnFacingTest, "MushokuRPG.Orsted.LockOnFacing", MTTest::Flags)
bool FMTOrstedLockOnFacingTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Orsted = Game.SpawnPlayer(TEXT("Orsted"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Target = Game.SpawnOpponent(TEXT("Arena_Rudeus"), FVector(0.f, 700.f, 0.f));
	if (!TestNotNull(TEXT("Orsted"), Orsted) || !TestNotNull(TEXT("target"), Target))
	{
		return false;
	}
	Game.Tick(0.3f);
	TestTrue(TEXT("the target is hostile"), Orsted->IsHostileTo(Target));
	Orsted->SetLockTarget(Target);
	Game.Tick(1.0f);
	TestTrue(FString::Printf(TEXT("locked on: faces the target (%.1f deg off)"), MTTest::YawError(Orsted, Target)), MTTest::YawError(Orsted, Target) < 3.f);
	AddInfo(FString::Printf(TEXT("lock-on from 90 deg off: %.2f deg off after 1.0 s"), MTTest::YawError(Orsted, Target)));
	TestTrue(TEXT("locked on: strafing (directional clips)"), Orsted->IsStrafingLocked());
	TestFalse(TEXT("locked on: no orient-to-movement"), Orsted->GetCharacterMovement()->bOrientRotationToMovement);
	if (const UMTAnimInstance* Anim = Cast<UMTAnimInstance>(Orsted->GetMesh()->GetAnimInstance()))
	{
		TestTrue(TEXT("anim instance strafes while locked on"), Anim->IsStrafing());
	}

	// The target moves around him: he keeps turning to it.
	Target->SetActorLocation(FVector(-650.f, 250.f, Target->GetActorLocation().Z));
	Game.Tick(0.8f);
	TestTrue(FString::Printf(TEXT("tracks a moving target (%.1f deg off)"), MTTest::YawError(Orsted, Target)), MTTest::YawError(Orsted, Target) < 3.f);
	AddInfo(FString::Printf(TEXT("target moved behind him: %.2f deg off after 0.8 s"), MTTest::YawError(Orsted, Target)));

	// A locked-on sprint breaks the facing (runs where it is steered) instead of strafing faster than any clip.
	Orsted->SetSprinting(true);
	Orsted->GetCharacterMovement()->Velocity = FVector(0.f, -650.f, 0.f);
	TestFalse(TEXT("sprinting away: not strafing"), Orsted->IsStrafingLocked());
	Game.Tick(1.f / 60.f);
	TestTrue(TEXT("sprinting away: orient to movement"), Orsted->GetCharacterMovement()->bOrientRotationToMovement);
	Orsted->SetSprinting(false);
	Game.Tick(0.8f);
	TestTrue(TEXT("sprint released: strafing again"), Orsted->IsStrafingLocked());
	TestTrue(FString::Printf(TEXT("sprint released: faces the target again (%.1f deg off)"), MTTest::YawError(Orsted, Target)), MTTest::YawError(Orsted, Target) < 3.f);
	AddInfo(FString::Printf(TEXT("sprint released: %.2f deg off after 0.8 s"), MTTest::YawError(Orsted, Target)));

	// Released lock: movement orientation again.
	Orsted->SetLockTarget(nullptr);
	Game.Tick(1.f / 60.f);
	TestTrue(TEXT("lock released: orient to movement"), Orsted->GetCharacterMovement()->bOrientRotationToMovement);
	return true;
}

// ---------------------------------------------------------------------------------------------------- abilities

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTOrstedDragonStepTest, "MushokuRPG.Orsted.DragonStep", MTTest::Flags)
bool FMTOrstedDragonStepTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Orsted = Game.SpawnPlayer(TEXT("Orsted"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Target = Game.SpawnOpponent(TEXT("Arena_Rudeus"), FVector(650.f, 180.f, 0.f));
	if (!TestNotNull(TEXT("Orsted"), Orsted) || !TestNotNull(TEXT("target"), Target))
	{
		return false;
	}
	Game.Tick(0.4f);
	Orsted->SetLockTarget(Target);
	const float StartDistance = FVector::Dist2D(Orsted->GetActorLocation(), Target->GetActorLocation());
	const float HealthBefore = Target->GetAttributes()->GetHealth();
	TestTrue(TEXT("Dragon Step activates"), Orsted->GetAbilities()->ActivateAbilityById(TEXT("Orsted_DragonStep")));
	Game.Tick(0.9f);
	const float EndDistance = FVector::Dist2D(Orsted->GetActorLocation(), Target->GetActorLocation());
	TestTrue(FString::Printf(TEXT("glides to the target (%.0f -> %.0f cm)"), StartDistance, EndDistance), EndDistance < 260.f && EndDistance > 60.f);
	TestTrue(FString::Printf(TEXT("the arrival palm lands (target health %.1f -> %.1f)"), HealthBefore, Target->GetAttributes()->GetHealth()),
		Target->GetAttributes()->GetHealth() < HealthBefore);
	TestTrue(FString::Printf(TEXT("faces the target after the step (%.1f deg off)"), MTTest::YawError(Orsted, Target)), MTTest::YawError(Orsted, Target) < 5.f);
	TestFalse(TEXT("no dodge / dash tag left behind"), Orsted->GetStateTags().HasTag(MTTags::State_Dodging));
	AddInfo(FString::Printf(TEXT("Dragon Step: %.0f -> %.0f cm from the target, target health %.1f -> %.1f, %.2f deg off afterwards"),
		StartDistance, EndDistance, HealthBefore, Target->GetAttributes()->GetHealth(), MTTest::YawError(Orsted, Target)));

	// A movement dash (Gale Step: no damage, no stagger) never strikes.
	AMTPlayerCharacter* Mover = Game.SpawnPlayer(TEXT("Rudeus"), FVector(-900.f, -900.f, 0.f), 0.f);
	AMTEnemyCharacter* Bystander = Game.SpawnOpponent(TEXT("Arena_Rudeus"), FVector(-500.f, -900.f, 0.f));
	Game.Tick(0.4f);
	const float BystanderHealth = Bystander->GetAttributes()->GetHealth();
	Mover->ApplyElementSlot(0, EMTElement::Wind);
	TestTrue(TEXT("Gale Step activates"), Mover->GetAbilities()->ActivateAbilityById(TEXT("Wind_GaleStep")));
	Game.Tick(0.8f);
	TestEqual(TEXT("a movement dash deals no damage"), Bystander->GetAttributes()->GetHealth(), BystanderHealth);
	AddInfo(FString::Printf(TEXT("Gale Step past a bystander: health %.1f -> %.1f"), BystanderHealth, Bystander->GetAttributes()->GetHealth()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTOrstedDisturbMagicTest, "MushokuRPG.Orsted.DisturbMagic", MTTest::Flags)
bool FMTOrstedDisturbMagicTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Orsted = Game.SpawnPlayer(TEXT("Orsted"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Rudeus = Game.SpawnOpponent(TEXT("Arena_Rudeus"), FVector(900.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Orsted"), Orsted) || !TestNotNull(TEXT("Rudeus"), Rudeus))
	{
		return false;
	}
	Game.Tick(0.4f);
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(Game.World);
	const FMTAbilityData* Bolt = Registry->FindAbility(TEXT("Rudeus_Basic"));
	if (!TestNotNull(TEXT("Rudeus_Basic row"), Bolt))
	{
		return false;
	}

	// Whiff: nothing to disturb -> no Dragon God Knowledge, full cooldown.
	TestTrue(TEXT("Disturb Magic activates"), Orsted->GetAbilities()->ActivateAbilityById(TEXT("Orsted_DisturbMagic")));
	Game.Tick(1.0f);
	TestFalse(TEXT("a whiff grants no Dragon God Knowledge"), Orsted->GetAttributes()->HasStatusEffect(TEXT("DragonGodKnowledge")));
	const float WhiffCooldown = Orsted->GetAbilities()->GetCooldownRemaining(TEXT("Orsted_DisturbMagic"));
	TestTrue(FString::Printf(TEXT("a whiff keeps most of the cooldown (%.2f s left)"), WhiffCooldown), WhiffCooldown > 3.5f);
	AddInfo(FString::Printf(TEXT("whiff: no Dragon God Knowledge, %.2f s of the 6 s cooldown left after 1 s"), WhiffCooldown));
	Game.Tick(6.f); // cooldown

	// A stone bullet from Rudeus, answered inside the window: the spell collapses and Orsted takes nothing.
	const float HealthBefore = Orsted->GetAttributes()->GetHealth();
	const FVector From = Rudeus->GetActorLocation() + FVector(-60.f, 0.f, 40.f);
	AMTProjectile* Spell = UMTAbility_Projectile::FireProjectile(Rudeus, *Bolt, From, Orsted->GetActorLocation(), 0.f);
	if (!TestNotNull(TEXT("stone bullet fired"), Spell))
	{
		return false;
	}
	TWeakObjectPtr<AMTProjectile> WeakSpell(Spell);
	// Wait until it is inside the 450 cm radius, then answer.
	for (int32 Frame = 0; Frame < 120 && WeakSpell.IsValid() && FVector::Dist(WeakSpell->GetActorLocation(), Orsted->GetActorLocation()) > 420.f; ++Frame)
	{
		Game.Tick(1.f / 60.f);
	}
	TestTrue(TEXT("Disturb Magic activates in time"), Orsted->GetAbilities()->ActivateAbilityById(TEXT("Orsted_DisturbMagic")));
	Game.Tick(0.6f);
	TestTrue(TEXT("the spell collapsed (stopped, then removed)"), !WeakSpell.IsValid() || WeakSpell->GetVelocity().IsNearlyZero());
	TestEqual(TEXT("Orsted took no damage"), Orsted->GetAttributes()->GetHealth(), HealthBefore);
	TestTrue(TEXT("success grants Dragon God Knowledge"), Orsted->GetAttributes()->HasStatusEffect(TEXT("DragonGodKnowledge")));
	const float SuccessCooldown = Orsted->GetAbilities()->GetCooldownRemaining(TEXT("Orsted_DisturbMagic"));
	TestTrue(FString::Printf(TEXT("success refunds most of the cooldown (%.2f s left)"), SuccessCooldown), SuccessCooldown < 3.f);
	AddInfo(FString::Printf(TEXT("timed Disturb Magic vs a Rudeus stone bullet: spell %s, Orsted health %.1f -> %.1f, Dragon God Knowledge %s, %.2f s cooldown left"),
		(!WeakSpell.IsValid() || WeakSpell->GetVelocity().IsNearlyZero()) ? TEXT("collapsed") : TEXT("still flying"), HealthBefore,
		Orsted->GetAttributes()->GetHealth(), Orsted->GetAttributes()->HasStatusEffect(TEXT("DragonGodKnowledge")) ? TEXT("on") : TEXT("off"), SuccessCooldown));
	return true;
}

// ---------------------------------------------------------------------------------------------------- presentation

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTPresentationFallbackTest, "MushokuRPG.Combat.PresentationFallbacks", MTTest::Flags)
bool FMTPresentationFallbackTest::RunTest(const FString& Parameters)
{
	// Data names meshes, VFX and sounds that are not authored yet (SM_StoneBullet, SM_EarthWall ...). A spell or wall
	// must then fall back to its blockout look: never invisible, never an invisible wall that still blocks.
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Rudeus = Game.SpawnPlayer(TEXT("Rudeus"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Target = Game.SpawnOpponent(TEXT("Arena_Orsted"), FVector(1500.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Rudeus"), Rudeus) || !TestNotNull(TEXT("target"), Target))
	{
		return false;
	}
	Game.Tick(0.4f);
	const FMTAbilityData* Bolt = UMTDataRegistry::Get(Game.World)->FindAbility(TEXT("Rudeus_Basic"));
	AMTProjectile* Spell = Bolt ? UMTAbility_Projectile::FireProjectile(Rudeus, *Bolt, Rudeus->GetActorLocation() + FVector(60.f, 0.f, 40.f),
		Target->GetActorLocation(), 0.f) : nullptr;
	if (TestNotNull(TEXT("stone bullet fired"), Spell))
	{
		// The runtime travel effect (attached to the projectile) draws the bullet and hides the body mesh; without one
		// the authored mesh or the blockout must show.
		const UStaticMeshComponent* Body = Spell->FindComponentByClass<UStaticMeshComponent>();
		const bool bBodyVisible = Body && Body->GetStaticMesh() != nullptr && Body->IsVisible();
		TArray<AActor*> Attached;
		Spell->GetAttachedActors(Attached);
		const bool bTravelEffect = Attached.ContainsByPredicate([](const AActor* Actor) { return Actor && Actor->IsA<AMTSpellVFX>(); });
		TestTrue(TEXT("the stone bullet is visible (runtime travel effect, authored mesh or blockout)"), bTravelEffect || bBodyVisible);
		AddInfo(FString::Printf(TEXT("stone bullet: travel effect %s, body %s"), bTravelEffect ? TEXT("attached") : TEXT("none"),
			bBodyVisible ? *GetNameSafe(Body->GetStaticMesh()) : TEXT("hidden")));
	}

	Rudeus->SetLockTarget(Target);
	Rudeus->ApplyElementSlot(0, EMTElement::Earth);
	TestTrue(TEXT("Earth Wall activates"), Rudeus->GetAbilities()->ActivateAbilityById(TEXT("Earth_EarthWall")));
	Game.Tick(1.5f);
	int32 Walls = 0;
	for (TActorIterator<AMTEarthWall> It(Game.World); It; ++It)
	{
		const UStaticMeshComponent* WallMesh = It->FindComponentByClass<UStaticMeshComponent>();
		TestTrue(FString::Printf(TEXT("%s is visible (authored mesh or blockout)"), *It->GetName()), WallMesh && WallMesh->GetStaticMesh() != nullptr);
		++Walls;
	}
	TestTrue(FString::Printf(TEXT("Earth Wall raised walls (%d)"), Walls), Walls > 0);
	AddInfo(FString::Printf(TEXT("Earth Wall raised %d walls, all with a visible mesh"), Walls));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTSpellLeavesCasterTest, "MushokuRPG.Combat.SpellsLeaveTheCaster", MTTest::Flags)
bool FMTSpellLeavesCasterTest::RunTest(const FString& Parameters)
{
	// Cast through the real ability path (hand socket inside the caster's capsule). The spawn overlap used to run
	// before the projectile knew its caster, so every bolt hit the player for 10 and burst in their hand.
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Rudeus = Game.SpawnPlayer(TEXT("Rudeus"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Target = Game.SpawnOpponent(TEXT("Arena_Orsted"), FVector(900.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Rudeus"), Rudeus) || !TestNotNull(TEXT("target"), Target))
	{
		return false;
	}
	Game.Tick(0.4f);
	Rudeus->SetLockTarget(Target);
	const float CasterBefore = Rudeus->GetAttributes()->GetHealth();
	const float TargetBefore = Target->GetAttributes()->GetHealth();
	TestTrue(TEXT("stone bullet activates"), Rudeus->GetAbilities()->ActivateAbilityById(TEXT("Rudeus_Basic")));
	Rudeus->GetAbilities()->ReleaseAbilityById(TEXT("Rudeus_Basic"));
	Game.Tick(2.f);
	const float CasterAfter = Rudeus->GetAttributes()->GetHealth();
	const float TargetAfter = Target->GetAttributes()->GetHealth();
	TestEqual(TEXT("the caster takes no damage from their own spell"), CasterAfter, CasterBefore);
	TestTrue(TEXT("the bolt reaches the target 9 m away"), TargetAfter < TargetBefore);
	AddInfo(FString::Printf(TEXT("caster %.0f -> %.0f, target %.0f -> %.0f"), CasterBefore, CasterAfter, TargetBefore, TargetAfter));
	return true;
}

// ---------------------------------------------------------------------------------------------------- ability overhaul
// Docs/Ability_Overhaul.md: every upgraded ability does what the design says, from the hotbar path, and leaves nothing
// behind.

namespace MTTest
{
	/** Distance (2D) between two actors. */
	float Apart(const AActor* A, const AActor* B)
	{
		return FVector::Dist2D(A->GetActorLocation(), B->GetActorLocation());
	}

	int32 CountLive(UWorld* World, UClass* Class)
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(World, Class); It; ++It)
		{
			if (!It->IsActorBeingDestroyed())
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTOverhaulDataTest, "MushokuRPG.Abilities.OverhaulData", MTTest::Flags)
bool FMTOverhaulDataTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	const UMTDataRegistry* Registry = UMTDataRegistry::Get(Game.World);
	struct FExpect { const TCHAR* Id; EMTAbilityBehavior Behavior; };
	const FExpect Expected[] = {
		{ TEXT("Rudeus_StoneCannon"), EMTAbilityBehavior::Projectile }, { TEXT("Rudeus_Quagmire"), EMTAbilityBehavior::Zone },
		{ TEXT("Rudeus_ElementalBarrage"), EMTAbilityBehavior::Barrage }, { TEXT("Orsted_DisturbMagic"), EMTAbilityBehavior::Disrupt },
		{ TEXT("Orsted_DragonStep"), EMTAbilityBehavior::Dash }, { TEXT("Orsted_DragonCrush"), EMTAbilityBehavior::Strike },
		{ TEXT("Fire_Fireball"), EMTAbilityBehavior::Projectile }, { TEXT("Fire_FlameWave"), EMTAbilityBehavior::Zone },
		{ TEXT("Fire_Inferno"), EMTAbilityBehavior::Zone }, { TEXT("Water_WaterBullet"), EMTAbilityBehavior::Projectile },
		{ TEXT("Water_WaterDragon"), EMTAbilityBehavior::Serpent }, { TEXT("Water_Flood"), EMTAbilityBehavior::Zone },
		{ TEXT("Earth_StoneCannon"), EMTAbilityBehavior::Projectile }, { TEXT("Earth_EarthWall"), EMTAbilityBehavior::Structure },
		{ TEXT("Earth_EarthSpikes"), EMTAbilityBehavior::Zone }, { TEXT("Wind_WindBlade"), EMTAbilityBehavior::Projectile },
		{ TEXT("Wind_Tornado"), EMTAbilityBehavior::Zone }, { TEXT("Wind_WindBurst"), EMTAbilityBehavior::Zone },
	};
	for (const FExpect& E : Expected)
	{
		const FMTAbilityData* Row = Registry->FindAbility(E.Id);
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), E.Id), Row))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s uses its overhaul behaviour"), E.Id), Row->Behavior == E.Behavior);
		TestFalse(FString::Printf(TEXT("%s has a runtime effect preset"), E.Id), Row->FX.Preset.IsNone());
		TestFalse(FString::Printf(TEXT("%s has a casting animation"), E.Id), Row->Montage.IsNull());
		TestTrue(FString::Printf(TEXT("%s hitbox is honest (x%.2f)"), E.Id, Row->HitForgiveness), Row->HitForgiveness >= 1.f && Row->HitForgiveness <= 1.15f);
	}
	for (const FName CharacterId : { FName(TEXT("Rudeus")), FName(TEXT("Orsted")) })
	{
		const FMTCharacterData* Character = Registry->FindCharacter(CharacterId);
		if (TestNotNull(*FString::Printf(TEXT("%s row"), *CharacterId.ToString()), Character))
		{
			TestEqual(*FString::Printf(TEXT("%s default loadout fills keys 1-4"), *CharacterId.ToString()), Character->DefaultLoadout.Num(), 4);
			for (const FName Id : Character->DefaultLoadout)
			{
				TestNotNull(*FString::Printf(TEXT("%s loadout ability %s"), *CharacterId.ToString(), *Id.ToString()), Registry->FindAbility(Id));
			}
		}
	}
	if (const FMTAbilityData* Step = Registry->FindAbility(TEXT("Orsted_DragonStep")))
	{
		TestTrue(TEXT("Dragon Step flows into Dragon Crush"), Step->ComboFollowUp == FName(TEXT("Orsted_DragonCrush")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTStoneCannonPierceTest, "MushokuRPG.Abilities.StoneCannonPierces", MTTest::Flags)
bool FMTStoneCannonPierceTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Rudeus = Game.SpawnPlayer(TEXT("Rudeus"), FVector::ZeroVector, 0.f);
	TArray<AMTEnemyCharacter*> Line;
	for (int32 i = 0; i < 4; ++i)
	{
		Line.Add(Game.SpawnOpponent(TEXT("Enemy_Goblin"), FVector(600.f + 220.f * i, 0.f, 0.f)));
	}
	AMTEnemyCharacter* Heavy = Game.SpawnOpponent(TEXT("Elite_WolfAlpha"), FVector(600.f, 900.f, 0.f));
	if (!TestNotNull(TEXT("Rudeus"), Rudeus) || Line.Contains(nullptr) || !TestNotNull(TEXT("heavy target"), Heavy))
	{
		return false;
	}
	Game.Tick(0.4f);
	const FMTAbilityData* Cannon = UMTDataRegistry::Get(Game.World)->FindAbility(TEXT("Rudeus_StoneCannon"));
	if (!TestNotNull(TEXT("Rudeus_StoneCannon row"), Cannon))
	{
		return false;
	}
	// Through a line of light enemies: the first three are passed through, the fourth stops it.
	TArray<float> Before;
	for (AMTEnemyCharacter* Goblin : Line)
	{
		Before.Add(Goblin->GetAttributes()->GetHealth());
	}
	const FVector From = Rudeus->GetActorLocation() + FVector(60.f, 0.f, 0.f);
	UMTAbility_Projectile::FireProjectile(Rudeus, *Cannon, From, From + FVector(3000.f, 0.f, 0.f), 0.f);
	Game.Tick(0.6f);
	int32 Damaged = 0;
	for (int32 i = 0; i < Line.Num(); ++i)
	{
		const bool bHit = !IsValid(Line[i]) || Line[i]->GetAttributes()->GetHealth() < Before[i];
		Damaged += bHit ? 1 : 0;
		TestTrue(FString::Printf(TEXT("goblin %d in the line is hit"), i + 1), bHit);
	}
	// A heavy target (900 max health) stops it on the first contact.
	const float HeavyBefore = Heavy->GetAttributes()->GetHealth();
	const FVector HeavyFrom = Rudeus->GetActorLocation() + FVector(60.f, 900.f, 0.f);
	AMTProjectile* Second = UMTAbility_Projectile::FireProjectile(Rudeus, *Cannon, HeavyFrom, HeavyFrom + FVector(3000.f, 0.f, 0.f), 0.f);
	TWeakObjectPtr<AMTProjectile> WeakSecond(Second);
	Game.Tick(0.3f);
	TestTrue(TEXT("the heavy target is hit"), Heavy->GetAttributes()->GetHealth() < HeavyBefore);
	TestTrue(TEXT("and the cannon stops on it"), !WeakSecond.IsValid() || WeakSecond->GetVelocity().IsNearlyZero());
	AddInfo(FString::Printf(TEXT("pierce: %d of 4 goblins hit; heavy target %.0f -> %.0f"), Damaged, HeavyBefore, Heavy->GetAttributes()->GetHealth()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTQuagmireDepthTest, "MushokuRPG.Abilities.QuagmireDepthAndSink", MTTest::Flags)
bool FMTQuagmireDepthTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Rudeus = Game.SpawnPlayer(TEXT("Rudeus"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Centre = Game.SpawnOpponent(TEXT("Enemy_Bandit"), FVector(1500.f, 0.f, 0.f));
	AMTEnemyCharacter* Edge = Game.SpawnOpponent(TEXT("Enemy_Bandit"), FVector(1500.f, 820.f, 0.f));
	AMTEnemyCharacter* Outside = Game.SpawnOpponent(TEXT("Enemy_Bandit"), FVector(1500.f, -1400.f, 0.f));
	const FMTAbilityData* Row = UMTDataRegistry::Get(Game.World)->FindAbility(TEXT("Rudeus_Quagmire"));
	if (!TestNotNull(TEXT("Rudeus"), Rudeus) || !TestNotNull(TEXT("centre"), Centre) || !TestNotNull(TEXT("edge"), Edge)
		|| !TestNotNull(TEXT("outside"), Outside) || !TestNotNull(TEXT("Rudeus_Quagmire row"), Row))
	{
		return false;
	}
	Game.Tick(0.4f);
	FActorSpawnParameters Params;
	Params.Owner = Rudeus;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMTZoneActor* Zone = Game.World->SpawnActor<AMTZoneActor>(AMTZoneActor::StaticClass(), FVector(1500.f, 0.f, 0.f), FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("quagmire zone"), Zone))
	{
		return false;
	}
	Zone->InitZone(*Row, Rudeus, 1.f, false);
	Game.Tick(0.2f);
	const float EarlySpeed = Centre->GetCharacterMovement()->MaxWalkSpeed;
	Game.Tick(1.8f); // past the 1.2 s transformation: the mud has fully formed
	const float CentreSpeed = Centre->GetCharacterMovement()->MaxWalkSpeed;
	const float EdgeSpeed = Edge->GetCharacterMovement()->MaxWalkSpeed;
	const float FreeSpeed = Outside->GetCharacterMovement()->MaxWalkSpeed;
	TestTrue(TEXT("the centre is in the quagmire"), Centre->GetStateTags().HasTag(MTTags::State_InQuagmire));
	TestTrue(TEXT("the edge is in the quagmire"), Edge->GetStateTags().HasTag(MTTags::State_InQuagmire));
	TestFalse(TEXT("outside is not"), Outside->GetStateTags().HasTag(MTTags::State_InQuagmire));
	TestTrue(FString::Printf(TEXT("the mud bites harder once formed (%.0f -> %.0f)"), EarlySpeed, CentreSpeed), CentreSpeed < EarlySpeed || EarlySpeed <= 0.f);
	TestTrue(FString::Printf(TEXT("deepest at the centre (%.0f < %.0f < %.0f cm/s)"), CentreSpeed, EdgeSpeed, FreeSpeed), CentreSpeed < EdgeSpeed && EdgeSpeed < FreeSpeed);
	TestTrue(FString::Printf(TEXT("bodies sink into it (%.1f cm at the centre)"), Centre->GetMudSink()), Centre->GetMudSink() > 12.f);
	TestTrue(TEXT("less at the edge"), Edge->GetMudSink() < Centre->GetMudSink());
	Game.Tick(10.f); // 9 s zone: dried
	TestFalse(TEXT("the quagmire dries"), Centre->GetStateTags().HasTag(MTTags::State_InQuagmire));
	TestTrue(FString::Printf(TEXT("bodies climb back out (%.1f cm)"), Centre->GetMudSink()), Centre->GetMudSink() < 0.5f);
	AddInfo(FString::Printf(TEXT("quagmire: speed centre %.0f, edge %.0f, outside %.0f cm/s; sink centre %.1f cm"), CentreSpeed, EdgeSpeed, FreeSpeed,
		Centre->GetMudSink()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTDisturbSealTest, "MushokuRPG.Orsted.DisturbMagicSeals", MTTest::Flags)
bool FMTDisturbSealTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Orsted = Game.SpawnPlayer(TEXT("Orsted"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Rudeus = Game.SpawnOpponent(TEXT("Arena_Rudeus"), FVector(1200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Orsted"), Orsted) || !TestNotNull(TEXT("Rudeus"), Rudeus))
	{
		return false;
	}
	Game.Tick(0.4f);
	// Rudeus starts charging a Stone Cannon (held: it stays in its forming phase).
	UMTAbilityComponent* RudeusAbilities = Rudeus->GetAbilities();
	TestTrue(TEXT("Rudeus starts forming a Stone Cannon"), RudeusAbilities->ActivateAbilityById(TEXT("Rudeus_StoneCannon")));
	Game.Tick(0.2f);
	const UMTAbility* Forming = RudeusAbilities->GetActiveAbility();
	TestTrue(TEXT("the spell is still forming"), Forming && Forming->GetPhase() == EMTAbilityPhase::Anticipation);
	Orsted->SetLockTarget(Rudeus);
	TestTrue(TEXT("Disturb Magic activates"), Orsted->GetAbilities()->ActivateAbilityById(TEXT("Orsted_DisturbMagic")));
	Game.Tick(0.6f);
	const UMTAbility* After = RudeusAbilities->GetActiveAbility();
	TestTrue(TEXT("the forming spell collapsed"), !After || After->GetAbilityId() != FName(TEXT("Rudeus_StoneCannon")));
	TestTrue(TEXT("and is sealed for a moment"), RudeusAbilities->IsAbilityLocked(TEXT("Rudeus_StoneCannon")));
	TestFalse(TEXT("a sealed spell cannot be cast"), RudeusAbilities->ActivateAbilityById(TEXT("Rudeus_StoneCannon")));
	TestTrue(TEXT("other magic still works (the other hand)"), RudeusAbilities->ActivateAbilityById(TEXT("Rudeus_Basic")));
	TestTrue(TEXT("Dragon God Knowledge triggers"), Orsted->GetAttributes()->HasStatusEffect(TEXT("DragonGodKnowledge")));
	TestTrue(FString::Printf(TEXT("most of the cooldown comes back (%.2f s)"), Orsted->GetAbilities()->GetCooldownRemaining(TEXT("Orsted_DisturbMagic"))),
		Orsted->GetAbilities()->GetCooldownRemaining(TEXT("Orsted_DisturbMagic")) < 3.f);
	const float RudeusHealth = Rudeus->GetAttributes()->GetHealth();
	TestEqual(TEXT("Disturb Magic deals no damage"), RudeusHealth, Rudeus->GetAttributes()->GetMaxHealth());
	Game.Tick(3.2f);
	TestFalse(TEXT("the seal wears off (never a permanent silence)"), RudeusAbilities->IsAbilityLocked(TEXT("Rudeus_StoneCannon")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTStepCrushComboTest, "MushokuRPG.Orsted.DragonStepIntoDragonCrush", MTTest::Flags)
bool FMTStepCrushComboTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Orsted = Game.SpawnPlayer(TEXT("Orsted"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Target = Game.SpawnOpponent(TEXT("Elite_WolfAlpha"), FVector(1200.f, 150.f, 0.f));
	AMTEnemyCharacter* Bystander = Game.SpawnOpponent(TEXT("Enemy_Bandit"), FVector(1300.f, -300.f, 0.f));
	if (!TestNotNull(TEXT("Orsted"), Orsted) || !TestNotNull(TEXT("target"), Target) || !TestNotNull(TEXT("bystander"), Bystander))
	{
		return false;
	}
	Game.Tick(0.4f);
	Orsted->SetLockTarget(Target);
	UMTAbilityComponent* Abilities = Orsted->GetAbilities();
	TestTrue(TEXT("Dragon Step activates"), Abilities->ActivateAbilityById(TEXT("Orsted_DragonStep")));
	Game.Tick(0.3f);
	const float Beside = MTTest::Apart(Orsted, Target);
	TestTrue(FString::Printf(TEXT("arrives beside the target (%.0f cm)"), Beside), Beside > 60.f && Beside < 200.f);
	TestTrue(TEXT("the follow-up window is open"), Abilities->IsComboWindowOpen(TEXT("Orsted_DragonCrush")));
	const float TargetBefore = Target->GetAttributes()->GetHealth();
	const float BystanderBefore = Bystander->GetAttributes()->GetHealth();
	TestTrue(TEXT("Dragon Crush activates out of the step"), Abilities->ActivateAbilityById(TEXT("Orsted_DragonCrush")));
	const UMTAbility* Crush = Abilities->GetActiveAbility();
	TestTrue(TEXT("it is the combo version"), Crush && Crush->IsComboActivation());
	Game.Tick(0.2f); // the combo wind-up is ~0.14 s
	const float Dealt = TargetBefore - Target->GetAttributes()->GetHealth();
	TestTrue(FString::Printf(TEXT("the primary target takes the full force (%.0f)"), Dealt), Dealt >= 300.f);
	Game.Tick(0.3f);
	TestTrue(TEXT("the shockwave hits the bystander too"), Bystander->GetAttributes()->GetHealth() < BystanderBefore);
	Game.Tick(0.6f); // the launch is in flight: measure where it carried the target
	TestTrue(FString::Printf(TEXT("the primary target is thrown (%.0f cm away)"), MTTest::Apart(Orsted, Target)), MTTest::Apart(Orsted, Target) > 300.f);
	AddInfo(FString::Printf(TEXT("step -> crush: arrived %.0f cm beside, primary took %.0f, bystander %.0f -> %.0f"), Beside, Dealt, BystanderBefore,
		Bystander->GetAttributes()->GetHealth()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTWindBurstTest, "MushokuRPG.Abilities.WindBurstEscape", MTTest::Flags)
bool FMTWindBurstTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Rudeus = Game.SpawnPlayer(TEXT("Rudeus"), FVector::ZeroVector, 0.f);
	TArray<AMTEnemyCharacter*> Around;
	for (int32 i = 0; i < 4; ++i)
	{
		const float Angle = i * HALF_PI;
		Around.Add(Game.SpawnOpponent(TEXT("Enemy_Bandit"), FVector(FMath::Cos(Angle) * 300.f, FMath::Sin(Angle) * 300.f, 0.f)));
	}
	if (!TestNotNull(TEXT("Rudeus"), Rudeus) || Around.Contains(nullptr))
	{
		return false;
	}
	Game.Tick(0.4f);
	TestTrue(TEXT("Wind Burst activates"), Rudeus->GetAbilities()->ActivateAbilityById(TEXT("Wind_WindBurst")));
	Game.Tick(1.8f); // the burst goes off on the cast's Release frame; the bandits land about a second later
	for (int32 i = 0; i < Around.Num(); ++i)
	{
		const float Now = MTTest::Apart(Rudeus, Around[i]);
		TestTrue(FString::Printf(TEXT("enemy %d is thrown away in every direction (%.0f cm)"), i + 1, Now), Now > 700.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTFloodCarryTest, "MushokuRPG.Abilities.FloodCarries", MTTest::Flags)
bool FMTFloodCarryTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Rudeus = Game.SpawnPlayer(TEXT("Rudeus"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Near = Game.SpawnOpponent(TEXT("Enemy_Bandit"), FVector(600.f, 0.f, 0.f));
	AMTEnemyCharacter* Wide = Game.SpawnOpponent(TEXT("Enemy_Bandit"), FVector(800.f, 1000.f, 0.f));
	if (!TestNotNull(TEXT("Rudeus"), Rudeus) || !TestNotNull(TEXT("near"), Near) || !TestNotNull(TEXT("wide"), Wide))
	{
		return false;
	}
	Game.Tick(0.4f);
	const float NearStart = Near->GetActorLocation().X;
	const float WideStart = Wide->GetActorLocation().X;
	const float HealthBefore = Near->GetAttributes()->GetHealth();
	TestTrue(TEXT("Flood activates"), Rudeus->GetAbilities()->ActivateAbilityById(TEXT("Water_Flood")));
	Game.Tick(2.2f);
	TestTrue(TEXT("the wave hits"), Near->GetAttributes()->GetHealth() < HealthBefore);
	TestTrue(FString::Printf(TEXT("and carries its victims along (%.0f cm)"), Near->GetActorLocation().X - NearStart), Near->GetActorLocation().X - NearStart > 800.f);
	TestTrue(FString::Printf(TEXT("across its whole 24 m front (%.0f cm at 10 m to the side)"), Wide->GetActorLocation().X - WideStart),
		Wide->GetActorLocation().X - WideStart > 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTTornadoLiftTest, "MushokuRPG.Abilities.TornadoLifts", MTTest::Flags)
bool FMTTornadoLiftTest::RunTest(const FString& Parameters)
{
	MTTest::FGameWorld Game;
	if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
	{
		return false;
	}
	AMTPlayerCharacter* Rudeus = Game.SpawnPlayer(TEXT("Rudeus"), FVector::ZeroVector, 0.f);
	AMTEnemyCharacter* Small = Game.SpawnOpponent(TEXT("Enemy_Goblin"), FVector(1200.f, 0.f, 0.f));
	AMTEnemyCharacter* Large = Game.SpawnOpponent(TEXT("Elite_WolfAlpha"), FVector(1200.f, 700.f, 0.f));
	if (!TestNotNull(TEXT("Rudeus"), Rudeus) || !TestNotNull(TEXT("small"), Small) || !TestNotNull(TEXT("large"), Large))
	{
		return false;
	}
	Game.Tick(0.4f);
	// Kept alive through the funnel's damage: a dead body has its movement switched off, which is not "frozen".
	Small->GetAttributes()->bGodMode = true;
	Rudeus->SetLockTarget(Small);
	const float Ground = Small->GetActorLocation().Z;
	const float LargeStart = FVector::Dist2D(Large->GetActorLocation(), Small->GetActorLocation());
	TestTrue(TEXT("Tornado activates"), Rudeus->GetAbilities()->ActivateAbilityById(TEXT("Wind_Tornado")));
	float Highest = Ground;
	for (int32 Frame = 0; Frame < 150; ++Frame)
	{
		Game.Tick(1.f / 60.f);
		Highest = FMath::Max(Highest, Small->GetActorLocation().Z);
	}
	TestTrue(FString::Printf(TEXT("a small enemy is lifted into the funnel (%.0f cm up)"), Highest - Ground), Highest - Ground > 120.f);
	TestTrue(FString::Printf(TEXT("a large one is dragged in, not lifted (%.0f -> %.0f cm)"), LargeStart, FVector::Dist2D(Large->GetActorLocation(), Small->GetActorLocation())),
		FVector::Dist2D(Large->GetActorLocation(), Small->GetActorLocation()) < LargeStart);
	Game.Tick(6.f);
	TestTrue(TEXT("the small enemy is dropped again (walking or falling, not frozen)"),
		Small->GetCharacterMovement()->MovementMode != MOVE_None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTEveryLoadoutAbilityTest, "MushokuRPG.Abilities.EveryLoadoutAbilityFires", MTTest::Flags)
bool FMTEveryLoadoutAbilityTest::RunTest(const FString& Parameters)
{
	// Every ability the ABILITIES menu can put on keys 1-4 fires through the real ability path, reaches its target area,
	// and leaves no zones, walls, serpents or effects behind.
	const TCHAR* Shared[] = { TEXT("Fire_Fireball"), TEXT("Fire_FlameWave"), TEXT("Fire_Inferno"), TEXT("Water_WaterBullet"),
		TEXT("Water_WaterDragon"), TEXT("Water_Flood"), TEXT("Earth_StoneCannon"), TEXT("Earth_EarthWall"), TEXT("Earth_EarthSpikes"),
		TEXT("Wind_WindBlade"), TEXT("Wind_Tornado"), TEXT("Wind_WindBurst") };
	struct FCast { FName Character; FName Ability; };
	TArray<FCast> Casts;
	for (const TCHAR* Id : Shared)
	{
		Casts.Add({ TEXT("Rudeus"), Id });
		Casts.Add({ TEXT("Orsted"), Id });
	}
	for (const TCHAR* Id : { TEXT("Rudeus_StoneCannon"), TEXT("Rudeus_Quagmire"), TEXT("Rudeus_ElementalBarrage") })
	{
		Casts.Add({ TEXT("Rudeus"), Id });
	}
	for (const TCHAR* Id : { TEXT("Orsted_DisturbMagic"), TEXT("Orsted_DragonStep"), TEXT("Orsted_DragonCrush") })
	{
		Casts.Add({ TEXT("Orsted"), Id });
	}
	int32 Passed = 0;
	for (const FCast& Cast : Casts)
	{
		MTTest::FGameWorld Game;
		if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
		{
			return false;
		}
		AMTPlayerCharacter* Caster = Game.SpawnPlayer(Cast.Character, FVector::ZeroVector, 0.f);
		AMTEnemyCharacter* Dummy = Game.SpawnOpponent(TEXT("Elite_WolfAlpha"), FVector(420.f, 0.f, 0.f));
		if (!Caster || !Dummy)
		{
			AddError(TEXT("could not spawn the caster or the dummy"));
			continue;
		}
		Game.Tick(0.4f);
		Caster->SetLockTarget(Dummy);
		Caster->GetAttributes()->RestoreStamina(1000.f);
		Caster->GetAttributes()->RestoreMana(10000.f);
		const float Before = Dummy->GetAttributes()->GetHealth();
		const bool bStarted = Caster->GetAbilities()->ActivateAbilityById(Cast.Ability);
		Game.Tick(0.1f);
		Caster->GetAbilities()->ReleaseAbilityById(Cast.Ability);
		// A short cooldown (Wind Blade 1 s) is over again long before 5 s: note whether it went on cooldown at all.
		bool bCooldown = false;
		for (int32 Step = 0; Step < 50; ++Step)
		{
			Game.Tick(0.1f);
			bCooldown |= Caster->GetAbilities()->GetCooldownRemaining(Cast.Ability) > 0.f;
		}
		const bool bHit = Dummy->GetAttributes()->GetHealth() < Before;
		const FString Name = FString::Printf(TEXT("%s: %s"), *Cast.Character.ToString(), *Cast.Ability.ToString());
		TestTrue(Name + TEXT(" starts from the hotbar path"), bStarted);
		TestTrue(Name + TEXT(" executes (goes on cooldown)"), bCooldown || Cast.Ability == FName(TEXT("Orsted_DisturbMagic")));
		const bool bSupportOnly = Cast.Ability == FName(TEXT("Earth_EarthWall")) || Cast.Ability == FName(TEXT("Rudeus_Quagmire"))
			|| Cast.Ability == FName(TEXT("Orsted_DisturbMagic"));
		if (!bSupportOnly)
		{
			TestTrue(Name + TEXT(" hits the dummy 4 m ahead"), bHit);
		}
		Game.Tick(16.f); // walls stand 15 s; zones, serpents and projectiles end sooner
		const int32 Leftovers = MTTest::CountLive(Game.World, AMTZoneActor::StaticClass()) + MTTest::CountLive(Game.World, AMTEarthWall::StaticClass())
			+ MTTest::CountLive(Game.World, AMTWaterSerpent::StaticClass()) + MTTest::CountLive(Game.World, AMTProjectile::StaticClass());
		// Pooled effect actors idle between uses; only an effect still playing is a leak.
		const UMTVFXSubsystem* VFX = UMTVFXSubsystem::Get(Game.World);
		const int32 Effects = VFX ? VFX->GetLiveEffects() : MTTest::CountLive(Game.World, AMTSpellVFX::StaticClass());
		TestEqual(*(Name + TEXT(" leaves no spell actors behind")), Leftovers, 0);
		TestTrue(FString::Printf(TEXT("%s leaves no running effects behind (%d still playing)"), *Name, Effects), Effects == 0);
		Passed += (bStarted && (bCooldown || !bSupportOnly)) ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("%d of %d hotbar casts started and executed"), Passed, Casts.Num()));
	return true;
}

// ---------------------------------------------------------------------------------------------------- required set

namespace MTTest
{
	/** Abilities that must stay in the game: Rudeus's and Orsted's signatures and the shared element spells. */
	struct FRequiredAbility
	{
		const TCHAR* Id;
		const TCHAR* Caster;
	};
	const FRequiredAbility RequiredAbilities[] = {
		{ TEXT("Rudeus_StoneCannon"), TEXT("Rudeus") }, { TEXT("Rudeus_Quagmire"), TEXT("Rudeus") },
		{ TEXT("Rudeus_ElementalBarrage"), TEXT("Rudeus") }, { TEXT("Orsted_DisturbMagic"), TEXT("Orsted") },
		{ TEXT("Orsted_DragonStep"), TEXT("Orsted") }, { TEXT("Orsted_DragonCrush"), TEXT("Orsted") },
		{ TEXT("Fire_Fireball"), TEXT("Rudeus") }, { TEXT("Fire_FlameWave"), TEXT("Rudeus") }, { TEXT("Fire_Inferno"), TEXT("Rudeus") },
		{ TEXT("Water_WaterBullet"), TEXT("Rudeus") }, { TEXT("Water_WaterDragon"), TEXT("Rudeus") }, { TEXT("Water_Flood"), TEXT("Rudeus") },
		{ TEXT("Earth_EarthWall"), TEXT("Rudeus") }, { TEXT("Earth_EarthSpikes"), TEXT("Rudeus") },
		{ TEXT("Wind_WindBlade"), TEXT("Rudeus") }, { TEXT("Wind_Tornado"), TEXT("Rudeus") }, { TEXT("Wind_WindBurst"), TEXT("Rudeus") },
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMTRequiredAbilitiesTest, "MushokuRPG.Abilities.RequiredSet", MTTest::Flags)
bool FMTRequiredAbilitiesTest::RunTest(const FString& Parameters)
{
	// Every must-keep ability: its data, icon, animation and sounds load, and cast for real at an opponent it
	// activates, shows a runtime effect (never an invisible spell) and does its job: damage, a zone, a wall or a dash.
	for (const MTTest::FRequiredAbility& Required : MTTest::RequiredAbilities)
	{
		const FName Id(Required.Id);
		MTTest::FGameWorld Game;
		if (!TestTrue(TEXT("game world with the data registry"), Game.IsValid()))
		{
			return false;
		}
		const FMTAbilityData* Data = UMTDataRegistry::Get(Game.World)->FindAbility(Id);
		if (!TestNotNull(*FString::Printf(TEXT("%s: data row"), Required.Id), Data))
		{
			continue;
		}
		const FString Name = FString::Printf(TEXT("%s (%s)"), *Data->DisplayName.ToString(), Required.Id);
		TestNotNull(*FString::Printf(TEXT("%s: icon"), *Name), Data->Icon.LoadSynchronous());
		TestNotNull(*FString::Printf(TEXT("%s: animation"), *Name), Data->Montage.LoadSynchronous());
		for (const TSoftObjectPtr<USoundBase>* Sound : { &Data->FX.CastSound, &Data->FX.TravelSound, &Data->FX.ImpactSound })
		{
			if (!Sound->IsNull())
			{
				TestNotNull(*FString::Printf(TEXT("%s: sound %s"), *Name, *Sound->ToString()), Sound->LoadSynchronous());
			}
		}

		// Melee needs arm's length, a dash a gap to close, spells a proper distance.
		const float Distance = Data->Behavior == EMTAbilityBehavior::Melee ? 200.f : Data->Behavior == EMTAbilityBehavior::Dash ? 650.f : 900.f;
		const bool bOrsted = FCString::Strcmp(Required.Caster, TEXT("Orsted")) == 0;
		AMTPlayerCharacter* Caster = Game.SpawnPlayer(Required.Caster, FVector::ZeroVector, 0.f);
		AMTEnemyCharacter* Target = Game.SpawnOpponent(bOrsted ? TEXT("Arena_Rudeus") : TEXT("Arena_Orsted"), FVector(Distance, 0.f, 0.f));
		if (!TestNotNull(*FString::Printf(TEXT("%s: caster"), *Name), Caster) || !TestNotNull(*FString::Printf(TEXT("%s: target"), *Name), Target))
		{
			continue;
		}
		Game.Tick(0.4f);
		Caster->GetAttributes()->bInfiniteMana = true;
		if (Data->Element != EMTElement::None && Data->CharacterRequirement.IsNone())
		{
			Caster->ApplyElementSlot(0, Data->Element);
		}
		Caster->SetLockTarget(Target);

		TMap<UClass*, int32> Spawned;
		const FDelegateHandle Handle = Game.World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
			[&Spawned](AActor* Actor) { ++Spawned.FindOrAdd(Actor->GetClass()); }));
		const FVector Start = Caster->GetActorLocation();
		const float HealthBefore = Target->GetAttributes()->GetHealth();
		const bool bActivated = Caster->GetAbilities()->ActivateAbilityById(Id);
		TestTrue(FString::Printf(TEXT("%s activates"), *Name), bActivated);
		Game.Tick(Data->bChargeable ? FMath::Clamp(Data->MaxChargeTime, 0.3f, 1.f) : 0.3f);
		Caster->GetAbilities()->ReleaseAbilityById(Id);
		Game.Tick(3.5f);
		Game.World->RemoveOnActorSpawnedHandler(Handle);

		auto Count = [&Spawned](const UClass* Class)
		{
			int32 N = 0;
			for (const TPair<UClass*, int32>& Pair : Spawned)
			{
				N += Pair.Key->IsChildOf(Class) ? Pair.Value : 0;
			}
			return N;
		};
		const int32 Effects = Count(AMTSpellVFX::StaticClass());
		const int32 Projectiles = Count(AMTProjectile::StaticClass());
		const int32 Zones = Count(AMTZoneActor::StaticClass());
		const int32 Walls = Count(AMTEarthWall::StaticClass());
		const float Damage = HealthBefore - Target->GetAttributes()->GetHealth();
		const float Moved = FVector::Dist2D(Start, Caster->GetActorLocation());
		TestTrue(FString::Printf(TEXT("%s shows a runtime effect (%d spawned)"), *Name, Effects), Effects > 0);

		bool bDidSomething = bActivated;
		switch (Data->Behavior)
		{
		case EMTAbilityBehavior::Projectile:
		case EMTAbilityBehavior::Sequence:
		case EMTAbilityBehavior::Melee:
			bDidSomething = Damage > 0.f;
			break;
		case EMTAbilityBehavior::Zone:
			bDidSomething = Zones > 0 || Damage > 0.f;
			break;
		case EMTAbilityBehavior::Structure:
			bDidSomething = Walls > 0;
			break;
		case EMTAbilityBehavior::Dash:
			bDidSomething = Moved > 150.f;
			break;
		default:
			// Counter: nothing to disturb here, a whiff still runs (the effect above) and goes on cooldown.
			break;
		}
		TestTrue(FString::Printf(TEXT("%s does its job"), *Name), bDidSomething);
		AddInfo(FString::Printf(TEXT("%s %s: %d effects, %d projectiles, %d zones, %d walls, target -%.1f health, caster moved %.0f cm"),
			*Name, bDidSomething ? TEXT("OK") : TEXT("NO EFFECT"), Effects, Projectiles, Zones, Walls, Damage, Moved));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

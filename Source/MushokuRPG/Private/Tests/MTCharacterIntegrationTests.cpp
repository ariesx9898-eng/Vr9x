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
		const UStaticMeshComponent* Body = Spell->FindComponentByClass<UStaticMeshComponent>();
		TestTrue(TEXT("the stone bullet is visible (authored mesh or blockout)"), Body && Body->GetStaticMesh() != nullptr && Body->IsVisible());
		AddInfo(FString::Printf(TEXT("stone bullet body: %s"), Body ? *GetNameSafe(Body->GetStaticMesh()) : TEXT("none")));
	}

	Rudeus->SetLockTarget(Target);
	Rudeus->ApplyElementSlot(0, EMTElement::Earth);
	TestTrue(TEXT("Earth Fortress activates"), Rudeus->GetAbilities()->ActivateAbilityById(TEXT("Earth_EarthFortress")));
	Game.Tick(1.5f);
	int32 Walls = 0;
	for (TActorIterator<AMTEarthWall> It(Game.World); It; ++It)
	{
		const UStaticMeshComponent* WallMesh = It->FindComponentByClass<UStaticMeshComponent>();
		TestTrue(FString::Printf(TEXT("%s is visible (authored mesh or blockout)"), *It->GetName()), WallMesh && WallMesh->GetStaticMesh() != nullptr);
		++Walls;
	}
	TestTrue(FString::Printf(TEXT("Earth Fortress raised walls (%d)"), Walls), Walls > 0);
	AddInfo(FString::Printf(TEXT("Earth Fortress raised %d walls, all with a visible mesh"), Walls));
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

#endif // WITH_DEV_AUTOMATION_TESTS

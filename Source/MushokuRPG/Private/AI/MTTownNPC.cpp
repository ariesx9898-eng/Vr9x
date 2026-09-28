#include "AI/MTTownNPC.h"

#include "AI/MTNPCScheduleComponent.h"
#include "Quests/MTInteractableComponent.h"
#include "Character/MTAttributeComponent.h"
#include "Core/MTGameEvents.h"
#include "Core/MTGameplayTags.h"
#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AMTTownNPC::AMTTownNPC(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TeamId = FGenericTeamId(3);
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	Schedule = CreateDefaultSubobject<UMTNPCScheduleComponent>(TEXT("Schedule"));
	QuestGiver = CreateDefaultSubobject<UMTQuestGiverComponent>(TEXT("QuestGiver"));

	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->bUseControllerDesiredRotation = false;
		Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
		Movement->MaxWalkSpeed = 160.f;
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// Villagers far away / off screen never pay for animation.
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}
}

void AMTTownNPC::SyncIdentity()
{
	if (!NpcId.IsNone())
	{
		GameplayId = NpcId;
	}
	if (QuestGiver)
	{
		QuestGiver->NpcId = NpcId;
		if (!NpcDisplayName.IsEmpty())
		{
			QuestGiver->DisplayName = NpcDisplayName;
		}
	}
}

void AMTTownNPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SyncIdentity();
}

void AMTTownNPC::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Before component BeginPlay, so the quest giver registers under the right id.
	SyncIdentity();
}

void AMTTownNPC::BeginPlay()
{
	SyncIdentity();
	Super::BeginPlay();

	SetGenericTeamId(FGenericTeamId(3));
	IdentityTags.AddTag(MTTags::NPC_Villager);
	if (UMTAttributeComponent* Attr = GetAttributes())
	{
		Attr->InitializeAttributes(NpcMaxHealth, 50.f, 100.f, 50.f, 0.f);
		Attr->bIsWeak = true;
	}
	SetWalking(true);
}

EMTNPCActivity AMTTownNPC::GetCurrentActivity() const
{
	return Schedule ? Schedule->CurrentActivity : EMTNPCActivity::Wander;
}

bool AMTTownNPC::IsCarrying() const
{
	return Schedule && Schedule->bCarrying;
}

void AMTTownNPC::HandleDeath(AActor* Killer)
{
	Super::HandleDeath(Killer);
	if (Schedule)
	{
		Schedule->SetComponentTickEnabled(false);
	}
	if (QuestGiver)
	{
		QuestGiver->bEnabled = false;
	}
	// Escorted / defended villagers fail their objectives.
	if (!GameplayId.IsNone())
	{
		if (UMTGameEvents* Events = UMTGameEvents::Get(this))
		{
			Events->OnProtectedTargetLost.Broadcast(GameplayId);
		}
	}
}

#include "Character/MTPlayerCharacter.h"
#include "UI/LaPlace/MTFrontEndSubsystem.h"
#include "Character/MTAttributeComponent.h"
#include "Abilities/MTAbility.h"
#include "Combat/MTTelegraphSubsystem.h"
#include "Combat/MTProjectile.h"
#include "Combat/MTForesightGhost.h"
#include "Core/MTGameplayTags.h"
#include "Progression/MTProgressionSubsystem.h"
#include "Save/MTSaveSubsystem.h"
#include "Quests/MTInteractableComponent.h"
#include "UI/MTHUD.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "VFX/MTVFXPreloadSubsystem.h"

AMTPlayerCharacter::AMTPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = DefaultArmLength;
	CameraBoom->SocketOffset = FVector(0.f, 40.f, 55.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 14.f;
	CameraBoom->ProbeChannel = ECC_Camera;
	// Position lag smooths motion; rotation lag stays off so mouse aim never feels floaty/jittery.
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 14.f;
	CameraBoom->CameraLagMaxDistance = 90.f;
	CameraBoom->bEnableCameraRotationLag = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(BaseFOV);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	TeamId = FGenericTeamId(1);
	GameplayId = TEXT("Player");
}

void AMTPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentFOV = BaseFOV;
	// Ground-decal materials compile their GPU pipelines now, not under the first spell impact.
	UMTVFXPreloadSubsystem::WarmUpDecals(this);

	// Build the character from saved progression (or the new-game default: Rudeus).
	UMTSaveSubsystem* Save = UMTSaveSubsystem::Get(this);
	const bool bLoaded = Save && Save->HasSave(TEXT("Slot0")) && Save->LoadGame(TEXT("Slot0"));
	if (!bLoaded)
	{
		if (const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
		{
			Progression->ApplyBuildTo(this);
		}
		else
		{
			ApplyCharacterLineage(TEXT("Rudeus"));
		}
	}
	if (const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		BaseFOV = Progression->GetSettings().FieldOfView;
		CurrentFOV = BaseFOV;
	}

	if (UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this))
	{
		Telegraphs->OnTelegraphPublished.AddDynamic(this, &AMTPlayerCharacter::HandleTelegraph);
	}
}

void AMTPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	CreateRuntimeInput();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(DefaultMappingContext ? DefaultMappingContext.Get() : RuntimeContext.Get(), 0);
		}
	}
}

UInputAction* AMTPlayerCharacter::MakeAction(FName Name, bool bAxis2D)
{
	if (TObjectPtr<UInputAction>* Existing = RuntimeActions.Find(Name))
	{
		return *Existing;
	}
	UInputAction* Action = NewObject<UInputAction>(this, Name);
	Action->ValueType = bAxis2D ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
	RuntimeActions.Add(Name, Action);
	return Action;
}

void AMTPlayerCharacter::CreateRuntimeInput()
{
	if (RuntimeContext)
	{
		return;
	}
	RuntimeContext = NewObject<UInputMappingContext>(this, TEXT("IMC_MTRuntime"));
	UInputMappingContext* IMC = RuntimeContext;

	auto Map = [IMC](UInputAction* Action, const FKey& Key) -> FEnhancedActionKeyMapping&
	{
		return IMC->MapKey(Action, Key);
	};
	auto Swizzle = [IMC]()
	{
		UInputModifierSwizzleAxis* M = NewObject<UInputModifierSwizzleAxis>(IMC);
		M->Order = EInputAxisSwizzle::YXZ;
		return M;
	};
	auto Negate = [IMC](bool bX, bool bY)
	{
		UInputModifierNegate* M = NewObject<UInputModifierNegate>(IMC);
		M->bX = bX;
		M->bY = bY;
		M->bZ = false;
		return M;
	};

	UInputAction* Move = MakeAction(TEXT("IA_Move"), true);
	Map(Move, EKeys::W).Modifiers.Add(Swizzle());
	{
		FEnhancedActionKeyMapping& S = Map(Move, EKeys::S);
		S.Modifiers.Add(Swizzle());
		S.Modifiers.Add(Negate(true, true));
	}
	Map(Move, EKeys::A).Modifiers.Add(Negate(true, false));
	Map(Move, EKeys::D);
	Map(Move, EKeys::Gamepad_Left2D);

	UInputAction* Look = MakeAction(TEXT("IA_Look"), true);
	Map(Look, EKeys::Mouse2D).Modifiers.Add(Negate(false, true));
	UInputAction* LookPad = MakeAction(TEXT("IA_LookGamepad"), true);
	Map(LookPad, EKeys::Gamepad_Right2D);

	Map(MakeAction(TEXT("IA_Jump"), false), EKeys::SpaceBar);
	Map(MakeAction(TEXT("IA_Jump"), false), EKeys::Gamepad_FaceButton_Bottom);
	Map(MakeAction(TEXT("IA_Sprint"), false), EKeys::LeftShift);
	Map(MakeAction(TEXT("IA_Sprint"), false), EKeys::Gamepad_LeftThumbstick);
	Map(MakeAction(TEXT("IA_Walk"), false), EKeys::LeftAlt);
	Map(MakeAction(TEXT("IA_Dodge"), false), EKeys::LeftControl);
	Map(MakeAction(TEXT("IA_Dodge"), false), EKeys::Gamepad_FaceButton_Right);
	Map(MakeAction(TEXT("IA_LockOn"), false), EKeys::Tab);
	Map(MakeAction(TEXT("IA_LockOn"), false), EKeys::MiddleMouseButton);
	Map(MakeAction(TEXT("IA_LockOn"), false), EKeys::Gamepad_RightThumbstick);
	Map(MakeAction(TEXT("IA_Interact"), false), EKeys::E);
	Map(MakeAction(TEXT("IA_Interact"), false), EKeys::Gamepad_FaceButton_Left);

	// Ability hotbar.
	Map(MakeAction(TEXT("IA_Basic"), false), EKeys::LeftMouseButton);
	Map(MakeAction(TEXT("IA_Basic"), false), EKeys::Gamepad_RightTrigger);
	// LA PLACE hotbar: LMB basic, 1-4 the chosen loadout, F special, G awakening.
	Map(MakeAction(TEXT("IA_Load1"), false), EKeys::One);
	Map(MakeAction(TEXT("IA_Load2"), false), EKeys::Two);
	Map(MakeAction(TEXT("IA_Load3"), false), EKeys::Three);
	Map(MakeAction(TEXT("IA_Load4"), false), EKeys::Four);
	Map(MakeAction(TEXT("IA_Special"), false), EKeys::F);
	Map(MakeAction(TEXT("IA_Awakening"), false), EKeys::G);
	Map(MakeAction(TEXT("IA_Load1"), false), EKeys::Gamepad_RightShoulder);
	Map(MakeAction(TEXT("IA_Load2"), false), EKeys::Gamepad_LeftShoulder);
	Map(MakeAction(TEXT("IA_Load3"), false), EKeys::Gamepad_LeftTrigger);
	Map(MakeAction(TEXT("IA_Load4"), false), EKeys::Gamepad_DPad_Up);
	Map(MakeAction(TEXT("IA_Special"), false), EKeys::Gamepad_FaceButton_Top);
	Map(MakeAction(TEXT("IA_Awakening"), false), EKeys::Gamepad_DPad_Down);

	// Menus.
	Map(MakeAction(TEXT("IA_MenuCharacter"), false), EKeys::C);
	Map(MakeAction(TEXT("IA_MenuRoll"), false), EKeys::K);
	Map(MakeAction(TEXT("IA_MenuQuests"), false), EKeys::J);
	Map(MakeAction(TEXT("IA_MenuMap"), false), EKeys::M);
	Map(MakeAction(TEXT("IA_MenuInventory"), false), EKeys::I);
	Map(MakeAction(TEXT("IA_MenuBack"), false), EKeys::Escape);
	Map(MakeAction(TEXT("IA_MenuBack"), false), EKeys::Gamepad_Special_Right);
	Map(MakeAction(TEXT("IA_MenuConfirm"), false), EKeys::Enter);
	Map(MakeAction(TEXT("IA_MenuUp"), false), EKeys::Up);
	Map(MakeAction(TEXT("IA_MenuDown"), false), EKeys::Down);
	Map(MakeAction(TEXT("IA_MenuLeft"), false), EKeys::Left);
	Map(MakeAction(TEXT("IA_MenuRight"), false), EKeys::Right);
	Map(MakeAction(TEXT("IA_MenuTabPrev"), false), EKeys::Q);
	Map(MakeAction(TEXT("IA_MenuTabNext"), false), EKeys::Gamepad_DPad_Right);
}

void AMTPlayerCharacter::BindSlot(UEnhancedInputComponent* EIC, UInputAction* Action, EMTAbilitySlot Slot)
{
	EIC->BindAction(Action, ETriggerEvent::Started, this, &AMTPlayerCharacter::OnSlotPressed, Slot);
	EIC->BindAction(Action, ETriggerEvent::Completed, this, &AMTPlayerCharacter::OnSlotReleased, Slot);
}

void AMTPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CreateRuntimeInput();
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogMushoku, Error, TEXT("Enhanced Input component missing - check DefaultInput.ini"));
		return;
	}
	auto A = [this](const TCHAR* Name) { return RuntimeActions.FindRef(FName(Name)).Get(); };

	EIC->BindAction(A(TEXT("IA_Move")), ETriggerEvent::Triggered, this, &AMTPlayerCharacter::OnMove);
	EIC->BindAction(A(TEXT("IA_Move")), ETriggerEvent::Completed, this, &AMTPlayerCharacter::OnMove);
	EIC->BindAction(A(TEXT("IA_Look")), ETriggerEvent::Triggered, this, &AMTPlayerCharacter::OnLook);
	EIC->BindAction(A(TEXT("IA_LookGamepad")), ETriggerEvent::Triggered, this, &AMTPlayerCharacter::OnLookGamepad);
	EIC->BindAction(A(TEXT("IA_Jump")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnJumpStarted);
	EIC->BindAction(A(TEXT("IA_Jump")), ETriggerEvent::Completed, this, &AMTPlayerCharacter::OnJumpCompleted);
	EIC->BindAction(A(TEXT("IA_Sprint")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnSprintStarted);
	EIC->BindAction(A(TEXT("IA_Sprint")), ETriggerEvent::Completed, this, &AMTPlayerCharacter::OnSprintCompleted);
	EIC->BindAction(A(TEXT("IA_Walk")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnWalkToggle);
	EIC->BindAction(A(TEXT("IA_Dodge")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnDodge);
	EIC->BindAction(A(TEXT("IA_LockOn")), ETriggerEvent::Started, this, &AMTPlayerCharacter::ToggleLockOn);
	EIC->BindAction(A(TEXT("IA_Interact")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnInteract);

	BindSlot(EIC, A(TEXT("IA_Basic")), EMTAbilitySlot::Basic);
	BindSlot(EIC, A(TEXT("IA_Load1")), EMTAbilitySlot::Loadout1);
	BindSlot(EIC, A(TEXT("IA_Load2")), EMTAbilitySlot::Loadout2);
	BindSlot(EIC, A(TEXT("IA_Load3")), EMTAbilitySlot::Loadout3);
	BindSlot(EIC, A(TEXT("IA_Load4")), EMTAbilitySlot::Loadout4);
	BindSlot(EIC, A(TEXT("IA_Special")), EMTAbilitySlot::Special);
	BindSlot(EIC, A(TEXT("IA_Awakening")), EMTAbilitySlot::Awakening);

	EIC->BindAction(A(TEXT("IA_MenuCharacter")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuKey, (int32)EMTMenuPage::Character);
	EIC->BindAction(A(TEXT("IA_MenuRoll")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuKey, (int32)EMTMenuPage::Roll);
	EIC->BindAction(A(TEXT("IA_MenuQuests")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuKey, (int32)EMTMenuPage::Quests);
	EIC->BindAction(A(TEXT("IA_MenuMap")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuKey, (int32)EMTMenuPage::Map);
	EIC->BindAction(A(TEXT("IA_MenuInventory")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuKey, (int32)EMTMenuPage::Inventory);
	EIC->BindAction(A(TEXT("IA_MenuBack")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuBack);
	EIC->BindAction(A(TEXT("IA_MenuConfirm")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuConfirm);
	EIC->BindAction(A(TEXT("IA_MenuUp")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuUp);
	EIC->BindAction(A(TEXT("IA_MenuDown")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuDown);
	EIC->BindAction(A(TEXT("IA_MenuLeft")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuLeft);
	EIC->BindAction(A(TEXT("IA_MenuRight")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuRight);
	EIC->BindAction(A(TEXT("IA_MenuTabPrev")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuTabPrev);
	EIC->BindAction(A(TEXT("IA_MenuTabNext")), ETriggerEvent::Started, this, &AMTPlayerCharacter::OnMenuTabNext);
}

AMTHUD* AMTPlayerCharacter::GetMTHUD() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	return PC ? Cast<AMTHUD>(PC->GetHUD()) : nullptr;
}

bool AMTPlayerCharacter::IsMenuOpen() const
{
	const AMTHUD* HUD = GetMTHUD();
	return HUD && HUD->IsMenuOpen();
}

// ------------------------------------------------------------------ Input handlers

void AMTPlayerCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	LastMoveInput = Input;
	if (IsMenuOpen() || !Controller || Input.IsNearlyZero())
	{
		return;
	}
	// Camera-relative movement on the ground plane.
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void AMTPlayerCharacter::OnLook(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	FVector2D Input = Value.Get<FVector2D>();
	float Sensitivity = 1.f;
	bool bInvert = false;
	if (const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Sensitivity = Progression->GetSettings().MouseSensitivity;
		bInvert = Progression->GetSettings().bInvertY;
	}
	// Soft lock: manual look is damped while locked on so the camera stays readable.
	const float LockDamp = GetLockTarget() ? 0.35f : 1.f;
	AddControllerYawInput(Input.X * LookRateMouse * Sensitivity * LockDamp);
	AddControllerPitchInput(Input.Y * LookRateMouse * Sensitivity * LockDamp * (bInvert ? -1.f : 1.f));
}

void AMTPlayerCharacter::OnLookGamepad(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	const FVector2D Input = Value.Get<FVector2D>();
	float Sensitivity = 1.f;
	bool bInvert = false;
	if (const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Sensitivity = Progression->GetSettings().GamepadSensitivity;
		bInvert = Progression->GetSettings().bInvertY;
	}
	const float Delta = GetWorld()->GetDeltaSeconds();
	AddControllerYawInput(Input.X * LookRateGamepad * Sensitivity * Delta);
	AddControllerPitchInput(-Input.Y * LookRateGamepad * Sensitivity * Delta * (bInvert ? -1.f : 1.f));
}

void AMTPlayerCharacter::OnJumpStarted()
{
	if (!IsMenuOpen() && !IsStaggered() && !GetAbilities()->IsCasting())
	{
		Jump();
	}
}

void AMTPlayerCharacter::OnJumpCompleted()
{
	StopJumping();
}

void AMTPlayerCharacter::OnSprintStarted()
{
	const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this);
	const bool bToggle = Progression && Progression->GetSettings().bToggleSprint;
	SetSprinting(bToggle ? !IsSprinting() : true);
	SetWalking(false);
}

void AMTPlayerCharacter::OnSprintCompleted()
{
	const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this);
	if (!(Progression && Progression->GetSettings().bToggleSprint))
	{
		SetSprinting(false);
	}
}

void AMTPlayerCharacter::OnWalkToggle()
{
	SetWalking(!bWalking);
}

void AMTPlayerCharacter::OnDodge()
{
	if (IsMenuOpen() || !Controller)
	{
		return;
	}
	// Directional dodge relative to the camera (backstep when no input).
	FVector Dir = FVector::ZeroVector;
	if (!LastMoveInput.IsNearlyZero())
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		Dir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X) * LastMoveInput.Y + FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y) * LastMoveInput.X;
	}
	Dodge(Dir);
}

void AMTPlayerCharacter::OnInteract()
{
	if (AMTHUD* HUD = GetMTHUD())
	{
		if (HUD->IsMenuOpen())
		{
			HUD->MenuNextTab(1);
			return;
		}
	}
	if (UMTInteractableComponent* Interactable = FocusedInteractable.Get())
	{
		if (Interactable->CanInteract(this))
		{
			Interactable->Interact(this);
		}
	}
}

void AMTPlayerCharacter::OnSlotPressed(EMTAbilitySlot Slot)
{
	if (AMTHUD* HUD = GetMTHUD())
	{
		if (HUD->IsMenuOpen())
		{
			if (Slot == EMTAbilitySlot::Basic)
			{
				HUD->MenuConfirm(); // clicks are also handled through HUD hitboxes
			}
			return;
		}
	}
	GetAbilities()->PressSlot(Slot);
}

void AMTPlayerCharacter::OnSlotReleased(EMTAbilitySlot Slot)
{
	GetAbilities()->ReleaseSlot(Slot);
}

void AMTPlayerCharacter::OnMenuKey(int32 Page)
{
	if (AMTHUD* HUD = GetMTHUD())
	{
		HUD->ToggleMenu((EMTMenuPage)Page);
	}
}

void AMTPlayerCharacter::OnMenuBack()
{
	if (AMTHUD* HUD = GetMTHUD())
	{
		if (HUD->IsMenuOpen())
		{
			HUD->MenuBack();
		}
		else if (UMTFrontEndSubsystem* FrontEnd = UMTFrontEndSubsystem::Get(this))
		{
			FrontEnd->OpenPause(Cast<APlayerController>(GetController()));
		}
	}
}

void AMTPlayerCharacter::OnMenuConfirm() { if (AMTHUD* HUD = GetMTHUD()) { HUD->MenuConfirm(); } }
void AMTPlayerCharacter::OnMenuUp() { if (AMTHUD* HUD = GetMTHUD()) { HUD->MenuNavigate(FIntPoint(0, -1)); } }
void AMTPlayerCharacter::OnMenuDown() { if (AMTHUD* HUD = GetMTHUD()) { HUD->MenuNavigate(FIntPoint(0, 1)); } }
void AMTPlayerCharacter::OnMenuLeft() { if (AMTHUD* HUD = GetMTHUD()) { HUD->MenuNavigate(FIntPoint(-1, 0)); } }
void AMTPlayerCharacter::OnMenuRight() { if (AMTHUD* HUD = GetMTHUD()) { HUD->MenuNavigate(FIntPoint(1, 0)); } }
void AMTPlayerCharacter::OnMenuTabPrev() { if (AMTHUD* HUD = GetMTHUD()) { if (HUD->IsMenuOpen()) { HUD->MenuNextTab(-1); } } }
void AMTPlayerCharacter::OnMenuTabNext() { if (AMTHUD* HUD = GetMTHUD()) { if (HUD->IsMenuOpen()) { HUD->MenuNextTab(1); } } }

// ------------------------------------------------------------------ Per-frame

void AMTPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	InteractionScanTimer -= DeltaSeconds;
	if (InteractionScanTimer <= 0.f)
	{
		InteractionScanTimer = 0.15f;
		UpdateInteractionFocus();
	}
	ForesightScanTimer -= DeltaSeconds;
	if (ForesightScanTimer <= 0.f)
	{
		ForesightScanTimer = 0.1f;
		UpdateSpellForesight();
	}
	UpdateLockOn(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
}

void AMTPlayerCharacter::UpdateCamera(float DeltaSeconds)
{
	// Sprint response: small FOV widening, eased; casting pulls the camera in slightly.
	const bool bMovingFast = IsSprinting() && GetVelocity().Size2D() > 300.f;
	const float TargetFOV = BaseFOV + (bMovingFast ? SprintFOVBonus : 0.f);
	CurrentFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaSeconds, 4.f);
	float Kick = 0.f;
	if (FOVKickDuration > 0.f)
	{
		// Punch out over the first 12% (never a snap), then ease back in: a single breath, not a wobble.
		FOVKickTime += DeltaSeconds;
		const float T = FMath::Clamp(FOVKickTime / FOVKickDuration, 0.f, 1.f);
		Kick = FOVKickDegrees * (T < 0.12f ? FMath::SmoothStep(0.f, 1.f, T / 0.12f) : FMath::Square(1.f - (T - 0.12f) / 0.88f));
		if (T >= 1.f)
		{
			FOVKickDuration = 0.f;
			FOVKickDegrees = 0.f;
			Kick = 0.f;
		}
	}
	FollowCamera->SetFieldOfView(CurrentFOV + Kick);

	float TargetArm = DefaultArmLength;
	if (GetStateTags().HasTag(MTTags::State_Transforming))
	{
		TargetArm = DefaultArmLength + 90.f; // awakening: camera subtly pulls outward
	}
	else if (GetStateTags().HasTag(MTTags::State_Charging))
	{
		TargetArm = DefaultArmLength - 40.f;
	}
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaSeconds, 3.f);
	UpdateCameraShake(DeltaSeconds);
}

void AMTPlayerCharacter::AddCameraShake(float Strength, float Duration)
{
	float Setting = 1.f;
	if (const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Setting = Progression->GetSettings().CameraShakeScale;
	}
	Strength = FMath::Clamp(Strength, 0.f, 1.f) * Setting;
	const float Remaining = ShakeDuration > 0.f ? ShakeStrength * (1.f - FMath::Clamp(ShakeTime / ShakeDuration, 0.f, 1.f)) : 0.f;
	if (Strength <= KINDA_SMALL_NUMBER || Strength < Remaining)
	{
		return;
	}
	ShakeStrength = Strength;
	ShakeDuration = FMath::Max(0.05f, Duration);
	ShakeTime = 0.f;
}

void AMTPlayerCharacter::AddFOVKick(float Degrees, float Duration)
{
	float Setting = 1.f;
	if (const UMTProgressionSubsystem* Progression = UMTProgressionSubsystem::Get(this))
	{
		Setting = Progression->GetSettings().CameraShakeScale;
	}
	Degrees = FMath::Clamp(Degrees, 0.f, 10.f) * FMath::Clamp(Setting, 0.f, 1.f);
	// Keep the stronger of the kick in flight and the new one.
	float Remaining = 0.f;
	if (FOVKickDuration > 0.f)
	{
		Remaining = FOVKickDegrees * (1.f - FMath::Clamp(FOVKickTime / FOVKickDuration, 0.f, 1.f));
	}
	if (Degrees <= KINDA_SMALL_NUMBER || Degrees < Remaining)
	{
		return;
	}
	FOVKickDegrees = Degrees;
	FOVKickDuration = FMath::Max(0.1f, Duration);
	FOVKickTime = 0.f;
}

void AMTPlayerCharacter::UpdateCameraShake(float DeltaSeconds)
{
	if (!FollowCamera)
	{
		return;
	}
	if (ShakeDuration <= 0.f)
	{
		return;
	}
	ShakeTime += DeltaSeconds;
	const float Remaining = 1.f - FMath::Clamp(ShakeTime / ShakeDuration, 0.f, 1.f);
	if (Remaining <= 0.f)
	{
		ShakeDuration = 0.f;
		FollowCamera->SetRelativeLocation(FVector::ZeroVector);
		FollowCamera->SetRelativeRotation(FRotator::ZeroRotator);
		return;
	}
	// Decaying Perlin jitter: a few centimetres and a fraction of a degree even at full strength.
	const float Amount = ShakeStrength * Remaining * Remaining;
	const float T = ShakeTime * 28.f;
	const FVector Offset(0.f, FMath::PerlinNoise1D(T) * 9.f, FMath::PerlinNoise1D(T + 37.f) * 7.f);
	const FRotator Tilt(FMath::PerlinNoise1D(T + 71.f) * 1.1f, FMath::PerlinNoise1D(T + 113.f) * 0.8f, FMath::PerlinNoise1D(T + 157.f) * 1.4f);
	FollowCamera->SetRelativeLocation(Offset * Amount);
	FollowCamera->SetRelativeRotation(Tilt * Amount);
}

void AMTPlayerCharacter::UpdateLockOn(float DeltaSeconds)
{
	AActor* Target = GetLockTarget();
	AMTCharacterBase* TargetCharacter = Cast<AMTCharacterBase>(Target);
	if (Target && (!TargetCharacter || !TargetCharacter->IsAlive() || FVector::Dist(Target->GetActorLocation(), GetActorLocation()) > LockOnRange * 1.3f))
	{
		// Target died or escaped: hop to the next valid target, else release.
		SetLockTarget(FindBestLockTarget(Target));
		Target = GetLockTarget();
	}

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!Target)
	{
		CMC->bOrientRotationToMovement = true;
		return;
	}
	// Strafe around the target, facing it smoothly. A sprint runs where it is steered instead (IsStrafingLocked); dodges
	// and dashes (Dragon Step, Gale Step) keep the direction they launched in, so their clip never skids sideways.
	const bool bDashing = IsDodging() || GetStateTags().HasTag(MTTags::State_Dodging);
	const bool bFaceTarget = IsStrafingLocked();
	CMC->bOrientRotationToMovement = !bFaceTarget && !bDashing;
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const FRotator Face(0.f, ToTarget.Rotation().Yaw, 0.f);
	if (bFaceTarget && !bDashing)
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Face, DeltaSeconds, 10.f));
	}
	if (Controller && !IsMenuOpen())
	{
		const FVector CameraAim = Target->GetActorLocation() - FVector(0.f, 0.f, 60.f) - FollowCamera->GetComponentLocation();
		FRotator Desired = CameraAim.Rotation();
		Desired.Pitch = FMath::Clamp(Desired.Pitch, -35.f, 10.f);
		Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaSeconds, 6.f));
	}
}

AActor* AMTPlayerCharacter::FindBestLockTarget(AActor* Exclude) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return nullptr;
	}
	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector ViewDir = ViewRot.Vector();

	AActor* Best = nullptr;
	float BestScore = -BIG_NUMBER;
	for (TActorIterator<AMTCharacterBase> It(GetWorld()); It; ++It)
	{
		AMTCharacterBase* Candidate = *It;
		if (Candidate == this || Candidate == Exclude || !Candidate->IsAlive() || !IsHostileTo(Candidate))
		{
			continue;
		}
		const FVector To = Candidate->GetActorLocation() - ViewLoc;
		const float Dist = FVector::Dist(Candidate->GetActorLocation(), GetActorLocation());
		if (Dist > LockOnRange)
		{
			continue;
		}
		const float Facing = FVector::DotProduct(To.GetSafeNormal(), ViewDir);
		if (Facing < 0.5f)
		{
			continue; // must be roughly on screen
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MTLockOnLOS), false, this);
		Params.AddIgnoredActor(Candidate);
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, Candidate->GetActorLocation(), ECC_Visibility, Params))
		{
			continue;
		}
		const float Score = Facing * 2.f - Dist / LockOnRange;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

void AMTPlayerCharacter::ToggleLockOn()
{
	if (IsMenuOpen())
	{
		return;
	}
	if (GetLockTarget())
	{
		SetLockTarget(nullptr);
		return;
	}
	SetLockTarget(FindBestLockTarget(nullptr));
}

FVector AMTPlayerCharacter::GetAimPoint() const
{
	if (GetLockTarget())
	{
		return Super::GetAimPoint();
	}
	// Free aim: trace from the camera through the screen centre.
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return Super::GetAimPoint();
	}
	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector End = ViewLoc + ViewRot.Vector() * 6000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTAimTrace), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc + ViewRot.Vector() * CameraBoom->TargetArmLength, End, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}
	return End;
}

void AMTPlayerCharacter::UpdateInteractionFocus()
{
	FocusedInteractable.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MTInteractScan), false, this);
	World->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(350.f), Params);
	// Pawns (NPCs) do not always respond to Visibility; scan them too.
	TArray<FOverlapResult> PawnOverlaps;
	FCollisionObjectQueryParams PawnParams;
	PawnParams.AddObjectTypesToQuery(ECC_Pawn);
	PawnParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	World->OverlapMultiByObjectType(PawnOverlaps, GetActorLocation(), FQuat::Identity, PawnParams, FCollisionShape::MakeSphere(350.f), Params);
	Overlaps.Append(PawnOverlaps);

	float BestScore = -BIG_NUMBER;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		UMTInteractableComponent* Interactable = Actor ? Actor->FindComponentByClass<UMTInteractableComponent>() : nullptr;
		if (!Interactable || !Interactable->CanInteract(this))
		{
			continue;
		}
		const FVector To = Actor->GetActorLocation() - GetActorLocation();
		const float Dist = To.Size();
		if (Dist > Interactable->InteractionRange + GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			continue;
		}
		const float Score = FVector::DotProduct(To.GetSafeNormal2D(), GetActorForwardVector()) - Dist / 1000.f;
		if (Score > BestScore)
		{
			BestScore = Score;
			FocusedInteractable = Interactable;
		}
	}
}

// ------------------------------------------------------------------ Demon Eye: Foresight

bool AMTPlayerCharacter::IsForesightActive() const
{
	return GetStateTags().HasTag(MTTags::State_Foresight);
}

void AMTPlayerCharacter::HandleTelegraph(const FMTAttackTelegraph& Telegraph)
{
	if (!IsForesightActive() || !Telegraph.bIsImportant || Telegraph.Attacker.Get() == this)
	{
		return;
	}
	// Only attacks that could matter to the player: near us, or aimed at our area.
	const float Dist = FVector::Dist2D(Telegraph.ImpactLocation, GetActorLocation());
	if (Dist > Telegraph.Radius + Telegraph.Length + 1200.f)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AMTForesightGhost* Ghost = GetWorld()->SpawnActor<AMTForesightGhost>(AMTForesightGhost::StaticClass(), Telegraph.PredictedAttackerLocation, Telegraph.PredictedAttackerRotation, Params))
	{
		Ghost->InitFromTelegraph(Telegraph, GetAttributes()->GetStatModifier().ForesightLeadBonus);
	}
}

void AMTPlayerCharacter::UpdateSpellForesight()
{
	if (!IsForesightActive())
	{
		ForesightMarkedSpells.Reset();
		return;
	}
	UMTTelegraphSubsystem* Telegraphs = UMTTelegraphSubsystem::Get(this);
	if (!Telegraphs)
	{
		return;
	}
	const float Lead = 0.6f + GetAttributes()->GetStatModifier().ForesightLeadBonus;
	for (AMTProjectile* Spell : Telegraphs->GetSpellsNear(GetActorLocation(), 3500.f, this))
	{
		if (!Spell || ForesightMarkedSpells.Contains(Spell))
		{
			continue;
		}
		const float TimeToHit = Spell->EstimateTimeToReach(GetActorLocation(), 250.f);
		if (TimeToHit > Lead)
		{
			continue;
		}
		ForesightMarkedSpells.Add(Spell);
		FMTAttackTelegraph Predicted;
		Predicted.Attacker = Spell;
		Predicted.AttackId = Spell->GetAbilityData().AbilityID;
		Predicted.ImpactLocation = GetActorLocation();
		Predicted.Direction = Spell->GetProjectileVelocity().GetSafeNormal();
		Predicted.Radius = 120.f;
		Predicted.ImpactTime = GetWorld()->GetTimeSeconds() + TimeToHit;
		Predicted.PredictedAttackerLocation = Spell->GetActorLocation();
		Predicted.bIsMagic = true;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AMTForesightGhost* Marker = GetWorld()->SpawnActor<AMTForesightGhost>(AMTForesightGhost::StaticClass(), Predicted.ImpactLocation, FRotator::ZeroRotator, Params))
		{
			Marker->InitFromTelegraph(Predicted, 0.f);
		}
	}
}

void AMTPlayerCharacter::HandleDeath(AActor* Killer)
{
	Super::HandleDeath(Killer);
	SetLockTarget(nullptr);
	// Respawn after a short delay at full health (no progression loss in this slice).
	FTimerHandle Handle;
	TWeakObjectPtr<AMTPlayerCharacter> WeakThis(this);
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (AMTPlayerCharacter* Self = WeakThis.Get())
		{
			if (APlayerController* PC = Cast<APlayerController>(Self->GetController()))
			{
				if (AGameModeBase* GM = Self->GetWorld()->GetAuthGameMode())
				{
					PC->UnPossess();
					Self->Destroy();
					GM->RestartPlayer(PC);
				}
			}
		}
	}), 4.f, false);
}

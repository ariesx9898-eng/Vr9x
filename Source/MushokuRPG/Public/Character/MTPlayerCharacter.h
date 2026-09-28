// Player character: third-person camera, Enhanced Input (created at runtime so the game
// runs before any input assets exist), lock-on, interaction focus and Demon Eye foresight.
#pragma once

#include "CoreMinimal.h"
#include "Character/MTCharacterBase.h"
#include "InputActionValue.h"
#include "Abilities/MTAbilityComponent.h"
#include "MTPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMTInteractableComponent;
class AMTHUD;
struct FMTAttackTelegraph;

UCLASS()
class MUSHOKURPG_API AMTPlayerCharacter : public AMTCharacterBase
{
	GENERATED_BODY()

public:
	AMTPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual FVector GetAimPoint() const override;

	UFUNCTION(BlueprintPure, Category = "Mushoku|Interaction")
	UMTInteractableComponent* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Mushoku|Combat")
	void ToggleLockOn();

	UFUNCTION(BlueprintPure, Category = "Mushoku|Combat")
	bool IsForesightActive() const;

	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Optional authored assets; if null, equivalents are created at runtime. */
	UPROPERTY(EditDefaultsOnly, Category = "Mushoku|Input") TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Mushoku|Camera") float BaseFOV = 90.f;
	UPROPERTY(EditAnywhere, Category = "Mushoku|Camera") float SprintFOVBonus = 6.f;
	UPROPERTY(EditAnywhere, Category = "Mushoku|Camera") float LockOnRange = 3000.f;
	UPROPERTY(EditAnywhere, Category = "Mushoku|Camera") float LookRateMouse = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Mushoku|Camera") float LookRateGamepad = 110.f;

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath(AActor* Killer) override;

	void CreateRuntimeInput();
	UInputAction* MakeAction(FName Name, bool bAxis2D);
	void BindSlot(class UEnhancedInputComponent* EIC, UInputAction* Action, EMTAbilitySlot Slot);

	// Input handlers
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnLookGamepad(const FInputActionValue& Value);
	void OnJumpStarted();
	void OnJumpCompleted();
	void OnSprintStarted();
	void OnSprintCompleted();
	void OnWalkToggle();
	void OnDodge();
	void OnInteract();
	void OnSlotPressed(EMTAbilitySlot Slot);
	void OnSlotReleased(EMTAbilitySlot Slot);
	void OnMenuKey(int32 Page);
	void OnMenuBack();
	void OnMenuConfirm();
	void OnMenuUp();
	void OnMenuDown();
	void OnMenuLeft();
	void OnMenuRight();
	void OnMenuTabPrev();
	void OnMenuTabNext();

	AMTHUD* GetMTHUD() const;
	bool IsMenuOpen() const;

	void UpdateInteractionFocus();
	void UpdateLockOn(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	AActor* FindBestLockTarget(AActor* Exclude) const;

	UFUNCTION() void HandleTelegraph(const FMTAttackTelegraph& Telegraph);
	void UpdateSpellForesight();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Camera") TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mushoku|Camera") TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RuntimeContext;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UInputAction>> RuntimeActions;

	TWeakObjectPtr<UMTInteractableComponent> FocusedInteractable;
	float InteractionScanTimer = 0.f;
	float ForesightScanTimer = 0.f;
	float CurrentFOV = 90.f;
	float DefaultArmLength = 380.f;
	FVector2D LastMoveInput = FVector2D::ZeroVector;
	TSet<TWeakObjectPtr<AActor>> ForesightMarkedSpells;
};

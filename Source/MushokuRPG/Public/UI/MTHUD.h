// Canvas-only HUD: works with zero UMG / texture assets. Draws player vitals, the ability hotbar,
// quest tracker, north-up minimap, lock-on reticle, boss bar, floating damage numbers, interaction
// prompt, toasts, awakening / Demon Eye overlays, the full-screen menus, the NPC quest dialog and
// the roll screen. Menu buttons are Canvas hit boxes (mouse) plus spatial keyboard/gamepad focus.
//
// Controller contract: while IsMenuOpen() show the cursor, set bEnableClickEvents = true (hit boxes
// are dispatched by APlayerController::InputKey) and route UI input to MenuNavigate / MenuConfirm /
// MenuBack / MenuNextTab.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "Core/MTTypes.h"
#include "Progression/MTRollSubsystem.h"
#include "MTHUD.generated.h"

class AMTCharacterBase;
class UFont;

UENUM(BlueprintType)
enum class EMTMenuPage : uint8
{
	None,
	Character,
	Element,
	Race,
	Mastery,
	Inventory,
	Quests,
	Map,
	Party,
	Settings,
	Roll
};

/** Original anime-fantasy palette: deep navy panels, thin gold trim, parchment text. */
namespace MTHUDStyle
{
	inline FLinearColor PanelColor(float Alpha = 0.88f) { return FLinearColor(0.04f, 0.05f, 0.09f, Alpha); }
	inline FLinearColor Gold(float Alpha = 1.f) { return FLinearColor(0.85f, 0.7f, 0.4f, Alpha); }
	inline FLinearColor Parchment(float Alpha = 1.f) { return FLinearColor(0.95f, 0.92f, 0.85f, Alpha); }
	inline FLinearColor Dim(float Alpha = 1.f) { return FLinearColor(0.6f, 0.58f, 0.54f, Alpha); }
	inline FLinearColor Good(float Alpha = 1.f) { return FLinearColor(0.45f, 0.85f, 0.5f, Alpha); }
	inline FLinearColor Bad(float Alpha = 1.f) { return FLinearColor(0.95f, 0.38f, 0.35f, Alpha); }
	inline FLinearColor Health(float Alpha = 1.f) { return FLinearColor(0.78f, 0.16f, 0.2f, Alpha); }
	inline FLinearColor Mana(float Alpha = 1.f) { return FLinearColor(0.25f, 0.5f, 0.98f, Alpha); }
	inline FLinearColor Stamina(float Alpha = 1.f) { return FLinearColor(0.85f, 0.78f, 0.3f, Alpha); }
	inline FLinearColor Poise(float Alpha = 1.f) { return FLinearColor(0.7f, 0.72f, 0.78f, Alpha); }
	inline FLinearColor Awakening(float Alpha = 1.f) { return FLinearColor(0.95f, 0.55f, 0.95f, Alpha); }
}

UCLASS()
class MUSHOKURPG_API AMTHUD : public AHUD
{
	GENERATED_BODY()

public:
	AMTHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

	/** Opens Page, or closes the menu when Page is already open (None closes everything). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void ToggleMenu(EMTMenuPage Page);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void CloseAll();
	/** True while a menu page or the NPC dialog is open (show cursor, block gameplay input). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|UI") bool IsMenuOpen() const { return OpenPage != EMTMenuPage::None || bDialogOpen; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|UI") EMTMenuPage GetOpenPage() const { return OpenPage; }
	UFUNCTION(BlueprintPure, Category = "Mushoku|UI") bool IsDialogOpen() const { return bDialogOpen; }

	/** Moves keyboard/gamepad focus. Screen space: X = right, Y = DOWN (so "up" is (0,-1)). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void MenuNavigate(FIntPoint Direction);
	/** Activates the focused button (or skips a running roll animation). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void MenuConfirm();
	/** Skips roll animation, else closes the dialog, else closes the menu. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void MenuBack();
	/** Cycles the menu pages (Character ... Roll). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void MenuNextTab(int32 Dir);

	/** Quest dialog: offers (Accept / Decline) and turn-ins (Turn In) from UMTQuestSubsystem. */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void OpenNPCDialog(FName NpcId, const FText& NpcName);

	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void AddDamageNumber(const FVector& WorldLocation, float Amount, EMTElement Element, bool bHeavy);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void ShowCenterMessage(const FText& Text, float Duration = 2.5f);

	/** Minimap radius in cm (enemies within this range are shown). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|UI") float MinimapRange = 6000.f;
	/** Distance at which an unlocked boss still shows its health bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mushoku|UI") float BossBarRange = 4000.f;

protected:
	UFUNCTION() void HandleNotification(FText Message, FLinearColor Color);

private:
	struct FHudButton
	{
		FName Id;
		FVector2D Pos = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		bool bEnabled = true;
	};
	struct FDamageNumber
	{
		FVector World = FVector::ZeroVector;
		float Amount = 0.f;
		FLinearColor Color = FLinearColor::White;
		bool bHeavy = false;
		float Age = 0.f;
		float Life = 1.1f;
		float Drift = 0.f;
	};
	struct FToast
	{
		FText Message;
		FLinearColor Color = FLinearColor::White;
		float Age = 0.f;
		float Life = 4.5f;
	};
	struct FNpcMarker
	{
		TWeakObjectPtr<AActor> Actor;
		bool bTurnIn = false;
	};

	// ---------------------------------------------------------------- Frame / gameplay HUD (MTHUD.cpp)
	void TickTransient(float DeltaSeconds);
	void UpdateWorldScan(AMTCharacterBase* Char);
	void DrawVitals(AMTCharacterBase* Char);
	void DrawHotbar(AMTCharacterBase* Char);
	void DrawTopCenter(AMTCharacterBase* Char);
	float DrawBossBar(AMTCharacterBase* Boss, float TopY);
	void DrawQuestTracker(float TopY);
	void DrawMinimap(AMTCharacterBase* Char);
	void DrawLockOn(AMTCharacterBase* Char);
	void DrawDamageNumbers();
	void DrawInteractionPrompt(AMTCharacterBase* Char);
	void DrawToasts();
	void DrawCenterMessage();
	void DrawStateOverlays(AMTCharacterBase* Char);
	void DrawEdgeGlow(const FLinearColor& Color, float MaxAlpha, float Thickness, int32 Bands);

	// ---------------------------------------------------------------- Menus (MTHUDMenus.cpp)
	void OpenPageInternal(EMTMenuPage Page);
	void DrawMenu();
	void DrawCharacterPage(float X, float Y, float W, float H);
	void DrawElementPage(float X, float Y, float W, float H);
	void DrawRacePage(float X, float Y, float W, float H);
	void DrawMasteryPage(float X, float Y, float W, float H);
	void DrawInventoryPage(float X, float Y, float W, float H);
	void DrawQuestsPage(float X, float Y, float W, float H);
	void DrawMapPage(float X, float Y, float W, float H);
	void DrawPartyPage(float X, float Y, float W, float H);
	void DrawSettingsPage(float X, float Y, float W, float H);
	void DrawRollPage(float X, float Y, float W, float H);
	void DrawNPCDialog();
	/** Ability row line used by the Character / Element / Race pages. Returns the height used. */
	float DrawAbilityLine(FName AbilityId, const FString& KeyLabel, float X, float Y, float W);
	void HandleButton(FName Id);
	void DoRoll();
	void FinishRollAnimation();
	void AdjustSetting(const FString& Key, int32 Dir);
	void FastTravelTo(FName LocationId);
	FString GetQuestTitle(FName QuestId) const;

	// ---------------------------------------------------------------- Drawing helpers (MTHUD.cpp)
	float Sc(float V) const { return V * UIScale; }
	UFont* SmallFont() const;
	UFont* MediumFont() const;
	UFont* LargeFont() const;
	void FillRect(float X, float Y, float W, float H, const FLinearColor& Color);
	void StrokeRect(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness = 1.f);
	void DrawPanelBox(float X, float Y, float W, float H, float Alpha = 0.88f, bool bOrnate = true);
	void DrawMeter(float X, float Y, float W, float H, float Fraction, float Trail, const FLinearColor& Fill, const FLinearColor& TrailColor);
	FVector2D MeasureStr(const FString& Str, UFont* Font, float Scale) const;
	/** HAlign / VAlign: 0 = left/top, 0.5 = centre, 1 = right/bottom. */
	void DrawStr(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, float HAlign = 0.f, float VAlign = 0.f, bool bShadow = true);
	/** Word-wrapped text; returns the height used. */
	float DrawWrapped(const FString& Str, float X, float Y, float MaxW, const FLinearColor& Color, UFont* Font, float Scale, int32 MaxLines = 12);
	FString FitStr(const FString& Str, float MaxW, UFont* Font, float Scale);
	/** Registers a hit box + focus target. Returns true while hovered. */
	bool DrawButtonBox(FName Id, const FString& Label, float X, float Y, float W, float H, bool bEnabled = true, bool bActive = false, const FLinearColor& Accent = FLinearColor::Transparent);
	void StrokeCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness = 1.f, int32 Segments = 32);
	void FillDisc(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Strips = 28);
	void DrawDiamondShape(const FVector2D& Center, float Radius, const FLinearColor& Color, bool bFilled);
	bool ProjectToScreen(const FVector& World, FVector2D& OutScreen);
	float LineHeight(UFont* Font, float Scale) const;

	// ---------------------------------------------------------------- State
	EMTMenuPage OpenPage = EMTMenuPage::None;
	bool bDialogOpen = false;
	FName DialogNpcId;
	FText DialogNpcName;
	FName DialogQuestId;

	FName SelCharacter;
	EMTElement SelElement = EMTElement::None;
	EMTRace SelRace = EMTRace::Human;
	bool bSelRaceValid = false;
	FName SelQuest;
	EMTRollCategory RollCategory = EMTRollCategory::Character;

	// Roll presentation.
	FMTRollResult LastRoll;
	bool bHasLastRoll = false;
	bool bRollAnimating = false;
	float RollAnimTime = 0.f;
	float RollRevealAge = 100.f;
	TArray<FString> RollReel;
	int32 RollReelStart = 0;
	static constexpr float RollAnimDuration = 1.6f;
	static constexpr int32 RollReelTicks = 26;

	// Buttons of the last drawn frame (hit boxes + keyboard focus).
	TArray<FHudButton> Buttons;
	FName FocusedButton;
	FVector2D MousePos = FVector2D::ZeroVector;
	bool bMouseValid = false;
	int32 HitBoxPriority = 0;

	// Frame.
	float UIScale = 1.f;
	float FrameDelta = 0.f;
	double LastDrawSeconds = 0.0;
	float HudTime = 0.f;
	TMap<FString, FString> FitCache;
	float FitCacheScale = 0.f;

	// Vitals trails.
	float HealthTrail = 1.f;
	float ManaTrail = 1.f;
	float StaminaTrail = 1.f;
	TWeakObjectPtr<AMTCharacterBase> BossTarget;
	float BossTrail = 1.f;

	// World scan.
	TArray<TWeakObjectPtr<AMTCharacterBase>> NearbyHostiles;
	TWeakObjectPtr<AMTCharacterBase> NearestBoss;
	TArray<FNpcMarker> NpcMarkers;
	float NpcMarkerRefresh = 0.f;

	TArray<FDamageNumber> DamageNumbers;
	TArray<FToast> Toasts;
	FText CenterText;
	float CenterAge = 0.f;
	float CenterDuration = 0.f;

	FGameplayTag TagAwakened;
	FGameplayTag TagForesight;
};

// Gameplay HUD. The Canvas draws the quest tracker, north-up minimap, lock-on reticle, boss bar,
// floating damage numbers, interaction prompt, toasts and awakening / Demon Eye overlays. The LA PLACE
// Slate widgets draw the hotbar and vitals (SMTHudOverlay) and the Adventurer's Journal pages, the
// roll screen and the NPC quest dialog (SMTJournal), which this HUD owns and drives.
//
// Controller contract: while IsMenuOpen() show the cursor (the journal handles the mouse itself) and
// route keyboard / gamepad UI input to MenuNavigate / MenuConfirm / MenuBack / MenuNextTab.
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

UCLASS()
class MUSHOKURPG_API AMTHUD : public AHUD
{
	GENERATED_BODY()

public:
	AMTHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;

	/** Opens Page, or closes the menu when Page is already open (None closes everything). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void ToggleMenu(EMTMenuPage Page);
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void CloseAll();
	/** True while a menu page or the NPC dialog is open (show cursor, block gameplay input). */
	UFUNCTION(BlueprintPure, Category = "Mushoku|UI") bool IsMenuOpen() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|UI") EMTMenuPage GetOpenPage() const;
	UFUNCTION(BlueprintPure, Category = "Mushoku|UI") bool IsDialogOpen() const;

	/** Moves keyboard/gamepad focus. Screen space: X = right, Y = DOWN (so "up" is (0,-1)). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void MenuNavigate(FIntPoint Direction);
	/** Activates the focused button (or skips a running roll animation). */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|UI") void MenuConfirm();
	/** Skips the roll reveal, else closes the dialog, else closes the menu. */
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
	/** LA PLACE Slate HUD (hotbar + vitals), drawn above the canvas. */
	TSharedPtr<class SMTHudOverlay> SlateHud;
	TSharedPtr<class SWidget> SlateHudContainer;
	/** LA PLACE journal and NPC dialog (Slate), above the hotbar and below the front end. */
	TSharedPtr<class SMTJournal> Journal;
	void SetSlateHudVisible(bool bVisible);
public:
	/** Hides the gameplay HUD (front-end menus, cinematics). */
	void SetGameplayHudHidden(bool bHidden) { bGameplayHudHidden = bHidden; SetSlateHudVisible(!bHidden); }
private:
	bool bGameplayHudHidden = false;
	// ---------------------------------------------------------------- Palette (original anime-fantasy style:
	// deep navy panels, thin gold trim, parchment text).
	static FLinearColor PanelColor(float Alpha = 0.88f) { return FLinearColor(0.04f, 0.05f, 0.09f, Alpha); }
	static FLinearColor Gold(float Alpha = 1.f) { return FLinearColor(0.85f, 0.7f, 0.4f, Alpha); }
	static FLinearColor Parchment(float Alpha = 1.f) { return FLinearColor(0.95f, 0.92f, 0.85f, Alpha); }
	static FLinearColor Dim(float Alpha = 1.f) { return FLinearColor(0.6f, 0.58f, 0.54f, Alpha); }
	static FLinearColor Good(float Alpha = 1.f) { return FLinearColor(0.45f, 0.85f, 0.5f, Alpha); }
	static FLinearColor Bad(float Alpha = 1.f) { return FLinearColor(0.95f, 0.38f, 0.35f, Alpha); }
	static FLinearColor Health(float Alpha = 1.f) { return FLinearColor(0.78f, 0.16f, 0.2f, Alpha); }
	static FLinearColor Mana(float Alpha = 1.f) { return FLinearColor(0.25f, 0.5f, 0.98f, Alpha); }
	static FLinearColor Stamina(float Alpha = 1.f) { return FLinearColor(0.85f, 0.78f, 0.3f, Alpha); }
	static FLinearColor Poise(float Alpha = 1.f) { return FLinearColor(0.7f, 0.72f, 0.78f, Alpha); }
	static FLinearColor Awakening(float Alpha = 1.f) { return FLinearColor(0.95f, 0.55f, 0.95f, Alpha); }
	static FLinearColor WithAlpha(FLinearColor Color, float Alpha) { Color.A = Alpha; return Color; }
	static float SafeFraction(float Value, float Max) { return Max > KINDA_SMALL_NUMBER ? FMath::Clamp(Value / Max, 0.f, 1.f) : 0.f; }
	/** Trail bar: snaps up on heal, eases down after damage. */
	static void UpdateTrail(float Current, float& Trail, float DeltaSeconds);
	static FString FormatCooldown(float Seconds);

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
	FString FitStr(const FString& Str, float MaxW, UFont* Font, float Scale);
	void StrokeCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness = 1.f, int32 Segments = 32);
	void FillDisc(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Strips = 28);
	void DrawDiamondShape(const FVector2D& Center, float Radius, const FLinearColor& Color, bool bFilled);
	bool ProjectToScreen(const FVector& World, FVector2D& OutScreen);
	float LineHeight(UFont* Font, float Scale) const;

	// ---------------------------------------------------------------- State
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

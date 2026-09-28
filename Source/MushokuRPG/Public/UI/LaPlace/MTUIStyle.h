// LA PLACE UI look: fonts (Cinzel, Cinzel Decorative, Cormorant Garamond), palette, texture brushes and the drawing
// helpers every Slate widget of the new UI uses (parchment panels, gold frames, glows, cooldown sweeps).
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

class FSlateWindowElementList;
struct FGeometry;
class USoundBase;
class UTexture2D;

namespace MTUI
{
	// Palette (linear colours).
	extern MUSHOKURPG_API const FLinearColor Ink;          // dark brown-black text on parchment
	extern MUSHOKURPG_API const FLinearColor InkSoft;
	extern MUSHOKURPG_API const FLinearColor Parchment;    // warm cream
	extern MUSHOKURPG_API const FLinearColor Gold;
	extern MUSHOKURPG_API const FLinearColor GoldBright;
	extern MUSHOKURPG_API const FLinearColor GoldDim;
	extern MUSHOKURPG_API const FLinearColor Night;        // deep blue-black panel fill
	extern MUSHOKURPG_API const FLinearColor Mana;
	extern MUSHOKURPG_API const FLinearColor Health;
	extern MUSHOKURPG_API const FLinearColor Stamina;
	extern MUSHOKURPG_API const FLinearColor Awakening;
	extern MUSHOKURPG_API const FLinearColor Danger;
	extern MUSHOKURPG_API const FLinearColor TextLight;    // cream text on dark

	// Fonts from Content/UI/Fonts (SIL Open Font License).
	MUSHOKURPG_API FSlateFontInfo Logo(float Size);        // Cinzel Decorative Black
	MUSHOKURPG_API FSlateFontInfo Title(float Size);       // Cinzel Decorative Bold
	MUSHOKURPG_API FSlateFontInfo Heading(float Size);     // Cinzel
	MUSHOKURPG_API FSlateFontInfo Body(float Size);        // Cormorant Garamond
	MUSHOKURPG_API FSlateFontInfo BodyItalic(float Size);

	/** Brush for a texture asset path (cached; the texture is kept alive). Null if the asset is missing. */
	MUSHOKURPG_API const FSlateBrush* Brush(const FString& TexturePath);
	/** Brush for /Game/LaPlace/UI/<Folder>/<Name>. */
	MUSHOKURPG_API const FSlateBrush* UIBrush(const TCHAR* Folder, const FString& Name);
	/** The ability's icon (data Icon path), or null. */
	MUSHOKURPG_API const FSlateBrush* AbilityIcon(FName AbilityId);
	MUSHOKURPG_API const FSlateBrush* White();

	/** UI scale for the current viewport height (1.0 at 1080 p). */
	MUSHOKURPG_API float Scale(const FGeometry& Geometry);

	// Drawing helpers (all positions / sizes in local widget units).
	MUSHOKURPG_API void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color, const FSlateBrush* Brush = nullptr);
	MUSHOKURPG_API void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness = 1.f);
	MUSHOKURPG_API void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& S, const FSlateFontInfo& Font, const FVector2D& Pos, const FLinearColor& Color, bool bShadow = true);
	/** Text aligned in a box: Align.X/Y in 0 (left/top) .. 0.5 (centre) .. 1 (right/bottom). */
	MUSHOKURPG_API void TextAligned(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& S, const FSlateFontInfo& Font, const FVector2D& Pos, const FVector2D& Size, const FVector2D& Align, const FLinearColor& Color, bool bShadow = true);
	MUSHOKURPG_API FVector2D Measure(const FString& S, const FSlateFontInfo& Font);
	/** Word-wrapped paragraph; returns the height used. */
	MUSHOKURPG_API float Paragraph(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& S, const FSlateFontInfo& Font, const FVector2D& Pos, float Width, const FLinearColor& Color, float LineSpacing = 1.15f);
	/** Soft round glow (additive-looking dot texture). */
	MUSHOKURPG_API void Glow(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Center, float Radius, const FLinearColor& Color);
	/** Parchment card with a darkened edge, gold border and filigree corners. */
	MUSHOKURPG_API void ParchmentPanel(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, float Opacity = 1.f);
	/** Dark translucent panel with a thin gold border (HUD, overlays). */
	MUSHOKURPG_API void DarkPanel(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, float Opacity = 1.f, bool bCorners = false);
	/** Gold border lines + optional corner ornaments. */
	MUSHOKURPG_API void GoldFrame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, float Opacity = 1.f, bool bCorners = true, float CornerSize = 46.f);
	/** Ornamental divider centred on Center with total Width. */
	MUSHOKURPG_API void Divider(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Center, float Width, float Opacity = 1.f);
	/** Pie wipe covering Fraction (0..1) of a square, clockwise from 12 o'clock (cooldowns). */
	MUSHOKURPG_API void RadialWipe(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, float Size, float Fraction, const FLinearColor& Color);
	/** Arc outline (charge progress). */
	MUSHOKURPG_API void Arc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Center, float Radius, float Fraction, const FLinearColor& Color, float Thickness = 2.f);
	/** Horizontal bar with a gradient fill, a trailing "damage" bar and a gold frame. */
	MUSHOKURPG_API void Bar(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, float Fill, float Trail, const FLinearColor& Color);

	/** 2D UI sound from /Game/LaPlace/Audio/UI/<Name> (silently skipped when missing). */
	MUSHOKURPG_API void Sound(const TCHAR* Name, float Volume = 0.8f);
}

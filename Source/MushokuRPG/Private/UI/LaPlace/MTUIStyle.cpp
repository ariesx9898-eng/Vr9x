#include "UI/LaPlace/MTUIStyle.h"
#include "Core/MTDataRegistry.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Layout/Geometry.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace MTUI
{
	const FLinearColor Ink(0.045f, 0.028f, 0.016f, 1.f);
	const FLinearColor InkSoft(0.12f, 0.08f, 0.05f, 1.f);
	const FLinearColor Parchment(0.86f, 0.74f, 0.53f, 1.f);
	const FLinearColor Gold(0.83f, 0.6f, 0.2f, 1.f);
	const FLinearColor GoldBright(1.f, 0.86f, 0.45f, 1.f);
	const FLinearColor GoldDim(0.42f, 0.29f, 0.1f, 1.f);
	const FLinearColor Night(0.012f, 0.016f, 0.03f, 1.f);
	const FLinearColor Mana(0.18f, 0.55f, 1.f, 1.f);
	const FLinearColor Health(0.75f, 0.07f, 0.05f, 1.f);
	const FLinearColor Stamina(0.55f, 0.72f, 0.2f, 1.f);
	const FLinearColor Awakening(0.95f, 0.72f, 0.25f, 1.f);
	const FLinearColor Danger(0.95f, 0.18f, 0.12f, 1.f);
	const FLinearColor TextLight(0.96f, 0.9f, 0.78f, 1.f);

	namespace
	{
		FSlateFontInfo FontFromFile(const TCHAR* File, float Size)
		{
			static TMap<FString, TSharedPtr<const FCompositeFont>> Cache;
			const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("UI/Fonts") / File);
			TSharedPtr<const FCompositeFont>* Found = Cache.Find(Path);
			if (!Found)
			{
				if (!FPaths::FileExists(Path))
				{
					return FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(Size));
				}
				Found = &Cache.Add(Path, MakeShared<FStandaloneCompositeFont>(FName(File), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
			}
			return FSlateFontInfo(*Found, Size);
		}

		struct FBrushEntry
		{
			TSharedPtr<FSlateBrush> Brush;
		};
	}

	FSlateFontInfo Logo(float Size) { return FontFromFile(TEXT("CinzelDecorative-Black.ttf"), Size); }
	FSlateFontInfo Title(float Size) { return FontFromFile(TEXT("CinzelDecorative-Bold.ttf"), Size); }
	FSlateFontInfo Heading(float Size) { return FontFromFile(TEXT("Cinzel.ttf"), Size); }
	FSlateFontInfo Body(float Size) { return FontFromFile(TEXT("CormorantGaramond.ttf"), Size); }
	FSlateFontInfo BodyItalic(float Size) { return FontFromFile(TEXT("CormorantGaramond-Italic.ttf"), Size); }

	const FSlateBrush* Brush(const FString& TexturePath)
	{
		static TMap<FString, TSharedPtr<FSlateBrush>> Cache;
		static TSet<FString> Missing;
		if (TexturePath.IsEmpty() || Missing.Contains(TexturePath))
		{
			return nullptr;
		}
		if (const TSharedPtr<FSlateBrush>* Found = Cache.Find(TexturePath))
		{
			return Found->Get();
		}
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Texture)
		{
			Missing.Add(TexturePath);
			return nullptr;
		}
		Texture->AddToRoot(); // UI textures live for the whole session
#if WITH_EDITOR
		// Editor builds compile textures asynchronously: until then the size is a placeholder's and the image blurry.
		FTextureCompilingManager::Get().FinishCompilation({ Texture });
#endif
		FVector2D Size(Texture->GetSizeX(), Texture->GetSizeY());
#if WITH_EDITORONLY_DATA
		if (Texture->Source.IsValid())
		{
			Size = FVector2D(Texture->Source.GetSizeX(), Texture->Source.GetSizeY());
		}
#endif
		TSharedPtr<FSlateBrush> NewBrush = MakeShared<FSlateBrush>();
		NewBrush->SetResourceObject(Texture);
		NewBrush->ImageSize = Size;
		NewBrush->DrawAs = ESlateBrushDrawType::Image;
		Cache.Add(TexturePath, NewBrush);
		return NewBrush.Get();
	}

	const FSlateBrush* UIBrush(const TCHAR* Folder, const FString& Name)
	{
		return Brush(FString::Printf(TEXT("/Game/LaPlace/UI/%s/%s.%s"), Folder, *Name, *Name));
	}

	const FSlateBrush* AbilityIcon(FName AbilityId)
	{
		if (AbilityId.IsNone())
		{
			return nullptr;
		}
		const FString Name = FString::Printf(TEXT("T_Icon_%s"), *AbilityId.ToString());
		if (const FSlateBrush* B = UIBrush(TEXT("Icons"), Name))
		{
			return B;
		}
		// Awakened upgrades share the base ability's icon.
		FString Base = AbilityId.ToString();
		if (Base.RemoveFromEnd(TEXT("_Awakened")))
		{
			return UIBrush(TEXT("Icons"), FString::Printf(TEXT("T_Icon_%s"), *Base));
		}
		return nullptr;
	}

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("GenericWhiteBox");
	}

	float Scale(const FGeometry& Geometry)
	{
		return FMath::Clamp(Geometry.GetLocalSize().Y / 1080.f, 0.55f, 2.2f);
	}

	static FPaintGeometry PG(const FGeometry& G, const FVector2D& Pos, const FVector2D& Size)
	{
		return G.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Pos)));
	}

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color, const FSlateBrush* InBrush)
	{
		FSlateDrawElement::MakeBox(Out, Layer, PG(G, Pos, Size), InBrush ? InBrush : White(), ESlateDrawEffect::None, Color);
	}

	void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2f> Points = { FVector2f(A), FVector2f(B) };
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	FVector2D Measure(const FString& S, const FSlateFontInfo& Font)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D(S.Len() * Font.Size * 0.5f, Font.Size * 1.3f);
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Font);
	}

	void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& S, const FSlateFontInfo& Font, const FVector2D& Pos, const FLinearColor& Color, bool bShadow)
	{
		const FVector2D Size = Measure(S, Font);
		if (bShadow)
		{
			const float Off = FMath::Max(1.f, Font.Size * 0.06f);
			FSlateDrawElement::MakeText(Out, Layer, PG(G, Pos + FVector2D(Off, Off), Size), S, Font, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.65f * Color.A));
		}
		FSlateDrawElement::MakeText(Out, Layer + 1, PG(G, Pos, Size), S, Font, ESlateDrawEffect::None, Color);
	}

	void TextAligned(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& S, const FSlateFontInfo& Font, const FVector2D& Pos, const FVector2D& Size, const FVector2D& Align, const FLinearColor& Color, bool bShadow)
	{
		const FVector2D TextSize = Measure(S, Font);
		const FVector2D At = Pos + (Size - TextSize) * Align;
		Text(Out, Layer, G, S, Font, At, Color, bShadow);
	}

	float Paragraph(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& S, const FSlateFontInfo& Font, const FVector2D& Pos, float Width, const FLinearColor& Color, float LineSpacing)
	{
		TArray<FString> Words;
		S.ParseIntoArrayWS(Words);
		FString Current;
		float Y = Pos.Y;
		const float LineHeight = Measure(TEXT("Ag"), Font).Y * LineSpacing;
		auto Flush = [&]()
		{
			if (!Current.IsEmpty())
			{
				Text(Out, Layer, G, Current, Font, FVector2D(Pos.X, Y), Color, false);
				Y += LineHeight;
				Current.Reset();
			}
		};
		for (const FString& Word : Words)
		{
			const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
			if (!Current.IsEmpty() && Measure(Candidate, Font).X > Width)
			{
				Flush();
				Current = Word;
			}
			else
			{
				Current = Candidate;
			}
		}
		Flush();
		return Y - Pos.Y;
	}

	void Glow(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Center, float Radius, const FLinearColor& Color)
	{
		static const FString DotPath(TEXT("/Game/LaPlace/VFX/Textures/T_VFX_Dot.T_VFX_Dot"));
		const FSlateBrush* Dot = Brush(DotPath);
		Box(Out, Layer, G, Center - FVector2D(Radius), FVector2D(Radius * 2.f), Color, Dot ? Dot : White());
	}

	static const FSlateBrush* CornerBrush(int32 Corner)
	{
		// Corner 0 TL, 1 TR, 2 BL, 3 BR: the same ornament mirrored through its UV region.
		static TSharedPtr<FSlateBrush> Corners[4];
		if (!Corners[Corner].IsValid())
		{
			const FSlateBrush* Base = UIBrush(TEXT("Frame"), TEXT("T_Ornament_Corner"));
			if (!Base)
			{
				return nullptr;
			}
			Corners[Corner] = MakeShared<FSlateBrush>(*Base);
			const bool bFlipX = Corner == 1 || Corner == 3;
			const bool bFlipY = Corner == 2 || Corner == 3;
			Corners[Corner]->SetUVRegion(FBox2f(FVector2f(bFlipX ? 1.f : 0.f, bFlipY ? 1.f : 0.f), FVector2f(bFlipX ? 0.f : 1.f, bFlipY ? 0.f : 1.f)));
		}
		return Corners[Corner].Get();
	}

	void GoldFrame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, float Opacity, bool bCorners, float CornerSize)
	{
		const FLinearColor Outer = GoldDim.CopyWithNewOpacity(0.9f * Opacity);
		const FLinearColor Inner = Gold.CopyWithNewOpacity(0.85f * Opacity);
		const FVector2D A = Pos, B = Pos + Size;
		auto Rect = [&](const FVector2D& P0, const FVector2D& P1, const FLinearColor& C, float T)
		{
			TArray<FVector2f> Points = { FVector2f(P0), FVector2f(P1.X, P0.Y), FVector2f(P1), FVector2f(P0.X, P1.Y), FVector2f(P0) };
			FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, C, true, T);
		};
		Rect(A, B, Outer, 2.f);
		Rect(A + FVector2D(4.f), B - FVector2D(4.f), Inner, 1.f);
		if (bCorners)
		{
			const FVector2D C(CornerSize, CornerSize * 1.17f);
			const FVector2D Positions[4] = { A - FVector2D(6.f), FVector2D(B.X - C.X + 6.f, A.Y - 6.f), FVector2D(A.X - 6.f, B.Y - C.Y + 6.f), B - C + FVector2D(6.f) };
			for (int32 i = 0; i < 4; ++i)
			{
				if (const FSlateBrush* Corner = CornerBrush(i))
				{
					Box(Out, Layer + 1, G, Positions[i], C, FLinearColor(1.f, 1.f, 1.f, Opacity), Corner);
				}
			}
		}
	}

	void ParchmentPanel(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, float Opacity)
	{
		// Soft drop shadow, parchment, darkened edges, then the gold frame.
		Box(Out, Layer, G, Pos + FVector2D(8.f, 10.f), Size, FLinearColor(0.f, 0.f, 0.f, 0.45f * Opacity));
		const FSlateBrush* Paper = UIBrush(TEXT("Frame"), TEXT("T_Parchment"));
		Box(Out, Layer + 1, G, Pos, Size, Paper ? FLinearColor(1.f, 1.f, 1.f, Opacity) : Parchment.CopyWithNewOpacity(Opacity), Paper);
		const float Edge = FMath::Min(Size.X, Size.Y) * 0.06f;
		Box(Out, Layer + 2, G, Pos, FVector2D(Size.X, Edge), FLinearColor(0.2f, 0.1f, 0.03f, 0.18f * Opacity));
		Box(Out, Layer + 2, G, FVector2D(Pos.X, Pos.Y + Size.Y - Edge), FVector2D(Size.X, Edge), FLinearColor(0.2f, 0.1f, 0.03f, 0.22f * Opacity));
		GoldFrame(Out, Layer + 3, G, Pos, Size, Opacity, true, FMath::Clamp(FMath::Min(Size.X, Size.Y) * 0.12f, 26.f, 70.f));
	}

	void DarkPanel(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, float Opacity, bool bCorners)
	{
		Box(Out, Layer, G, Pos, Size, FLinearColor(0.012f, 0.014f, 0.024f, 0.72f * Opacity));
		Box(Out, Layer, G, Pos, FVector2D(Size.X, Size.Y * 0.45f), FLinearColor(0.08f, 0.07f, 0.1f, 0.25f * Opacity));
		GoldFrame(Out, Layer + 1, G, Pos, Size, Opacity * 0.9f, bCorners, FMath::Clamp(FMath::Min(Size.X, Size.Y) * 0.35f, 18.f, 40.f));
	}

	void Divider(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Center, float Width, float Opacity)
	{
		if (const FSlateBrush* D = UIBrush(TEXT("Frame"), TEXT("T_Ornament_Divider")))
		{
			const float Height = Width * D->ImageSize.Y / FMath::Max(1.f, (float)D->ImageSize.X);
			Box(Out, Layer, G, Center - FVector2D(Width * 0.5f, Height * 0.5f), FVector2D(Width, Height), FLinearColor(1.f, 1.f, 1.f, Opacity), D);
		}
		else
		{
			Line(Out, Layer, G, Center - FVector2D(Width * 0.5f, 0.f), Center + FVector2D(Width * 0.5f, 0.f), Gold.CopyWithNewOpacity(Opacity), 2.f);
		}
	}

	void RadialWipe(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, float Size, float Fraction, const FLinearColor& Color)
	{
		Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
		if (Fraction <= 0.f)
		{
			return;
		}
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White());
		const FSlateRenderTransform& T = G.GetAccumulatedRenderTransform();
		const FColor C = Color.ToFColor(true);
		const FVector2f Center(Pos.X + Size * 0.5f, Pos.Y + Size * 0.5f);
		const float Half = Size * 0.5f;
		// Fan from the centre; each step's point is projected onto the square's edge so the wipe fills the icon.
		const int32 Steps = FMath::Max(2, FMath::CeilToInt(64 * Fraction));
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indexes;
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, Center, FVector2f(0.5f, 0.5f), C));
		for (int32 i = 0; i <= Steps; ++i)
		{
			const float A = -PI * 0.5f + 2.f * PI * Fraction * (float)i / Steps;
			const FVector2f Dir(FMath::Cos(A), FMath::Sin(A));
			const float K = Half / FMath::Max(FMath::Abs(Dir.X), FMath::Abs(Dir.Y));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, Center + Dir * K, FVector2f(0.5f, 0.5f), C));
			if (i > 0)
			{
				Indexes.Add(0);
				Indexes.Add(i);
				Indexes.Add(i + 1);
			}
		}
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Handle, Verts, Indexes, nullptr, 0, 0);
	}

	void Arc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Center, float Radius, float Fraction, const FLinearColor& Color, float Thickness)
	{
		Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
		if (Fraction <= 0.f)
		{
			return;
		}
		const int32 Steps = FMath::Max(3, FMath::CeilToInt(48 * Fraction));
		TArray<FVector2f> Points;
		for (int32 i = 0; i <= Steps; ++i)
		{
			const float A = -PI * 0.5f + 2.f * PI * Fraction * (float)i / Steps;
			Points.Add(FVector2f(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius));
		}
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	void Bar(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Pos, const FVector2D& Size, float Fill, float Trail, const FLinearColor& Color)
	{
		Fill = FMath::Clamp(Fill, 0.f, 1.f);
		Trail = FMath::Clamp(FMath::Max(Trail, Fill), 0.f, 1.f);
		Box(Out, Layer, G, Pos, Size, FLinearColor(0.02f, 0.015f, 0.015f, 0.85f));
		if (Trail > Fill)
		{
			Box(Out, Layer + 1, G, Pos + FVector2D(Size.X * Fill, 0.f), FVector2D(Size.X * (Trail - Fill), Size.Y), FLinearColor(0.95f, 0.85f, 0.6f, 0.55f));
		}
		if (Fill > 0.f)
		{
			Box(Out, Layer + 1, G, Pos, FVector2D(Size.X * Fill, Size.Y), Color);
			// Highlight band for a rounded, glassy look.
			Box(Out, Layer + 2, G, Pos, FVector2D(Size.X * Fill, Size.Y * 0.38f), FLinearColor(1.f, 1.f, 1.f, 0.16f));
		}
		TArray<FVector2f> Points = { FVector2f(Pos), FVector2f(Pos.X + Size.X, Pos.Y), FVector2f(Pos + Size), FVector2f(Pos.X, Pos.Y + Size.Y), FVector2f(Pos) };
		FSlateDrawElement::MakeLines(Out, Layer + 3, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Gold.CopyWithNewOpacity(0.85f), true, 1.f);
	}

	void Sound(const TCHAR* Name, float Volume)
	{
		UWorld* World = nullptr;
		if (GEngine && GEngine->GameViewport)
		{
			World = GEngine->GameViewport->GetWorld();
		}
		if (!World)
		{
			return;
		}
		static TMap<FString, TWeakObjectPtr<USoundBase>> Cache;
		static TSet<FString> Missing;
		const FString Path = FString::Printf(TEXT("/Game/LaPlace/Audio/UI/%s.%s"), Name, Name);
		if (Missing.Contains(Path))
		{
			return;
		}
		USoundBase* S = Cache.FindRef(Path).Get();
		if (!S)
		{
			S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!S)
			{
				Missing.Add(Path);
				return;
			}
			Cache.Add(Path, S);
		}
		UGameplayStatics::PlaySound2D(World, S, Volume);
	}
}

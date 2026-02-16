// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextToImageLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "Containers/UnrealString.h"

#include <cmath>

// Single translation unit for stb implementations
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "ThirdParty/stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT(x) check(x)
#ifdef _MSC_VER
	#define _CRT_SECURE_NO_WARNINGS
#endif
#include "ThirdParty/stb_image_write.h"

namespace
{
	static inline unsigned char Clamp255(int32 v)
	{
		if (v < 0) return 0;
		if (v > 255) return 255;
		return static_cast<unsigned char>(v);
	}

	/** Resolve font path: if relative, try under project content and plugin content. */
	FString ResolveFontPath(const FString& FontPath)
	{
		if (FPaths::IsRelative(FontPath))
		{
			FString Abs = FPaths::ProjectDir() / FontPath;
			if (FPaths::FileExists(Abs)) return FPaths::ConvertRelativePathToFull(Abs);
			Abs = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("TextToImage"), FontPath);
			if (FPaths::FileExists(Abs)) return FPaths::ConvertRelativePathToFull(Abs);
			return FPaths::ConvertRelativePathToFull(FontPath);
		}
		return FontPath;
	}

	/** Core render: fill RGBA buffer (W*H*4). TextColor/BackgroundColor: alpha 0 background = transparent. */
	bool RenderTextToRgba(
		const TArray<uint8>& FontBuffer,
		const FString& Text,
		int32 W,
		int32 H,
		FLinearColor TextColor,
		FLinearColor BackgroundColor,
		TArray<uint8>& OutRgba)
	{
		if (FontBuffer.Num() == 0 || Text.IsEmpty() || W <= 0 || H <= 0) return false;

		stbtt_fontinfo font;
		if (!stbtt_InitFont(&font, const_cast<unsigned char*>(FontBuffer.GetData()), 0))
		{
			UE_LOG(LogTemp, Error, TEXT("TextToImage: Failed to init font."));
			return false;
		}

		OutRgba.SetNumUninitialized(W * H * 4);
		const uint8 Br = Clamp255(FMath::RoundToInt(BackgroundColor.R * 255.f));
		const uint8 Bg = Clamp255(FMath::RoundToInt(BackgroundColor.G * 255.f));
		const uint8 Bb = Clamp255(FMath::RoundToInt(BackgroundColor.B * 255.f));
		const uint8 Ba = Clamp255(FMath::RoundToInt(BackgroundColor.A * 255.f));
		for (int32 i = 0; i < W * H; ++i)
		{
			OutRgba[i * 4 + 0] = Br;
			OutRgba[i * 4 + 1] = Bg;
			OutRgba[i * 4 + 2] = Bb;
			OutRgba[i * 4 + 3] = Ba;
		}
		const int32 Tr = Clamp255(FMath::RoundToInt(TextColor.R * 255.f));
		const int32 Tg = Clamp255(FMath::RoundToInt(TextColor.G * 255.f));
		const int32 Tb = Clamp255(FMath::RoundToInt(TextColor.B * 255.f));

		const float NominalPixelHeight = 64.0f;
		float scale = stbtt_ScaleForPixelHeight(&font, NominalPixelHeight);

		int ascent = 0, descent = 0, lineGap = 0;
		stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
		float baseline = ascent * scale;

		// Measure total width at nominal scale (ASCII/Latin-1)
		int totalWidth = 0;
		const FTCHARToUTF8 Conv(*Text);
		const char* textUtf8 = Conv.Get();
		for (const char* p = textUtf8; *p; ++p)
		{
			int ax = 0, lsb = 0;
			stbtt_GetCodepointHMetrics(&font, static_cast<int>(static_cast<unsigned char>(*p)), &ax, &lsb);
			totalWidth += static_cast<int>(FMath::FloorToInt(ax * scale + 0.5f));
			if (*(p + 1))
			{
				totalWidth += static_cast<int>(FMath::FloorToInt(
					stbtt_GetCodepointKernAdvance(&font,
						static_cast<int>(static_cast<unsigned char>(*p)),
						static_cast<int>(static_cast<unsigned char>(*(p + 1)))) * scale + 0.5f));
			}
		}

		if (totalWidth <= 0) return true;

		float margin = 0.05f * W;
		float targetWidth = W - 2.0f * margin;
		float finalScale = scale * (targetWidth / static_cast<float>(totalWidth));
		float finalBaseline = ascent * finalScale;

		int penX = FMath::FloorToInt((W - (totalWidth * (finalScale / scale))) / 2.0f + 0.5f);
		int penY = FMath::FloorToInt(finalBaseline + (H - (ascent - descent) * finalScale) / 2.0f + 0.5f);

		unsigned char* img = OutRgba.GetData();

		for (const char* p = textUtf8; *p; ++p)
		{
			int codepoint = static_cast<unsigned char>(*p);
			int advance = 0, lsb = 0;
			stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &lsb);
			int kern = 0;
			if (*(p + 1)) kern = stbtt_GetCodepointKernAdvance(&font, codepoint, static_cast<unsigned char>(*(p + 1)));

			int glyphW = 0, glyphH = 0, xoff = 0, yoff = 0;
			unsigned char* glyphBitmap = stbtt_GetCodepointBitmap(&font, 0, finalScale, codepoint, &glyphW, &glyphH, &xoff, &yoff);
			if (glyphBitmap)
			{
				int dstX = penX + xoff;
				int dstY = penY + yoff;
				for (int gy = 0; gy < glyphH; ++gy)
				{
					int iy = dstY + gy;
					if (iy < 0 || iy >= H) continue;
					for (int gx = 0; gx < glyphW; ++gx)
					{
						int ix = dstX + gx;
						if (ix < 0 || ix >= W) continue;
						unsigned char srcA = glyphBitmap[gy * glyphW + gx];
						if (srcA == 0) continue;
						int idx = (iy * W + ix) * 4;
						unsigned char dstA = img[idx + 3];
						int outA = srcA + (dstA * (255 - srcA) + 127) / 255;
						int dstR = img[idx + 0], dstG = img[idx + 1], dstB = img[idx + 2];
						int outR = (srcA * Tr + dstR * dstA * (255 - srcA) / 255) / (outA ? outA : 1);
						int outG = (srcA * Tg + dstG * dstA * (255 - srcA) / 255) / (outA ? outA : 1);
						int outB = (srcA * Tb + dstB * dstA * (255 - srcA) / 255) / (outA ? outA : 1);
						img[idx + 0] = Clamp255(outR);
						img[idx + 1] = Clamp255(outG);
						img[idx + 2] = Clamp255(outB);
						img[idx + 3] = Clamp255(outA);
					}
				}
				stbtt_FreeBitmap(glyphBitmap, nullptr);
			}
			penX += FMath::FloorToInt(advance * finalScale + 0.5f);
			penX += FMath::FloorToInt(kern * finalScale + 0.5f);
		}

		return true;
	}

	bool CreateTextureFromRgba(int32 Width, int32 Height, const TArray<uint8>& Rgba, UTexture2D*& OutTexture)
	{
		OutTexture = nullptr;
		if (Rgba.Num() != Width * Height * 4) return false;

		// Create transient RGBA texture and upload our buffer
		OutTexture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8);
		if (!OutTexture || !OutTexture->GetPlatformData()) return false;

		OutTexture->SRGB = false;
		OutTexture->CompressionSettings = TC_Default;
		OutTexture->LODGroup = TEXTUREGROUP_UI;

		FTexture2DMipMap& Mip = OutTexture->GetPlatformData()->Mips[0];
		void* Dest = Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (!Dest) return false;
		FMemory::Memcpy(Dest, Rgba.GetData(), static_cast<SIZE_T>(Rgba.Num()));
		Mip.BulkData.Unlock();
		OutTexture->UpdateResource();
		return true;
	}
}

bool UTextToImageLibrary::RenderTextToTexture(const FString& FontPath, const FString& Text, int32 Size, UTexture2D*& OutTexture)
{
	return RenderTextToTextureEx(FontPath, Text, Size, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), OutTexture);
}

bool UTextToImageLibrary::RenderTextToTextureEx(const FString& FontPath, const FString& Text, int32 Size, FLinearColor TextColor, FLinearColor BackgroundColor, UTexture2D*& OutTexture)
{
	OutTexture = nullptr;
	FString Resolved = ResolveFontPath(FontPath);
	TArray<uint8> FontBuffer;
	if (!FFileHelper::LoadFileToArray(FontBuffer, *Resolved))
	{
		UE_LOG(LogTemp, Error, TEXT("TextToImage: Could not load font: %s"), *Resolved);
		return false;
	}
	TArray<uint8> Rgba;
	if (!RenderTextToRgba(FontBuffer, Text, Size, Size, TextColor, BackgroundColor, Rgba)) return false;
	return CreateTextureFromRgba(Size, Size, Rgba, OutTexture);
}

bool UTextToImageLibrary::RenderTextToPng(const FString& FontPath, const FString& Text, int32 Size, const FString& OutputPngPath, UTexture2D*& OutTexture)
{
	return RenderTextToPngEx(FontPath, Text, Size, OutputPngPath, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), OutTexture);
}

bool UTextToImageLibrary::RenderTextToPngEx(const FString& FontPath, const FString& Text, int32 Size, const FString& OutputPngPath, FLinearColor TextColor, FLinearColor BackgroundColor, UTexture2D*& OutTexture)
{
	OutTexture = nullptr;
	FString Resolved = ResolveFontPath(FontPath);
	TArray<uint8> FontBuffer;
	if (!FFileHelper::LoadFileToArray(FontBuffer, *Resolved))
	{
		UE_LOG(LogTemp, Error, TEXT("TextToImage: Could not load font: %s"), *Resolved);
		return false;
	}
	TArray<uint8> Rgba;
	if (!RenderTextToRgba(FontBuffer, Text, Size, Size, TextColor, BackgroundColor, Rgba)) return false;

	FTCHARToUTF8 PathConv(*OutputPngPath);
	int ok = stbi_write_png(PathConv.Get(), Size, Size, 4, Rgba.GetData(), Size * 4);
	if (!ok)
	{
		UE_LOG(LogTemp, Error, TEXT("TextToImage: Failed to write PNG: %s"), *OutputPngPath);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("TextToImage: Wrote %s (%dx%d)"), *OutputPngPath, Size, Size);
	return CreateTextureFromRgba(Size, Size, Rgba, OutTexture);
}

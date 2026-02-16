// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Texture2D.h"
#include "Math/Color.h"
#include "TextToImageLibrary.generated.h"

UCLASS(meta = (DisplayName = "Text To Image"))
class TEXTTOIMAGE_API UTextToImageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Renders text into a square texture using a TTF font.
	 * @param FontPath Absolute or project-relative path to a .ttf font file
	 * @param Text The string to render (ASCII/Latin-1)
	 * @param Size Width and height of the output texture in pixels
	 * @param OutTexture The created texture (valid only if the function returns true)
	 * @return True if the texture was created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "TextToImage", meta = (DisplayName = "Render Text To Texture"))
	static bool RenderTextToTexture(
		const FString& FontPath,
		const FString& Text,
		int32 Size,
		UTexture2D*& OutTexture);

	/**
	 * Renders text and saves it as a PNG file. Optionally returns a texture.
	 * @param FontPath Absolute or project-relative path to a .ttf font file
	 * @param Text The string to render
	 * @param Size Width and height of the image in pixels
	 * @param OutputPngPath Full path for the output PNG file
	 * @param OutTexture Optional; if not null, the created texture is assigned (caller must pass a variable to assign)
	 * @return True if the PNG was written (and texture created if requested)
	 */
	UFUNCTION(BlueprintCallable, Category = "TextToImage", meta = (DisplayName = "Render Text To PNG"))
	static bool RenderTextToPng(
		const FString& FontPath,
		const FString& Text,
		int32 Size,
		const FString& OutputPngPath,
		UTexture2D*& OutTexture);

	/**
	 * Extended render with text and background color. Used by editor and Blueprint.
	 * @param BackgroundColor Alpha 0 = transparent background.
	 */
	UFUNCTION(BlueprintCallable, Category = "TextToImage", meta = (DisplayName = "Render Text To Texture (with Colors)"))
	static bool RenderTextToTextureEx(
		const FString& FontPath,
		const FString& Text,
		int32 Size,
		FLinearColor TextColor,
		FLinearColor BackgroundColor,
		UTexture2D*& OutTexture);

	/** Extended render to PNG with text and background color. */
	UFUNCTION(BlueprintCallable, Category = "TextToImage", meta = (DisplayName = "Render Text To PNG (with Colors)"))
	static bool RenderTextToPngEx(
		const FString& FontPath,
		const FString& Text,
		int32 Size,
		const FString& OutputPngPath,
		FLinearColor TextColor,
		FLinearColor BackgroundColor,
		UTexture2D*& OutTexture);
};

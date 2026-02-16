// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextToImageEditorModule.h"
#include "TextToImageEditorSlateWindow.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FTextToImageEditorModule"

void FTextToImageEditorModule::StartupModule()
{
	RegisterMenus();
}

void FTextToImageEditorModule::ShutdownModule()
{
	UnregisterMenus();
}

void FTextToImageEditorModule::RegisterMenus()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTextToImageEditorModule::RegisterContentBrowserPathContextMenu));
}

void FTextToImageEditorModule::UnregisterMenus()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FTextToImageEditorModule::RegisterContentBrowserPathContextMenu()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.PathViewContextMenu");
	if (!Menu) return;

	FToolMenuSection& Section = Menu->FindOrAddSection("ContentBrowserNewAsset");
	Section.AddMenuEntry(
		"CreateTextureFromText",
		LOCTEXT("CreateTextureFromText", "Create Texture from Text..."),
		LOCTEXT("CreateTextureFromTextTooltip", "Opens a window to create a texture from typed text (font, colors, size) and import it here."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FString DestinationPath = TEXT("/Game");
			FContentBrowserModule& CBModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			if (IContentBrowserSingleton* CB = CBModule.GetContentBrowserSingleton())
			{
				TArray<FString> SelectedPaths;
				CB->GetSelectedPathViewFolders(SelectedPaths);
				if (SelectedPaths.Num() > 0) DestinationPath = SelectedPaths[0];
			}
			FTextToImageEditorSlateWindow::OpenWindow(DestinationPath);
		}))
	);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTextToImageEditorModule, TextToImageEditor)

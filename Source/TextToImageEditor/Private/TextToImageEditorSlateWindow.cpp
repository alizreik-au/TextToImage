// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextToImageEditorSlateWindow.h"
#include "TextToImageLibrary.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Engine/Texture2D.h"
#include "Styling/AppStyle.h"
#include "Containers/Ticker.h"

#define LOCTEXT_NAMESPACE "TextToImageEditor"

namespace
{
	constexpr int32 PreviewSize = 256;
	constexpr float PreviewDebounceSeconds = 0.2f;
}

class STextToImageDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STextToImageDialog) {}
		SLATE_ARGUMENT(FString, DestinationPath)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		DestinationPath = InArgs._DestinationPath;
		if (DestinationPath.IsEmpty()) DestinationPath = TEXT("/Game");

		TextContent = TEXT("Hello");
		FontPath = FPaths::ProjectDir() / TEXT("Content");
		SizeValue = 512;
		AssetName = TEXT("T_TextTexture");
		TextColor = FLinearColor::White;
		BackgroundColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.f);
		bTransparentBackground = false;

		ChildSlot
		[
			SNew(SBorder)
			.Padding(12.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2)
						[SNew(SBox).WidthOverride(80.f)[SNew(STextBlock).Text(LOCTEXT("TextLabel", "Text:"))]]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0)
						[SAssignNew(TextEdit, SEditableTextBox).Text(FText::FromString(TextContent)).OnTextChanged(this, &STextToImageDialog::OnTextChanged)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2)
						[SNew(SBox).WidthOverride(80.f)[SNew(STextBlock).Text(LOCTEXT("TextColorLabel", "Text R,G,B:"))]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
						[SNew(SBox).WidthOverride(48)[SAssignNew(TextRSpin, SSpinBox<float>).Value(1.f).MinValue(0.f).MaxValue(1.f).OnValueChanged(this, &STextToImageDialog::OnTextColorChanged)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
						[SNew(SBox).WidthOverride(48)[SAssignNew(TextGSpin, SSpinBox<float>).Value(1.f).MinValue(0.f).MaxValue(1.f).OnValueChanged(this, &STextToImageDialog::OnTextColorChanged)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
						[SNew(SBox).WidthOverride(48)[SAssignNew(TextBSpin, SSpinBox<float>).Value(1.f).MinValue(0.f).MaxValue(1.f).OnValueChanged(this, &STextToImageDialog::OnTextColorChanged)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
						[SNew(SColorBlock).Color(this, &STextToImageDialog::GetTextColor).Size(FVector2D(20, 20))]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2)
						[SNew(SBox).WidthOverride(80.f)[SNew(STextBlock).Text(LOCTEXT("BgLabel", "Background:"))]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
						[SNew(SBox).WidthOverride(48)[SAssignNew(BgRSpin, SSpinBox<float>).Value(0.02f).MinValue(0.f).MaxValue(1.f).OnValueChanged(this, &STextToImageDialog::OnBgColorChanged)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
						[SNew(SBox).WidthOverride(48)[SAssignNew(BgGSpin, SSpinBox<float>).Value(0.02f).MinValue(0.f).MaxValue(1.f).OnValueChanged(this, &STextToImageDialog::OnBgColorChanged)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
						[SNew(SBox).WidthOverride(48)[SAssignNew(BgBSpin, SSpinBox<float>).Value(0.02f).MinValue(0.f).MaxValue(1.f).OnValueChanged(this, &STextToImageDialog::OnBgColorChanged)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
						[SNew(SBox).WidthOverride(48)[SAssignNew(BgASpin, SSpinBox<float>).Value(1.f).MinValue(0.f).MaxValue(1.f).OnValueChanged(this, &STextToImageDialog::OnBgColorChanged)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
						[SNew(SColorBlock).Color(this, &STextToImageDialog::GetBackgroundColor).Size(FVector2D(20, 20))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
						[SNew(SCheckBox).IsChecked(this, &STextToImageDialog::IsTransparentChecked).OnCheckStateChanged(this, &STextToImageDialog::OnTransparentChanged)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
						[SNew(STextBlock).Text(LOCTEXT("Transparent", "Transparent"))]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2)
						[SNew(SBox).WidthOverride(80.f)[SNew(STextBlock).Text(LOCTEXT("FontPathLabel", "Font path:"))]]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0)
						[SAssignNew(FontPathEdit, SEditableTextBox).Text(FText::FromString(FontPath)).OnTextChanged(this, &STextToImageDialog::OnFontPathChanged)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
						[SNew(SButton).Text(LOCTEXT("Browse", "Browse")).OnClicked(this, &STextToImageDialog::OnBrowseFont)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2)
						[SNew(SBox).WidthOverride(80.f)[SNew(STextBlock).Text(LOCTEXT("SizeLabel", "Size (px):"))]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
						[SAssignNew(SizeSpin, SSpinBox<int32>).Value(SizeValue).MinValue(16).MaxValue(4096).OnValueChanged(this, &STextToImageDialog::OnSizeChanged)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0)
						[SNew(SBox).WidthOverride(80.f)[SNew(STextBlock).Text(LOCTEXT("AssetNameLabel", "Asset name:"))]]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0)
						[SAssignNew(AssetNameEdit, SEditableTextBox).Text(FText::FromString(AssetName)).OnTextChanged(this, &STextToImageDialog::OnAssetNameChanged)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
				[
					SNew(SBox).HeightOverride(PreviewSize + 8)
					[
						SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(4)
						[
							SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								SAssignNew(PreviewImage, SImage)
								.DesiredSizeOverride(FVector2D(PreviewSize, PreviewSize))
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
						[SNew(SButton).Text(LOCTEXT("Create", "Create Texture")).OnClicked(this, &STextToImageDialog::OnCreateTexture)]
					+ SHorizontalBox::Slot().AutoWidth()
						[SNew(SButton).Text(LOCTEXT("Cancel", "Cancel")).OnClicked(this, &STextToImageDialog::OnCancel)]
				]
			]
		];

		SchedulePreviewRefresh();
	}

private:
	FString DestinationPath;
	FString TextContent;
	FString FontPath;
	int32 SizeValue;
	FString AssetName;
	FLinearColor TextColor;
	FLinearColor BackgroundColor;
	bool bTransparentBackground = false;

	TSharedPtr<SEditableTextBox> TextEdit;
	TSharedPtr<SEditableTextBox> FontPathEdit;
	TSharedPtr<SEditableTextBox> AssetNameEdit;
	TSharedPtr<SSpinBox<int32>> SizeSpin;
	TSharedPtr<SSpinBox<float>> TextRSpin, TextGSpin, TextBSpin;
	TSharedPtr<SSpinBox<float>> BgRSpin, BgGSpin, BgBSpin, BgASpin;
	TSharedPtr<SImage> PreviewImage;

	UTexture2D* PreviewTexture = nullptr;
	TSharedPtr<FSlateDynamicImageBrush> PreviewBrush;
	FDelegateHandle DebounceHandle;
	float DebounceAccum = 0.f;

	FLinearColor GetTextColor() const { return TextColor; }
	FLinearColor GetBackgroundColor() const { return BackgroundColor; }
	ECheckBoxState IsTransparentChecked() const { return bTransparentBackground ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

	void OnTextChanged(const FText& In) { TextContent = In.ToString(); SchedulePreviewRefresh(); }
	void OnFontPathChanged(const FText& In) { FontPath = In.ToString(); SchedulePreviewRefresh(); }
	void OnAssetNameChanged(const FText& In) { AssetName = In.ToString(); }
	void OnSizeChanged(int32 V) { SizeValue = V; SchedulePreviewRefresh(); }
	void OnTransparentChanged(ECheckBoxState S) { bTransparentBackground = (S == ECheckBoxState::Checked); BackgroundColor.A = bTransparentBackground ? 0.f : (BgASpin.IsValid() ? BgASpin->GetValue() : 1.f); SchedulePreviewRefresh(); }

	void OnTextColorChanged(float)
	{
		if (TextRSpin.IsValid() && TextGSpin.IsValid() && TextBSpin.IsValid())
		{
			TextColor.R = TextRSpin->GetValue();
			TextColor.G = TextGSpin->GetValue();
			TextColor.B = TextBSpin->GetValue();
			SchedulePreviewRefresh();
		}
	}
	void OnBgColorChanged(float)
	{
		if (BgRSpin.IsValid() && BgGSpin.IsValid() && BgBSpin.IsValid() && BgASpin.IsValid())
		{
			BackgroundColor.R = BgRSpin->GetValue();
			BackgroundColor.G = BgGSpin->GetValue();
			BackgroundColor.B = BgBSpin->GetValue();
			if (!bTransparentBackground) BackgroundColor.A = BgASpin->GetValue();
			SchedulePreviewRefresh();
		}
	}

	FReply OnBrowseFont()
	{
		void* ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared()).IsValid() ? FSlateApplication::Get().FindWidgetWindow(AsShared())->GetNativeWindow()->GetOSWindowHandle() : nullptr;
		IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
		if (Desktop && ParentWindow)
		{
			TArray<FString> OutFiles;
			if (Desktop->OpenFileDialog(ParentWindow, LOCTEXT("ChooseFont", "Choose font").ToString(), FontPath, TEXT(""), TEXT("TrueType fonts (*.ttf)|*.ttf|All files (*.*)|*.*"), EFileDialogFlags::None, OutFiles) && OutFiles.Num() > 0)
			{
				FontPath = OutFiles[0];
				if (FontPathEdit.IsValid()) FontPathEdit->SetText(FText::FromString(FontPath));
				SchedulePreviewRefresh();
			}
		}
		return FReply::Handled();
	}

	void SchedulePreviewRefresh()
	{
		if (DebounceHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(DebounceHandle);
		DebounceHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &STextToImageDialog::TickPreview), PreviewDebounceSeconds);
	}

	bool TickPreview(float DeltaTime)
	{
		RefreshPreview();
		DebounceHandle.Reset();
		return false;
	}

	void RefreshPreview()
	{
		FLinearColor Bg = BackgroundColor;
		if (bTransparentBackground) Bg.A = 0.f;
		UTexture2D* Tex = nullptr;
		if (!UTextToImageLibrary::RenderTextToTextureEx(FontPath, TextContent, PreviewSize, TextColor, Bg, Tex) || !Tex)
		{
			return;
		}
		PreviewTexture = Tex;
		PreviewBrush = MakeShareable(new FSlateDynamicImageBrush(Tex, FVector2D(PreviewSize, PreviewSize), FName(*Tex->GetName())));
		if (PreviewImage.IsValid()) PreviewImage->SetImage(PreviewBrush.ToSharedRef());
	}

	FReply OnCreateTexture()
	{
		FLinearColor Bg = BackgroundColor;
		if (bTransparentBackground) Bg.A = 0.f;

		UTexture2D* Dummy = nullptr;
		if (!UTextToImageLibrary::RenderTextToTextureEx(FontPath, TextContent, SizeValue, TextColor, Bg, Dummy))
		{
			return FReply::Handled();
		}
		// We need the RGBA to write PNG - use RenderTextToPngEx which does both
		FString SavedDir = FPaths::ProjectSavedDir() / TEXT("TextToImage");
		IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
		if (!PF.DirectoryExists(*SavedDir)) PF.CreateDirectoryTree(*SavedDir);

		FString SafeName = AssetName.IsEmpty() ? TEXT("TextTexture") : AssetName;
		FString PngPath = SavedDir / SafeName + TEXT(".png");
		Dummy = nullptr;
		if (!UTextToImageLibrary::RenderTextToPngEx(FontPath, TextContent, SizeValue, PngPath, TextColor, Bg, Dummy))
		{
			return FReply::Handled();
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		TArray<UAssetImportTask*> Tasks;
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = PngPath;
		Task->DestinationPath = DestinationPath;
		Task->DestinationName = FPaths::GetBaseFilename(SafeName);
		Task->bSave = true;
		Task->bReplaceExisting = true;
		Task->bAutomated = true;
		Tasks.Add(Task);
		AssetTools.ImportAssetTasks(Tasks);
		const TArray<UObject*>& Imported = Task->GetObjects();
		if (Imported.Num() > 0 && Imported[0])
		{
			TArray<UObject*> ToSync;
			ToSync.Add(Imported[0]);
			AssetTools.SyncBrowserToAssets(ToSync);
		}

		TSharedPtr<SWindow> Parent = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (Parent.IsValid()) Parent->RequestDestroyWindow();
		return FReply::Handled();
	}

	FReply OnCancel()
	{
		TSharedPtr<SWindow> Parent = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (Parent.IsValid()) Parent->RequestDestroyWindow();
		return FReply::Handled();
	}
};

void FTextToImageEditorSlateWindow::OpenWindow(const FString& InDestinationPath)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Create Texture from Text"))
		.ClientSize(FVector2D(480, 520))
		.SupportsMinimize(false).SupportsMaximize(false);

	Window->SetContent(SNew(STextToImageDialog).DestinationPath(InDestinationPath));

	TSharedPtr<SWindow> ParentWindow;
	if (FSlateApplication::IsInitialized())
	{
		ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	}
	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}
}

#undef LOCTEXT_NAMESPACE

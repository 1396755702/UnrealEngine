// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TranslationEditorPrivatePCH.h"
#include "TranslationPickerEditWindow.h"
#include "Editor/Documentation/Public/SDocumentationToolTip.h"
#include "TranslationUnit.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

void STranslationPickerEditWindow::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	PickedTexts = InArgs._PickedTexts;

	WindowContents = SNew(SBox);

	auto TextsBox = SNew(SVerticalBox);

	// Display name of the current language
	TextsBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(5))
		[
			SNew(STextBlock)
			.Text(FText::FromString(FInternationalization::Get().GetCurrentCulture()->GetDisplayName()))
			.Justification(ETextJustify::Center)
		];

	// Add a new Translation Picker Edit Widget for each picked text
	for (FText PickedText : PickedTexts)
	{
		TSharedPtr<SEditableTextBox> TextBox;
		int32 DefaultPadding = 0.0f;

		TSharedRef<STranslationPickerEditWidget> NewEditWidget = 
			SNew(STranslationPickerEditWidget)
			.PickedText(PickedText)
			.bAllowEditing(true);

		EditWidgets.Add(NewEditWidget);

		TextsBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(5))
			[
				SNew(SBorder)
				[
					NewEditWidget
				]
			];
	}

	TSharedPtr<SEditableTextBox> TextBox;
	int32 DefaultPadding = 0.0f;

	// Layout the Translation Picker Edit Widgets and some save/close buttons below them
	WindowContents->SetContent(
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(8, 5, 8, 5))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(DefaultPadding)
					.AutoHeight()
					[
						TextsBox
					]
					+ SVerticalBox::Slot()
					.Padding(DefaultPadding)
					.AutoHeight()
					.HAlign(HAlign_Right)
					[
						SNew(SUniformGridPanel)
						.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
						.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
						.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
							.OnClicked(this, &STranslationPickerEditWindow::SaveAllAndClose)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(FMargin(0, 0, 3, 0))
								.VAlign(VAlign_Center)
								.AutoWidth()
								[
									SNew(STextBlock).Text(LOCTEXT("SaveAllAndClose", "Save all and close"))
								]
							]
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
							.OnClicked(this, &STranslationPickerEditWindow::Close)
							.Text(LOCTEXT("CancelButton", "Cancel"))
						]
					]
				]
			]
		]

	);

	ChildSlot
	[
		WindowContents.ToSharedRef()
	];
}

FReply STranslationPickerEditWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Close();
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply STranslationPickerEditWindow::Close()
{
	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().RequestDestroyWindow(ParentWindow.Pin().ToSharedRef());
		ParentWindow.Reset();
	}

	return FReply::Handled();
}

FReply STranslationPickerEditWindow::SaveAllAndClose()
{
	
	TArray<UTranslationUnit*> TempArray;

	for (TSharedRef<STranslationPickerEditWidget> EditWidget : EditWidgets)
	{
		UTranslationUnit* TranslationUnit = EditWidget->GetTranslationUnitWithAnyChanges();
		if (TranslationUnit != nullptr)
		{
			TempArray.Add(TranslationUnit);
		}
	}

	if (TempArray.Num() > 0)
	{
		// Save the data via translation data manager
		FTranslationDataManager::SaveSelectedTranslations(TempArray);
	}

	Close();

	return FReply::Handled();
}

void STranslationPickerEditWidget::Construct(const FArguments& InArgs)
{
	PickedText = InArgs._PickedText;
	bAllowEditing = InArgs._bAllowEditing;
	int32 DefaultPadding = 0.0f;

	bool bCultureInvariant = PickedText.IsCultureInvariant();
	bool bShouldGatherForLocalization = FTextInspector::ShouldGatherForLocalization(PickedText);

	// Get all the data we need and format it properly
	TOptional<FString> NamespaceString = FTextInspector::GetNamespace(PickedText);
	TOptional<FString> KeyString = FTextInspector::GetKey(PickedText);
	const FString* SourceString = FTextInspector::GetSourceString(PickedText);
	const FString& TranslationString = FTextInspector::GetDisplayString(PickedText);
	FString LocresFullPath;

	FString ManifestAndArchiveNameString;
	if (NamespaceString && KeyString)
	{
		FString LocResId;
		if (FTextLocalizationManager::Get().GetLocResID(NamespaceString.GetValue(), KeyString.GetValue(), LocResId))
		{
			LocresFullPath = *LocResId;
			ManifestAndArchiveNameString = FPaths::GetBaseFilename(*LocResId);
		}
	}

	FText Namespace = FText::FromString(NamespaceString.Get(TEXT("")));
	FText Key = FText::FromString(KeyString.Get(TEXT("")));
	FText Source = SourceString != nullptr ? FText::FromString(*SourceString) : FText::GetEmpty();
	FText ManifestAndArchiveName = FText::FromString(ManifestAndArchiveNameString);
	FText Translation = FText::FromString(TranslationString);

	FText SourceWithLabel = FText::Format(LOCTEXT("SourceLabel", "Source: {0}"), Source);
	FText NamespaceWithLabel = FText::Format(LOCTEXT("NamespaceLabel", "Namespace: {0}"), Namespace);
	FText KeyWithLabel = FText::Format(LOCTEXT("KeyLabel", "Key: {0}"), Key);
	FText ManifestAndArchiveNameWithLabel = FText::Format(LOCTEXT("LocresFileLabel", "Manifest/Archive File: {0}"), FText::FromString(ManifestAndArchiveNameString));

	// Save the necessary data in UTranslationUnit for later.  This is what we pass to TranslationDataManager to save our edits
	TranslationUnit = NewObject<UTranslationUnit>();
	TranslationUnit->Namespace = NamespaceString.Get(TEXT(""));
	TranslationUnit->Source = SourceString != nullptr ? *SourceString : "";
	TranslationUnit->Translation = TranslationString;
	TranslationUnit->LocresPath = LocresFullPath;
	
	// Can only save if we have all the required information
	bool bHasRequiredLocalizationInfo = NamespaceString.IsSet() && SourceString != nullptr && LocresFullPath.Len() > 0;

	TSharedRef<SHorizontalBox> LocalizationInfoAndSaveButtonSlot = SNew(SHorizontalBox);

	if (bCultureInvariant)
	{
		LocalizationInfoAndSaveButtonSlot->AddSlot()
			.AutoWidth()
			.Padding(FMargin(5))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CultureInvariantLabel","This text is culture-invariant"))
				.Justification(ETextJustify::Center)
			];
	}
	else if (!bShouldGatherForLocalization)
	{
		LocalizationInfoAndSaveButtonSlot->AddSlot()
			.AutoWidth()
			.Padding(FMargin(5))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NotGatheredForLocalizationLabel", "This text is not gathered for localization"))
				.Justification(ETextJustify::Center)
			];
	}
	else if (!bHasRequiredLocalizationInfo)
	{
		LocalizationInfoAndSaveButtonSlot->AddSlot()
			.AutoWidth()
			.Padding(FMargin(5))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RequiredLocalizationInfoNotFound", "The required localization info for this text was not found."))
				.Justification(ETextJustify::Center)
			];
	}
	else
	{
		LocalizationInfoAndSaveButtonSlot->AddSlot()
			.AutoWidth()
			.Padding(FMargin(5))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(STextBlock)
					.Text(NamespaceWithLabel)
				]
				+ SVerticalBox::Slot()
					[
						SNew(STextBlock)
						.Text(KeyWithLabel)
					]
				+ SVerticalBox::Slot()
					[
						SNew(STextBlock)
						.Text(ManifestAndArchiveNameWithLabel)
					]
			];
		LocalizationInfoAndSaveButtonSlot->AddSlot()
			.AutoWidth()
			.Padding(FMargin(5))
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &STranslationPickerEditWidget::SaveAndPreview)
				.IsEnabled(bHasRequiredLocalizationInfo)
				.Visibility(bAllowEditing ? EVisibility::Visible : EVisibility::Collapsed)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(FMargin(0, 0, 3, 0))
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(bHasRequiredLocalizationInfo ? LOCTEXT("SaveAndPreviewButtonText", "Save and preview") : LOCTEXT("SaveAndPreviewButtonDisabledText", "Cannot Save"))
					]
				]
			];
	}

	TSharedRef<SHorizontalBox> TranslationSlot = SNew(SHorizontalBox);
	
	if (!bHasRequiredLocalizationInfo && SourceString->Equals(TranslationString))
	{
		// If loc info lookup failed and the translated and source string are the same, don't bother displaying the translation
		TranslationSlot->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		TranslationSlot->AddSlot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TranslationLabel", "Translation: "))
			];
		TranslationSlot->AddSlot()
			[
				SAssignNew(TextBox, SMultiLineEditableTextBox)
				.IsEnabled(bAllowEditing && bHasRequiredLocalizationInfo)
				.Text(Translation)
				.HintText(LOCTEXT("TranslationEditTextBox_HintText", "Enter/edit translation here."))
			];
	}

	// Layout all our data
	ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(5))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(5))
				[
					SNew(STextBlock)
					.Text(SourceWithLabel)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(5))
				[
					TranslationSlot
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(5))
			[
				LocalizationInfoAndSaveButtonSlot
			]
		];
}

FReply STranslationPickerEditWidget::SaveAndPreview()
{
	// Update translation string from entered text
	TranslationUnit->Translation = TextBox->GetText().ToString();

	// Save the data via translation data manager
	TArray<UTranslationUnit*> TempArray;
	TempArray.Add(TranslationUnit);
	FTranslationDataManager::SaveSelectedTranslations(TempArray);

	return FReply::Handled();
}

UTranslationUnit* STranslationPickerEditWidget::GetTranslationUnitWithAnyChanges()
{
	if (TranslationUnit)
	{
		// Update translation string from entered text
		TranslationUnit->Translation = TextBox->GetText().ToString();

		return TranslationUnit;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
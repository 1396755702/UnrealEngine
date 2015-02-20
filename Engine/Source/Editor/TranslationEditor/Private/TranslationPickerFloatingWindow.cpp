// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TranslationEditorPrivatePCH.h"
#include "TranslationPickerFloatingWindow.h"
#include "Editor/Documentation/Public/SDocumentationToolTip.h"
#include "TranslationPickerEditWindow.h"
#include "Editor/TranslationEditor/Private/TranslationPickerWidget.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

void STranslationPickerFloatingWindow::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	WindowContents = SNew(SToolTip);

	ChildSlot
	[
		WindowContents.ToSharedRef()
	];
}

void STranslationPickerFloatingWindow::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(FSlateApplication::Get().GetCursorPos(), FSlateApplication::Get().GetInteractiveTopLevelWindows(), true);

	if (Path.IsValid())
	{
		// If the path of widgets we're hovering over changed since last time (or if this is the first tick and LastTickHoveringWidgetPath hasn't been set yet)
		if (!LastTickHoveringWidgetPath.IsValid() || LastTickHoveringWidgetPath.ToWidgetPath().ToString() != Path.ToString())
		{
			// Clear all previous text and widgets
			PickedTexts.Empty();
			WindowContents->SetContentWidget(SNew(SBox));

			// Search everything under the cursor for any FText we know how to parse
			for (int32 PathIndex = Path.Widgets.Num() - 1; PathIndex >= 0; PathIndex--)
			{
				// General Widget case
				TSharedRef<SWidget> PathWidget = Path.Widgets[PathIndex].Widget;
				GetTextFromWidget(PathWidget);

				// Tooltip case
				TSharedPtr<IToolTip> Tooltip = PathWidget->GetToolTip();
				//FText TooltipDescription = FText::GetEmpty();

				if (Tooltip.IsValid() && !Tooltip->IsEmpty())
				{
					GetTextFromWidget(Tooltip->AsWidget());
				}
			}

			auto TextsBox = SNew(SVerticalBox);

			// Add a new Translation Picker Edit Widget for each picked text
			for (FText PickedText : PickedTexts)
			{
				TextsBox->AddSlot()
					.FillHeight(1)
					.Padding(FMargin(5))
					[
						SNew(SBorder)
						[
							SNew(STranslationPickerEditWidget)
							.PickedText(PickedText)
						]
					];
			}

			FText Instructions = FText::GetEmpty();
			if (PickedTexts.Num() >= 1)
			{
				Instructions = LOCTEXT("TranslationPickerEscToEdit", "Press Esc to edit this translation");
			}
			else
			{
				Instructions = LOCTEXT("TranslationPickerHoverToViewEditEscToQuit", "Hover over text to view/edit translation, or press Esc to quit");
			}

			TextsBox->AddSlot()
				.AutoHeight()
				.Padding(FMargin(5))
				[
					SNew(STextBlock)
					.Text(Instructions)
					.Justification(ETextJustify::Center)
				];

			WindowContents->SetContentWidget(TextsBox);
		}
	}

	// kind of a hack, but we need to maintain keyboard focus otherwise we wont get our keypress to 'pick'
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::SetDirectly);
	if (ParentWindow.IsValid())
	{
		// also kind of a hack, but this is the only way at the moment to get a 'cursor decorator' without using the drag-drop code path
		ParentWindow.Pin()->MoveWindowTo(FSlateApplication::Get().GetCursorPos() + FSlateApplication::Get().GetCursorSize());
	}

	LastTickHoveringWidgetPath = FWeakWidgetPath(Path);
}

FText STranslationPickerFloatingWindow::GetTextFromWidget(TSharedRef<SWidget> Widget)
{
	FText OriginalText = FText::GetEmpty();

	// Parse STextBlocks
	STextBlock* TextBlock = (STextBlock*)&Widget.Get();
	if ((Widget->GetTypeAsString() == "STextBlock" && TextBlock))
	{
		OriginalText = TextBlock->GetText();
	}

	// Parse SToolTips
	else if (Widget->GetTypeAsString() == "SToolTip")
	{
		SToolTip* ToolTip = ((SToolTip*)&Widget.Get());
		if (ToolTip != nullptr)
		{
			OriginalText = GetTextFromWidget(ToolTip->GetContentWidget());
			if (OriginalText.IsEmpty())
			{
				OriginalText = ToolTip->GetTextTooltip();
			}
		}
	}
	// Parse SDocumentationToolTips
	else if (Widget->GetTypeAsString() == "SDocumentationToolTip")
	{
		SDocumentationToolTip* DocumentationToolTip = (SDocumentationToolTip*)&Widget.Get();
		if (DocumentationToolTip != nullptr)
		{
			OriginalText = DocumentationToolTip->GetTextTooltip();
		}
	}
	
	// Don't show the same text twice
	bool bAlreadyPicked = false;
	for (FText PickedText : PickedTexts)
	{
		if (OriginalText.EqualTo(PickedText))
		{
			bAlreadyPicked = true;
		}
	}
	
	if (!OriginalText.IsEmpty() && !bAlreadyPicked)
	{
		PickedTexts.Add(OriginalText);
	}

	return OriginalText;
}

FReply STranslationPickerFloatingWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		// Open a different window to allow editing of the translation
		TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(LOCTEXT("TranslationPickerEditWindowTitle", "Edit Translation(s)"))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.CreateTitleBar(true)
		.SizingRule(ESizingRule::Autosized);
		NewWindow->MoveWindowTo(FSlateApplication::Get().GetCursorPos());

		NewWindow->SetContent(
			SNew(STranslationPickerEditWindow)
			.ParentWindow(NewWindow)
			.PickedTexts(PickedTexts)
			);

		TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
		if (RootWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(NewWindow, RootWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(NewWindow);
		}

		TranslationPickerManager::ClosePickerWindow();
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE
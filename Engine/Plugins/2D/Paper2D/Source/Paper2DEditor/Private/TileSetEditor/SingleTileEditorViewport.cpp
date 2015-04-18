// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "PaperStyle.h"
#include "SingleTileEditorViewport.h"
#include "SpriteEditor/SpriteEditorCommands.h"
#include "SingleTileEditorViewportClient.h"

#define LOCTEXT_NAMESPACE "TileSetEditor"

//////////////////////////////////////////////////////////////////////////
// STileSetEditorViewportToolbar

// In-viewport toolbar widget used in the tile set editor
class STileSetEditorViewportToolbar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(STileSetEditorViewportToolbar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider);

	// SCommonEditorViewportToolbarBase interface
	virtual TSharedRef<SWidget> GenerateShowMenu() const override;
	// End of SCommonEditorViewportToolbarBase
};

void STileSetEditorViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InInfoProvider);
}

TSharedRef<SWidget> STileSetEditorViewportToolbar::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		ShowMenuBuilder.AddMenuEntry(FSpriteGeometryEditCommands::Get().SetShowNormals);
	}

	return ShowMenuBuilder.MakeWidget();
}

//////////////////////////////////////////////////////////////////////////
// SSingleTileEditorViewport

SSingleTileEditorViewport::~SSingleTileEditorViewport()
{
	TypedViewportClient.Reset();
}

void SSingleTileEditorViewport::Construct(const FArguments& InArgs, TSharedPtr<class FSingleTileEditorViewportClient> InViewportClient)
{
	TypedViewportClient = InViewportClient;

	SEditorViewport::Construct(SEditorViewport::FArguments());

	TSharedRef<SWidget> ParentContents = ChildSlot.GetWidget();

	this->ChildSlot
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			ParentContents
		]

		+SOverlay::Slot()
		.VAlign(VAlign_Bottom)
		[
			SNew(SBorder)
			.BorderImage(FPaperStyle::Get()->GetBrush("Paper2D.Common.ViewportTitleBackground"))
			.HAlign(HAlign_Fill)
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SVerticalBox)
				// Title text/icon
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.FillWidth(1.f)
					[
						SNew(STextBlock)
						.TextStyle(FPaperStyle::Get(), "Paper2D.Common.ViewportTitleTextStyle")
						.Text(this, &SSingleTileEditorViewport::GetTitleText)
					]
				]
			]
		]
	];
}

TSharedPtr<SWidget> SSingleTileEditorViewport::MakeViewportToolbar()
{
	return SNew(STileSetEditorViewportToolbar, SharedThis(this));
}

TSharedRef<FEditorViewportClient> SSingleTileEditorViewport::MakeEditorViewportClient()
{
	TypedViewportClient->VisibilityDelegate.BindSP(this, &SSingleTileEditorViewport::IsVisible);

	return TypedViewportClient.ToSharedRef();
}

TSharedRef<class SEditorViewport> SSingleTileEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SSingleTileEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SSingleTileEditorViewport::OnFloatingButtonClicked()
{
}

FText SSingleTileEditorViewport::GetTitleText() const
{
	const int32 CurrentTileIndex = TypedViewportClient->GetTileIndex();
	if (CurrentTileIndex != INDEX_NONE)
	{
		FNumberFormattingOptions NoGroupingFormat;
		NoGroupingFormat.SetUseGrouping(false);

		return FText::Format(LOCTEXT("SingleTileEditorViewportTitle", "Editing tile #{0}"), FText::AsNumber(CurrentTileIndex, &NoGroupingFormat));
	}
	else
	{
		return LOCTEXT("SingleTileEditorViewportTitle_NoTile", "Tile Editor - Select a tile");
	}
}

bool SSingleTileEditorViewport::IsVisible() const
{
	return true;//@TODO: Determine this better so viewport ticking optimizations can take place
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
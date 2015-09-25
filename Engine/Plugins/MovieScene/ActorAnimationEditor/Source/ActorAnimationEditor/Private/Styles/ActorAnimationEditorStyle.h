// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)


/**
 * Implements the visual style of the messaging debugger UI.
 */
class FActorAnimationEditorStyle
	: public FSlateStyleSet
{
public:

	/** Default constructor. */
	 FActorAnimationEditorStyle()
		 : FSlateStyleSet("ActorAnimationEditorStyle")
	 {
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon20x20(20.0f, 20.0f);
		const FVector2D Icon40x40(40.0f, 40.0f);

		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("MovieScene/ActorAnimationEditor/Content"));

		// tab icons
		Set("ActorAnimationEditor.Tabs.Sequencer", new IMAGE_BRUSH("icon_tab_sequencer_16x", Icon16x16));
		Set("ActorAnimationEditor.CreateNewActorAnimationInLevel", new IMAGE_BRUSH("CreateNewActorAnimationInLevel_16x", Icon16x16));
		Set("ActorAnimationEditor.CreateNewActorAnimationInLevel.Small", new IMAGE_BRUSH("CreateNewActorAnimationInLevel_16x", Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	 }

	 /** Virtual destructor. */
	 virtual ~FActorAnimationEditorStyle()
	 {
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	 }
};


#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

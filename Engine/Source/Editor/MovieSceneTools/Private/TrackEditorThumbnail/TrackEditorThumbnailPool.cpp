// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"


/* FShotSequencerSection structors
 *****************************************************************************/

FTrackEditorThumbnailPool::FTrackEditorThumbnailPool(TSharedPtr<ISequencer> InSequencer, uint32 InMaxThumbnailsToDrawAtATime)
	: Sequencer(InSequencer)
	, ThumbnailsNeedingDraw()
	, MaxThumbnailsToDrawAtATime(InMaxThumbnailsToDrawAtATime)
{ }


/* FShotSequencerSection interface
 *****************************************************************************/

void FTrackEditorThumbnailPool::AddThumbnailsNeedingRedraw(const TArray<TSharedPtr<FTrackEditorThumbnail>>& InThumbnails)
{
	ThumbnailsNeedingDraw.Append(InThumbnails);
}


bool FTrackEditorThumbnailPool::DrawThumbnails()
{
	if (ThumbnailsNeedingDraw.Num() == 0)
	{
		return false;
	}

	uint32 ThumbnailsDrawn = 0;

	for (int32 ThumbnailsIndex = 0; ThumbnailsDrawn < MaxThumbnailsToDrawAtATime && ThumbnailsIndex < ThumbnailsNeedingDraw.Num(); ++ThumbnailsIndex)
	{
		TSharedPtr<FTrackEditorThumbnail> Thumbnail = ThumbnailsNeedingDraw[ThumbnailsIndex];
			
		if (Thumbnail->IsVisible())
		{
			
			bool bIsEnabled = Sequencer.Pin()->IsPerspectiveViewportCameraCutEnabled();

			Sequencer.Pin()->SetPerspectiveViewportCameraCutEnabled(false);
			
			Thumbnail->DrawThumbnail();
			
			Sequencer.Pin()->SetPerspectiveViewportCameraCutEnabled(bIsEnabled);
			++ThumbnailsDrawn;

			ThumbnailsNeedingDraw.Remove(Thumbnail);
		}
		else if (!Thumbnail->IsValid())
		{
			ensure(0);

			ThumbnailsNeedingDraw.Remove(Thumbnail);
		}
	}
		
	if (ThumbnailsDrawn > 0)
	{
		// Ensure all buffers are updated
		FlushRenderingCommands();
	}

	return ThumbnailsDrawn > 0;
}


void FTrackEditorThumbnailPool::RemoveThumbnailsNeedingRedraw(const TArray< TSharedPtr<FTrackEditorThumbnail> >& InThumbnails)
{
	for (int32 i = 0; i < InThumbnails.Num(); ++i)
	{
		ThumbnailsNeedingDraw.Remove(InThumbnails[i]);
	}
}

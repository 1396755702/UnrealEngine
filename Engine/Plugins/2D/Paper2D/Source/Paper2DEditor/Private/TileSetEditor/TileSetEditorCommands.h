// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperStyle.h"

class FTileSetEditorCommands : public TCommands<FTileSetEditorCommands>
{
public:
	FTileSetEditorCommands()
		: TCommands<FTileSetEditorCommands>(
			TEXT("TileSetEditor"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "TileSetEditor", "Tile Set Editor"), // Localized context name for displaying
			NAME_None, // Parent
			FPaperStyle::Get()->GetStyleSetName() // Icon Style Set
			)
	{
	}

	// TCommand<> interface
	virtual void RegisterCommands() override;
	// End of TCommand<> interface

public:
	// Show toggles
	TSharedPtr<FUICommandInfo> SetShowCollision;
	TSharedPtr<FUICommandInfo> SetShowGrid;
};
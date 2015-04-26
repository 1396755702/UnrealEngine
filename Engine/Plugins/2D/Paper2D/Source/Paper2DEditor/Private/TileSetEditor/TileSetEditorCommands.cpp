// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "TileSetEditorCommands.h"

//////////////////////////////////////////////////////////////////////////
// FTileSetEditorCommands

#define LOCTEXT_NAMESPACE "TileSetEditor"

void FTileSetEditorCommands::RegisterCommands()
{
	// Show toggles
	UI_COMMAND(SetShowCollision, "Collision", "Toggles display of the collision geometry, if any exists.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::C, EModifierKey::Alt));
	UI_COMMAND(SetShowGrid, "Grid", "Display the grid.", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(SetShowTilesWithCollision, "Colliding Tiles", "Toggles highlight of tiles that have custom collision geometry.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetShowTilesWithMetaData, "Metadata Tiles", "Toggles highlight of tiles that have custom metadata.", EUserInterfaceActionType::RadioButton, FInputChord());

	// Collision commands
	UI_COMMAND(ApplyCollisionEdits, "Refresh Maps", "Refreshes tile maps that use this tile set to ensure they have up-to-date collision geometry.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
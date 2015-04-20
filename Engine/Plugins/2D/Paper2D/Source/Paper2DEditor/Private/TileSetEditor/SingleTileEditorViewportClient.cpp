// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "SingleTileEditorViewportClient.h"
#include "SpriteEditor/SpriteEditorCommands.h"
#include "PaperEditorShared/SpriteGeometryEditMode.h"
#include "ScopedTransaction.h"
#include "TileSetEditorSettings.h"

#define LOCTEXT_NAMESPACE "TileSetEditor"

//////////////////////////////////////////////////////////////////////////
// FSingleTileEditorViewportClient

FSingleTileEditorViewportClient::FSingleTileEditorViewportClient(UPaperTileSet* InTileSet)
	: TileSet(InTileSet)
	, TileBeingEditedIndex(INDEX_NONE)
	, bManipulating(false)
	, bManipulationDirtiedSomething(false)
	, ScopedTransaction(nullptr)
{
	//@TODO: Should be able to set this to false eventually
	SetRealtime(true);

	// The tile map editor fully supports mode tools and isn't doing any incompatible stuff with the Widget
	Widget->SetUsesEditorModeTools(ModeTools);

	DrawHelper.bDrawGrid = GetDefault<UTileSetEditorSettings>()->bShowGridByDefault;
	DrawHelper.bDrawPivot = false;

	PreviewScene = &OwnedPreviewScene;
	((FAssetEditorModeManager*)ModeTools)->SetPreviewScene(PreviewScene);

	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.CompositeEditorPrimitives = true;

	// Create a render component for the tile preview
	PreviewTileSpriteComponent = NewObject<UPaperSpriteComponent>();

	FStringAssetReference DefaultTranslucentMaterialName("/Paper2D/TranslucentUnlitSpriteMaterial.TranslucentUnlitSpriteMaterial");
	UMaterialInterface* TranslucentMaterial = Cast<UMaterialInterface>(DefaultTranslucentMaterialName.TryLoad());
	PreviewTileSpriteComponent->SetMaterial(0, TranslucentMaterial);

	PreviewScene->AddComponent(PreviewTileSpriteComponent, FTransform::Identity);
}

FBox FSingleTileEditorViewportClient::GetDesiredFocusBounds() const
{
	UPaperSpriteComponent* ComponentToFocusOn = PreviewTileSpriteComponent;
	return ComponentToFocusOn->Bounds.GetBox();
}


void FSingleTileEditorViewportClient::Tick(float DeltaSeconds)
{
	FPaperEditorViewportClient::Tick(DeltaSeconds);

	if (!GIntraFrameDebuggingGameThread)
	{
		OwnedPreviewScene.GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

FLinearColor FSingleTileEditorViewportClient::GetBackgroundColor() const
{
	return TileSet->BackgroundColor;
}

void FSingleTileEditorViewportClient::TrackingStarted(const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge)
{
	//@TODO: Should push this into FEditorViewportClient
	// Begin transacting.  Give the current editor mode an opportunity to do the transacting.
	const bool bTrackingHandledExternally = ModeTools->StartTracking(this, Viewport);

	if (!bManipulating && bIsDragging && !bTrackingHandledExternally)
	{
		BeginTransaction(LOCTEXT("ModificationInViewport", "Modification in Viewport"));
		bManipulating = true;
		bManipulationDirtiedSomething = false;
	}
}

void FSingleTileEditorViewportClient::TrackingStopped()
{
	// Stop transacting.  Give the current editor mode an opportunity to do the transacting.
	const bool bTransactingHandledByEditorMode = ModeTools->EndTracking(this, Viewport);

	if (bManipulating && !bTransactingHandledByEditorMode)
	{
		EndTransaction();
		bManipulating = false;
	}
}

FVector2D FSingleTileEditorViewportClient::SelectedItemConvertWorldSpaceDeltaToLocalSpace(const FVector& WorldSpaceDelta) const
{
	const FVector ProjectionX = WorldSpaceDelta.ProjectOnTo(PaperAxisX);
	const FVector ProjectionY = WorldSpaceDelta.ProjectOnTo(PaperAxisY);

	const float XValue = FMath::Sign(ProjectionX | PaperAxisX) * ProjectionX.Size();
	const float YValue = FMath::Sign(ProjectionY | PaperAxisY) * ProjectionY.Size();

	return FVector2D(XValue, YValue);
}

FVector2D FSingleTileEditorViewportClient::WorldSpaceToTextureSpace(const FVector& SourcePoint) const
{
	const FVector ProjectionX = SourcePoint.ProjectOnTo(PaperAxisX);
	const FVector ProjectionY = -SourcePoint.ProjectOnTo(PaperAxisY);

	const float XValue = FMath::Sign(ProjectionX | PaperAxisX) * ProjectionX.Size();
	const float YValue = FMath::Sign(ProjectionY | PaperAxisY) * ProjectionY.Size();

	return FVector2D(XValue, YValue);
}

FVector FSingleTileEditorViewportClient::TextureSpaceToWorldSpace(const FVector2D& SourcePoint) const
{
	return (SourcePoint.X * PaperAxisX) - (SourcePoint.Y * PaperAxisY);
}

float FSingleTileEditorViewportClient::SelectedItemGetUnitsPerPixel() const
{
	return 1.0f;
}

void FSingleTileEditorViewportClient::BeginTransaction(const FText& SessionName)
{
	if (ScopedTransaction == nullptr)
	{
		ScopedTransaction = new FScopedTransaction(SessionName);

		TileSet->Modify();
	}
}

void FSingleTileEditorViewportClient::MarkTransactionAsDirty()
{
	bManipulationDirtiedSomething = true;
	Invalidate();
}

void FSingleTileEditorViewportClient::EndTransaction()
{
	if (bManipulationDirtiedSomething)
	{
		TileSet->PostEditChange();
	}

	bManipulationDirtiedSomething = false;

	if (ScopedTransaction != nullptr)
	{
		delete ScopedTransaction;
		ScopedTransaction = nullptr;
	}
}

void FSingleTileEditorViewportClient::InvalidateViewportAndHitProxies()
{
	Invalidate();
}

void FSingleTileEditorViewportClient::SetTileIndex(int32 InTileIndex)
{
	const bool bNewIndexValid = (InTileIndex >= 0) && (InTileIndex < TileSet->GetTileCount());
	TileBeingEditedIndex = bNewIndexValid ? InTileIndex : INDEX_NONE;

	FSpriteGeometryEditMode* GeometryEditMode = ModeTools->GetActiveModeTyped<FSpriteGeometryEditMode>(FSpriteGeometryEditMode::EM_SpriteGeometry);
	check(GeometryEditMode);
	FSpriteGeometryCollection* GeomToEdit = nullptr;
	
	if (TileBeingEditedIndex != INDEX_NONE)
	{
		if (FPaperTileMetadata* Metadata = TileSet->GetMutableTileMetadata(InTileIndex))
		{
			GeomToEdit = &(Metadata->CollisionData);
		}
	}

	// Tell the geometry editor about the new tile (if it exists)
	GeometryEditMode->SetGeometryBeingEdited(GeomToEdit, /*bAllowCircles=*/ true, /*bAllowSubtractivePolygons=*/ false);

	// Update the visual representation
	UPaperSprite* DummySprite = nullptr;
	if (TileBeingEditedIndex != INDEX_NONE)
	{
		//@TODO: Should use this to pick the correct material
		//const bool bUseTranslucentBlend = Texture->HasAlphaChannel();

		DummySprite = NewObject<UPaperSprite>();
 		DummySprite->SpriteCollisionDomain = ESpriteCollisionMode::None;
 		DummySprite->PivotMode = ESpritePivotMode::Center_Center;
 		DummySprite->CollisionGeometry.GeometryType = ESpritePolygonMode::SourceBoundingBox;
 		DummySprite->RenderGeometry.GeometryType = ESpritePolygonMode::SourceBoundingBox;
		DummySprite->PixelsPerUnrealUnit = 1.0f;

		FSpriteAssetInitParameters SpriteReinitParams;
		SpriteReinitParams.Texture = TileSet->TileSheet;
		TileSet->GetTileUV(TileBeingEditedIndex, /*out*/ SpriteReinitParams.Offset);
		SpriteReinitParams.Dimension = FVector2D(TileSet->TileWidth, TileSet->TileHeight);
		DummySprite->InitializeSprite(SpriteReinitParams);
	}
	PreviewTileSpriteComponent->SetSprite(DummySprite);

	// Update the default geometry bounds
	const FVector2D HalfTileSize(TileSet->TileWidth * 0.5f, TileSet->TileHeight * 0.5f);
	FBox2D DesiredBounds(ForceInitToZero);
	DesiredBounds.Min = -HalfTileSize;
	DesiredBounds.Max = HalfTileSize;
	GeometryEditMode->SetNewGeometryPreferredBounds(DesiredBounds);

	// Redraw the viewport
	Invalidate();
}

void FSingleTileEditorViewportClient::OnActiveTileIndexChanged(const FIntPoint& TopLeft, const FIntPoint& Dimensions)
{
	const int32 NewIndex = (TileSet->GetTileCountX() * TopLeft.Y) + TopLeft.X;
	SetTileIndex(NewIndex);
}

int32 FSingleTileEditorViewportClient::GetTileIndex() const
{
	return TileBeingEditedIndex;
}

void FSingleTileEditorViewportClient::ActivateEditMode(TSharedPtr<FUICommandList> InCommandList)
{
	// Activate the sprite geometry edit mode
	//@TODO: ModeTools->SetToolkitHost(SpriteEditorPtr.Pin()->GetToolkitHost());
	ModeTools->SetDefaultMode(FSpriteGeometryEditMode::EM_SpriteGeometry);
	ModeTools->ActivateDefaultMode();
	ModeTools->SetWidgetMode(FWidget::WM_Translate);

	FSpriteGeometryEditMode* GeometryEditMode = ModeTools->GetActiveModeTyped<FSpriteGeometryEditMode>(FSpriteGeometryEditMode::EM_SpriteGeometry);
	check(GeometryEditMode);
	GeometryEditMode->SetEditorContext(this);
	GeometryEditMode->BindCommands(InCommandList /*SpriteEditorViewportPtr.Pin()->GetCommandList()*/);

	const FLinearColor CollisionShapeColor(0.0f, 0.7f, 1.0f, 1.0f); //@TODO: Duplicated constant from SpriteEditingConstants
	GeometryEditMode->SetGeometryColors(CollisionShapeColor, FLinearColor::White);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
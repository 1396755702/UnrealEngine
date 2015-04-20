// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperSprite.h"
#include "PaperTileLayer.h"
#include "PaperTileMapComponent.generated.h"

/**
 * A component that handles rendering and collision for a single instance of a UPaperTileMap asset.
 *
 * This component is created when you drag a tile map asset from the content browser into a Blueprint, or
 * contained inside of the actor created when you drag one into the level.
 *
 * NOTE: This is an early access preview class.  While not considered production-ready, it is a step beyond
 * 'experimental' and is being provided as a preview of things to come:
 *  - We will try to provide forward-compatibility for content you create.
 *  - The classes may change significantly in the future.
 *  - The code is in an early state and may not meet the desired polish / quality bar.
 *  - There is probably no documentation or example content yet.
 *  - They will be promoted out of 'Early Access' when they are production ready.
 *
 * @see UPrimitiveComponent, UPaperTileMap
 */

UCLASS(hideCategories=Object, ClassGroup=Paper2D, EarlyAccessPreview, meta=(BlueprintSpawnableComponent))
class PAPER2D_API UPaperTileMapComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY()
	int32 MapWidth_DEPRECATED;

	UPROPERTY()
	int32 MapHeight_DEPRECATED;

	UPROPERTY()
	int32 TileWidth_DEPRECATED;

	UPROPERTY()
	int32 TileHeight_DEPRECATED;

	UPROPERTY()
	class UPaperTileSet* DefaultLayerTileSet_DEPRECATED;

	UPROPERTY()
	UMaterialInterface* Material_DEPRECATED;

	UPROPERTY()
	TArray<class UPaperTileLayer*> TileLayers_DEPRECATED;

	// The color of the tile map (multiplied with the per-layer color and passed to the material as a vertex color)
	UPROPERTY(EditAnywhere, Category=Materials)
	FLinearColor TileMapColor;

public:
	// The tile map used by this component
	UPROPERTY(Category=Setup, EditAnywhere, BlueprintReadOnly)
	class UPaperTileMap* TileMap;

protected:
	friend class FPaperTileMapRenderSceneProxy;

	void RebuildRenderData(class FPaperTileMapRenderSceneProxy* Proxy);

public:
	// UObject interface
	virtual void PostInitProperties() override;
#if WITH_EDITORONLY_DATA
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#endif
	// End of UObject interface

	// UActorComponent interface
	virtual const UObject* AdditionalStatObject() const override;
	// End of UActorComponent interface

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual class UBodySetup* GetBodySetup() override;
	virtual void GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel) override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual int32 GetNumMaterials() const override;
	// End of UPrimitiveComponent interface

	// Creates a new tile map internally, replacing the TileMap reference (or dropping the previous owned one)
	void CreateNewOwnedTileMap();

	// Does this component own the tile map (is it instanced instead of being an asset reference)?
	UFUNCTION(BlueprintCallable, Category="Sprite")
	bool OwnsTileMap() const;

	/** Change the PaperTileMap used by this instance. */
	UFUNCTION(BlueprintCallable, Category="Sprite")
	virtual bool SetTileMap(class UPaperTileMap* NewTileMap);

	// Returns the contents of a specified tile cell
	UFUNCTION(BlueprintPure, Category="Sprite", meta=(Layer="0"))
	FPaperTileInfo GetTile(int32 X, int32 Y, int32 Layer) const;

	// Modifies the contents of a specified tile cell (Note: This will only work on components that own their own tile map (OwnsTileMap returns true), you cannot modify standalone tile map assets) 
	UFUNCTION(BlueprintCallable, Category="Sprite", meta=(Layer="0"))
	void SetTile(int32 X, int32 Y, int32 Layer, FPaperTileInfo NewValue);

	// Resizes the tile map (Note: This will only work on components that own their own tile map (OwnsTileMap returns true), you cannot modify standalone tile map assets) 
	UFUNCTION(BlueprintCallable, Category="Sprite")
	void ResizeMap(int32 NewWidthInTiles, int32 NewHeightInTiles);

	// Creates and adds a new layer to the tile map
	// Note: This will only work on components that own their own tile map (OwnsTileMap returns true), you cannot modify standalone tile map assets
	UFUNCTION(BlueprintCallable, Category="Sprite")
	class UPaperTileLayer* AddNewLayer();

	// Gets the tile map global color multiplier (multiplied with the per-layer color and passed to the material as a vertex color)
	UFUNCTION(BlueprintPure, Category="Sprite")
	FLinearColor GetTileMapColor() const;

	// Sets the tile map global color multiplier (multiplied with the per-layer color and passed to the material as a vertex color)
	UFUNCTION(BlueprintCallable, Category="Sprite")
	void SetTileMapColor(FLinearColor NewColor);

	// Gets the per-layer color multiplier for a specific layer (multiplied with the tile map color and passed to the material as a vertex color)
	UFUNCTION(BlueprintPure, Category = "Sprite")
	FLinearColor GetLayerColor(int32 Layer = 0) const;

	// Sets the per-layer color multiplier for a specific layer (multiplied with the tile map color and passed to the material as a vertex color)
	// Note: This will only work on components that own their own tile map (OwnsTileMap returns true), you cannot modify standalone tile map assets
	UFUNCTION(BlueprintCallable, Category = "Sprite")
	void SetLayerColor(FLinearColor NewColor, int32 Layer = 0);

	// Returns the wireframe color to use for this component.
	FLinearColor GetWireframeColor() const;
};

// Allow the old name to continue to work for one release
DEPRECATED(4.7, "UPaperTileMapRenderComponent has been renamed to UPaperTileMapComponent")
typedef UPaperTileMapComponent UPaperTileMapRenderComponent;

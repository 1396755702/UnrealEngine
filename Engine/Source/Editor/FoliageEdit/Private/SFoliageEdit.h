// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Forwards declarations
struct FFoliageMeshUIInfo;
class IDetailsView;

typedef TSharedPtr<FFoliageMeshUIInfo> FFoliageMeshUIInfoPtr; //should match typedef in FoliageEdMode.h
typedef STreeView<FFoliageMeshUIInfoPtr> SFoliageMeshTree;

class SFoliageEdit : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFoliageEdit) {}
	SLATE_END_ARGS()

public:
	/** SCompoundWidget functions */
	void Construct(const FArguments& InArgs);

	/** Does a full refresh on the list. */
	void RefreshFullList();

	/** Handles adding a new item to the list and refreshing the list in it's entirety. */
	FReply OnDrop_ListView(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

	/** Gets FoliageEditMode. Used by the cluster details to notify changes */
	class FEdModeFoliage* GetFoliageEditMode() const { return FoliageEditMode; }

	/** @return the SWidget containing the context menu */
	TSharedPtr<SWidget> ConstructFoliageMeshContextMenu() const;

private:
	/** Clears all the tools selection by setting them to false. */
	void ClearAllToolSelection();

	/** Binds UI commands for the toolbar. */
	void BindCommands();

	/** Creates the toolbar. */
	TSharedRef<SWidget> BuildToolBar();

	/** Adds the foliage type asset to the foliage actor's list of meshes. */
	void AddFoliageType(const class FAssetData& AssetData);

	/** Delegate callbacks for the UI */

	/** Sets the tool mode to Paint. */
	void OnSetPaint();

	/** Checks if the tool mode is Paint. */
	bool IsPaintTool() const;

	/** Sets the tool mode to Reapply Settings. */
	void OnSetReapplySettings();

	/** Checks if the tool mode is Reapply Settings. */
	bool IsReapplySettingsTool() const;

	/** Sets the tool mode to Select. */
	void OnSetSelectInstance();

	/** Checks if the tool mode is Select. */
	bool IsSelectTool() const;

	/** Sets the tool mode to Lasso Select. */
	void OnSetLasso();

	/** Checks if the tool mode is Lasso Select. */
	bool IsLassoSelectTool() const;

	/** Sets the tool mode to Paint Bucket. */
	void OnSetPaintFill();

	/** Checks if the tool mode is Paint Bucket. */
	bool IsPaintFillTool() const;

	/** Sets the brush Radius for the brush. */
	void SetRadius(float InRadius);

	/** Retrieves the brush Radius for the brush. */
	TOptional<float> GetRadius() const;

	/** Sets the Paint Density for the brush. */
	void SetPaintDensity(float InDensity);

	/** Retrieves the Paint Density for the brush. */
	TOptional<float> GetPaintDensity() const;

	/** Sets the Erase Density for the brush. */
	void SetEraseDensity(float InDensity);

	/** Retrieves the Erase Density for the brush. */
	TOptional<float> GetEraseDensity() const;

	/** Gets the visibility of the Add Foliage Type text in the header row button */
	EVisibility GetAddFoliageTypeButtonTextVisibility() const;

	/** Gets the asset picker for adding a foliage type. */
	TSharedRef<SWidget> OnGetAddFoliageMeshAssetPicker();

	/** Retrieves the text for the filters option */
	FText GetFilterText() const;

	/** Retrieves the tooltip text for the filters option */
	FText GetFilterTooltipText() const;

	/** Sets the filter settings for if painting will occur on Landscapes. */
	void OnCheckStateChanged_Landscape(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on Landscapes. */
	ECheckBoxState GetCheckState_Landscape() const;

	/** Retrieves the tooltip text for the landscape filter */
	FText GetTooltipText_Landscape() const;

	/** Sets the filter settings for if painting will occur on Static Meshes. */
	void OnCheckStateChanged_StaticMesh(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on Static Meshes. */
	ECheckBoxState GetCheckState_StaticMesh() const;

	/** Retrieves the tooltip text for the static mesh filter */
	FText GetTooltipText_StaticMesh() const;

	/** Sets the filter settings for if painting will occur on BSPs. */
	void OnCheckStateChanged_BSP(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on BSPs. */
	ECheckBoxState GetCheckState_BSP() const;

	/** Retrieves the tooltip text for the BSP filter */
	FText GetTooltipText_BSP() const;

	/** Sets the filter settings for if painting will occur on translucent meshes. */
	void OnCheckStateChanged_Translucent(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on translucent meshes. */
	ECheckBoxState GetCheckState_Translucent() const;

	/** Retrieves the tooltip text for the translucent filter */
	FText GetTooltipText_Translucent() const;

	/** Checks if the text in the empty list overlay should appear. If the list is has items but the the drag and drop override is true, it will return EVisibility::Visible. */
	EVisibility GetVisibility_EmptyList() const;

	/** Should the drop area be visible currently?  Happens when the user is dragging static meshes */
	EVisibility GetVisibility_FoliageDropTarget() const;

	/** Checks if the list should appear. */
	EVisibility GetVisibility_NonEmptyList() const;

	/** Checks if the radius spinbox should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_Radius() const;

	/** Checks if the paint density spinbox should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_PaintDensity() const;

	/** Checks if the erase density spinbox should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_EraseDensity() const;

	/** Checks if the filters should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_Filters() const;

	/** Generates a row widget for foliage mesh item */
	TSharedRef<ITableRow> MeshTreeGenerateRow(FFoliageMeshUIInfoPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	
	/** Generates a list of children items for foliage item */
	void MeshTreeGetChildren(FFoliageMeshUIInfoPtr Item, TArray<FFoliageMeshUIInfoPtr>& OutChildren);

	/** Handler for mesh list view selection changes  */
	void MeshTreeOnSelectionChanged(FFoliageMeshUIInfoPtr Item, ESelectInfo::Type SelectInfo);

	/** Fills 'Replace' menu command  */
	void FillReplaceFoliageTypeSubmenu(FMenuBuilder& MenuBuilder);
	
	/** Handler for 'Remove' command  */
	void OnRemoveFoliageType();
	
	/** Handler for 'Show in CB' command  */
	void OnShowFoliageTypeInCB();

	/** Handler for 'Replace' command  */
	void OnReplaceFoliageTypeSelected(const class FAssetData& AssetData);

	/** Handler for 'Select All' command  */
	void OnSelectAllInstances();

	/** Handler for 'Deselect All' command  */
	void OnDeselectAllInstances();

	/** Handler for 'Select Invalid Instances' command  */
	void OnSelectInvalidInstances();
	
	/** Toggle all meshes on/off */
	ECheckBoxState GetState_AllMeshes() const;
	void OnCheckStateChanged_AllMeshes(ECheckBoxState InState);

	/** Text for foliage meshes list header */
	FText GetMeshesHeaderText() const;
	
	/** Mesh list sorting support */
	EColumnSortMode::Type GetMeshColumnSortMode() const;
	void OnMeshesColumnSortModeChanged(EColumnSortPriority::Type InPriority, const FName& InColumnName, EColumnSortMode::Type InSortMode);

	/** Handler to trigger a refresh of the details view when the active tool changes */
	void HandleOnToolChanged();

	/** Refreshes the mesh details widget to match the current selection */
	void RefreshMeshDetailsWidget();

private:
	/** Foliage mesh tree widget  */
	TSharedPtr<SFoliageMeshTree>	MeshTreeWidget;
	
	/** Foliage mesh details widget  */
	TSharedPtr<IDetailsView>		MeshDetailsWidget;
	
	/** Command list for binding functions for the toolbar. */
	TSharedPtr<FUICommandList>		UICommandList;

	/** Pointer to the foliage edit mode. */
	FEdModeFoliage*					FoliageEditMode;

	/** The Add Foliage Type combo button */
	TSharedPtr<class SComboButton> AddFoliageTypeCombo;

	/** The header row of the foliage mesh tree */
	TSharedPtr<class SHeaderRow> MeshTreeHeaderRowWidget;
};

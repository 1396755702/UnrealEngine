// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Developer/MeshUtilities/Public/MeshUtilities.h"

/*-----------------------------------------------------------------------------
   SMeshMergingDialog
-----------------------------------------------------------------------------*/
class SMeshMergingDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMeshMergingDialog)
	{
	}

	// The parent window hosting this dialog
	SLATE_ATTRIBUTE(TWeakPtr<SWindow>, ParentWindow)

	SLATE_END_ARGS()

public:
	/** **/
	SMeshMergingDialog();
	virtual ~SMeshMergingDialog();

	/** SWidget functions */
	void Construct(const FArguments& InArgs);

private:
	/** Called when the Cancel button is clicked */
	FReply OnCancelClicked();

	/** Called when the Merge button is clicked */
	FReply OnMergeClicked();

	/**  */
	ECheckBoxState GetGenerateLightmapUV() const;
	void SetGenerateLightmapUV(ECheckBoxState NewValue);

	/** Target lightmap channel */
	bool IsLightmapChannelEnabled() const;
	void SetTargetLightMapChannel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void SetTargetLightMapResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/**  */
	ECheckBoxState GetExportSpecificLODEnabled() const;
	void SetExportSpecificLODEnabled(ECheckBoxState NewValue);
	bool IsExportSpecificLODEnabled() const;
	void SetExportSpecificLODIndex(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
		
	/**  */
	ECheckBoxState GetImportVertexColors() const;
	void SetImportVertexColors(ECheckBoxState NewValue);

	/**  */
	ECheckBoxState GetPivotPointAtZero() const;
	void SetPivotPointAtZero(ECheckBoxState NewValue);

	ECheckBoxState GetPlaceInWorld() const;
	void SetPlaceInWorld(ECheckBoxState NewValue);

	/** Material merging */
	bool IsMaterialMergingEnabled() const;
	ECheckBoxState GetMergeMaterials() const;
	void SetMergeMaterials(ECheckBoxState NewValue);

	ECheckBoxState GetExportNormalMap() const;
	void SetExportNormalMap(ECheckBoxState NewValue);

	ECheckBoxState GetExportMetallicMap() const;
	void SetExportMetallicMap(ECheckBoxState NewValue);

	ECheckBoxState GetExportRoughnessMap() const;
	void SetExportRoughnessMap(ECheckBoxState NewValue);

	ECheckBoxState GetExportSpecularMap() const;
	void SetExportSpecularMap(ECheckBoxState NewValue);

	void SetMergedMaterialAtlasResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	
	/** Called when actors selection is changed */
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh=false);
	
	/** Generates destination package name using currently selected actors */
	void GenerateNewPackageName();

	/** Destination package name accessors */
	FText GetMergedMeshPackageName() const;
	void OnMergedMeshPackageNameTextCommited(const FText& InText, ETextCommit::Type InCommitType);
	
	/** Called when the select package name  button is clicked. Brings asset path picker dialog */
	FReply OnSelectPackageNameClicked();

	/** */
	void RunMerging();

private:
	/** Pointer to the parent window */
	TAttribute<TWeakPtr<SWindow>> ParentWindow;

	/** Current mesh merging settings */
	FMeshMergingSettings MergingSettings;

	/** Merged mesh destination package name */
	FString MergedMeshPackageName;

	/** Whether to spawn merged actor in the world */
	bool bPlaceInWorld;

	bool bExportSpecificLOD;
	int32 ExportLODIndex;
	TArray<TSharedPtr<FString>>	ExportLODOptions;

	/**  */
	TArray<TSharedPtr<FString>>	LightMapResolutionOptions;
	TArray<TSharedPtr<FString>>	LightMapChannelOptions;
	
	/**  */
	TArray<TSharedPtr<FString>>	MergedMaterialResolutionOptions;
};

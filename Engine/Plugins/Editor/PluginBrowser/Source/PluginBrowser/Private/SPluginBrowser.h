// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SBreadcrumbTrail.h"

/**
 * Stores data about a breadcrumb entry for the plugins editor
 */
class FPluginCategoryBreadcrumb
{

public:

	/** Initializing constructor */
	FPluginCategoryBreadcrumb( TSharedPtr< class FPluginCategory > InitCategoryItem )
		: CategoryItem( InitCategoryItem )
	{
	}

	/** @return Gets the category item for this breadcrumb */
	const TSharedPtr< class FPluginCategory >& GetCategoryItem() const
	{
		return CategoryItem;
	}


private:

	/** The category tree item data for this breadcrumb */
	TSharedPtr< class FPluginCategory > CategoryItem;
};



typedef TSharedPtr< FPluginCategoryBreadcrumb > FPluginCategoryBreadcrumbPtr;
typedef SBreadcrumbTrail< FPluginCategoryBreadcrumbPtr > SPluginCategoryBreadcrumbTrail;


/**
 * Implementation of main plugin editor Slate widget
 */
class SPluginBrowser : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SPluginBrowser )
	{
	}

	SLATE_END_ARGS()


	/** Widget constructor */
	void Construct( const FArguments& Args );

	/** @return Returns the plugin text filter object, so that child widgets can subscribe to find out about changes */
	FPluginTextFilter& GetPluginTextFilter()
	{
		return *PluginTextFilter;
	}

	/** @return Returns the currently selected category */
	TSharedPtr< class FPluginCategory > GetSelectedCategory() const;

	/** Called when the selected category changes so we can invalidate the list */
	void OnCategorySelectionChanged();

	/** Refresh the whole window */
	void SetNeedsRefresh();

private:

	/** @return Is the "restart required" notice visible? */
	EVisibility HandleRestartEditorNoticeVisibility() const;

	/** Handle the "restart now" button being clicked */
	FReply HandleRestartEditorButtonClicked() const;

	/** Called when the text in the search box was changed */
	void SearchBox_OnPluginSearchTextChanged( const FText& NewText );

	/** Called when a breadcrumb is clicked on the breadcrumb trail */
	void BreadcrumbTrail_OnCrumbClicked( const FPluginCategoryBreadcrumbPtr& CrumbData );

	/** Called to refresh the breadcrumb trail immediately */
	void RefreshBreadcrumbTrail();

	/** One-off active timer to trigger a refresh of the breadcrumb trail as needed */
	EActiveTimerReturnType TriggerBreadcrumbRefresh(double InCurrentTime, float InDeltaTime);

private:

	/** The plugin categories widget */
	TSharedPtr< class SPluginCategories > PluginCategories;

	/** The plugin list widget */
	TSharedPtr< class SPluginList > PluginList;

	/** Text filter object for typing in filter text to the search box */
	TSharedPtr< FPluginTextFilter > PluginTextFilter;

	/** Breadcrumb trail widget for the currently selected category */
	TSharedPtr< SPluginCategoryBreadcrumbTrail > BreadcrumbTrail;
};


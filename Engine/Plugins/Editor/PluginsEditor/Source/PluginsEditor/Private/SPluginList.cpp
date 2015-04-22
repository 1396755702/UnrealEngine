// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginsEditorPrivatePCH.h"
#include "SPluginList.h"
#include "SPluginsEditor.h"
#include "PluginListItem.h"
#include "SPluginListTile.h"
#include "IPluginManager.h"
#include "PluginCategoryTreeItem.h"


#define LOCTEXT_NAMESPACE "PluginList"


void SPluginList::Construct( const FArguments& Args, const TSharedRef< SPluginsEditor > Owner )
{
	OwnerWeak = Owner;

	// Find out when the plugin text filter changes
	Owner->GetPluginTextFilter().OnChanged().AddSP( this, &SPluginList::OnPluginTextFilterChanged );

	bIsActiveTimerRegistered = false;
	RebuildAndFilterPluginList();

	// @todo plugedit: Have optional compact version with only plugin icon + name + version?  Only expand selected?

	PluginListView = 
		SNew( SPluginListView )

		.SelectionMode( ESelectionMode::None )		// No need to select plugins!

		.ListItemsSource( &PluginListItems )
		.OnGenerateRow( this, &SPluginList::PluginListView_OnGenerateRow )
		;

	ChildSlot.AttachWidget( PluginListView.ToSharedRef() );
}


SPluginList::~SPluginList()
{
	const TSharedPtr< SPluginsEditor > Owner( OwnerWeak.Pin() );
	if( Owner.IsValid() )
	{
		Owner->GetPluginTextFilter().OnChanged().RemoveAll( this );
	}
}


/** @return Gets the owner of this list */
SPluginsEditor& SPluginList::GetOwner()
{
	return *OwnerWeak.Pin();
}


TSharedRef<ITableRow> SPluginList::PluginListView_OnGenerateRow( FPluginListItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable )
{
	return
		SNew( STableRow< FPluginListItemPtr >, OwnerTable )
		[
			SNew( SPluginListTile, SharedThis( this ), Item.ToSharedRef() )
		];
}



void SPluginList::GetPlugins( const TSharedPtr< FPluginCategoryTreeItem >& Category )
{
	// Add plugins from this category
	const TArray< TSharedPtr< FPluginStatus > >& Plugins = Category->GetPlugins();
	for( auto PluginIt( Plugins.CreateConstIterator() ); PluginIt; ++PluginIt )
	{
		const auto& PluginStatus = *( *PluginIt );

		// Skip plugins that don't match the current filter criteria
		bool bPassesFilter = true;
		if( !OwnerWeak.Pin()->GetPluginTextFilter().PassesFilter( PluginStatus ) )
		{
			bPassesFilter = false;
		}

		if( bPassesFilter )
		{
			const TSharedRef< FPluginListItem > NewItem( new FPluginListItem() );
			NewItem->PluginStatus = PluginStatus;
			PluginListItems.Add( NewItem );
		}
	}
}


void SPluginList::RebuildAndFilterPluginList()
{
	// Build up the initial list of plugins
	{
		PluginListItems.Reset();

		// Get the currently selected category
		const auto& SelectedCategory = OwnerWeak.Pin()->GetSelectedCategory();
		if( SelectedCategory.IsValid() )
		{
			GetPlugins( SelectedCategory );
		}

		// Sort the plugins alphabetically
		{
			struct FPluginListItemSorter
			{
				bool operator()( const FPluginListItemPtr& A, const FPluginListItemPtr& B ) const
				{
					return A->PluginStatus.Descriptor.FriendlyName < B->PluginStatus.Descriptor.FriendlyName;
				}
			};
			PluginListItems.Sort( FPluginListItemSorter() );
		}
	}


	// Update the list widget
	if( PluginListView.IsValid() )
	{
		PluginListView->RequestListRefresh();
	}
}

EActiveTimerReturnType SPluginList::TriggerListRebuild(double InCurrentTime, float InDeltaTime)
{
	RebuildAndFilterPluginList();

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SPluginList::OnPluginTextFilterChanged()
{
	SetNeedsRefresh();
}


void SPluginList::SetNeedsRefresh()
{
	if (!bIsActiveTimerRegistered)
	{
		bIsActiveTimerRegistered = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SPluginList::TriggerListRebuild));
	}
}


#undef LOCTEXT_NAMESPACE
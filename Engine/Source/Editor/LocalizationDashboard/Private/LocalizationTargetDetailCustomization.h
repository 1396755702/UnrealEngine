// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IDetailCustomization.h"
#include "LocalizationTarget.h"

class SCulturePicker;

class FLocalizationTargetDetailCustomization : public IDetailCustomization
{
public:
	FLocalizationTargetDetailCustomization();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	FLocalizationTargetSettings* GetTargetSettings() const;
	TSharedPtr<IPropertyHandle> GetTargetSettingsPropertyHandle() const;

private:
	FText GetTargetName() const;
	bool IsTargetNameUnique(const FString& Name) const;
	void OnTargetNameChanged(const FText& NewText);
	void OnTargetNameCommitted(const FText& NewText, ETextCommit::Type Type);

	void RebuildTargetDependenciesBox();
	void RebuildTargetsList();
	TSharedRef<ITableRow> OnGenerateTargetRow(ULocalizationTarget* OtherLocalizationTarget, const TSharedRef<STableViewBase>& Table);
	void OnTargetDependencyCheckStateChanged(ULocalizationTarget* const OtherLocalizationTarget, const ECheckBoxState State);
	ECheckBoxState IsTargetDependencyChecked(ULocalizationTarget* const OtherLocalizationTarget) const;

	FText GetNativeCultureName() const;
	FText GetNativeCultureDisplayName() const;
	void OnNativeCultureSelected(FCulturePtr SelectedCulture, ESelectInfo::Type SelectInfo);

	void Gather();
	void ImportAllCultures();
	void ExportAllCultures();
	void Compile();
	void RefreshWordCounts();
	void UpdateTargetFromReports();

	void BuildListedCulturesList();
	void RebuildListedCulturesList();
	TSharedRef<ITableRow> OnGenerateCultureRow(TSharedPtr<IPropertyHandle> CulturePropertyHandle, const TSharedRef<STableViewBase>& Table);

	bool IsCultureSelectableAsSupported(FCulturePtr Culture);
	void OnNewSupportedCultureSelected(FCulturePtr SelectedCulture, ESelectInfo::Type SelectInfo);

private:
	IDetailLayoutBuilder* DetailLayoutBuilder;

	TWeakObjectPtr<ULocalizationTargetSet> TargetSet;
	TWeakObjectPtr<ULocalizationTarget> LocalizationTarget;

	TSharedPtr<IPropertyHandle> TargetSettingsPropertyHandle;

	TSharedPtr<SEditableTextBox> TargetNameEditableTextBox;

	TSharedPtr<SHorizontalBox> TargetDependenciesHorizontalBox;
	TArray< TSharedPtr<SWidget> > TargetDependenciesWidgets;
	TArray<ULocalizationTarget*> TargetDependenciesOptionsList;
	TSharedPtr< SListView<ULocalizationTarget*> > TargetDependenciesListView;

	TSharedPtr<IPropertyHandle> NativeCultureStatisticsPropertyHandle;
	TSharedPtr<IPropertyHandle> NativeCultureNamePropertyHandle;
	TSharedPtr<SComboButton> NativeCultureComboButton;
	TArray<FCulturePtr> AllCultures;

	TSharedPtr<IPropertyHandle> SupportedCulturesStatisticsPropertyHandle;
	FSimpleDelegate SupportedCulturesStatisticsPropertyHandle_OnNumElementsChanged;
	TSharedPtr< SListView< TSharedPtr<IPropertyHandle> > > SupportedCultureListView;
	TSharedPtr<SComboButton> AddNewSupportedCultureComboButton;
	TSharedPtr<SCulturePicker> SupportedCulturePicker;
	TArray< TSharedPtr<IPropertyHandle> > ListedCultureStatisticProperties;

	/* If set, the entry at the index specified needs to be initialized as soon as possible. */
	int32 NewEntryIndexToBeInitialized;
	FCulturePtr SelectedNewCulture;
};

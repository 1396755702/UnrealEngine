// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"

#include "SlateBasics.h"
#include "StandaloneRenderer.h"
#include "LaunchEngineLoop.h"

#include "P4Env.h"

#include "ProcessHelper.h"
#include "SWidgetSwitcher.h"
#include "SDockTab.h"
#include "SlateFwd.h"

/**
 * Simple text combo box widget.
 */
class SSimpleTextComboBox : public SCompoundWidget
{
	/* Delegate for selection changed event. */
	DECLARE_DELEGATE_TwoParams(FOnSelectionChanged, int32, ESelectInfo::Type);

	SLATE_BEGIN_ARGS(SSimpleTextComboBox){}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

public:
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		OnSelectionChanged = InArgs._OnSelectionChanged;

		this->ChildSlot
			[
				SNew(SComboBox<TSharedPtr<FString> >)
				.OptionsSource(&OptionsSource)
				.OnSelectionChanged(this, &SSimpleTextComboBox::ComboBoxSelectionChanged)
				.OnGenerateWidget(this, &SSimpleTextComboBox::MakeWidgetFromOption)
				[
					SNew(STextBlock).Text(this, &SSimpleTextComboBox::GetCurrentFTextOption)
				]
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Adds value to the options source.
	 *
	 * @param Value Value to add.
	 */
	void Add(const FString& Value)
	{
		OptionsSource.Add(MakeShareable(new FString(Value)));
	}

	/**
	 * Gets current option value.
	 *
	 * @returns Reference to current option value.
	 */
	const FString& GetCurrentOption() const
	{
		return *OptionsSource[CurrentOption];
	}

	/**
	 * Checks if options source is empty.
	 *
	 * @returns True if options source is empty. False otherwise.
	 */
	bool IsEmpty()
	{
		return OptionsSource.Num() == 0;
	}

	/**
	 * Clears the options source.
	 */
	void Clear()
	{
		OptionsSource.Empty();
	}

private:
	/**
	 * Combo box selection changed event handling.
	 *
	 * @param Value Value of the picked selection.
	 * @param SelectInfo Selection info enum.
	 */
	void ComboBoxSelectionChanged(TSharedPtr<FString> Value, ESelectInfo::Type SelectInfo)
	{
		CurrentOption = -1;
		for (int32 OptionId = 0; OptionId < OptionsSource.Num(); ++OptionId)
		{
			if (OptionsSource[OptionId]->Equals(*Value))
			{
				CurrentOption = OptionId;
			}
		}

		OnSelectionChanged.ExecuteIfBound(CurrentOption, SelectInfo);
	}

	/**
	 * Coverts given value into widget.
	 *
	 * @param Value Value of the option.
	 *
	 * @return Widget.
	 */
	TSharedRef<SWidget> MakeWidgetFromOption(TSharedPtr<FString> Value)
	{
		return SNew(STextBlock).Text(FText::FromString(*Value));
	}

	/**
	 * Gets current option converted to FText class.
	 *
	 * @returns Current option converted to FText class.
	 */
	FText GetCurrentFTextOption() const
	{
		return FText::FromString(OptionsSource.Num() == 0 ? "" : GetCurrentOption());
	}

	/* Currently selected option. */
	int32 CurrentOption = 0;
	/* Array of options. */
	TArray<TSharedPtr<FString> > OptionsSource;
	/* Delegate that will be called when selection had changed. */
	FOnSelectionChanged OnSelectionChanged;
};

/**
 * Widget to store and select enabled widgets by radio buttons.
 */
class SRadioContentSelection : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, int32);

	/**
	 * Radio content selection item.
	 */
	class FItem : public TSharedFromThis<FItem>
	{
	public:
		/**
		 * Constructor
		 *
		 * @param Parent Parent SRadioContentSelection widget reference.
		 * @param Id Id of the item.
		 * @param Name Name of the item.
		 * @param Content Content widget to store by this item.
		 */
		FItem(SRadioContentSelection& Parent, int32 Id, const FString& Name, TSharedRef<SWidget> Content)
			: Parent(Parent), Id(Id), Name(Name), Content(Content)
		{

		}

		/**
		 * Item initialization method.
		 */
		void Init()
		{
			Disable();

			WholeItemWidget = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SCheckBox)
					.Style(FCoreStyle::Get(), "RadioButton")
					.OnCheckStateChanged(this, &FItem::OnCheckStateChange)
					.IsChecked(this, &FItem::IsCheckboxChecked)
					[
						SNew(STextBlock)
						.Text(FText::FromString(GetName()))
					]
				]
				+ SVerticalBox::Slot().VAlign(VAlign_Fill).Padding(19.0f, 2.0f, 2.0f, 2.0f)
				[
					GetContent()
				];
		}

		/**
		 * Gets this item name.
		 *
		 * @returns Item name.
		 */
		const FString& GetName() const
		{
			return Name;
		}
		
		/**
		 * Gets this item content widget.
		 *
		 * @returns Content widget.
		 */
		TSharedRef<SWidget> GetContent()
		{
			return Content;
		}
		
		/**
		 * Gets widget that represents this item.
		 *
		 * @returns Widget that represents this item.
		 */
		TSharedRef<SWidget> GetWidget()
		{
			return WholeItemWidget.ToSharedRef();
		}

		/**
		 * Enables this item.
		 */
		void Enable()
		{
			GetContent()->SetEnabled(true);
		}

		/**
		 * Disables this item.
		 */
		void Disable()
		{
			GetContent()->SetEnabled(false);
		}

		/**
		 * Tells if check box needs to be in checked state.
		 *
		 * @returns State of the check box enum.
		 */
		ECheckBoxState IsCheckboxChecked() const
		{
			return Parent.GetChosen() == Id
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
		}

	private:
		/**
		 * Function that is called when check box state is changed.
		 *
		 * @parent InNewState New state enum.
		 */
		void OnCheckStateChange(ECheckBoxState InNewState)
		{
			Parent.ChooseEnabledItem(Id);
		}

		/* Item id. */
		int32 Id;
		/* Item name. */
		FString Name;

		/* Outer item widget. */
		TSharedPtr<SWidget> WholeItemWidget;
		/* Stored widget. */
		TSharedRef<SWidget> Content;

		/* Parent reference. */
		SRadioContentSelection& Parent;
	};

	SLATE_BEGIN_ARGS(SRadioContentSelection){}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
			OnSelectionChanged = InArgs._OnSelectionChanged;

			MainBox = SNew(SVerticalBox);

			this->ChildSlot
				[
					MainBox.ToSharedRef()
				];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Adds widget to this selection with given name.
	 *
	 * @param Name Name of the widget in this selection.
	 * @param Content The widget itself.
	 */
	void Add(const FString& Name, TSharedRef<SWidget> Content)
	{
		TSharedRef<FItem> Item = MakeShareable(new FItem(*this, Items.Num(), Name, Content));
		Item->Init();
		Items.Add(Item);
		RebuildMainBox();
	}

	/**
	 * Enables chosen item and disable others.
	 *
	 * @param ItemId Chosen item id.
	 */
	void ChooseEnabledItem(int32 ItemId)
	{
		Chosen = ItemId;

		for (int32 CurrentItemId = 0; CurrentItemId < Items.Num(); ++CurrentItemId)
		{
			if (CurrentItemId == ItemId)
			{
				Items[CurrentItemId]->Enable();
			}
			else
			{
				Items[CurrentItemId]->Disable();
			}
		}

		OnSelectionChanged.ExecuteIfBound(Chosen);
	}

	/**
	 * Gets currently chosen item from this widget.
	 *
	 * @returns Chosen item id.
	 */
	int32 GetChosen() const
	{
		return Chosen;
	}

	/**
	 * Gets widget based on item id.
	 *
	 * @param ItemId Widget item id.
	 *
	 * @returns Reference to requested widget.
	 */
	TSharedRef<SWidget> GetContentWidget(int32 ItemId)
	{
		return Items[ItemId]->GetContent();
	}

private:
	/**
	 * Function to rebuild main and fill it with items provided.
	 */
	void RebuildMainBox()
	{
		MainBox->ClearChildren();

		for (auto Item : Items)
		{
			MainBox->AddSlot().AutoHeight().Padding(FMargin(0, 20, 0, 0))
				[
					Item->GetWidget()
				];
		}

		if (Items.Num() > 0)
		{
			ChooseEnabledItem(0);
		}
	}

	/* Delegate that will be called when the selection changed. */
	FOnSelectionChanged OnSelectionChanged;

	/* Main box ptr. */
	TSharedPtr<SVerticalBox> MainBox;
	/* Array of items/widgets. */
	TArray<TSharedRef<FItem> > Items;
	/* Chosen widget. */
	int32 Chosen;
};

/**
 * Sync latest promoted widget.
 */
class SLatestPromoted : public SCompoundWidget, public ILabelNameProvider
{
public:
	SLATE_BEGIN_ARGS(SLatestPromoted) {}
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		DataReady = MakeShareable(FPlatformProcess::CreateSynchEvent(true));

		this->ChildSlot
			[
				SNew(STextBlock).Text(FText::FromString("This option will sync to the latest promoted label for given game."))
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Gets command line from current options picked by user.
	 *
	 * @returns UAT UnrealSync command line.
	 */
	FString GetLabelName() const override
	{
		DataReady->Wait();

		return CurrentLabelName;
	}

	/**
	 * Gets message to display when sync task has started.
	 */
	virtual FString GetStartedMessage() const
	{
			return "Sync to latest started. Pending label data. Please wait.";
	}

	/**
	 * Gets message to display when sync task has finished.
	 */
	virtual FString GetFinishedMessage() const
	{
		return "Sync to latest finished.";
	}

	FString GetGameName() const override
	{
		DataReady->Wait();

		return ILabelNameProvider::GetGameName();
	}

	/**
	 * Refresh data.
	 *
	 * @param GameName Current game name.
	 */
	void RefreshData(const FString& GameName) override
	{
		ILabelNameProvider::RefreshData(GameName);

		auto Labels = FUnrealSync::GetPromotedLabelsForGame(GameName);
		if (Labels->Num() != 0)
		{
			CurrentLabelName = (*Labels)[0];
		}

		DataReady->Trigger();
	}

	/**
	 * Reset data.
	 *
	 * @param GameName Current game name.
	 */
	void ResetData(const FString& GameName) override
	{
		ILabelNameProvider::ResetData(GameName);

		DataReady->Reset();
	}

	/**
	 * Tells if this particular widget has data ready for sync.
	 *
	 * @returns True if ready. False otherwise.
	 */
	bool IsReadyForSync() const override
	{
		return true;
	}

private:
	/* Currently picked game name. */
	FString CurrentGameName;

	/* Current label name. */
	FString CurrentLabelName;

	/* Event that will trigger when data is ready. */
	TSharedPtr<FEvent> DataReady;
};

/**
 * Base class for pick label widgets.
 */
class SPickLabel : public SCompoundWidget, public ILabelNameProvider
{
public:
	SLATE_BEGIN_ARGS(SPickLabel) {}
		SLATE_ATTRIBUTE(FText, Title)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		LabelsCombo = SNew(SSimpleTextComboBox);
		LabelsCombo->Add("Needs refreshing");

		this->ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(InArgs._Title)
				]
				+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
				[
					LabelsCombo.ToSharedRef()
				]
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Gets command line from current options picked by user.
	 *
	 * @returns UAT UnrealSync command line.
	 */
	FString GetLabelName() const override
	{
		return LabelsCombo->GetCurrentOption();
	}

	/**
	 * Tells if this particular widget has data ready for sync.
	 *
	 * @returns True if ready. False otherwise.
	 */
	bool IsReadyForSync() const override
	{
		return FUnrealSync::HasValidData() && !LabelsCombo->IsEmpty();
	}

	/**
	 * Reset data.
	 *
	 * @param GameName Current game name.
	 */
	void ResetData(const FString& GameName) override
	{
		ILabelNameProvider::ResetData(GameName);

		LabelsCombo->Clear();
		LabelsCombo->Add("Loading P4 labels data...");
		LabelsCombo->SetEnabled(false);
	}

protected:
	/**
	 * Reset current labels combo options.
	 *
	 * @param LabelOptions Option to fill labels combo.
	 */
	void SetLabelOptions(const TArray<FString> &LabelOptions)
	{
		LabelsCombo->Clear();

		if (FUnrealSync::HasValidData())
		{
			for (auto LabelName : LabelOptions)
			{
				LabelsCombo->Add(LabelName);
			}

			LabelsCombo->SetEnabled(true);
		}
		else
		{
			LabelsCombo->Add("P4 data loading failed.");
			LabelsCombo->SetEnabled(false);
		}
	}

private:
	/* Labels combo widget. */
	TSharedPtr<SSimpleTextComboBox> LabelsCombo;
};

/**
 * Pick promoted label widget.
 */
class SPickPromoted : public SPickLabel
{
public:
	/**
	 * Refresh widget method.
	 *
	 * @param GameName Game name to refresh for.
	 */
	void RefreshData(const FString& GameName) override
	{
		SetLabelOptions(*FUnrealSync::GetPromotedLabelsForGame(GameName));

		SPickLabel::RefreshData(GameName);
	}
};

/**
 * Pick promotable label widget.
 */
class SPickPromotable : public SPickLabel
{
public:
	/**
	 * Refresh widget method.
	 *
	 * @param GameName Game name to refresh for.
	 */
	void RefreshData(const FString& GameName) override
	{
		SetLabelOptions(*FUnrealSync::GetPromotableLabelsForGame(GameName));

		SPickLabel::RefreshData(GameName);
	}
};

/**
 * Pick any label widget.
 */
class SPickAny : public SPickLabel
{
public:
	/**
	 * Refresh widget method.
	 *
	 * @param GameName Game name to refresh for.
	 */
	void RefreshData(const FString& GameName) override
	{
		SetLabelOptions(*FUnrealSync::GetAllLabels());

		SPickLabel::RefreshData(GameName);
	}
};

/**
 * Pick game widget.
 */
class SPickGameWidget : public SCompoundWidget
{
public:
	/* Delegate for game picked event. */
	DECLARE_DELEGATE_OneParam(FOnGamePicked, const FString&);

	SLATE_BEGIN_ARGS(SPickGameWidget) {}
		SLATE_EVENT(FOnGamePicked, OnGamePicked)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		OnGamePicked = InArgs._OnGamePicked;

		GamesOptions = SNew(SSimpleTextComboBox)
			.OnSelectionChanged(this, &SPickGameWidget::OnComboBoxSelectionChanged);

		auto PossibleGameNames = FUnrealSync::GetPossibleGameNames();

		GamesOptions->Add(FUnrealSync::GetSharedPromotableDisplayName());

		for (auto PossibleGameName : *PossibleGameNames)
		{
			GamesOptions->Add(PossibleGameName);
		}

		this->ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString("Pick game to sync: "))
				]
				+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
					[
						GamesOptions.ToSharedRef()
					]
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Gets current game selected.
	 *
	 * @returns Currently selected game.
	 */
	const FString& GetCurrentGame()
	{
		return GamesOptions->GetCurrentOption();
	}

private:
	/**
	 * Function is called when combo box selection is changed.
	 * It called OnGamePicked event.
	 */
	void OnComboBoxSelectionChanged(int32 SelectionId, ESelectInfo::Type SelectionInfo)
	{
		OnGamePicked.ExecuteIfBound(GamesOptions->GetCurrentOption());
	}

	/* Game options widget. */
	TSharedPtr<SSimpleTextComboBox> GamesOptions;
	/* Delegate that is going to be called when game was picked by the user. */
	FOnGamePicked OnGamePicked;
};

/**
 * Main tab widget.
 */
class SMainTabWidget : public SCompoundWidget
{
	/**
	 * External threads requests dispatcher.
	 */
	class FExternalThreadsDispatcher : public TSharedFromThis<FExternalThreadsDispatcher, ESPMode::ThreadSafe>
	{
	public:
		/** Delegate that represents rendering thread request. */
		DECLARE_DELEGATE(FRenderingThreadRequest);

		/**
		 * Executes all pending requests.
		 *
		 * Some Slate methods are restricted for either game or Slate rendering
		 * thread. If external thread tries to execute them assert raises, which is
		 * telling that it's unsafe.
		 *
		 * This method executes all pending requests on Slate thread, so you can
		 * use those to execute some restricted methods.
		 */
		EActiveTimerReturnType ExecuteRequests(double CurrentTime, float DeltaTime)
		{
			FScopeLock Lock(&RequestsMutex);
			for (int32 ReqId = 0; ReqId < Requests.Num(); ++ReqId)
			{
				Requests[ReqId].Execute();
			}

			Requests.Empty(4);

			return EActiveTimerReturnType::Continue;
		}

		/**
		 * Adds delegate to pending requests list.
		 */
		void AddRenderingThreadRequest(FRenderingThreadRequest Request)
		{
			FScopeLock Lock(&RequestsMutex);
			Requests.Add(MoveTemp(Request));
		}

	private:
		/** Mutex for pending requests list. */
		FCriticalSection RequestsMutex;

		/** Pending requests list. */
		TArray<FRenderingThreadRequest> Requests;
	};

public:
	SMainTabWidget()
		: ExternalThreadsDispatcher(MakeShareable(new FExternalThreadsDispatcher()))
	{}

	SLATE_BEGIN_ARGS(SMainTabWidget) {}
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		RadioSelection = SNew(SRadioContentSelection);

		AddToRadioSelection("Sync to the latest promoted", SNew(SLatestPromoted));
		AddToRadioSelection("Sync to chosen promoted label", SNew(SPickPromoted).Title(FText::FromString("Pick promoted label: ")));
		AddToRadioSelection("Sync to chosen promotable label since last promoted", SNew(SPickPromotable).Title(FText::FromString("Pick promotable label: ")));
		AddToRadioSelection("Sync to any chosen label", SNew(SPickAny).Title(FText::FromString("Pick label: ")));

		PickGameWidget = SNew(SPickGameWidget)
			.OnGamePicked(this, &SMainTabWidget::OnCurrentGameChanged);

		ArtistSyncCheckBox = SNew(SCheckBox)
			[
				SNew(STextBlock).Text(FText::FromString("Artist sync?"))
			];

#if UE_BUILD_DEBUG
		auto bDebug = FParse::Param(FCommandLine::Get(), TEXT("Debug"));
#else
		const auto bDebug = false;
#endif

		if (!bDebug)
		{
			// Checked by default.
			ArtistSyncCheckBox->ToggleCheckedState();
		}

		PreviewSyncCheckBox = SNew(SCheckBox)
			[
				SNew(STextBlock).Text(FText::FromString("Preview sync?"))
			];

		GoBackButton = SNew(SButton)
			.IsEnabled(false)
			.OnClicked(this, &SMainTabWidget::OnGoBackButtonClick)
			[
				SNew(STextBlock).Text(FText::FromString("Go back"))
			];

		LogListView = SNew(SListView<TSharedPtr<FString> >)
			.ListItemsSource(&LogLines)
			.OnGenerateRow(this, &SMainTabWidget::GenerateLogItem);

		TSharedPtr<SVerticalBox> MainBox;
		
		Switcher = SNew(SWidgetSwitcher)
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(MainBox, SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(5.0f)
				[
					PickGameWidget.ToSharedRef()
				]
				+ SVerticalBox::Slot().VAlign(VAlign_Fill)
				[
					RadioSelection.ToSharedRef()
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton).HAlign(HAlign_Right)
						.OnClicked(this, &SMainTabWidget::OnReloadLabels)
						.IsEnabled(this, &SMainTabWidget::IsReloadLabelsReady)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(FText::FromString("Reload Labels"))
						]
					]
					+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						[
							ArtistSyncCheckBox.ToSharedRef()
						]
						+ SVerticalBox::Slot()
							[
								PreviewSyncCheckBox.ToSharedRef()
							]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(30, 0, 0, 0)
						[
							SNew(SButton).HAlign(HAlign_Right)
							.OnClicked(this, &SMainTabWidget::OnSync)
							.IsEnabled(this, &SMainTabWidget::IsSyncReady)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("Sync!"))
							]
						]
					+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).HAlign(HAlign_Right)
							.OnClicked(this, &SMainTabWidget::RunUE4)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("Run UE4"))
							]
						]
				]
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					LogListView->AsWidget()
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(CancelButton, SButton)
						.OnClicked(this, &SMainTabWidget::OnCancelButtonClick)
						[
							SNew(STextBlock).Text(FText::FromString("Cancel"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						GoBackButton.ToSharedRef()
					]
				]
			];
		
		this->ChildSlot
			[
				Switcher.ToSharedRef()
			];

		if (bDebug)
		{
			MainBox->InsertSlot(2).AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("OverrideSyncStepSpec", "Override sync step specification"))
					]
					+ SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox).OnTextCommitted(this, &SMainTabWidget::OnOverrideSyncStepText)
					]
				];
		}

		FUnrealSync::RegisterOnDataReset(FUnrealSync::FOnDataReset::CreateRaw(this, &SMainTabWidget::DataReset));
		FUnrealSync::RegisterOnDataLoaded(FUnrealSync::FOnDataLoaded::CreateRaw(this, &SMainTabWidget::DataLoaded));

		RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateThreadSafeSP(&ExternalThreadsDispatcher.Get(), &FExternalThreadsDispatcher::ExecuteRequests));

		OnReloadLabels();
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Destructor.
	 */
	virtual ~SMainTabWidget()
	{
		FUnrealSync::TerminateLoadingProcess();
		FUnrealSync::TerminateSyncingProcess();
	}

private:
	/**
	 * Queues adding lines to the log for execution on Slate rendering thread.
	 */
	void ExternalThreadAddLinesToLog(TSharedPtr<TArray<TSharedPtr<FString> > > Lines)
	{
		ExternalThreadsDispatcher->AddRenderingThreadRequest(FExternalThreadsDispatcher::FRenderingThreadRequest::CreateSP(this, &SMainTabWidget::AddLinesToLog, Lines));
	}

	/**
	 * Adds lines to the log.
	 *
	 * @param Lines Lines to add.
	 */
	void AddLinesToLog(TSharedPtr<TArray<TSharedPtr<FString> > > Lines)
	{
		for (const auto& Line : *Lines)
		{
			LogLines.Add(Line);
		}

		LogListView->RequestScrollIntoView(LogLines.Last());
	}

	/**
	 * Generates list view item from FString.
	 *
	 * @param Value Value to convert.
	 * @param OwnerTable List view that will contain this item.
	 *
	 * @returns Converted STableRow object.
	 */
	TSharedRef<ITableRow> GenerateLogItem(TSharedPtr<FString> Value, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
			[
				SNew(STextBlock).Text(FText::FromString(*Value))
			];
	}

	/**
	 * Gets current sync command line provider.
	 *
	 * @returns Current sync cmd line provider.
	 */
	ILabelNameProvider& GetCurrentSyncCmdLineProvider()
	{
		return *SyncCommandLineProviders[RadioSelection->GetChosen()];
	}

	/**
	 * Gets current sync command line provider.
	 *
	 * @returns Current sync cmd line provider.
	 */
	const ILabelNameProvider& GetCurrentSyncCmdLineProvider() const
	{
		return *SyncCommandLineProviders[RadioSelection->GetChosen()];
	}

	/**
	 * Performs a procedure that needs to be done when GoBack button is clicked.
	 * It tells the app to go back to the beginning.
	 */
	void GoBack()
	{
		Switcher->SetActiveWidgetIndex(0);
		LogLines.Empty();
		LogListView->RequestListRefresh();
		OnReloadLabels();
	}

	/**
	 * Function that is called on when go back button is clicked.
	 * It tells the app to go back to the beginning.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnGoBackButtonClick()
	{
		GoBack();

		return FReply::Handled();
	}

	/**
	 * Function that is called on when cancel button is clicked.
	 * It tells the app to cancel the sync and go back to the beginning.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnCancelButtonClick()
	{
		FUnrealSync::TerminateLoadingProcess();
		FUnrealSync::TerminateSyncingProcess();

		ExternalThreadsDispatcher->AddRenderingThreadRequest(FExternalThreadsDispatcher::FRenderingThreadRequest::CreateSP(this, &SMainTabWidget::GoBack));

		return FReply::Handled();
	}

	/**
	 * Tells Sync button if it should be enabled. It depends on the combination
	 * of user choices and if P4 data is currently ready.
	 *
	 * @returns True if Sync button should be enabled. False otherwise.
	 */
	bool IsSyncReady() const
	{
		return GetCurrentSyncCmdLineProvider().IsReadyForSync();
	}

	/**
	 * Tells reload labels button if it should be enabled.
	 *
	 * @returns True if reload labels button should be enabled. False otherwise.
	 */
	bool IsReloadLabelsReady() const
	{
		return !FUnrealSync::IsLoadingInProgress();
	}

	/**
	 * Launches reload labels background process.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnReloadLabels()
	{
		FUnrealSync::StartLoadingData();

		return FReply::Handled();
	}

	/**
	 * Executes UE4Editor.exe for current branch.
	 */
	FReply RunUE4()
	{
#if PLATFORM_WINDOWS
		auto ProcessPath = FPaths::Combine(*FPaths::GetPath(FPlatformProcess::GetApplicationName(FPlatformProcess::GetCurrentProcessId())), TEXT("UE4Editor.exe"));
#else
		// Needs to be implemented on other platforms.
#endif

		RunProcess(ProcessPath);

		return FReply::Handled();
	}

	/**
	 * Sync button click function. Gets all widget data, constructs command
	 * line and launches syncing.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnSync()
	{
		FSyncSettings Settings(
			ArtistSyncCheckBox->IsChecked(),
			PreviewSyncCheckBox->IsChecked(),
			OverrideSyncStep);

		Switcher->SetActiveWidgetIndex(1);
		FUnrealSync::LaunchSync(Settings, GetCurrentSyncCmdLineProvider(),
			FUnrealSync::FOnSyncFinished::CreateRaw(this, &SMainTabWidget::SyncingFinished),
			FUnrealSync::FOnSyncProgress::CreateRaw(this, &SMainTabWidget::SyncingProgress));

		GoBackButton->SetEnabled(false);
		CancelButton->SetEnabled(true);

		return FReply::Handled();
	}

	/**
	 * Function that is called whenever monitoring thread will update its log.
	 *
	 * @param Log Log of the operation up to this moment.
	 *
	 * @returns True if process should continue, false otherwise.
	 */
	bool SyncingProgress(const FString& Log)
	{
		if (Log.IsEmpty())
		{
			return true;
		}

		static FString Buffer;

		Buffer += Log.Replace(TEXT("\r"), TEXT(""));

		FString Line;
		FString Rest;

		TSharedPtr<TArray<TSharedPtr<FString> > > Lines = MakeShareable(new TArray<TSharedPtr<FString> >());

		while (Buffer.Split("\n", &Line, &Rest))
		{
			Lines->Add(MakeShareable(new FString(Line)));

			Buffer = Rest;
		}

		ExternalThreadAddLinesToLog(Lines);

		return true;
	}

	/**
	 * Function that is called when P4 syncing is finished.
	 *
	 * @param bSuccess True if syncing finished. Otherwise false.
	 */
	void SyncingFinished(bool bSuccess)
	{
		CancelButton->SetEnabled(false);
		GoBackButton->SetEnabled(true);
	}

	/**
	 * Function to call when current game changed. Is refreshing
	 * label lists in the widget.
	 *
	 * @param CurrentGameName Game name chosen.
	 */
	void OnCurrentGameChanged(const FString& CurrentGameName)
	{
		if (!FUnrealSync::HasValidData())
		{
			return;
		}

		for (auto SyncCommandLineProvider : SyncCommandLineProviders)
		{
			SyncCommandLineProvider->RefreshData(
				CurrentGameName.Equals(FUnrealSync::GetSharedPromotableDisplayName())
				? ""
				: CurrentGameName);
		}
	}

	/**
	 * Function to call when P4 cached data is reset.
	 */
	void DataReset()
	{
		for (auto SyncCommandLineProvider : SyncCommandLineProviders)
		{
			SyncCommandLineProvider->ResetData(
				PickGameWidget->GetCurrentGame().Equals(FUnrealSync::GetSharedPromotableDisplayName())
				? ""
				: PickGameWidget->GetCurrentGame());
		}
	}

	/**
	 * Function to call when P4 cached data is loaded.
	 */
	void DataLoaded()
	{
		const FString& GameName = PickGameWidget->GetCurrentGame();

		for (auto SyncCommandLineProvider : SyncCommandLineProviders)
		{
			SyncCommandLineProvider->RefreshData(
				GameName.Equals(FUnrealSync::GetSharedPromotableDisplayName())
				? ""
				: GameName);
		}
	}

	/**
	 * Happens when someone edits the override sync step text box.
	 */
	void OnOverrideSyncStepText(const FText& CommittedText, ETextCommit::Type Type);

	/**
	 * Method to add command line provider/widget to correct arrays.
	 *
	 * @param Name Name of the radio selection widget item.
	 * @param Widget Widget to add.
	 */
	template <typename TWidgetType>
	void AddToRadioSelection(const FString& Name, TSharedRef<TWidgetType> Widget)
	{
		RadioSelection->Add(Name, Widget);
		SyncCommandLineProviders.Add(Widget);
	}

	/* Main widget switcher. */
	TSharedPtr<SWidgetSwitcher> Switcher;

	/* Radio selection used to chose method to sync. */
	TSharedPtr<SRadioContentSelection> RadioSelection;

	/* Widget to pick game used to sync. */
	TSharedPtr<SPickGameWidget> PickGameWidget;
	/* Check box to tell if this should be an artist sync. */
	TSharedPtr<SCheckBox> ArtistSyncCheckBox;
	/* Check box to tell if this should be a preview sync. */
	TSharedPtr<SCheckBox> PreviewSyncCheckBox;

	/* External thread requests dispatcher. */
	TSharedRef<FExternalThreadsDispatcher, ESPMode::ThreadSafe> ExternalThreadsDispatcher;

	/* Report log. */
	TArray<TSharedPtr<FString> > LogLines;
	/* Log list view. */
	TSharedPtr<SListView<TSharedPtr<FString> > > LogListView;

	/* Go back button reference. */
	TSharedPtr<SButton> GoBackButton;
	/* Cancel button reference. */
	TSharedPtr<SButton> CancelButton;

	/* Array of sync command line providers. */
	TArray<TSharedRef<ILabelNameProvider> > SyncCommandLineProviders;

	/* Override sync step value. */
	FString OverrideSyncStep;
};

void SMainTabWidget::OnOverrideSyncStepText(const FText& CommittedText, ETextCommit::Type Type)
{
	OverrideSyncStep = CommittedText.ToString();
}

/**
 * Creates and returns main tab.
 *
 * @param Args Args used to spawn tab.
 *
 * @returns Main UnrealSync tab.
 */
TSharedRef<SDockTab> GetMainTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> MainTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(FText::FromString("Syncing"))
		.ToolTipText(FText::FromString("Sync Unreal Engine tool."));

	MainTab->SetContent(
		SNew(SMainTabWidget)
		);

	FGlobalTabmanager::Get()->SetActiveTab(MainTab);

	return MainTab;
}

/**
 * Creates and returns P4 settings tab.
 *
 * @param Args Args used to spawn tab.
 *
 * @returns P4 settings UnrealSync tab.
 */
TSharedRef<SDockTab> GetP4EnvTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> P4EnvTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(FText::FromString("Perforce Settings"))
		.ToolTipText(FText::FromString("Perforce settings."));

	P4EnvTab->SetContent(
		SNew(SP4EnvTabWidget)
		);

	return P4EnvTab;
}

void InitGUI(const TCHAR* CommandLine, bool bP4EnvTabOnly)
{
	// crank up a normal Slate application using the platform's standalone renderer
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

	// set the application name
	FGlobalTabmanager::Get()->SetApplicationTitle(NSLOCTEXT("UnrealSync", "AppTitle", "Unreal Sync"));

	auto TabStack = FTabManager::NewStack();

	if (!bP4EnvTabOnly)
	{
		TabStack->AddTab("MainTab", ETabState::OpenedTab);
		FGlobalTabmanager::Get()->RegisterTabSpawner("MainTab", FOnSpawnTab::CreateStatic(&GetMainTab));
		TabStack->SetForegroundTab(FTabId("MainTab"));
	}

	FGlobalTabmanager::Get()->RegisterTabSpawner("P4EnvTab", FOnSpawnTab::CreateStatic(&GetP4EnvTab));
	TabStack->AddTab("P4EnvTab", ETabState::OpenedTab);

	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("UnrealSyncLayout")
		->AddArea(
			FTabManager::NewArea(720, 370)
			->SetWindow(FVector2D(420, 10), false)
			->Split(TabStack)
		);

	FGlobalTabmanager::Get()->RestoreFrom(Layout, TSharedPtr<SWindow>());
	
	// loop while the server does the rest
	while (!GIsRequestingExit)
	{
		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
		FPlatformProcess::Sleep(0);
	}

	FSlateApplication::Shutdown();
}
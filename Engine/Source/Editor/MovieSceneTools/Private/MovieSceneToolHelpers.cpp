// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneAudioTrack.h"
#include "MovieScene3DTransformSection.h"
#include "MovieScene3DTransformTrack.h"
#include "MovieSceneCinematicShotSection.h"
#include "MovieSceneCinematicShotTrack.h"
#include "LevelSequence.h"
#include "AssetRegistryModule.h"
#include "CoreMisc.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Editor/MainFrame/Public/MainFrame.h"
#include "SlateBasics.h"
#include "SlateExtras.h"
#include "INotificationWidget.h"
#include "MovieSceneCaptureHelpers.h"
#include "FbxImporter.h"
#include "MatineeImportTools.h"

/* MovieSceneToolHelpers
 *****************************************************************************/

void MovieSceneToolHelpers::TrimSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time, bool bTrimLeft)
{
	for (auto Section : Sections)
	{
		if (Section.IsValid())
		{
			Section->TrimSection(Time, bTrimLeft);
		}
	}
}


void MovieSceneToolHelpers::SplitSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time)
{
	for (auto Section : Sections)
	{
		if (Section.IsValid())
		{
			Section->SplitSection(Time);
		}
	}
}

bool MovieSceneToolHelpers::ParseShotName(const FString& ShotName, FString& ShotPrefix, uint32& ShotNumber, uint32& TakeNumber)
{
	// Parse a shot name
	// 
	// sht010:
	//  ShotPrefix = sht
	//  ShotNumber = 10
	//  TakeNumber = 1 (default)
	// 
	// sp020_002
	//  ShotPrefix = sp
	//  ShotNumber = 20
	//  TakeNumber = 2
	//
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	uint32 FirstShotNumberIndex = INDEX_NONE;
	uint32 LastShotNumberIndex = INDEX_NONE;
	bool bInShotNumber = false;

	uint32 FirstTakeNumberIndex = INDEX_NONE;
	uint32 LastTakeNumberIndex = INDEX_NONE;
	bool bInTakeNumber = false;

	bool bFoundTakeSeparator = false;
	TakeNumber = ProjectSettings->FirstTakeNumber;

	for (int32 CharIndex = 0; CharIndex < ShotName.Len(); ++CharIndex)
	{
		if (FChar::IsDigit(ShotName[CharIndex]))
		{
			// Find shot number indices
			if (FirstShotNumberIndex == INDEX_NONE)
			{
				bInShotNumber = true;
				FirstShotNumberIndex = CharIndex;
			}
			if (bInShotNumber)
			{
				LastShotNumberIndex = CharIndex;
			}

			if (FirstShotNumberIndex != INDEX_NONE && LastShotNumberIndex != INDEX_NONE)
			{
				if (bFoundTakeSeparator)
				{
					// Find take number indices
					if (FirstTakeNumberIndex == INDEX_NONE)
					{
						bInTakeNumber = true;
						FirstTakeNumberIndex = CharIndex;
					}
					if (bInTakeNumber)
					{
						LastTakeNumberIndex = CharIndex;
					}
				}
			}
		}

		if (FirstShotNumberIndex != INDEX_NONE && LastShotNumberIndex != INDEX_NONE)
		{
			if (ShotName[CharIndex] == ProjectSettings->TakeSeparator[0])
			{
				bFoundTakeSeparator = true;
			}
		}
	}

	if (FirstShotNumberIndex != INDEX_NONE)
	{
		ShotPrefix = ShotName.Left(FirstShotNumberIndex);
		ShotNumber = FCString::Atoi(*ShotName.Mid(FirstShotNumberIndex, LastShotNumberIndex-FirstShotNumberIndex+1));
	}

	if (FirstTakeNumberIndex != INDEX_NONE)
	{
		TakeNumber = FCString::Atoi(*ShotName.Mid(FirstTakeNumberIndex, LastTakeNumberIndex-FirstTakeNumberIndex+1));
	}

	return FirstShotNumberIndex != INDEX_NONE;
}


FString MovieSceneToolHelpers::ComposeShotName(const FString& ShotPrefix, uint32 ShotNumber, uint32 TakeNumber)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	FString ShotName = ShotPrefix;

	FString ShotFormat = TEXT("%0") + FString::Printf(TEXT("%d"), ProjectSettings->ShotNumDigits) + TEXT("d");
	FString TakeFormat = TEXT("%0") + FString::Printf(TEXT("%d"), ProjectSettings->TakeNumDigits) + TEXT("d");

	ShotName += FString::Printf(*ShotFormat, ShotNumber);
	if (TakeNumber != INDEX_NONE)
	{
		ShotName += ProjectSettings->TakeSeparator;
		ShotName += FString::Printf(*TakeFormat, TakeNumber);
	}
	return ShotName;
}

bool IsPackageNameUnique(const TArray<FAssetData>& ObjectList, FString& NewPackageName)
{
	for (auto AssetObject : ObjectList)
	{
		if (AssetObject.PackageName.ToString() == NewPackageName)
		{
			return false;
		}
	}
	return true;
}

FString MovieSceneToolHelpers::GenerateNewShotPath(UMovieScene* SequenceMovieScene, FString& NewShotName)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> ObjectList;
	AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), ObjectList);

	UObject* SequenceAsset = SequenceMovieScene->GetOuter();
	UPackage* SequencePackage = SequenceAsset->GetOutermost();
	FString SequencePackageName = SequencePackage->GetName(); // ie. /Game/cine/max/master
	int LastSlashPos = SequencePackageName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	FString SequencePath = SequencePackageName.Left(LastSlashPos);

	FString NewShotPrefix;
	uint32 NewShotNumber = INDEX_NONE;
	uint32 NewTakeNumber = INDEX_NONE;
	ParseShotName(NewShotName, NewShotPrefix, NewShotNumber, NewTakeNumber);

	FString NewShotDirectory = ComposeShotName(NewShotPrefix, NewShotNumber, INDEX_NONE);
	FString NewShotPath = SequencePath;

	FString ShotDirectory = ProjectSettings->ShotDirectory;
	if (!ShotDirectory.IsEmpty())
	{
		NewShotPath /= ShotDirectory;
	}
	NewShotPath /= NewShotDirectory; // put this in the shot directory, ie. /Game/cine/max/shots/shot0010

	// Make sure this shot path is unique
	FString NewPackageName = NewShotPath;
	NewPackageName /= NewShotName; // ie. /Game/cine/max/shots/shot0010/shot0010_001
	if (!IsPackageNameUnique(ObjectList, NewPackageName))
	{
		while (1)
		{
			NewShotNumber += ProjectSettings->ShotIncrement;
			NewShotName = ComposeShotName(NewShotPrefix, NewShotNumber, NewTakeNumber);
			NewShotDirectory = ComposeShotName(NewShotPrefix, NewShotNumber, INDEX_NONE);
			NewShotPath = SequencePath;
			if (!ShotDirectory.IsEmpty())
			{
				NewShotPath /= ShotDirectory;
			}
			NewShotPath /= NewShotDirectory;

			NewPackageName = NewShotPath;
			NewPackageName /= NewShotName;
			if (IsPackageNameUnique(ObjectList, NewPackageName))
			{
				break;
			}
		}
	}

	return NewShotPath;
}


FString MovieSceneToolHelpers::GenerateNewShotName(const TArray<UMovieSceneSection*>& AllSections, float Time)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	UMovieSceneCinematicShotSection* BeforeShot = nullptr;
	UMovieSceneCinematicShotSection* NextShot = nullptr;

	float MinEndDiff = FLT_MAX;
	float MinStartDiff = FLT_MAX;
	for (auto Section : AllSections)
	{
		if (Section->GetEndTime() >= Time)
		{
			float EndDiff = Section->GetEndTime() - Time;
			if (MinEndDiff > EndDiff)
			{
				MinEndDiff = EndDiff;
				BeforeShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}
		if (Section->GetStartTime() <= Time)
		{
			float StartDiff = Time - Section->GetStartTime();
			if (MinStartDiff > StartDiff)
			{
				MinStartDiff = StartDiff;
				NextShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}
	}
	
	// There aren't any shots, let's create the first shot name
	if (BeforeShot == nullptr || NextShot == nullptr)
	{
		// Default case
	}
	// This is the last shot
	else if (BeforeShot == NextShot)
	{
		FString NextShotPrefix = ProjectSettings->ShotPrefix;
		uint32 NextShotNumber = ProjectSettings->FirstShotNumber;
		uint32 NextTakeNumber = ProjectSettings->FirstTakeNumber;

		if (ParseShotName(NextShot->GetShotDisplayName().ToString(), NextShotPrefix, NextShotNumber, NextTakeNumber))
		{
			uint32 NewShotNumber = NextShotNumber + ProjectSettings->ShotIncrement;
			return ComposeShotName(NextShotPrefix, NewShotNumber, ProjectSettings->FirstTakeNumber);
		}
	}
	// This is in between two shots
	else 
	{
		FString BeforeShotPrefix = ProjectSettings->ShotPrefix;
		uint32 BeforeShotNumber = ProjectSettings->FirstShotNumber;
		uint32 BeforeTakeNumber = ProjectSettings->FirstTakeNumber;

		FString NextShotPrefix = ProjectSettings->ShotPrefix;
		uint32 NextShotNumber = ProjectSettings->FirstShotNumber;
		uint32 NextTakeNumber = ProjectSettings->FirstTakeNumber;

		if (ParseShotName(BeforeShot->GetShotDisplayName().ToString(), BeforeShotPrefix, BeforeShotNumber, BeforeTakeNumber) &&
			ParseShotName(NextShot->GetShotDisplayName().ToString(), NextShotPrefix, NextShotNumber, NextTakeNumber))
		{
			if (BeforeShotNumber < NextShotNumber)
			{
				uint32 NewShotNumber = BeforeShotNumber + ( (NextShotNumber - BeforeShotNumber) / 2); // what if we can't find one? or conflicts with another?
				return ComposeShotName(BeforeShotPrefix, NewShotNumber, ProjectSettings->FirstTakeNumber);
			}
		}
	}

	// Default case
	return ComposeShotName(ProjectSettings->ShotPrefix, ProjectSettings->FirstShotNumber, ProjectSettings->FirstTakeNumber);
}

void MovieSceneToolHelpers::GatherTakes(const UMovieSceneSection* Section, TArray<uint32>& TakeNumbers, uint32& CurrentTakeNumber)
{
	const UMovieSceneCinematicShotSection* Shot = Cast<const UMovieSceneCinematicShotSection>(Section);
	
	if (Shot->GetSequence() == nullptr)
	{
		return;
	}

	FAssetData ShotData(Shot->GetSequence()->GetOuter());

	FString ShotPackagePath = ShotData.PackagePath.ToString();

	FString ShotPrefix;
	uint32 ShotNumber = INDEX_NONE;
	CurrentTakeNumber = INDEX_NONE;

	if (ParseShotName(Shot->GetShotDisplayName().ToString(), ShotPrefix, ShotNumber, CurrentTakeNumber))
	{
		// Gather up all level sequence assets
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> ObjectList;
		AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), ObjectList);

		for (auto AssetObject : ObjectList)
		{
			FString AssetPackagePath = AssetObject.PackagePath.ToString();

			if (AssetPackagePath == ShotPackagePath)
			{
				FString AssetShotPrefix;
				uint32 AssetShotNumber = INDEX_NONE;
				uint32 AssetTakeNumber = INDEX_NONE;

				if (ParseShotName(AssetObject.AssetName.ToString(), AssetShotPrefix, AssetShotNumber, AssetTakeNumber))
				{
					if (AssetShotPrefix == ShotPrefix && AssetShotNumber == ShotNumber)
					{
						TakeNumbers.Add(AssetTakeNumber);
					}
				}
			}
		}
	}

	TakeNumbers.Sort();
}

UObject* MovieSceneToolHelpers::GetTake(const UMovieSceneSection* Section, uint32 TakeNumber)
{
	const UMovieSceneCinematicShotSection* Shot = Cast<const UMovieSceneCinematicShotSection>(Section);

	FAssetData ShotData(Shot->GetSequence()->GetOuter());

	FString ShotPackagePath = ShotData.PackagePath.ToString();
	int32 ShotLastSlashPos = INDEX_NONE;
	ShotPackagePath.FindLastChar(TCHAR('/'), ShotLastSlashPos);
	ShotPackagePath = ShotPackagePath.Left(ShotLastSlashPos);

	FString ShotPrefix;
	uint32 ShotNumber = INDEX_NONE;
	uint32 TakeNumberDummy = INDEX_NONE;

	if (ParseShotName(Shot->GetShotDisplayName().ToString(), ShotPrefix, ShotNumber, TakeNumberDummy))
	{
		// Gather up all level sequence assets
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> ObjectList;
		AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), ObjectList);

		for (auto AssetObject : ObjectList)
		{
			FString AssetPackagePath = AssetObject.PackagePath.ToString();
			int32 AssetLastSlashPos = INDEX_NONE;
			AssetPackagePath.FindLastChar(TCHAR('/'), AssetLastSlashPos);
			AssetPackagePath = AssetPackagePath.Left(AssetLastSlashPos);

			if (AssetPackagePath == ShotPackagePath)
			{
				FString AssetShotPrefix;
				uint32 AssetShotNumber = INDEX_NONE;
				uint32 AssetTakeNumber = INDEX_NONE;

				if (ParseShotName(AssetObject.AssetName.ToString(), AssetShotPrefix, AssetShotNumber, AssetTakeNumber))
				{
					if (AssetShotPrefix == ShotPrefix && AssetShotNumber == ShotNumber && TakeNumber == AssetTakeNumber)
					{
						return AssetObject.GetAsset();
					}
				}
			}
		}
	}

	return nullptr;
}

class SEnumCombobox : public SComboBox<TSharedPtr<int32>>
{
public:
	SLATE_BEGIN_ARGS(SEnumCombobox) {}

	SLATE_ATTRIBUTE(TOptional<uint8>, IntermediateValue)
	SLATE_ATTRIBUTE(int32, CurrentValue)
	SLATE_ARGUMENT(FOnEnumSelectionChanged, OnEnumSelectionChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UEnum* InEnum)
	{
		Enum = InEnum;
		IntermediateValue = InArgs._IntermediateValue;
		CurrentValue = InArgs._CurrentValue;
		check(CurrentValue.IsBound());
		OnEnumSelectionChangedDelegate = InArgs._OnEnumSelectionChanged;

		bUpdatingSelectionInternally = false;

		for (int32 i = 0; i < Enum->NumEnums() - 1; i++)
		{
			if (Enum->HasMetaData( TEXT("Hidden"), i ) == false)
			{
				VisibleEnumNameIndices.Add(MakeShareable(new int32(i)));
			}
		}

		SComboBox::Construct(SComboBox<TSharedPtr<int32>>::FArguments()
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Light")
			.OptionsSource(&VisibleEnumNameIndices)
			.OnGenerateWidget_Lambda([this](TSharedPtr<int32> InItem)
			{
				return SNew(STextBlock)
					.Text(Enum->GetDisplayNameText(*InItem));
			})
			.OnSelectionChanged(this, &SEnumCombobox::OnComboSelectionChanged)
			.OnComboBoxOpening(this, &SEnumCombobox::OnComboMenuOpening)
			.ContentPadding(FMargin(2, 0))
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
				.Text(this, &SEnumCombobox::GetCurrentValue)
			]);
	}

private:
	FText GetCurrentValue() const
	{
		if(IntermediateValue.IsSet() && IntermediateValue.Get().IsSet())
		{
			int32 IntermediateNameIndex = Enum->GetIndexByValue(IntermediateValue.Get().GetValue());
			return Enum->GetDisplayNameText(IntermediateNameIndex);
		}

		int32 CurrentNameIndex = Enum->GetIndexByValue(CurrentValue.Get());
		return Enum->GetDisplayNameText(CurrentNameIndex);
	}

	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<int32> InItem)
	{
		return SNew(STextBlock)
			.Text(Enum->GetDisplayNameText(*InItem));
	}

	void OnComboSelectionChanged(TSharedPtr<int32> InSelectedItem, ESelectInfo::Type SelectInfo)
	{
		if (bUpdatingSelectionInternally == false)
		{
			OnEnumSelectionChangedDelegate.ExecuteIfBound(*InSelectedItem, SelectInfo);
		}
	}

	void OnComboMenuOpening()
	{
		int32 CurrentNameIndex = Enum->GetIndexByValue(CurrentValue.Get());
		TSharedPtr<int32> FoundNameIndexItem;
		for ( int32 i = 0; i < VisibleEnumNameIndices.Num(); i++ )
		{
			if ( *VisibleEnumNameIndices[i] == CurrentNameIndex )
			{
				FoundNameIndexItem = VisibleEnumNameIndices[i];
				break;
			}
		}
		if ( FoundNameIndexItem.IsValid() )
		{
			bUpdatingSelectionInternally = true;
			SetSelectedItem(FoundNameIndexItem);
			bUpdatingSelectionInternally = false;
		}
	}	

private:
	const UEnum* Enum;

	TAttribute<int32> CurrentValue;

	TAttribute<TOptional<uint8>> IntermediateValue;

	TArray<TSharedPtr<int32>> VisibleEnumNameIndices;

	bool bUpdatingSelectionInternally;

	FOnEnumSelectionChanged OnEnumSelectionChangedDelegate;
};

TSharedRef<SWidget> MovieSceneToolHelpers::MakeEnumComboBox(const UEnum* InEnum, TAttribute<int32> InCurrentValue, FOnEnumSelectionChanged InOnSelectionChanged, TAttribute<TOptional<uint8>> InIntermediateValue)
{
	return SNew(SEnumCombobox, InEnum)
		.CurrentValue(InCurrentValue)
		.OnEnumSelectionChanged(InOnSelectionChanged)
		.IntermediateValue(InIntermediateValue);
}

bool MovieSceneToolHelpers::ShowImportEDLDialog(UMovieScene* InMovieScene, float InFrameRate, FString InOpenDirectory)
{
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		FString ExtensionStr;
		ExtensionStr += TEXT("CMX 3600 EDL (*.edl)|*.edl|");

		bOpen = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			NSLOCTEXT("UnrealEd", "MovieSceneToolHelpersImportEDL", "Import EDL from...").ToString(), 
			InOpenDirectory,
			TEXT(""), 
			*ExtensionStr,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}
	if (!bOpen)
	{
		return false;
	}

	if (!OpenFilenames.Num())
	{
		return false;
	}

	const FScopedTransaction Transaction( NSLOCTEXT( "MovieSceneTools", "ImportEDL", "Import EDL" ) );

	return MovieSceneCaptureHelpers::ImportEDL(InMovieScene, InFrameRate, OpenFilenames[0]);
}

bool MovieSceneToolHelpers::ShowExportEDLDialog(const UMovieScene* InMovieScene, float InFrameRate, FString InSaveDirectory)
{
	FString SequenceName = InMovieScene->GetOuter()->GetName();
	FString SequencePath = InMovieScene->GetOuter()->GetPathName();
	SequencePath = FPaths::GetPath(SequencePath);

	// Pop open a dialog to request the location of the edl
	FString SaveDirectory = InSaveDirectory;
	if (InSaveDirectory.IsEmpty())
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bSave = false;
		if (DesktopPlatform)
		{
			void* ParentWindowWindowHandle = NULL;

			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
			const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
			if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
			{
				ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
			}

			bSave = DesktopPlatform->OpenDirectoryDialog(
				ParentWindowWindowHandle,
				NSLOCTEXT("UnrealEd", "MovieSceneToolHelpersExportEDL", "Export EDL to...").ToString(), 
				SequencePath,
				SaveDirectory 
				);
		}
		if (!bSave)
		{
			return false;
		}
	}

	if (MovieSceneCaptureHelpers::ExportEDL(InMovieScene, InFrameRate, SaveDirectory))
	{
		FNotificationInfo NotificationInfo(NSLOCTEXT("MovieSceneTools", "EDLExportFinished", "EDL Export finished"));
		NotificationInfo.ExpireDuration = 5.f;
		NotificationInfo.bFireAndForget = true;
		NotificationInfo.Hyperlink = FSimpleDelegate::CreateStatic( [](FString InDirectory) { FPlatformProcess::ExploreFolder( *InDirectory ); }, SaveDirectory);
		NotificationInfo.HyperlinkText = NSLOCTEXT("MovieSceneTools", "OpenEDLExportFolder", "Open EDL Export Folder...");
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		
		return true;
	}

	return false;
}

bool MovieSceneToolHelpers::ImportFBX(UMovieScene* InMovieScene, FGuid InObjectBinding)
{
	/*
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		FString ExtensionStr;
		ExtensionStr += TEXT("FBX (*.fbx)|*.fbx|");

		bOpen = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			NSLOCTEXT("UnrealEd", "MovieSceneToolHelpersImportFBX", "Import FBX from...").ToString(), 
			FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT),
			TEXT(""), 
			*ExtensionStr,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}
	if (!bOpen)
	{
		return false;
	}

	if (!OpenFilenames.Num())
	{
		return false;
	}

	UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();

	const FString FileExtension = FPaths::GetExtension(OpenFilenames[0]);
	if (!FbxImporter->ImportFromFile(*OpenFilenames[0], FileExtension, true))
	{
		// Log the error message and fail the import.
		FbxImporter->ReleaseScene();
		return false;
	}

	const FScopedTransaction Transaction( NSLOCTEXT( "MovieSceneTools", "ImportFBX", "Import FBX" ) );

	//@todo Find transform tracks explicitly for now. This should be dynamic in the future
	UMovieScene3DTransformTrack* TransformTrack = InMovieScene->FindTrack<UMovieScene3DTransformTrack>(InObjectBinding); 
	if (!TransformTrack)
	{
		InMovieScene->Modify();
		TransformTrack = InMovieScene->AddTrack<UMovieScene3DTransformTrack>(InObjectBinding);
	}

	bool bSectionAdded = false;
	UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->FindOrAddSection(0.f, bSectionAdded));
	if (!TransformSection)
	{
		return false;
	}

	if (bSectionAdded)
	{
		TransformSection->SetIsInfinite(true);
	}

	float MinTime = FLT_MAX;
	float MaxTime = -FLT_MAX;

	UnFbx::FFbxCurvesAPI CurveAPI;
	FbxImporter->PopulateAnimatedCurveData(CurveAPI);
	TArray<FString> AnimatedNodeNames;
	CurveAPI.GetAnimatedNodeNameArray(AnimatedNodeNames);
	for (FString NodeName : AnimatedNodeNames)
	{
		FInterpCurveFloat Translation[3];
		FInterpCurveFloat EulerRotation[3];

		CurveAPI.GetConvertedTransformCurveData(NodeName, Translation[0], Translation[1], Translation[2], EulerRotation[0], EulerRotation[1], EulerRotation[2]);

		for (int32 CurveIndex = 0; CurveIndex < 2; ++CurveIndex)
		{
			for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
			{
				EAxis::Type ChannelAxis = EAxis::X;
				if (ChannelIndex == 1)
				{
					ChannelAxis = EAxis::Y;
				}
				else if (ChannelIndex == 2)
				{
					ChannelAxis = EAxis::Z;
				}
	
				FInterpCurveFloat* CurveFloat = nullptr;
				FRichCurve* ChannelCurve = nullptr;
				
				if (CurveIndex == 0)
				{
					CurveFloat = &Translation[ChannelIndex];
					ChannelCurve = &TransformSection->GetTranslationCurve(ChannelAxis);
				}
				else
				{
					CurveFloat = &EulerRotation[ChannelIndex];
					ChannelCurve = &TransformSection->GetRotationCurve(ChannelAxis);
				}

				if (ChannelCurve != nullptr && CurveFloat != nullptr)
				{
					ChannelCurve->Reset();
					for (int32 KeyIndex = 0; KeyIndex < CurveFloat->Points.Num(); ++KeyIndex)
					{
						MinTime = FMath::Min(MinTime, CurveFloat->Points[KeyIndex].InVal);
						MaxTime = FMath::Max(MaxTime, CurveFloat->Points[KeyIndex].InVal);
						FMatineeImportTools::SetOrAddKey(*ChannelCurve, CurveFloat->Points[KeyIndex].InVal, CurveFloat->Points[KeyIndex].OutVal, CurveFloat->Points[KeyIndex].ArriveTangent, CurveFloat->Points[KeyIndex].LeaveTangent, CurveFloat->Points[KeyIndex].InterpMode);
					}
				}
			}
		}
	}
		
	TransformSection->SetStartTime(MinTime);
	TransformSection->SetEndTime(MaxTime);
	
	FbxImporter->ReleaseScene();
	*/
	return true;
}
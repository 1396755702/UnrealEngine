// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "AnimationRecorder.h"
#include "Animation/AnimCompress.h"
#include "Animation/AnimCompress_BitwiseCompressOnly.h"
#include "SCreateAnimationDlg.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "EngineLogs.h"
#include "Animation/AnimationSettings.h"
#include "Toolkits/AssetEditorManager.h"
#include "ActorRecording.h"

#define LOCTEXT_NAMESPACE "FAnimationRecorder"

/////////////////////////////////////////////////////

static TAutoConsoleVariable<float> CVarAnimRecorderSampleRate(
	TEXT("AnimRecorder.SampleRate"),
	DEFAULT_SAMPLERATE,
	TEXT("Sets the sample rate for the animation recorder system"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAnimRecorderWorldSpace(
	TEXT("AnimRecorder.RecordInWorldSpace"),
	1,
	TEXT("True to record anim keys in world space, false to record only in local space."),
	ECVF_Default);

FAnimationRecorder::FAnimationRecorder()
	: AnimationObject(nullptr)
	, bRecordLocalToWorld(false)
	, bAutoSaveAsset(false)

{
	SetSampleRateAndLength(FAnimationRecordingSettings::DefaultSampleRate, FAnimationRecordingSettings::DefaultMaximumLength);
}

FAnimationRecorder::~FAnimationRecorder()
{
	StopRecord(false);
}

void FAnimationRecorder::SetSampleRateAndLength(float SampleRateHz, float LengthInSeconds)
{
	if (SampleRateHz <= 0.f)
	{
		// invalid rate passed in, fall back to default
		SampleRateHz = FAnimationRecordingSettings::DefaultSampleRate;
	}

	if (LengthInSeconds <= 0.f)
	{
		// invalid length passed in, default to unbounded
		SampleRateHz = FAnimationRecordingSettings::DefaultSampleRate;
	}

	IntervalTime = 1.0f / SampleRateHz;
	if (LengthInSeconds == FAnimationRecordingSettings::UnboundedMaximumLength)
	{
		// invalid length passed in, default to unbounded
		MaxFrame = UnBoundedFrameCount;
	}
	else
	{
		MaxFrame = SampleRateHz * LengthInSeconds;
	}
}

bool FAnimationRecorder::SetAnimCompressionScheme(TSubclassOf<UAnimCompress> SchemeClass)
{
	if (AnimationObject)
	{
		UAnimCompress* const SchemeObject = NewObject<UAnimCompress>(GetTransientPackage(), SchemeClass);
		if (SchemeObject)
		{
			AnimationObject->CompressionScheme = SchemeObject;
			return true;
		}
	}

	return false;
}

// Internal. Pops up a dialog to get saved asset path
static bool PromptUserForAssetPath(FString& AssetPath, FString& AssetName)
{
	TSharedRef<SCreateAnimationDlg> NewAnimDlg = SNew(SCreateAnimationDlg);
	if (NewAnimDlg->ShowModal() != EAppReturnType::Cancel)
	{
		AssetPath = NewAnimDlg->GetFullAssetPath();
		AssetName = NewAnimDlg->GetAssetName();
		return true;
	}

	return false;
}

bool FAnimationRecorder::TriggerRecordAnimation(USkeletalMeshComponent* Component)
{
	FString AssetPath;
	FString AssetName;

	if (!Component || !Component->SkeletalMesh || !Component->SkeletalMesh->Skeleton)
	{
		return false;
	}

	// ask for path
	if (PromptUserForAssetPath(AssetPath, AssetName))
	{
		return TriggerRecordAnimation(Component, AssetPath, AssetName);
	}

	return false;
}

bool FAnimationRecorder::TriggerRecordAnimation(USkeletalMeshComponent* Component, const FString& InAssetPath, const FString& InAssetName)
{
	if (!Component || !Component->SkeletalMesh || !Component->SkeletalMesh->Skeleton)
	{
		return false;
	}

	// create the asset
	FText InvalidPathReason;
	bool const bValidPackageName = FPackageName::IsValidLongPackageName(InAssetPath, false, &InvalidPathReason);
	if (bValidPackageName == false)
	{
		UE_LOG(LogAnimation, Log, TEXT("%s is an invalid asset path, prompting user for new asset path. Reason: %s"), *InAssetPath, *InvalidPathReason.ToString());
	}

	FString ValidatedAssetPath = InAssetPath;
	FString ValidatedAssetName = InAssetName;

	UObject* Parent = bValidPackageName ? CreatePackage(nullptr, *ValidatedAssetPath) : nullptr;
	if (Parent == nullptr)
	{
		// bad or no path passed in, do the popup
		if (PromptUserForAssetPath(ValidatedAssetPath, ValidatedAssetName) == false)
		{
			return false;
		}

		Parent = CreatePackage(nullptr, *ValidatedAssetPath);
	}

	UObject* const Object = LoadObject<UObject>(Parent, *ValidatedAssetName, nullptr, LOAD_None, nullptr);
	// if object with same name exists, warn user
	if (Object)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_AssetExist", "Asset with same name exists. Can't overwrite another asset"));
		return false;		// failed
	}

	// If not, create new one now.
	UAnimSequence* const NewSeq = NewObject<UAnimSequence>(Parent, *ValidatedAssetName, RF_Public | RF_Standalone);
	if (NewSeq)
	{
		// set skeleton
		NewSeq->SetSkeleton(Component->SkeletalMesh->Skeleton);
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(NewSeq);
		StartRecord(Component, NewSeq);

		return true;
	}

	return false;
}

/** Helper function to get space bases depending on master pose component */
static void GetBoneTransforms(USkeletalMeshComponent* Component, TArray<FTransform>& BoneTransforms)
{
	const USkinnedMeshComponent* const MasterPoseComponentInst = Component->MasterPoseComponent.Get();
	if(MasterPoseComponentInst)
	{
		const TArray<FTransform>& SpaceBases = MasterPoseComponentInst->GetSpaceBases();
		BoneTransforms.Reset(BoneTransforms.Num());
		BoneTransforms.AddUninitialized(SpaceBases.Num());
		for(int32 BoneIndex = 0; BoneIndex < SpaceBases.Num(); BoneIndex++)
		{
			if(BoneIndex < Component->GetMasterBoneMap().Num())
			{
				int32 MasterBoneIndex = Component->GetMasterBoneMap()[BoneIndex];

				// If ParentBoneIndex is valid, grab matrix from MasterPoseComponent.
				if(MasterBoneIndex != INDEX_NONE && MasterBoneIndex < SpaceBases.Num())
				{
					BoneTransforms[BoneIndex] = SpaceBases[MasterBoneIndex];
				}
				else
				{
					BoneTransforms[BoneIndex] = FTransform::Identity;
				}
			}
			else
			{
				BoneTransforms[BoneIndex] = FTransform::Identity;
			}
		}
	}
	else
	{
		BoneTransforms =  Component->GetSpaceBases();
	}
}

void FAnimationRecorder::StartRecord(USkeletalMeshComponent* Component, UAnimSequence* InAnimationObject)
{
	TimePassed = 0.f;
	AnimationObject = InAnimationObject;

	AnimationObject->RawAnimationData.Empty();
	AnimationObject->TrackToSkeletonMapTable.Empty();
	AnimationObject->AnimationTrackNames.Empty();

	GetBoneTransforms(Component, PreviousSpacesBases);
	PreviousComponentToWorld = Component->ComponentToWorld;

	LastFrame = 0;
	AnimationObject->SequenceLength = 0.f;
	AnimationObject->NumFrames = 0;

	USkeleton* AnimSkeleton = AnimationObject->GetSkeleton();
	// add all frames
	for (int32 BoneIndex=0; BoneIndex <PreviousSpacesBases.Num(); ++BoneIndex)
	{
		// verify if this bone exists in skeleton
		int32 BoneTreeIndex = AnimSkeleton->GetSkeletonBoneIndexFromMeshBoneIndex(Component->SkeletalMesh, BoneIndex);
		if (BoneTreeIndex != INDEX_NONE)
		{
			// add tracks for the bone existing
			FName BoneTreeName = AnimSkeleton->GetReferenceSkeleton().GetBoneName(BoneTreeIndex);
			AnimationObject->AnimationTrackNames.Add(BoneTreeName);
			// add mapping to skeleton bone track
			AnimationObject->TrackToSkeletonMapTable.Add(FTrackToSkeletonMap(BoneTreeIndex));
		}
	}

	int32 NumTracks = AnimationObject->AnimationTrackNames.Num();
	// add tracks
	AnimationObject->RawAnimationData.AddZeroed(NumTracks);

	// init notifies
	AnimationObject->InitializeNotifyTrack();

	// record the first frame
	Record(Component, PreviousComponentToWorld, PreviousSpacesBases, 0);
}

void FAnimationRecorder::FixupNotifies()
{
	if (AnimationObject)
	{
		// build notify tracks - first find how many tracks we want
		for (FAnimNotifyEvent& Event : AnimationObject->Notifies)
		{
			if (Event.TrackIndex >= AnimationObject->AnimNotifyTracks.Num())
			{
				AnimationObject->AnimNotifyTracks.SetNum(Event.TrackIndex + 1);

				// remake track names to create a nice sequence
				const int32 TrackNum = AnimationObject->AnimNotifyTracks.Num();
				for (int32 TrackIndex = 0; TrackIndex < TrackNum; ++TrackIndex)
				{
					FAnimNotifyTrack& Track = AnimationObject->AnimNotifyTracks[TrackIndex];
					Track.TrackName = *FString::FromInt(TrackIndex + 1);
				}
			}
		}

		// now build tracks
		for (int32 EventIndex = 0; EventIndex < AnimationObject->Notifies.Num(); ++EventIndex)
		{
			FAnimNotifyEvent& Event = AnimationObject->Notifies[EventIndex];
			AnimationObject->AnimNotifyTracks[Event.TrackIndex].Notifies.Add(&AnimationObject->Notifies[EventIndex]);
		}
	}
}

UAnimSequence* FAnimationRecorder::StopRecord(bool bShowMessage)
{
	if (AnimationObject)
	{
		int32 NumFrames = LastFrame  + 1;
		AnimationObject->NumFrames = NumFrames;

		// can't use TimePassed. That is just total time that has been passed, not necessarily match with frame count
		AnimationObject->SequenceLength = (NumFrames>1) ? (NumFrames-1) * IntervalTime : MINIMUM_ANIMATION_LENGTH;

		FixupNotifies();

		// force anim settings for speed, we dont want any fancy recompression at present
		UAnimationSettings* AnimationSettings = GetMutableDefault<UAnimationSettings>();
		TSubclassOf<UAnimCompress> OldDefaultCompressionAlgorithm = AnimationSettings->DefaultCompressionAlgorithm;
		TEnumAsByte<AnimationCompressionFormat> OldRotationCompressionFormat = AnimationSettings->RotationCompressionFormat;
		TEnumAsByte<AnimationCompressionFormat> OldTranslationCompressionFormat = AnimationSettings->TranslationCompressionFormat;

		AnimationSettings->DefaultCompressionAlgorithm = UAnimCompress_BitwiseCompressOnly::StaticClass();
		AnimationSettings->RotationCompressionFormat = ACF_None;
		AnimationSettings->TranslationCompressionFormat = ACF_None;

		// post-process applies compression etc.
		AnimationObject->PostProcessSequence();

		// restore old settings
		AnimationSettings->DefaultCompressionAlgorithm = OldDefaultCompressionAlgorithm;
		AnimationSettings->RotationCompressionFormat = OldRotationCompressionFormat;
		AnimationSettings->TranslationCompressionFormat = OldTranslationCompressionFormat;

		AnimationObject->MarkPackageDirty();
		
		// save the package to disk, for convenience and so we can run this in standalone mode
		if (bAutoSaveAsset)
		{
			UPackage* const Package = AnimationObject->GetOutermost();
			FString const PackageName = Package->GetName();
			FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

			UPackage::SavePackage(Package, NULL, RF_Standalone, *PackageFileName, GError, nullptr, false, true, SAVE_NoError);
		}

		UAnimSequence* ReturnObject = AnimationObject;

		// notify to user
		if (bShowMessage)
		{
			const FText NotificationText = FText::Format(LOCTEXT("RecordAnimation", "'{0}' has been successfully recorded [{1} frames : {2} sec(s) @ {3} Hz]"),
				FText::FromString(AnimationObject->GetName()),
				FText::AsNumber(AnimationObject->NumFrames),
				FText::AsNumber(AnimationObject->SequenceLength),
				FText::AsNumber(1.f / IntervalTime)
				);
					
			if (GIsEditor)
			{
				FNotificationInfo Info(NotificationText);
				Info.ExpireDuration = 8.0f;
				Info.bUseLargeFont = false;
				Info.Hyperlink = FSimpleDelegate::CreateLambda([=]()
				{
					TArray<UObject*> Assets;
					Assets.Add(ReturnObject);
					FAssetEditorManager::Get().OpenEditorForAssets(Assets);
				});
				Info.HyperlinkText = FText::Format(LOCTEXT("OpenNewAnimationHyperlink", "Open {0}"), FText::FromString(AnimationObject->GetName()));
				TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
				if ( Notification.IsValid() )
				{
					Notification->SetCompletionState( SNotificationItem::CS_Success );
				}
			}

			FAssetRegistryModule::AssetCreated(AnimationObject);
		}

		AnimationObject = NULL;
		PreviousSpacesBases.Empty();

		return ReturnObject;
	}

	UniqueNotifies.Empty();
	UniqueNotifyStates.Empty();

	return NULL;
}

void FAnimationRecorder::UpdateRecord(USkeletalMeshComponent* Component, float DeltaTime)
{
	// if no animation object, return
	if (!AnimationObject || !Component)
	{
		return;
	}

	// no sim time, no record
	if (DeltaTime <= 0.f)
	{
		return;
	}

	// in-editor we can get a long frame update because of the modal dialog used to pick paths
	if(DeltaTime > IntervalTime && (LastFrame == 0 || LastFrame == 1))
	{
		DeltaTime = IntervalTime;
	}

	float const PreviousTimePassed = TimePassed;
	TimePassed += DeltaTime;

	// time passed has been updated
	// now find what frames we need to update
	int32 FramesRecorded = LastFrame;
	int32 FramesToRecord = FPlatformMath::TruncToInt(TimePassed / IntervalTime);

	// notifies need to be done regardless of sample rate
	if (Component->GetAnimInstance())
	{
		RecordNotifies(Component, Component->GetAnimInstance()->NotifyQueue.AnimNotifies, DeltaTime, TimePassed);
	}

	TArray<FTransform> SpaceBases;
	GetBoneTransforms(Component, SpaceBases);

	if (FramesRecorded < FramesToRecord)
	{
		check(SpaceBases.Num() == PreviousSpacesBases.Num());

		TArray<FTransform> BlendedSpaceBases;
		BlendedSpaceBases.AddZeroed(SpaceBases.Num());

		UE_LOG(LogAnimation, Log, TEXT("DeltaTime : %0.2f, Current Frame Count : %d, Frames To Record : %d, TimePassed : %0.2f"), DeltaTime
			, FramesRecorded, FramesToRecord, TimePassed);

		// if we need to record frame
		while (FramesToRecord > FramesRecorded)
		{
			// find what frames we need to record
			// convert to time
			float CurrentTime = (FramesRecorded + 1) * IntervalTime;
			float BlendAlpha = (CurrentTime - PreviousTimePassed) / DeltaTime;

			UE_LOG(LogAnimation, Log, TEXT("Current Frame Count : %d, BlendAlpha : %0.2f"), FramesRecorded + 1, BlendAlpha);

			// for now we just concern component space, not skeleton space
			for (int32 BoneIndex = 0; BoneIndex<SpaceBases.Num(); ++BoneIndex)
			{
				BlendedSpaceBases[BoneIndex].Blend(PreviousSpacesBases[BoneIndex], SpaceBases[BoneIndex], BlendAlpha);
			}

			FTransform BlendedComponentToWorld;
			BlendedComponentToWorld.Blend(PreviousComponentToWorld, Component->ComponentToWorld, BlendAlpha);

			Record(Component, BlendedComponentToWorld, BlendedSpaceBases, FramesRecorded + 1);
			++FramesRecorded;
		}
	}

	//save to current transform
	PreviousSpacesBases = SpaceBases;
	PreviousComponentToWorld = Component->ComponentToWorld;

	// if we passed MaxFrame, just stop it
	if (MaxFrame != UnBoundedFrameCount && FramesRecorded >= MaxFrame)
	{
		UE_LOG(LogAnimation, Log, TEXT("Animation Recording exceeds the time limited (%d mins). Stopping recording animation... "), (int32)((float)MaxFrame / ((1.0f / IntervalTime) * 60.0f)));
		StopRecord(true);
	}
}

void FAnimationRecorder::Record( USkeletalMeshComponent* Component, FTransform const& ComponentToWorld, const TArray<FTransform>& SpacesBases, int32 FrameToAdd )
{
	if (ensure(AnimationObject))
	{
		USkeleton* AnimSkeleton = AnimationObject->GetSkeleton();
		for (int32 TrackIndex=0; TrackIndex <AnimationObject->RawAnimationData.Num(); ++TrackIndex)
		{
			// verify if this bone exists in skeleton
			int32 BoneTreeIndex = AnimationObject->GetSkeletonIndexFromTrackIndex(TrackIndex);
			if (BoneTreeIndex != INDEX_NONE)
			{
				int32 BoneIndex = AnimSkeleton->GetMeshBoneIndexFromSkeletonBoneIndex(Component->SkeletalMesh, BoneTreeIndex);
				int32 ParentIndex = Component->SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
				FTransform LocalTransform = SpacesBases[BoneIndex];
				if ( ParentIndex != INDEX_NONE )
				{
					LocalTransform.SetToRelativeTransform(SpacesBases[ParentIndex]);
				}
				// if record local to world, we'd like to consider component to world to be in root
				else if (bRecordLocalToWorld)
				{
					LocalTransform *= ComponentToWorld;
				}

				FRawAnimSequenceTrack& RawTrack = AnimationObject->RawAnimationData[TrackIndex];
				RawTrack.PosKeys.Add(LocalTransform.GetTranslation());
				RawTrack.RotKeys.Add(LocalTransform.GetRotation());
				RawTrack.ScaleKeys.Add(LocalTransform.GetScale3D());

				// verification
				check (FrameToAdd == RawTrack.PosKeys.Num()-1);
			}
		}

		LastFrame = FrameToAdd;
	}
}

void FAnimationRecorder::RecordNotifies(USkeletalMeshComponent* Component, const TArray<const struct FAnimNotifyEvent*>& AnimNotifies, float DeltaTime, float RecordTime)
{
	if (ensure(AnimationObject))
	{
		// flag notifies as possibly unused this frame
		for (auto& ActiveNotify : ActiveNotifies)
		{
			ActiveNotify.Value = false;
		}

		int32 AddedThisFrame = 0;
		for(const FAnimNotifyEvent* NotifyEvent : AnimNotifies)
		{
			// we don't want to insert notifies with duration more than once
			if(NotifyEvent->GetDuration() > 0.0f)
			{
				// if this event is active already then don't add it
				bool bAlreadyActive = false;
				for(auto& ActiveNotify : ActiveNotifies)
				{
					if(NotifyEvent == ActiveNotify.Key)
					{
						// flag as active
						ActiveNotify.Value = true;
						bAlreadyActive = true;
						break;
					}
				}

				// already active, so skip adding
				if(bAlreadyActive)
				{
					continue;
				}
				else
				{
					// add a new active notify with duration
					ActiveNotifies.Add(TPairInitializer<const FAnimNotifyEvent*, bool>(NotifyEvent, true));
				}
			}

			// make a new notify from this event & set the current time
			FAnimNotifyEvent NewEvent = *NotifyEvent;
			NewEvent.SetTime(RecordTime);

			// see if we need to create a new notify
			if(NotifyEvent->Notify)
			{
				UAnimNotify** FoundNotify = UniqueNotifies.Find(NotifyEvent->Notify);
				if(FoundNotify == nullptr)
				{
					NewEvent.Notify = Cast<UAnimNotify>(StaticDuplicateObject(NewEvent.Notify, AnimationObject));
					UniqueNotifies.Add(NotifyEvent->Notify, NewEvent.Notify);
				}
				else
				{
					NewEvent.Notify = *FoundNotify;
				}
			}

			// see if we need to create a new notify state
			if (NotifyEvent->NotifyStateClass)
			{
				UAnimNotifyState** FoundNotifyState = UniqueNotifyStates.Find(NotifyEvent->NotifyStateClass);
				if (FoundNotifyState == nullptr)
				{
					NewEvent.NotifyStateClass = Cast<UAnimNotifyState>(StaticDuplicateObject(NewEvent.NotifyStateClass, AnimationObject));
					UniqueNotifyStates.Add(NotifyEvent->NotifyStateClass, NewEvent.NotifyStateClass);
				}
				else
				{
					NewEvent.NotifyStateClass = *FoundNotifyState;
				}
			}

			AnimationObject->Notifies.Add(NewEvent);
			AddedThisFrame++;
		}

		// remove all notifies that didnt get added this time
		ActiveNotifies.RemoveAll([](TPair<const FAnimNotifyEvent*, bool>& ActiveNotify){ return !ActiveNotify.Value; });

		UE_LOG(LogAnimation, Log, TEXT("Added notifies : %d"), AddedThisFrame);
	}
}

void FAnimationRecorderManager::Tick(float DeltaTime)
{
	for (auto& Inst : RecorderInstances)
	{
		Inst.Update(DeltaTime);
	}
}

void FAnimationRecorderManager::Tick(USkeletalMeshComponent* Component, float DeltaTime)
{
	for (auto& Inst : RecorderInstances)
	{
		if(Inst.SkelComp == Component)
		{
			Inst.Update(DeltaTime);
		}
	}
}

FAnimationRecorderManager::FAnimationRecorderManager()
{
}

FAnimationRecorderManager::~FAnimationRecorderManager()
{
}

FAnimationRecorderManager& FAnimationRecorderManager::Get()
{
	static FAnimationRecorderManager AnimRecorderManager;
	return AnimRecorderManager;
}


FAnimRecorderInstance::FAnimRecorderInstance()
	: SkelComp(nullptr)
	, Recorder(nullptr)
{
}

void FAnimRecorderInstance::Init(USkeletalMeshComponent* InComponent, const FString& InAssetPath, const FString& InAssetName, float SampleRateHz, float MaxLength, bool bRecordInWorldSpace, bool bAutoSaveAsset)
{
	SkelComp = InComponent;
	AssetPath = InAssetPath;
	AssetName = InAssetName;
	Recorder = MakeShareable(new FAnimationRecorder());
	Recorder->SetSampleRateAndLength(SampleRateHz, MaxLength);
	Recorder->bRecordLocalToWorld = bRecordInWorldSpace;
	Recorder->SetAnimCompressionScheme(UAnimCompress_BitwiseCompressOnly::StaticClass());
	Recorder->bAutoSaveAsset = bAutoSaveAsset;

	if (InComponent)
	{
		CachedSkelCompForcedLodModel = InComponent->ForcedLodModel;
		InComponent->ForcedLodModel = 1;
	}
}

void FAnimRecorderInstance::Init(USkeletalMeshComponent* InComponent, UAnimSequence* InSequence, float SampleRateHz, float MaxLength, bool bRecordInWorldSpace, bool bAutoSaveAsset)
{
	SkelComp = InComponent;
	Sequence = InSequence;
	Recorder = MakeShareable(new FAnimationRecorder());
	Recorder->SetSampleRateAndLength(SampleRateHz, MaxLength);
	Recorder->bRecordLocalToWorld = bRecordInWorldSpace;
	Recorder->SetAnimCompressionScheme(UAnimCompress_BitwiseCompressOnly::StaticClass());
	Recorder->bAutoSaveAsset = bAutoSaveAsset;

	if (InComponent)
	{
		CachedSkelCompForcedLodModel = InComponent->ForcedLodModel;
		InComponent->ForcedLodModel = 1;
	}
}

FAnimRecorderInstance::~FAnimRecorderInstance()
{
}

bool FAnimRecorderInstance::BeginRecording()
{
	if (Recorder.IsValid())
	{
		if(Sequence.IsValid())
		{
			Recorder->StartRecord(SkelComp.Get(), Sequence.Get());
			return true;
		}
		else
		{
			return Recorder->TriggerRecordAnimation(SkelComp.Get(), AssetPath, AssetName);
		}
	}

	return false;
}

void FAnimRecorderInstance::Update(float DeltaTime)
{
	if (Recorder.IsValid())
	{
		Recorder->UpdateRecord(SkelComp.Get(), DeltaTime);
	}
}

void FAnimRecorderInstance::FinishRecording()
{
	const FText FinishRecordingAnimationSlowTask = LOCTEXT("FinishRecordingAnimationSlowTask", "Finalizing recorded animation");
	if (Recorder.IsValid())
	{
		Recorder->StopRecord(true);
	}

	// restore force lod setting
	if (SkelComp.IsValid())
	{
		SkelComp->ForcedLodModel = CachedSkelCompForcedLodModel;
	}
}

bool FAnimationRecorderManager::RecordAnimation(USkeletalMeshComponent* Component, const FString& AssetPath, const FString& AssetName, const FAnimationRecordingSettings& Settings)
{
	if (Component)
	{
		FAnimRecorderInstance NewInst;
		NewInst.Init(Component, AssetPath, AssetName, Settings.SampleRate, Settings.Length, Settings.bRecordInWorldSpace, Settings.bAutoSaveAsset);
		bool const bSuccess = NewInst.BeginRecording();
		if (bSuccess)
		{
			RecorderInstances.Add(NewInst);
		}

	#if WITH_EDITOR
			// if recording via PIE, be sure to stop recording cleanly when PIE ends
			UWorld const* const World = Component->GetWorld();
			if (World && World->IsPlayInEditor())
			{
				FEditorDelegates::EndPIE.AddRaw(this, &FAnimationRecorderManager::HandleEndPIE);
			}
	#endif

		return bSuccess;
	}

	return false;
}

bool FAnimationRecorderManager::RecordAnimation(USkeletalMeshComponent* Component, UAnimSequence* Sequence, const FAnimationRecordingSettings& Settings)
{
	if (Component)
	{
		FAnimRecorderInstance NewInst;
		NewInst.Init(Component, Sequence, Settings.SampleRate, Settings.Length, Settings.bRecordInWorldSpace, Settings.bAutoSaveAsset);
		bool const bSuccess = NewInst.BeginRecording();
		if (bSuccess)
		{
			RecorderInstances.Add(NewInst);
		}

	#if WITH_EDITOR
			// if recording via PIE, be sure to stop recording cleanly when PIE ends
			UWorld const* const World = Component->GetWorld();
			if (World && World->IsPlayInEditor())
			{
				FEditorDelegates::EndPIE.AddRaw(this, &FAnimationRecorderManager::HandleEndPIE);
			}
	#endif

		return bSuccess;
	}

	return false;
}

void FAnimationRecorderManager::HandleEndPIE(bool bSimulating)
{
	StopRecordingAllAnimations();
}

bool FAnimationRecorderManager::IsRecording(USkeletalMeshComponent* Component)
{
	for (FAnimRecorderInstance& Instance : RecorderInstances)
	{
		if (Instance.SkelComp == Component)
		{
			return Instance.Recorder->InRecording();
		}
	}

	return false;
}

bool FAnimationRecorderManager::IsRecording()
{
	for (FAnimRecorderInstance& Instance : RecorderInstances)
	{
		if (Instance.Recorder->InRecording())
		{
			return true;
		}
	}

	return false;
}

UAnimSequence* FAnimationRecorderManager::GetCurrentlyRecordingSequence(USkeletalMeshComponent* Component)
{
	for (FAnimRecorderInstance& Instance : RecorderInstances)
	{
		if (Instance.SkelComp == Component)
		{
			return Instance.Recorder->GetAnimationObject();
		}
	}

	return nullptr;
}

float FAnimationRecorderManager::GetCurrentRecordingTime(USkeletalMeshComponent* Component)
{
	for (FAnimRecorderInstance& Instance : RecorderInstances)
	{
		if (Instance.SkelComp == Component)
		{
			return Instance.Recorder->GetTimeRecorded();
		}
	}

	return 0.0f;
}

void FAnimationRecorderManager::StopRecordingAnimation(USkeletalMeshComponent* Component)
{
	for (int32 Idx = 0; Idx < RecorderInstances.Num(); ++Idx)
	{
		FAnimRecorderInstance& Inst = RecorderInstances[Idx];
		if (Inst.SkelComp == Component)
		{
			// stop and finalize recoded data
			Inst.FinishRecording();

			// remove instance, which will clean itself up
			RecorderInstances.RemoveAtSwap(Idx);

			// all done
			break;
		}
	}
}

void FAnimationRecorderManager::StopRecordingDeadAnimations()
{
	RecorderInstances.RemoveAll([](FAnimRecorderInstance& Instance)
		{
			if (!Instance.SkelComp.IsValid())
			{
				// stop and finalize recorded data
				Instance.FinishRecording();

				// make sure we are cleaned up
				return true;
			}

			return false;
		}
	);
}

void FAnimationRecorderManager::StopRecordingAllAnimations()
{
	for (auto& Inst : RecorderInstances)
	{
		Inst.FinishRecording();
	}

	RecorderInstances.Empty();
}

#undef LOCTEXT_NAMESPACE


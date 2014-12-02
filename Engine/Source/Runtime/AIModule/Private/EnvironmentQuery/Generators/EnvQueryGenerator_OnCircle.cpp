// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_OnCircle.h"
#include "AI/Navigation/RecastNavMesh.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

struct FBatchTracingHelper
{
	UWorld* World;
	enum ECollisionChannel Channel;
	const FCollisionQueryParams Params;
	const FVector Extent;

	FBatchTracingHelper(UWorld* InWorld, enum ECollisionChannel InChannel, const FCollisionQueryParams& InParams, const FVector& InExtent)
		: World(InWorld), Channel(InChannel), Params(InParams), Extent(InExtent)
	{

	}

	FORCEINLINE_DEBUGGABLE FVector RunLineTrace(const FVector& StartPos, const FVector& EndPos)
	{
		FHitResult OutHit;
		const bool bHit = World->LineTraceSingle(OutHit, StartPos, EndPos, Channel, Params);
		return bHit ? OutHit.ImpactPoint : EndPos;
	}

	FORCEINLINE_DEBUGGABLE FVector RunSphereTrace(const FVector& StartPos, const FVector& EndPos)
	{
		FHitResult OutHit;
		const bool bHit = World->SweepSingle(OutHit, StartPos, EndPos, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Extent.X), Params);
		return bHit ? OutHit.ImpactPoint : EndPos;
	}

	FORCEINLINE_DEBUGGABLE FVector RunCapsuleTrace(const FVector& StartPos, const FVector& EndPos)
	{
		FHitResult OutHit;
		const bool bHit = World->SweepSingle(OutHit, StartPos, EndPos, FQuat::Identity, Channel, FCollisionShape::MakeCapsule(Extent.X, Extent.Z), Params);
		return bHit ? OutHit.ImpactPoint : EndPos;
	}

	FORCEINLINE_DEBUGGABLE FVector RunBoxTrace(const FVector& StartPos, const FVector& EndPos)
	{
		FHitResult OutHit;
		const bool bHit = World->SweepSingle(OutHit, StartPos, EndPos, FQuat((EndPos - StartPos).Rotation()), Channel, FCollisionShape::MakeBox(Extent), Params);
		return bHit ? OutHit.ImpactPoint : EndPos;
	}

	template<EEnvTraceShape::Type TraceType>
	void DoSingleSourceMultiDestinations(const FVector& Source, TArray<FVector>& DestinationAndClampedResult)
	{
		UE_LOG(LogEQS, Error, TEXT("FBatchTracingHelper::DoSingleSourceMultiDestinations called with unhandled trace type: %d"), int32(TraceType));
	}
};

template<>
void FBatchTracingHelper::DoSingleSourceMultiDestinations<EEnvTraceShape::Line>(const FVector& Source, TArray<FVector>& DestinationAndClampedResult)
{
	for (auto& OutLocation : DestinationAndClampedResult)
	{
		OutLocation = RunLineTrace(Source, OutLocation);
	}
}

template<>
void FBatchTracingHelper::DoSingleSourceMultiDestinations<EEnvTraceShape::Box>(const FVector& Source, TArray<FVector>& DestinationAndClampedResult)
{
	for (auto& OutLocation : DestinationAndClampedResult)
	{
		OutLocation = RunBoxTrace(Source, OutLocation);
	}
}

template<>
void FBatchTracingHelper::DoSingleSourceMultiDestinations<EEnvTraceShape::Sphere>(const FVector& Source, TArray<FVector>& DestinationAndClampedResult)
{
	for (auto& OutLocation : DestinationAndClampedResult)
	{
		OutLocation = RunSphereTrace(Source, OutLocation);
	}
}

template<>
void FBatchTracingHelper::DoSingleSourceMultiDestinations<EEnvTraceShape::Capsule>(const FVector& Source, TArray<FVector>& DestinationAndClampedResult)
{
	for (auto& OutLocation : DestinationAndClampedResult)
	{
		OutLocation = RunCapsuleTrace(Source, OutLocation);
	}
}

//----------------------------------------------------------------------//
// UEnvQueryGenerator_OnCircle
//----------------------------------------------------------------------//
UEnvQueryGenerator_OnCircle::UEnvQueryGenerator_OnCircle(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	CircleCenter = UEnvQueryContext_Querier::StaticClass();
	ItemType = UEnvQueryItemType_Point::StaticClass();
	Radius.Value = 1000.0f;
	ItemSpacing.Value = 50.0f;
	ArcDirection.DirMode = EEnvDirection::TwoPoints;
	ArcDirection.LineFrom = UEnvQueryContext_Querier::StaticClass();
	ArcDirection.Rotation = UEnvQueryContext_Querier::StaticClass();
	Angle.Value = 360.f;
	AngleRadians = FMath::DegreesToRadians(360.f);

	TraceData.bCanProjectDown = false;
	TraceData.bCanDisableTrace = true;
	TraceData.TraceMode = EEnvQueryTrace::Navigation;

	ProjectionData.TraceMode = EEnvQueryTrace::None;
}

void UEnvQueryGenerator_OnCircle::PostLoad()
{
	Super::PostLoad();

	if (Angle.Value <= 0)
	{
		Angle.Value = 360.f;
	}
	AngleRadians = FMath::DegreesToRadians(Angle.Value);
}

FVector UEnvQueryGenerator_OnCircle::CalcDirection(FEnvQueryInstance& QueryInstance) const
{
	AActor* Querier = Cast<AActor>(QueryInstance.Owner.Get());
	check(Querier != NULL);

	FVector Direction;
	if (bDefineArc)
	{
		// By default, use Querier rotation for arc direction.
		Direction = Querier->GetActorRotation().Vector();
		if (ArcDirection.DirMode == EEnvDirection::TwoPoints)
		{
			TArray<FVector> Start;
			TArray<FVector> End;
			QueryInstance.PrepareContext(ArcDirection.LineFrom, Start);
			QueryInstance.PrepareContext(ArcDirection.LineTo, End);

			if (Start.Num() > 0 && End.Num() > 0)
			{
				Direction = (End[0] - Start[0]).GetSafeNormal();
			}
			else
			{
				UE_VLOG(Querier, LogEQS, Warning, TEXT("UEnvQueryGenerator_OnCircle::CalcDirection failed to calc direction in %s. Using querier facing."), *QueryInstance.QueryName);
			}
		}
		else
		{
			TArray<FRotator> Rot;
			QueryInstance.PrepareContext(ArcDirection.Rotation, Rot);

			if (Rot.Num() > 0)
			{
				Direction = Rot[0].Vector();
			}
			else
			{
				UE_VLOG(Querier, LogEQS, Warning, TEXT("UEnvQueryGenerator_OnCircle::CalcDirection failed to calc direction in %s. Using querier facing."), *QueryInstance.QueryName);
			}
		}
	}
	else
	{	// Don't rotate based on querier!  Instead, use a stable rotation so the points on the circle don't rotate!
		Direction = FVector(1, 0, 0);
	}

	return Direction;
}

void UEnvQueryGenerator_OnCircle::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	float AngleDegree = 360.f;
	float RadiusValue = 0.f;
	float ItemSpace = 1.0f;

	if (QueryInstance.GetParamValue(Angle, AngleDegree, TEXT("Angle")) == false
		|| QueryInstance.GetParamValue(Radius, RadiusValue, TEXT("Radius")) == false
		|| QueryInstance.GetParamValue(ItemSpacing, ItemSpace, TEXT("ItemSpacing")) == false
		|| (AngleDegree <= 0.f) || (AngleDegree > 360.f)
		|| (RadiusValue <= 0.f)
		|| (ItemSpace <= 0.f)
	   )
	{
		return;
	}

	AngleRadians = FMath::DegreesToRadians(Angle.Value);

	// first generate points on a circle
	const float CircumferenceLength = 2.f * PI * RadiusValue;
	const float ArcAnglePercentage = Angle.Value / 360.f;
	const float ArcLength = CircumferenceLength * ArcAnglePercentage;
	const int32 StepsCount = FMath::CeilToInt(ArcLength / ItemSpace) + 1;
	const float AngleStep = AngleDegree / (StepsCount - 1);

	FVector StartDirection = CalcDirection(QueryInstance);
	StartDirection = StartDirection.RotateAngleAxis(-AngleDegree/2, FVector::UpVector) * RadiusValue;
	
	TSharedPtr<FVectorAndDataContainer> CenterLocationCandidates;
	PrepareGeneratorContext(QueryInstance, CenterLocationCandidates);
	check(CenterLocationCandidates.Get() != NULL);

	int32 NumCenterLocations = CenterLocationCandidates.Get()->GetNumEntries();
	if (NumCenterLocations > 0)
	{
		for (int i = 0; i < NumCenterLocations; ++i)
		{
			GenerateItemsForCircle(CenterLocationCandidates, i, CenterLocationCandidates.Get()->GetVector(i),
									StartDirection, StepsCount, AngleStep, QueryInstance);
		}
	}
// 	else
// 	{
// 		// No center locations were generated, so we can't generate any item locations based on those centers.
// 		// This is a perfectly valid outcome though, so don't spam the log!
// // 		UE_LOG(LogEQS, Warning,
// // 			TEXT("Unable to generate any center location candidates at all, Generate Items has failed.  %s in %s."),
// // 			*GetName(), *QueryInstance.QueryName);
// 	}
}

void UEnvQueryGenerator_OnCircle::PrepareGeneratorContext(FEnvQueryInstance& QueryInstance,
														  TSharedPtr<FVectorAndDataContainer>& CenterLocationCandidates) const
{
	FVectorAndDataContainer* CandidateData = CenterLocationCandidates.Get();
	if (CandidateData == NULL)
	{	// Make sure we have the data holder!
		CenterLocationCandidates = MakeShareable(new FVectorAndDataContainer);
		CandidateData = CenterLocationCandidates.Get();
	}
	else // We already had it... so make sure it's clear to start, just in case we need to clear old data.
	{
		CandidateData->Reset();
	}
	check(CandidateData != NULL);

	// Now use the generic vector context preparation.
	QueryInstance.PrepareContext(CircleCenter, CandidateData->GetLocationArray());

	// If there are no entries, make a fake one based on the querier.
	if (CandidateData->GetNumEntries() < 1)
	{	
		UE_LOG(LogEQS, Warning, TEXT("No context locations found, using querier as context for %s in instance %s"), *GetName(), *QueryInstance.QueryName);
		AActor* Querier = Cast<AActor>(QueryInstance.Owner.Get());
		if (Querier != NULL)
		{
			FVector CenterLocation = Querier->GetActorLocation();
			CandidateData->GetLocationArray().Add(CenterLocation);
		}
	}
}

void UEnvQueryGenerator_OnCircle::GenerateItemsForCircle(TSharedPtr<FVectorAndDataContainer>& CenterLocationCandidates,
	int32 CenterLocationCandidateIndex,
	const FVector& CenterLocation, const FVector& StartDirection,
	int32 StepsCount, float AngleStep, FEnvQueryInstance& OutQueryInstance) const
{
	TArray<FVector> ItemCandidates;
	ItemCandidates.AddZeroed(StepsCount);
	for (int32 Step = 0; Step < StepsCount; ++Step)
	{
		ItemCandidates[Step] = CenterLocation + StartDirection.RotateAngleAxis(AngleStep*Step, FVector::UpVector);
	}

#if WITH_RECAST
	// @todo this needs to be optimized to batch raycasts
	const ARecastNavMesh* NavMesh = 
		(TraceData.TraceMode == EEnvQueryTrace::Navigation) || (ProjectionData.TraceMode == EEnvQueryTrace::Navigation) ?
		FEQSHelpers::FindNavMeshForQuery(OutQueryInstance) : NULL;

	if (NavMesh)
	{
		NavMesh->BeginBatchQuery();
	}
#endif

	switch (TraceData.TraceMode)
	{
		case EEnvQueryTrace::Navigation:
#if WITH_RECAST
			if (NavMesh != NULL)
			{
				TSharedPtr<const FNavigationQueryFilter> NavigationFilter = UNavigationQueryFilter::GetQueryFilter(NavMesh, TraceData.NavigationFilter);

				TArray<FNavigationRaycastWork> RaycastWorkload;
				RaycastWorkload.Reserve(ItemCandidates.Num());

				for (const auto& ItemLocation : ItemCandidates)
				{
					RaycastWorkload.Add(FNavigationRaycastWork(CenterLocation, ItemLocation));
				}

				NavMesh->BatchRaycast(RaycastWorkload, NavigationFilter);
			
				for (int32 ItemIndex = 0; ItemIndex < ItemCandidates.Num(); ++ItemIndex)
				{
					ItemCandidates[ItemIndex] = RaycastWorkload[ItemIndex].HitLocation.Location;
				}
			}
#endif
			break;

		case EEnvQueryTrace::Geometry:
		{
			ECollisionChannel TraceCollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceData.TraceChannel);
			FVector TraceExtent(TraceData.ExtentX, TraceData.ExtentY, TraceData.ExtentZ);

			FCollisionQueryParams TraceParams(TEXT("EnvQueryTrace"), TraceData.bTraceComplex);
			TraceParams.bTraceAsyncScene = true;

			FBatchTracingHelper TracingHelper(OutQueryInstance.World, TraceCollisionChannel, TraceParams, TraceExtent);

			switch (TraceData.TraceShape)
			{
				case EEnvTraceShape::Line:		
					TracingHelper.DoSingleSourceMultiDestinations<EEnvTraceShape::Line>(CenterLocation, ItemCandidates);
					break;

				case EEnvTraceShape::Sphere:	
					TracingHelper.DoSingleSourceMultiDestinations<EEnvTraceShape::Sphere>(CenterLocation, ItemCandidates);
					break;

				case EEnvTraceShape::Capsule:
					TracingHelper.DoSingleSourceMultiDestinations<EEnvTraceShape::Capsule>(CenterLocation, ItemCandidates);
					break;

				case EEnvTraceShape::Box:
					TracingHelper.DoSingleSourceMultiDestinations<EEnvTraceShape::Box>(CenterLocation, ItemCandidates);
					break;

				default:
					UE_VLOG(Cast<AActor>(OutQueryInstance.Owner.Get()), LogEQS, Warning, TEXT("UEnvQueryGenerator_OnCircle::CalcDirection failed to calc direction in %s. Using querier facing."), *OutQueryInstance.QueryName);
					break;
			}
		}
			break;

		case EEnvQueryTrace::None:
			// Just accept the ItemCandidates as they already are (points on a circle), without using navigation OR collision.
			break;

		default:
			UE_VLOG(Cast<AActor>(OutQueryInstance.Owner.Get()), LogEQS, Warning, TEXT("UEnvQueryGenerator_OnCircle::CalcDirection has invalid value for TraceData.TraceMode.  Query: %s"), *OutQueryInstance.QueryName);
			break;
	}

#if WITH_RECAST
	if (NavMesh)
	{
		ProjectAndFilterNavPoints(ItemCandidates, NavMesh);
		NavMesh->FinishBatchQuery();
	}
#endif

	AddItemDataForCircle(ItemCandidates, CenterLocationCandidates, CenterLocationCandidateIndex, OutQueryInstance);
}

void UEnvQueryGenerator_OnCircle::AddItemDataForCircle(const TArray<FVector>& ItemCandidates,
													   TSharedPtr<FVectorAndDataContainer>& CenterLocationCandidates,
													   int32 CenterLocationCandidateIndex,
													   FEnvQueryInstance& OutQueryInstance) const
{
	for (int32 Step = 0; Step < ItemCandidates.Num(); ++Step)
	{
		OutQueryInstance.AddItemData<UEnvQueryItemType_Point>(ItemCandidates[Step]);
	}
}

FText UEnvQueryGenerator_OnCircle::GetDescriptionTitle() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("DescriptionTitle"), Super::GetDescriptionTitle());
	Args.Add(TEXT("DescribeContext"), UEnvQueryTypes::DescribeContext(CircleCenter));

	return FText::Format(LOCTEXT("DescriptionGenerateCircleAroundContext", "{DescriptionTitle}: generate items on circle around {DescribeContext}"), Args);
}

FText UEnvQueryGenerator_OnCircle::GetDescriptionDetails() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Radius"), FText::FromString(UEnvQueryTypes::DescribeFloatParam(Radius)));
	Args.Add(TEXT("ItemSpacing"), FText::FromString(UEnvQueryTypes::DescribeFloatParam(ItemSpacing)));
	Args.Add(TEXT("TraceData"), TraceData.ToText(FEnvTraceData::Detailed));

	FText Desc = FText::Format(LOCTEXT("OnCircleDescription", "radius: {Radius}, item span: {ItemSpacing}\n{TraceData}"), Args);

	if (bDefineArc)
	{
		FFormatNamedArguments ArcArgs;
		ArcArgs.Add(TEXT("Description"), Desc);
		ArcArgs.Add(TEXT("AngleValue"), Angle.Value);
		ArcArgs.Add(TEXT("ArcDirection"), ArcDirection.ToText());
		Desc = FText::Format(LOCTEXT("DescriptionWithArc", "{Description}\nLimit to {AngleValue} angle both sides on {ArcDirection}"), ArcArgs);
	}

	FText ProjDesc = ProjectionData.ToText(FEnvTraceData::Brief);
	if (!ProjDesc.IsEmpty())
	{
		FFormatNamedArguments ProjArgs;
		ProjArgs.Add(TEXT("Description"), Desc);
		ProjArgs.Add(TEXT("ProjectionDescription"), ProjDesc);
		Desc = FText::Format(LOCTEXT("OnCircle_DescriptionWithProjection", "{Description}, {ProjectionDescription}"), ProjArgs);
	}

	return Desc;
}

#if WITH_EDITOR

void UEnvQueryGenerator_OnCircle::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent) 
{
	static const FName NAME_Angle = GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_OnCircle, Angle);
	static const FName NAME_Radius = GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_OnCircle, Radius);

	if (PropertyChangedEvent.Property != NULL)
	{
		const FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropName == NAME_Angle)
		{
			Angle.Value = FMath::Clamp(Angle.Value, 0.0f, 360.f);
			AngleRadians = FMath::DegreesToRadians(Angle.Value);
			bDefineArc = Angle.Value < 360.f && Angle.Value > 0.f;
		}
		else if (PropName == NAME_Radius)
		{
			if (Radius.Value <= 0.f)
			{
				Radius.Value = 100.f;
			}
		}

		ItemSpacing.Value = FMath::Max(1.0f, ItemSpacing.Value);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProceduralFoliageActor.generated.h"

class UProceduralFoliageComponent;

UCLASS()
class FOLIAGE_API AProceduralFoliageActor: public AVolume
{
	GENERATED_BODY()
public:
	AProceduralFoliageActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(Category = ProceduralFoliageTile, VisibleAnywhere, BlueprintReadOnly)
	UProceduralFoliageComponent* ProceduralComponent;

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif
};

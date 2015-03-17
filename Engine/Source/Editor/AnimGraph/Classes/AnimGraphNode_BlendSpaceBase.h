// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimGraphNode_Base.h"
#include "Animation/BlendSpaceBase.h"
#include "AnimGraphNode_BlendSpaceBase.generated.h"

UCLASS(Abstract, MinimalAPI)
class UAnimGraphNode_BlendSpaceBase : public UAnimGraphNode_Base
{
	GENERATED_BODY()
public:
	ANIMGRAPH_API UAnimGraphNode_BlendSpaceBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// UEdGraphNode interface
	ANIMGRAPH_API virtual FLinearColor GetNodeTitleColor() const override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base interface
	ANIMGRAPH_API virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const override;
	ANIMGRAPH_API virtual void PreloadRequiredAssets() override;
	ANIMGRAPH_API virtual void PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const override;
	// End of UAnimGraphNode_Base interface

protected:
	static void GetBlendSpaceEntries(bool bWantAimOffsets, FGraphContextMenuBuilder& ContextMenuBuilder);

	UBlendSpaceBase* GetBlendSpace() const { return Cast<UBlendSpaceBase>(GetAnimationAsset()); }
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5TargetSettings.h: Declares the UHTML5TargetSettings class.
=============================================================================*/

#pragma once

#include "HTML5TargetSettings.generated.h"


/**
 * Implements the settings for the HTML5 target platform.
 */
UCLASS(config=Engine, defaultconfig)
class HTML5PLATFORMEDITOR_API UHTML5TargetSettings
	: public UObject
{
	GENERATED_BODY()
public:
	UHTML5TargetSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
public:


 	/**
 	 * Setting to control HTML5 Heap size (in Development)
 	 */
 	UPROPERTY(GlobalConfig, EditAnywhere, Category=Memory, Meta = (DisplayName = "Development Heap Size (in MB)", ClampMin="1", ClampMax="4096"))
 	int32 HeapSizeDevelopment;

	/**
 	 * Setting to control HTML5 Heap size
 	 */
 	UPROPERTY(GlobalConfig, EditAnywhere, Category=Memory, Meta = (DisplayName = "Heap Size (in MB)", ClampMin="1", ClampMax="4096"))
 	int32 HeapSizeShipping;

	/**
 	 * Port to use when deploying game from the editor
 	 */
 	UPROPERTY(GlobalConfig, EditAnywhere, Category=Memory, Meta = (DisplayName = "Port to use when deploying game from the editor", ClampMin="49152", ClampMax="65535"))
 	int32 DeployServerPort;
};

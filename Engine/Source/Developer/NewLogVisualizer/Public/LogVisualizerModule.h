// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILogVisualizer.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

class SDockTab;
class ISlateStyle;
class FSpawnTabArgs;
class FNewLogVisualizerModule : public IModuleInterface, public IModularFeature
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

private:
	void HandleExperimentalSettingChanged(FName PropertyName);
	TSharedRef<SDockTab> SpawnLogVisualizerTab(const FSpawnTabArgs& SpawnTabArgs);
	void OnTabClosed(TSharedRef<SDockTab>);
};

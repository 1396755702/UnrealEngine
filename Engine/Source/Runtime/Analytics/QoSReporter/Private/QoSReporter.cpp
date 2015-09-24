// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "QoSReporterPrivatePCH.h"

#include "Core.h"
#include "Analytics.h"
#include "EngineVersion.h"
#include "QoSReporter.h"

#ifndef WITH_QOSREPORTER
	#error "WITH_QOSREPORTER should be defined in Build.cs file"
#endif

bool FQoSReporter::bIsInitialized;
TSharedPtr<IAnalyticsProvider> FQoSReporter::Analytics;

/**
* External code should bind this delegate if QoS reporting is desired,
* preferably in private code that won't be redistributed.
*/
QOSREPORTER_API FAnalytics::FProviderConfigurationDelegate& GetQoSOverrideConfigDelegate()
{
	static FAnalytics::FProviderConfigurationDelegate Delegate;
	return Delegate;
}


/**
* Get analytics pointer
*/
IAnalyticsProvider& FQoSReporter::GetProvider()
{
	checkf(bIsInitialized && IsAvailable(), TEXT("FQoSReporter::GetProvider called outside of Initialize/Shutdown."));

	return *Analytics.Get();
}

void FQoSReporter::Initialize()
{
	checkf(!bIsInitialized, TEXT("FQoSReporter::Initialize called more than once."));

	// allow overriding from configs
	bool bEnabled = true;
	if (GConfig->GetBool(TEXT("QoSReporter"), TEXT("bEnabled"), bEnabled, GEngineIni) && !bEnabled)
	{
		UE_LOG(LogQoSReporter, Verbose, TEXT("QoSReporter disabled by config setting"));
		return;
	}

	// Setup some default engine analytics if there is nothing custom bound
	FAnalytics::FProviderConfigurationDelegate DefaultEngineAnalyticsConfig;
	DefaultEngineAnalyticsConfig.BindLambda(
		[=](const FString& KeyName, bool bIsValueRequired) -> FString
	{
		static TMap<FString, FString> ConfigMap;
		if (ConfigMap.Num() == 0)
		{
			ConfigMap.Add(TEXT("ProviderModuleName"), TEXT("QoSReporter"));
			ConfigMap.Add(TEXT("APIKeyQoS"), FString::Printf(TEXT("%s.%s"), FApp::GetGameName(), FAnalytics::ToString(FAnalytics::Get().GetBuildType())));
		}

		// Check for overrides
		if (GetQoSOverrideConfigDelegate().IsBound())
		{
			const FString OverrideValue = GetQoSOverrideConfigDelegate().Execute(KeyName, bIsValueRequired);
			if (!OverrideValue.IsEmpty())
			{
				return OverrideValue;
			}
		}

		FString* ConfigValue = ConfigMap.Find(KeyName);
		return ConfigValue != NULL ? *ConfigValue : TEXT("");
	});

	// Connect the engine analytics provider (if there is a configuration delegate installed)
	Analytics = FAnalytics::Get().CreateAnalyticsProvider(
		FName(*DefaultEngineAnalyticsConfig.Execute(TEXT("ProviderModuleName"), true)),
		DefaultEngineAnalyticsConfig);

	// check if Configs override the heartbeat interval
	
	float ConfigHeartbeatInterval = 0.0;
	if (GConfig->GetFloat(TEXT("QoSReporter"), TEXT("HeartbeatInterval"), ConfigHeartbeatInterval, GEngineIni))
	{
		HeartbeatInterval = ConfigHeartbeatInterval;
		UE_LOG(LogQoSReporter, Verbose, TEXT("HeartbeatInterval configured to %f from config."), HeartbeatInterval);
	}

	bIsInitialized = true;
}

void FQoSReporter::Shutdown()
{
	Analytics.Reset();
	bIsInitialized = false;
}

double FQoSReporter::ModuleInitializationTime = FPlatformTime::Seconds();
bool FQoSReporter::bStartupEventReported = false;

void FQoSReporter::ReportStartupCompleteEvent()
{
	if (bStartupEventReported || !Analytics.IsValid())
	{
		return;
	}

	double CurrentTime = FPlatformTime::Seconds();
	double Difference = CurrentTime - ModuleInitializationTime;

	TArray<FAnalyticsEventAttribute> ParamArray;
	ParamArray.Add(FAnalyticsEventAttribute(EQoSEvents::ToString(EQoSEventParam::StartupTime), Difference));
	Analytics->RecordEvent(EQoSEvents::ToString(EQoSEventParam::StartupTime), ParamArray);

	bStartupEventReported = true;
}

double FQoSReporter::HeartbeatInterval = 300;
double FQoSReporter::LastHeartbeatTimestamp = FPlatformTime::Seconds();

void FQoSReporter::Tick()
{
	if (!Analytics.IsValid())
	{
		return;
	}

	double CurrentTime = FPlatformTime::Seconds();
	if (HeartbeatInterval > 0 && CurrentTime - LastHeartbeatTimestamp > HeartbeatInterval)
	{
		SendHeartbeat();
		LastHeartbeatTimestamp = CurrentTime;
	}
}

void FQoSReporter::SendHeartbeat()
{
	checkf(Analytics.IsValid(), TEXT("SendHeartbeat() should not be called if Analytics provider was not configured"));

	extern ENGINE_API float GAverageFPS;
	TArray<FAnalyticsEventAttribute> ParamArray;
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("AverageFPS"), GAverageFPS));
	Analytics->RecordEvent(EQoSEvents::ToString(EQoSEventParam::Heartbeat), ParamArray);
}

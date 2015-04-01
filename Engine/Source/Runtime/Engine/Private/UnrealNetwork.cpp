// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "GeneralProjectSettings.h"

FNetworkVersion::FGetLocalNetworkVersionOverride FNetworkVersion::GetLocalNetworkVersionOverride;
FNetworkVersion::FIsNetworkCompatibleOverride FNetworkVersion::IsNetworkCompatibleOverride;

enum ENetworkVersionHistory
{
	HISTORY_INITIAL				= 1,
	HISTORY_INTERNAL_ACK		= 3				// We no longer save packet/channel sequence in stream. We can derive this for 100% reliable connections.
};

const uint32 FNetworkVersion::InternalProtocolVersion = HISTORY_INTERNAL_ACK;

uint32 FNetworkVersion::GetLocalNetworkVersion()
{
	if ( GetLocalNetworkVersionOverride.IsBound() )
	{
		const uint32 LocalNetworkVersion = GetLocalNetworkVersionOverride.Execute();

		UE_LOG( LogNet, Log, TEXT( "GetLocalNetworkVersionOverride: LocalNetworkVersion: %i" ), LocalNetworkVersion );

		return LocalNetworkVersion;
	}

	// Get the project name (NOT case sensitive)
	const FString ProjectName( FString( FApp::GetGameName() ).ToLower() );

	// Get the project version string (IS case sensitive!)
	const FString& ProjectVersion = Cast<UGeneralProjectSettings>(UGeneralProjectSettings::StaticClass()->GetDefaultObject())->ProjectVersion;

	// Start with engine version as seed, and then hash with project name + project version
	const uint32 VersionHash = FCrc::StrCrc32( *ProjectVersion, FCrc::StrCrc32( *ProjectName, GEngineNetVersion ) );

	// Hash with internal protocol version
	uint32 LocalNetworkVersion = FCrc::MemCrc32( &InternalProtocolVersion, sizeof( InternalProtocolVersion ), VersionHash );

	if ( !GEngineVersion.IsPromotedBuild() )
	{
		// Further hash with date time if this is a non promoted build
		const FString DateTimeString = FString::Printf( TEXT( "%s_%s" ), ANSI_TO_TCHAR( __DATE__ ), ANSI_TO_TCHAR( __TIME__ ) );
		const uint32 LocalNetworkVersionNonPromoted = FCrc::StrCrc32( *DateTimeString, LocalNetworkVersion );

		UE_LOG( LogNet, Log, TEXT( "GetLocalNetworkVersion: NON-PROMOTED: Date: %s, ProjectName: %s, ProjectVersion: %s, InternalProtocolVersion: %i, LocalNetworkVersionNonPromoted: %u" ), *DateTimeString, *ProjectName, *ProjectVersion, InternalProtocolVersion, LocalNetworkVersionNonPromoted );

		return LocalNetworkVersionNonPromoted;
	}

	UE_LOG( LogNet, Log, TEXT( "GetLocalNetworkVersion: GEngineNetVersion: %i, ProjectName: %s, ProjectVersion: %s, InternalProtocolVersion: %i, LocalNetworkVersion: %u" ), GEngineNetVersion, *ProjectName, *ProjectVersion, InternalProtocolVersion, LocalNetworkVersion );

	return LocalNetworkVersion;
}

bool FNetworkVersion::IsNetworkCompatible( const uint32 LocalNetworkVersion, const uint32 RemoteNetworkVersion )
{
	if ( IsNetworkCompatibleOverride.IsBound() )
	{
		return IsNetworkCompatibleOverride.Execute( LocalNetworkVersion, RemoteNetworkVersion );
	}

	return LocalNetworkVersion == RemoteNetworkVersion;
}

// ----------------------------------------------------------------

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HttpNetworkReplayStreaming.h"

DEFINE_LOG_CATEGORY_STATIC( LogHttpReplay, Log, All );

void HttpStreamFArchive::Serialize( void* V, int64 Length ) 
{
	if ( IsLoading() )
	{
		if ( Pos + Length > Buffer.Num() )
		{
			ArIsError = true;
			return;
		}
			
		FMemory::Memcpy( V, Buffer.GetData() + Pos, Length );

		Pos += Length;
	}
	else
	{
		check( Pos <= Buffer.Num() );

		const int32 SpaceNeeded = Length - ( Buffer.Num() - Pos );

		if ( SpaceNeeded > 0 )
		{
			Buffer.AddZeroed( SpaceNeeded );
		}

		FMemory::Memcpy( Buffer.GetData() + Pos, V, Length );

		Pos += Length;
	}
}

int64 HttpStreamFArchive::Tell() 
{
	return Pos;
}

int64 HttpStreamFArchive::TotalSize()
{
	return Buffer.Num();
}

void HttpStreamFArchive::Seek( int64 InPos ) 
{
	Pos = InPos;
}

void FHttpNetworkReplayStreamer::StartStreaming( FString& StreamName, bool bRecord, const FOnStreamReadyDelegate& Delegate )
{
	if ( !SessionName.IsEmpty() )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::StartStreaming. SessionName already set." ) );
		return;
	}

	if ( IsHttpBusy() )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::StartStreaming. BUSY" ) );
		return;
	}

	// Remember the delegate, which we'll call as soon as the header is available
	RememberedDelegate = Delegate;

	// Setup the archives
	StreamArchive.ArIsLoading = !bRecord;
	StreamArchive.ArIsSaving = !StreamArchive.ArIsLoading;

	HeaderArchive.ArIsLoading = !bRecord;
	HeaderArchive.ArIsSaving = !StreamArchive.ArIsLoading;

	LastFlushTime = FPlatformTime::Seconds();

	const bool bOverrideRecordingSession = true;//false;

	// Override session name if requested
	if ( StreamArchive.ArIsLoading || bOverrideRecordingSession )
	{
		SessionName = StreamName;
	}

	// Initialize the server URL
	GConfig->GetString( TEXT( "HttpNetworkReplayStreaming" ), TEXT( "ServerURL" ), ServerURL, GEngineIni );

	// Create the Http request and add to pending request list
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	StreamFileCount = 0;

	if ( StreamArchive.ArIsLoading )
	{
		// Notify the http server that we want to start downloading a replay
		HttpRequest->SetURL( FString::Printf( TEXT( "%sstartdownloading?Version=%s&Session=%s" ), *ServerURL, TEXT( "Test" ), *SessionName ) );
		HttpRequest->SetVerb( TEXT( "POST" ) );

		HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpStartDownloadingFinished );

		HttpState = EHttptate::StartDownloading;
		
		// Set the next streamer state to download the header
		StreamerState = EStreamerState::NeedToDownloadHeader;
	}
	else
	{
		// Notify the http server that we want to start upload a replay
		if ( !SessionName.IsEmpty() )
		{
			HttpRequest->SetURL( FString::Printf( TEXT( "%sstartuploading?Version=%s&Session=%s" ), *ServerURL, TEXT( "Test" ), *SessionName ) );
		}
		else
		{
			HttpRequest->SetURL( FString::Printf( TEXT( "%sstartuploading?Version=%s" ), *ServerURL, TEXT( "Test" ) ) );
		}

		HttpRequest->SetVerb( TEXT( "POST" ) );

		HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpStartUploadingFinished );

		SessionName.Empty();

		HttpState = EHttptate::StartUploading;

		// Set the next streamer state to upload the header
		StreamerState = EStreamerState::NeedToUploadHeader;
	}
	
	HttpRequest->ProcessRequest();
}

void FHttpNetworkReplayStreamer::StopStreaming()
{
	check( bStopStreamingCalled == false );

	if ( StreamerState == EStreamerState::Idle )
	{
		return;
	}

	// We need to queued this operation up, and finish it once all other operations are done
	// (wait for header to upload, and any current stream to finish uploading, then we can flush what's left in StreamArchive)
	bStopStreamingCalled = true;
}

void FHttpNetworkReplayStreamer::UploadHeader()
{
	if ( SessionName.IsEmpty() || !HeaderArchive.ArIsSaving )
	{
		// IF there is no active session, or we are not recording, we don't need to flush
		return;
	}

	if ( HeaderArchive.Buffer.Num() == 0 )
	{
		// We haven't serialized the header yet, don't do anything yet
		return;
	}

	if ( IsHttpBusy() )
	{
		// If we're currently busy we can't flush right now
		return;
	}

	// First upload the header
	UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::UploadHeader. Header. StreamFileCount: %i, Size: %i" ), StreamFileCount, StreamArchive.Buffer.Num() );

	// Create the Http request and add to pending request list
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpHeaderUploadFinished );

	HttpState = EHttptate::UploadingHeader;

	HttpRequest->SetURL( FString::Printf( TEXT( "%supload?Version=%s&Session=%s&Filename=replay.header" ), *ServerURL, TEXT( "Test" ), *SessionName ) );
	HttpRequest->SetVerb( TEXT( "POST" ) );
	HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/octet-stream" ) );
	HttpRequest->SetContent( HeaderArchive.Buffer );

	// We're done with the header archive
	HeaderArchive.Buffer.Empty();
	HeaderArchive.Pos = 0;

	HttpRequest->ProcessRequest();

	LastFlushTime = FPlatformTime::Seconds();
}

void FHttpNetworkReplayStreamer::FlushStream()
{
	if ( SessionName.IsEmpty() || !StreamArchive.ArIsSaving )
	{
		// IF there is no active session, or we are not recording, we don't need to flush
		return;
	}

	if ( IsHttpBusy() )
	{
		// If we're currently busy we can't flush right now
		return;
	}

	// Upload any new streamed data to the http server
	UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::FlushStream. StreamFileCount: %i, Size: %i" ), StreamFileCount, StreamArchive.Buffer.Num() );

	// Create the Http request and add to pending request list
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpUploadFinished );

	HttpState = EHttptate::UploadingStream;

	HttpRequest->SetURL( FString::Printf( TEXT( "%supload?Version=%s&Session=%s&Filename=stream.%i" ), *ServerURL, TEXT( "Test" ), *SessionName, StreamFileCount ) );
	HttpRequest->SetVerb( TEXT( "POST" ) );
	HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/octet-stream" ) );
	HttpRequest->SetContent( StreamArchive.Buffer );

	StreamArchive.Buffer.Empty();
	StreamArchive.Pos = 0;

	StreamFileCount++;

	HttpRequest->ProcessRequest();

	LastFlushTime = FPlatformTime::Seconds();
}

void FHttpNetworkReplayStreamer::DownloadHeader()
{
	// Download header first
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetURL( FString::Printf( TEXT( "%sdownload?Version=%s&Session=%s&Filename=replay.header" ), *ServerURL, TEXT( "Test" ), *SessionName ) );
	HttpRequest->SetVerb( TEXT( "GET" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpDownloadHeaderFinished );

	HttpState = EHttptate::DownloadingHeader;

	HttpRequest->ProcessRequest();
}

void FHttpNetworkReplayStreamer::DownloadNextChunk()
{
	if ( StreamFileCount >= NumDownloadChunks || NumDownloadChunks == 0 || StreamArchive.Pos < StreamArchive.Buffer.Num() )
	{
		// We don't need to download the next chunk yet
		return;
	}

	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Download the next stream chunk
	HttpRequest->SetURL( FString::Printf( TEXT( "%sdownload?Version=%s&Session=%s&Filename=stream.%i" ), *ServerURL, TEXT( "Test" ), *SessionName, StreamFileCount ) );
	HttpRequest->SetVerb( TEXT( "GET" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpDownloadFinished );

	HttpState = EHttptate::DownloadingStream;

	HttpRequest->ProcessRequest();
}

FArchive* FHttpNetworkReplayStreamer::GetHeaderArchive()
{
	return ( HeaderArchive.IsSaving() || HeaderArchive.Pos < HeaderArchive.Buffer.Num() ) ? &HeaderArchive : NULL;
}

FArchive* FHttpNetworkReplayStreamer::GetStreamingArchive()
{
	return ( StreamArchive.IsSaving() || IsDataAvailable() ) ? &StreamArchive : NULL;
}

FArchive* FHttpNetworkReplayStreamer::GetMetadataArchive()
{
	return NULL;
}

bool FHttpNetworkReplayStreamer::IsDataAvailable() const
{
	// If we are loading, and we have more data
	if ( StreamArchive.IsLoading() && StreamArchive.Pos < StreamArchive.Buffer.Num() && NumDownloadChunks > 0 )
	{
		return true;
	}
	
	return false;
}

void FHttpNetworkReplayStreamer::HttpStartUploadingFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	check( HttpState == EHttptate::StartUploading );
	check( StreamerState == EStreamerState::NeedToUploadHeader );

	HttpState = EHttptate::Idle;

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		SessionName = HttpResponse->GetHeader( TEXT( "Session" ) );

		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpStartUploadingFinished. SessionName: %s" ), *SessionName );
	}
	else
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpStartUploadingFinished. FAILED" ) );
	}
}

void FHttpNetworkReplayStreamer::HttpStopUploadingFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	check( HttpState == EHttptate::StopUploading );
	check( StreamerState == EStreamerState::StreamingUpFinal );

	HttpState = EHttptate::Idle;
	StreamerState = EStreamerState::Idle;

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpStopUploadingFinished. SessionName: %s" ), *SessionName );
	}
	else
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpStopUploadingFinished. FAILED" ) );
	}

	StreamArchive.ArIsLoading = false;
	StreamArchive.ArIsSaving = false;
	StreamArchive.Buffer.Empty();
	StreamArchive.Pos = 0;
	StreamFileCount = 0;
	bStopStreamingCalled = false;
	SessionName.Empty();
}

void FHttpNetworkReplayStreamer::HttpHeaderUploadFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	check( HttpState == EHttptate::UploadingHeader );
	check( StreamerState == EStreamerState::NeedToUploadHeader );

	HttpState = EHttptate::Idle;

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpHeaderUploadFinished." ) );

		StreamerState = EStreamerState::StreamingUp;
	}
	else
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpHeaderUploadFinished. FAILED" ) );

		StreamerState = EStreamerState::Idle;

		// FIXME: Notify demo driver with delegate
	}
}

void FHttpNetworkReplayStreamer::HttpUploadFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	check( HttpState == EHttptate::UploadingStream );
	check( StreamerState == EStreamerState::StreamingUp || StreamerState == EStreamerState::StreamingUpFinal );

	HttpState = EHttptate::Idle;

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpUploadFinished." ) );

		if ( StreamerState == EStreamerState::StreamingUpFinal )
		{
			// Create the Http request and add to pending request list
			TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

			HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpStopUploadingFinished );

			HttpState = EHttptate::StopUploading;

			HttpRequest->SetURL( FString::Printf( TEXT( "%sstopuploading?Version=%s&Session=%s&NumChunks=%i" ), *ServerURL, TEXT( "Test" ), *SessionName, StreamFileCount ) );
			HttpRequest->SetVerb( TEXT( "POST" ) );
			HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/octet-stream" ) );

			HttpRequest->ProcessRequest();
		}
	}
	else
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpUploadFinished. FAILED" ) );
	}
}

void FHttpNetworkReplayStreamer::HttpStartDownloadingFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	check( HttpState == EHttptate::StartDownloading );
	check( StreamerState == EStreamerState::NeedToDownloadHeader );

	HttpState = EHttptate::Idle;

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		FString NumChunksString = HttpResponse->GetHeader( TEXT( "NumChunks" ) );
		FString State = HttpResponse->GetHeader( TEXT( "State" ) );

		NumDownloadChunks = FCString::Atoi( *NumChunksString );

		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpStartDownloadingFinished. State: %s, NumChunks: %i" ), *State, NumDownloadChunks );

		// First, download the header
		if ( NumDownloadChunks > 0 )
		{			
			StreamerState = EStreamerState::NeedToDownloadHeader;
		}
		else
		{
			UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpStartDownloadingFinished. NO CHUNKS" ) );

			RememberedDelegate.ExecuteIfBound( false, StreamArchive.IsSaving() );

			// Reset delegate
			RememberedDelegate = FOnStreamReadyDelegate();

			StreamerState = EStreamerState::Idle;
		}
	}
	else
	{
		// FAIL
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpStartDownloadingFinished. FAILED" ) );

		RememberedDelegate.ExecuteIfBound( false, StreamArchive.IsSaving() );

		// Reset delegate
		RememberedDelegate = FOnStreamReadyDelegate();

		StreamerState = EStreamerState::Idle;
	}
}

void FHttpNetworkReplayStreamer::HttpDownloadHeaderFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	check( HttpState == EHttptate::DownloadingHeader );
	check( StreamerState == EStreamerState::NeedToDownloadHeader );
	check( StreamArchive.IsLoading() );

	HttpState = EHttptate::Idle;

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		HeaderArchive.Buffer.Append( HttpResponse->GetContent() );

		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadHeaderFinished. Size: %i" ), HeaderArchive.Buffer.Num() );

		RememberedDelegate.ExecuteIfBound( true, StreamArchive.IsSaving() );
		StreamerState = EStreamerState::StreamingDown;
	}
	else
	{
		// FAIL
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadFinished. FAILED." ) );

		StreamArchive.Buffer.Empty();
		RememberedDelegate.ExecuteIfBound( false, StreamArchive.IsSaving() );

		StreamerState = EStreamerState::Idle;
	}

	// Reset delegate
	RememberedDelegate = FOnStreamReadyDelegate();
}

void FHttpNetworkReplayStreamer::HttpDownloadFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	check( HttpState == EHttptate::DownloadingStream );
	check( StreamArchive.IsLoading() );

	HttpState = EHttptate::Idle;

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		StreamArchive.Buffer.Append( HttpResponse->GetContent() );
		
		StreamFileCount++;

		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadFinished. Progress: %i / %i" ), StreamFileCount, NumDownloadChunks );
	}
	else
	{
		// FAIL
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadFinished. FAILED." ) );

		StreamArchive.Buffer.Empty();
	}
}

void FHttpNetworkReplayStreamer::Tick( float DeltaTime )
{
	if ( IsHttpBusy() )
	{
		return;
	}

	if ( SessionName.IsEmpty() )
	{
		check( StreamerState == EStreamerState::Idle );
		return;
	}

	if ( bStopStreamingCalled )
	{
		// If http isn't busy and we need to flush the last chunk, then go to that state now
		if ( StreamerState == EStreamerState::StreamingUp )
		{
			StreamerState = EStreamerState::StreamingUpFinal;
		}
		else if ( StreamerState == EStreamerState::StreamingDown )
		{
			StreamerState = EStreamerState::Idle;	// FIXME: Implement StopDownloading
		}

		bStopStreamingCalled = false;
	}

	if ( StreamerState == EStreamerState::NeedToUploadHeader )
	{
		// If we're waiting on the header, don't do anything until the header has data and is uploaded
		UploadHeader();
	}
	else if ( StreamerState == EStreamerState::StreamingUp )
	{
		const double FLUSH_TIME_IN_SECONDS = 10;

		if ( FPlatformTime::Seconds() - LastFlushTime > FLUSH_TIME_IN_SECONDS )
		{
			FlushStream();
		}
	}
	else if ( StreamerState == EStreamerState::StreamingUpFinal )
	{
		FlushStream();
	}
	else if ( StreamerState == EStreamerState::NeedToDownloadHeader )
	{
		// If we're waiting on the header to download, don't do anything until the header has been downloaded
		DownloadHeader();
	}
	else if ( StreamerState == EStreamerState::StreamingDown )
	{
		DownloadNextChunk();
	}
}

bool FHttpNetworkReplayStreamer::IsHttpBusy() const
{
	return HttpState != EHttptate::Idle;
}

bool FHttpNetworkReplayStreamer::IsStreaming() const
{
	return StreamerState != EStreamerState::Idle;
}

IMPLEMENT_MODULE( FHttpNetworkReplayStreamingFactory, HttpNetworkReplayStreaming )

TSharedPtr< INetworkReplayStreamer > FHttpNetworkReplayStreamingFactory::CreateReplayStreamer()
{
	TSharedPtr< FHttpNetworkReplayStreamer > Streamer( new FHttpNetworkReplayStreamer );

	HttpStreamers.Add( Streamer );

	return Streamer;
}

void FHttpNetworkReplayStreamingFactory::Tick( float DeltaTime )
{
	for ( int i = HttpStreamers.Num() - 1; i >= 0; i-- )
	{
		HttpStreamers[i]->Tick( DeltaTime );
		
		// We can release our hold when streaming is completely done
		if ( HttpStreamers[i].IsUnique() && !HttpStreamers[i]->IsStreaming() )
		{
			HttpStreamers.RemoveAt( i );
		}
	}
}

bool FHttpNetworkReplayStreamingFactory::IsTickable() const
{
	return true;
}

TStatId FHttpNetworkReplayStreamingFactory::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT( FHttpNetworkReplayStreamingFactory, STATGROUP_Tickables );
}

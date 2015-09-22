// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneFloatTrack.h"
#include "MovieSceneSlomoTrack.generated.h"


/**
 * Implements a movie scene track that controls a movie scene's playback speed.
 */
UCLASS(MinimalAPI)
class UMovieSceneSlomoTrack
	: public UMovieSceneFloatTrack
{
	GENERATED_BODY()

public:

	// UMovieSceneFloatTrack overrides

	virtual bool HasShowableData() const override;
};

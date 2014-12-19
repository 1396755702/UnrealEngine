// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Interface for a class to provide mesh painting support for a subclass of UMeshComponent
 */
class IMeshPaintGeometryAdapter
{
public:
	virtual bool Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) = 0;
	virtual int32 GetNumTexCoords() const = 0;
	virtual void GetTriangleInfo(int32 TriIndex, struct FTexturePaintTriangleInfo& OutTriInfo) const = 0;

	virtual bool SupportsTexturePaint() const = 0;
	virtual bool SupportsVertexPaint() const = 0;
	virtual bool LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) = 0;
	virtual ~IMeshPaintGeometryAdapter() {}
};

/**
 * Factory for IMeshPaintGeometryAdapter
 */
class IMeshPaintGeometryAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintGeometryAdapter> Construct(class UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) const = 0;
};


/**
 * MeshPaint module interface
 */
class IMeshPaintModule : public IModuleInterface
{
public:
	virtual void RegisterGeometryAdapterFactory(TSharedRef<IMeshPaintGeometryAdapterFactory> Factory) = 0;
	virtual void UnregisterGeometryAdapterFactory(TSharedRef<IMeshPaintGeometryAdapterFactory> Factory) = 0;
};


// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Niagara/NiagaraCommon.h"

#include "NiagaraComponent.generated.h"

/**
* Contains the simulation data for a single FNiagaraSimulation
* all buffers for particle attributes are stored here for convenient access
*/
class FNiagaraEmitterParticleData
{
public:
	FNiagaraEmitterParticleData()
		: CurrentBuffer(0),
		NumParticles(0),
		ParticleAllocation(0)
	{
	}

	~FNiagaraEmitterParticleData() {}

	void Allocate(uint32 NumExpectedParticles)
	{
		ParticleBuffers[CurrentBuffer].Reset(NumExpectedParticles * AttrMap.Num());
		ParticleBuffers[CurrentBuffer].AddUninitialized(NumExpectedParticles * AttrMap.Num());

		ParticleAllocation = NumExpectedParticles;
	}

	void Reset()
	{
		AttrMap.Empty();
		NumParticles = 0;
		ParticleAllocation = 0;
		CurrentBuffer = 0;
	}

	const FVector4 *GetAttributeData(const FNiagaraVariableInfo& AttrID) const
	{
		const uint32* Offset = AttrMap.Find(AttrID);
		return Offset ? ParticleBuffers[CurrentBuffer].GetData() + (*Offset * ParticleAllocation) : NULL;
	}

	FVector4 *GetAttributeDataWrite(const FNiagaraVariableInfo& AttrID)
	{
		const uint32* Offset = AttrMap.Find(AttrID);
		return Offset ? ParticleBuffers[CurrentBuffer].GetData() + (*Offset * ParticleAllocation) : NULL;
	}

	bool HasAttriubte(const FNiagaraVariableInfo& AttrID)
	{
		return AttrMap.Find(AttrID) != NULL;
	}

	void SetAttributes(const TArray<FNiagaraVariableInfo>& Attrs)
	{
		Reset();
		for (const FNiagaraVariableInfo& Attr : Attrs)
		{
			//TODO: Support attributes of any type. This may as well wait until the move of matrix ops etc over into UNiagaraScriptStructs.
			uint32 Idx = AttrMap.Num();
			AttrMap.Add(Attr, Idx);
		}
	}

	int GetNumAttributes()				{ return AttrMap.Num(); }

	uint32 GetNumParticles() const		{ return NumParticles; }
	uint32 GetParticleAllocation() const { return ParticleAllocation; }
	void SetNumParticles(uint32 Num)	{ NumParticles = Num;  }
	void SwapBuffers()					{ CurrentBuffer ^= 0x1; }

	FVector4 *GetCurrentBuffer()		{ return ParticleBuffers[CurrentBuffer].GetData(); }
	FVector4 *GetPreviousBuffer()		{ return ParticleBuffers[CurrentBuffer^0x1].GetData(); }

	int GetBytesUsed()	{ return (ParticleBuffers[0].Num() + ParticleBuffers[1].Num()) * 16 + AttrMap.Num() * 4; }
private:
	uint32 CurrentBuffer, NumParticles, ParticleAllocation;
	TArray<FVector4> ParticleBuffers[2];
	TMap<FNiagaraVariableInfo, uint32> AttrMap;
};



/**
* UNiagaraComponent is the primitive component for a Niagara effect.
* @see ANiagaraActor
* @see UNiagaraEffect
*/
UCLASS()
class ENGINE_API UNiagaraComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	UNiagaraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
private:
	UPROPERTY()
	//class UNiagaraEffect *Effect;
	class UNiagaraEffect *Asset;
	class FNiagaraEffectInstance *EffectInstance;

	// Begin UActorComponent interface.
protected:
	virtual void OnRegister() override;
	virtual void OnUnregister()  override;
	virtual void SendRenderDynamicData_Concurrent() override;
public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	// End UActorComponent interface.

	// Begin UPrimitiveComponent Interface
	virtual int32 GetNumMaterials() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// End UPrimitiveComponent Interface

	void SetAsset(UNiagaraEffect *InAsset);
	UNiagaraEffect *GetAsset() const { return Asset; }

	FNiagaraEffectInstance *GetEffectInstance()	const { return EffectInstance; }
	void SetEffectInstance(FNiagaraEffectInstance *InInstance)	{ EffectInstance = InInstance; }

	// Begin UObject interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject interface.

	static const TArray<FNiagaraVariableInfo>& GetSystemConstants();

};






/**
* Scene proxy for drawing niagara particle simulations.
*/
class FNiagaraSceneProxy : public FPrimitiveSceneProxy
{
public:

	FNiagaraSceneProxy(const UNiagaraComponent* InComponent);
	~FNiagaraSceneProxy();

	/** Called on render thread to assign new dynamic data */
	void SetDynamicData_RenderThread(struct FNiagaraDynamicDataBase* NewDynamicData);
	TArray<class NiagaraEffectRenderer*> &GetEffectRenderers() { return EffectRenderers; }
	void AddEffectRenderer(NiagaraEffectRenderer *Renderer)	{ EffectRenderers.Add(Renderer); }
	ENGINE_API void UpdateEffectRenderers(FNiagaraEffectInstance *InEffect);

private:
	void ReleaseRenderThreadResources();

	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources() override;

	virtual void OnActorPositionChanged() override;
	virtual void OnTransformChanged() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)  override;

	/*
	virtual bool CanBeOccluded() const override
	{
	return !MaterialRelevance.bDisableDepthTest;
	}
	*/
	virtual uint32 GetMemoryFootprint() const override;

	uint32 GetAllocatedSize() const;


private:
	//class NiagaraEffectRenderer *EffectRenderer;
	TArray<class NiagaraEffectRenderer *>EffectRenderers;
};

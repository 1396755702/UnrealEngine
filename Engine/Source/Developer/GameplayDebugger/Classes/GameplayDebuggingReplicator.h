// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameplayDebuggingTypes.h"
#include "GameplayDebuggingReplicator.generated.h"
/**
*	Transient actor used to communicate between server and client, mostly for RPC
*/

class UGameplayDebuggingComponent;
class AGameplayDebuggingHUDComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, class AActor*);

UCLASS(config = Engine, NotBlueprintable, Transient, hidecategories = Actor, notplaceable)
class GAMEPLAYDEBUGGER_API AGameplayDebuggingReplicator : public AActor
{
	GENERATED_BODY()
public:
	AGameplayDebuggingReplicator(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(config)
	FString DebugComponentClassName;

	UPROPERTY(config)
	FString DebugComponentHUDClassName;

	UPROPERTY(config)
	FString DebugComponentControllerClassName;

	UPROPERTY(config)
	int32 MaxEQSQueries;

	UPROPERTY(Replicated, Transient)
	UGameplayDebuggingComponent* DebugComponent;

	UPROPERTY(Replicated, Transient)
	APlayerController* LocalPlayerOwner;

	UPROPERTY(Replicated, Transient)
	AActor*	LastSelectedActorToDebug;

	UPROPERTY(Replicated, Transient)
	bool bIsGlobalInWorld;

	UPROPERTY(ReplicatedUsing = OnRep_AutoActivate, Transient)
	bool bAutoActivate;

	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool OverHead;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool Basic;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool BehaviorTree;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView|EQS")
	bool EQS;

	UPROPERTY(Transient, EditAnywhere, Category = "DataView|EQS", meta = (EditCondition = "EQS"))
	bool EnableEQSOnHUD;

	UPROPERTY(Transient, EditAnywhere, Category = "DataView|EQS", meta = (EditCondition = "EQS"))
	int32 ActiveEQSIndex;

	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool Perception;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool GameView1;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool GameView2;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool GameView3;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool GameView4;
	
	UPROPERTY(Transient, EditAnywhere, Category = DataView)
	bool GameView5;

	UPROPERTY()
	class UTexture2D* DefaultTexture_Red;

	UPROPERTY()
	class UTexture2D* DefaultTexture_Green;

	UFUNCTION(reliable, server="ServerReplicateMessage_Implementation", WithValidation="ServerReplicateMessage_Validate")
	void ServerReplicateMessage(class AActor* Actor, uint32 InMessage, uint32 DataView = 0);
	virtual void ServerReplicateMessage_Implementation(AActor* Actor, uint32 InMessage, uint32 DataView);
	virtual bool ServerReplicateMessage_Validate(AActor* , uint32 , uint32 );

	UFUNCTION(reliable, client="ClientReplicateMessage_Implementation", WithValidation="ClientReplicateMessage_Validate")
	void ClientReplicateMessage(class AActor* Actor, uint32 InMessage, uint32 DataView = 0);
	virtual void ClientReplicateMessage_Implementation(AActor* Actor, uint32 InMessage, uint32 DataView);
	virtual bool ClientReplicateMessage_Validate(AActor* , uint32 , uint32 );

	UFUNCTION(Reliable, Client="ClientAutoActivate_Implementation")
	void ClientAutoActivate();
	virtual void ClientAutoActivate_Implementation();

	UFUNCTION(Reliable, Client="ClientEnableTargetSelection_Implementation", WithValidation="ClientEnableTargetSelection_Validate")
	void ClientEnableTargetSelection(bool bEnable, APlayerController* Context);
	virtual void ClientEnableTargetSelection_Implementation(bool bEnable, APlayerController* Context);
	virtual bool ClientEnableTargetSelection_Validate(bool , APlayerController* );

#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	virtual void OnRep_AutoActivate();

	virtual class UNetConnection* GetNetConnection() override;

	virtual bool IsNetRelevantFor(const APlayerController* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	virtual void PostNetInit() override;
	
	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	UGameplayDebuggingComponent* GetDebugComponent();

	bool IsToolCreated();
	void CreateTool();
	void EnableTool();
	bool IsDrawEnabled();
	void EnableDraw(bool bEnable);
	void SetAsGlobalInWorld(bool IsGlobal) { bIsGlobalInWorld = IsGlobal;  }
	bool IsGlobalInWorld() { return bIsGlobalInWorld;  }

	void SetLocalPlayerOwner(APlayerController* PC) { LocalPlayerOwner = PC; }
	APlayerController* GetLocalPlayerOwner() { return LocalPlayerOwner; }

	FORCEINLINE AActor* GetSelectedActorToDebug() { return LastSelectedActorToDebug; }
	
	UFUNCTION(reliable, server="ServerSetActorToDebug_Implementation", WithValidation="ServerSetActorToDebug_Validate")
	void ServerSetActorToDebug(AActor* InActor);
	virtual void ServerSetActorToDebug_Implementation(AActor* InActor);
	virtual bool ServerSetActorToDebug_Validate(AActor* );

	/**
	 * Iterates through the pawn list to find the next pawn of the specified type to debug
	 */
	void DebugNextPawn(UClass* CompareClass, APawn* CurrentPawn = nullptr);

	/**
	 * Iterates through the pawn list to find the previous pawn of the specified type to debug
	 */
	void DebugPrevPawn(UClass* CompareClass, APawn* CurrentPawn = nullptr);

	uint32 DebuggerShowFlags;

	static FOnSelectionChanged OnSelectionChangedDelegate;
	FOnChangeEQSQuery OnChangeEQSQuery;
protected:
	void OnDebugAIDelegate(class UCanvas* Canvas, class APlayerController* PC);
	void DrawDebugDataDelegate(class UCanvas* Canvas, class APlayerController* PC);
	void DrawDebugData(class UCanvas* Canvas, class APlayerController* PC);

private:
	uint32 bEnabledDraw : 1;
	uint32 LastDrawAtFrame;
	float PlayerControllersUpdateDelay;

	TWeakObjectPtr<UClass> DebugComponentClass;
	TWeakObjectPtr<UClass> DebugComponentHUDClass;
	TWeakObjectPtr<UClass> DebugComponentControllerClass;
	TWeakObjectPtr<AGameplayDebuggingHUDComponent>	DebugRenderer;
};

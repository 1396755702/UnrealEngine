// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//
// Basic beacon
//
#pragma once
#include "TimerManager.h"
#include "Runtime/Online/OnlineSubsystemUtils/Classes/OnlineBeacon.h"
#include "OnlineBeaconClient.generated.h"

/**
 * Delegate triggered on failures to connect to a host beacon
 */
DECLARE_DELEGATE(FOnHostConnectionFailure);

/**
 * Base class for any actor that would like to communicate outside the normal Unreal Networking gameplay path
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API AOnlineBeaconClient : public AOnlineBeacon
{
	GENERATED_BODY()
public:
	AOnlineBeaconClient(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Begin AActor Interface
	void OnNetCleanup(class UNetConnection* Connection);
	// End AActor Interface

	// Begin FNetworkNotify Interface
	virtual void NotifyControlMessage(class UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch) override;
	// End FNetworkNotify Interface

	// Begin OnlineBeacon Interface
	virtual void OnFailure() override;
	virtual void DestroyBeacon() override;
	// End OnlineBeacon Interface

	/**
	 * Initialize the client beacon with connection endpoint
	 *	Creates the net driver and attempts to connect with the destination
	 *
	 * @param URL destination
	 *
	 * @return true if connection is being attempted, false otherwise
	 */
	bool InitClient(FURL& URL);
	
	/**
	 * A connection has been made and RPC/replication can begin
	 */
	virtual void OnConnected() {};

	/**
	 * Delegate triggered on failures to connect to a host beacon
	 */
	FOnHostConnectionFailure& OnHostConnectionFailure() { return HostConnectionFailure; }

	/**
	 * Get the owner of this beacon actor, some host that is listening for connections
	 * (server side only, clients have no access)
	 *
	 * @return owning host of this actor
	 */
	class AOnlineBeaconHostObject* GetBeaconOwner() const;
	
	/**
	 * Set the owner of this beacon actor, some host that is listening for connections
	 * (server side only, clients have no access)
	 *
	 * @return owning host of this actor
	 */
	void SetBeaconOwner(class AOnlineBeaconHostObject* InBeaconOwner);

protected:

	/** Owning beacon host of this beacon actor */
	UPROPERTY()
	class AOnlineBeaconHostObject* BeaconOwner;

	/** Delegate for host beacon connection failures */
	FOnHostConnectionFailure HostConnectionFailure;

	/** Handle for efficient management of OnFailure timer */
	FTimerHandle TimerHandle_OnFailure;

private:

	/**
	 * Called on the server side to open up the actor channel that will allow RPCs to occur
	 */
	UFUNCTION(client="ClientOnConnected_Implementation", reliable)
	virtual void ClientOnConnected();
	virtual void ClientOnConnected_Implementation();

	friend class AOnlineBeaconHost;
	friend class AOnlineBeaconHostObject;
};

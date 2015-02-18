// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameplayPrediction.h"

UAbilityTask_WaitInputRelease::UAbilityTask_WaitInputRelease(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StartTime = 0.f;
}

void UAbilityTask_WaitInputRelease::OnReleaseCallback()
{
	float ElapsedTime = GetWorld()->GetTimeSeconds() - StartTime;

	if (!Ability.IsValid() || !AbilitySystemComponent.IsValid())
	{
		return;
	}

	AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityReplicatedClientEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(DelegateHandle);

	if (IsPredictingClient())
	{
		// Tell the server about this
		AbilitySystemComponent->ServerSetReplicatedClientEvent(EAbilityReplicatedClientEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey(), AbilitySystemComponent->ScopedPredictionKey);
	}

	// We are done. Kill us so we don't keep getting broadcast messages
	OnRelease.Broadcast(ElapsedTime);
	EndTask();
}

UAbilityTask_WaitInputRelease* UAbilityTask_WaitInputRelease::WaitInputRelease(class UObject* WorldContextObject)
{
	return NewTask<UAbilityTask_WaitInputRelease>(WorldContextObject);
}

void UAbilityTask_WaitInputRelease::Activate()
{
	StartTime = GetWorld()->GetTimeSeconds();
	if (Ability.IsValid())
	{
		DelegateHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityReplicatedClientEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()).AddUObject(this, &UAbilityTask_WaitInputRelease::OnReleaseCallback);
		if (IsForRemoteClient())
		{
			AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityReplicatedClientEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey());
		}
	}
}

void UAbilityTask_WaitInputRelease::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);
}

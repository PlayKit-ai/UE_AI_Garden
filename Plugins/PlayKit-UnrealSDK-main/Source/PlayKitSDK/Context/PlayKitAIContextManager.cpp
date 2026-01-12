// Copyright PlayKit. All Rights Reserved.

#include "PlayKitAIContextManager.h"
#include "PlayKitSDK/NPC/PlayKitNPCClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UPlayKitAIContextManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[AIContextManager] Initialized"));
}

void UPlayKitAIContextManager::Deinitialize()
{
	DisableAutoCompact();
	NPCStates.Empty();
	Super::Deinitialize();
}

UPlayKitAIContextManager* UPlayKitAIContextManager::Get(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UPlayKitAIContextManager>();
}

//========== Player Description ==========//

void UPlayKitAIContextManager::SetPlayerDescription(const FString& Description)
{
	PlayerDescription = Description;
	OnPlayerDescriptionChanged.Broadcast(Description);
	UE_LOG(LogTemp, Log, TEXT("[AIContextManager] Player description set"));
}

void UPlayKitAIContextManager::ClearPlayerDescription()
{
	PlayerDescription.Empty();
	OnPlayerDescriptionChanged.Broadcast(FString());
	UE_LOG(LogTemp, Log, TEXT("[AIContextManager] Player description cleared"));
}

//========== NPC Tracking ==========//

void UPlayKitAIContextManager::RegisterNPC(UPlayKitNPCClient* NPC)
{
	if (!NPC)
	{
		return;
	}

	FNPCConversationState State;
	State.NPC = NPC;
	State.LastInteractionTime = FDateTime::UtcNow();
	State.MessageCount = NPC->GetHistoryLength();
	NPCStates.Add(NPC, State);

	UE_LOG(LogTemp, Log, TEXT("[AIContextManager] Registered NPC: %s"), *NPC->GetName());
}

void UPlayKitAIContextManager::UnregisterNPC(UPlayKitNPCClient* NPC)
{
	if (!NPC)
	{
		return;
	}

	NPCStates.Remove(NPC);
	UE_LOG(LogTemp, Log, TEXT("[AIContextManager] Unregistered NPC: %s"), *NPC->GetName());
}

void UPlayKitAIContextManager::RecordConversation(UPlayKitNPCClient* NPC)
{
	if (!NPC)
	{
		return;
	}

	FNPCConversationState* State = NPCStates.Find(NPC);
	if (!State)
	{
		// Auto-register if not already registered
		RegisterNPC(NPC);
		State = NPCStates.Find(NPC);
	}

	if (State)
	{
		State->LastInteractionTime = FDateTime::UtcNow();
		State->MessageCount = NPC->GetHistoryLength();
		State->bEligibleForCompaction = false;
	}
}

TArray<UPlayKitNPCClient*> UPlayKitAIContextManager::GetRegisteredNPCs() const
{
	TArray<UPlayKitNPCClient*> NPCs;

	for (const auto& Pair : NPCStates)
	{
		if (Pair.Key && Pair.Value.NPC.IsValid())
		{
			NPCs.Add(Pair.Key);
		}
	}

	return NPCs;
}

FNPCConversationState UPlayKitAIContextManager::GetNPCState(UPlayKitNPCClient* NPC) const
{
	const FNPCConversationState* State = NPCStates.Find(NPC);
	return State ? *State : FNPCConversationState();
}

//========== Auto Compaction ==========//

void UPlayKitAIContextManager::EnableAutoCompact(float TimeoutSeconds, int32 MinMessages)
{
	AutoCompactTimeoutSeconds = TimeoutSeconds;
	AutoCompactMinMessages = MinMessages;
	bAutoCompactEnabled = true;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
			AutoCompactTimerHandle,
			this,
			&UPlayKitAIContextManager::CheckAutoCompaction,
			60.0f, // Check every minute
			true
		);
	}

	UE_LOG(LogTemp, Log, TEXT("[AIContextManager] Auto compact enabled: timeout=%.0fs, minMessages=%d"),
		TimeoutSeconds, MinMessages);
}

void UPlayKitAIContextManager::DisableAutoCompact()
{
	bAutoCompactEnabled = false;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(AutoCompactTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[AIContextManager] Auto compact disabled"));
}

bool UPlayKitAIContextManager::IsEligibleForCompaction(UPlayKitNPCClient* NPC) const
{
	const FNPCConversationState* State = NPCStates.Find(NPC);
	if (!State || !State->NPC.IsValid())
	{
		return false;
	}

	// Check message count
	if (State->MessageCount < AutoCompactMinMessages)
	{
		return false;
	}

	// Check time since last interaction
	FTimespan TimeSinceInteraction = FDateTime::UtcNow() - State->LastInteractionTime;
	if (TimeSinceInteraction.GetTotalSeconds() < AutoCompactTimeoutSeconds)
	{
		return false;
	}

	return true;
}

void UPlayKitAIContextManager::CompactConversation(UPlayKitNPCClient* NPC)
{
	if (!NPC)
	{
		return;
	}

	// For now, this is a placeholder. In a full implementation,
	// this would call an AI service to summarize the conversation
	// and replace the history with a compressed version.

	UE_LOG(LogTemp, Log, TEXT("[AIContextManager] Compacting conversation for NPC: %s"), *NPC->GetName());

	// Mark as compacted
	FNPCConversationState* State = NPCStates.Find(NPC);
	if (State)
	{
		State->bEligibleForCompaction = false;
	}

	OnNPCCompacted.Broadcast(NPC);
}

int32 UPlayKitAIContextManager::CompactAllEligible()
{
	int32 CompactedCount = 0;

	TArray<UPlayKitNPCClient*> EligibleNPCs;

	for (auto& Pair : NPCStates)
	{
		if (IsEligibleForCompaction(Pair.Key))
		{
			EligibleNPCs.Add(Pair.Key);
		}
	}

	for (UPlayKitNPCClient* NPC : EligibleNPCs)
	{
		CompactConversation(NPC);
		CompactedCount++;
	}

	return CompactedCount;
}

void UPlayKitAIContextManager::CheckAutoCompaction()
{
	if (!bAutoCompactEnabled)
	{
		return;
	}

	// Update eligibility for all NPCs
	for (auto& Pair : NPCStates)
	{
		Pair.Value.bEligibleForCompaction = IsEligibleForCompaction(Pair.Key);
	}

	// Compact eligible NPCs
	int32 Compacted = CompactAllEligible();

	if (Compacted > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[AIContextManager] Auto compacted %d NPC conversations"), Compacted);
	}
}

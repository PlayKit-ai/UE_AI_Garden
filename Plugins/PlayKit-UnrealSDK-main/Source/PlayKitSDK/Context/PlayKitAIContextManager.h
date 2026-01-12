// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayKitAIContextManager.generated.h"

class UPlayKitNPCClient;

/**
 * NPC Conversation State
 */
USTRUCT(BlueprintType)
struct FNPCConversationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UPlayKitNPCClient> NPC;

	UPROPERTY(BlueprintReadOnly)
	FDateTime LastInteractionTime;

	UPROPERTY(BlueprintReadOnly)
	int32 MessageCount = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bEligibleForCompaction = false;
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCCompacted, UPlayKitNPCClient*, NPC);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCompactionFailed, UPlayKitNPCClient*, NPC, FString, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDescriptionChanged, FString, NewDescription);

/**
 * PlayKit AI Context Manager
 * Manages global AI context, player descriptions, and NPC conversation tracking
 */
UCLASS(BlueprintType)
class PLAYKITSDK_API UPlayKitAIContextManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Get the singleton instance
	 */
	UFUNCTION(BlueprintPure, Category="PlayKit|Context", meta=(WorldContext="WorldContextObject"))
	static UPlayKitAIContextManager* Get(UObject* WorldContextObject);

	//========== Player Description ==========//

	/** Set the player description for context */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Context")
	void SetPlayerDescription(const FString& Description);

	/** Get the current player description */
	UFUNCTION(BlueprintPure, Category="PlayKit|Context")
	FString GetPlayerDescription() const { return PlayerDescription; }

	/** Clear the player description */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Context")
	void ClearPlayerDescription();

	//========== NPC Tracking ==========//

	/** Register an NPC for tracking */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Context")
	void RegisterNPC(UPlayKitNPCClient* NPC);

	/** Unregister an NPC */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Context")
	void UnregisterNPC(UPlayKitNPCClient* NPC);

	/** Record a conversation with an NPC */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Context")
	void RecordConversation(UPlayKitNPCClient* NPC);

	/** Get all registered NPCs */
	UFUNCTION(BlueprintPure, Category="PlayKit|Context")
	TArray<UPlayKitNPCClient*> GetRegisteredNPCs() const;

	/** Get conversation state for an NPC */
	UFUNCTION(BlueprintPure, Category="PlayKit|Context")
	FNPCConversationState GetNPCState(UPlayKitNPCClient* NPC) const;

	//========== Auto Compaction ==========//

	/** Enable automatic conversation compaction */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Context")
	void EnableAutoCompact(float TimeoutSeconds = 300.0f, int32 MinMessages = 10);

	/** Disable automatic compaction */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Context")
	void DisableAutoCompact();

	/** Check if NPC is eligible for compaction */
	UFUNCTION(BlueprintPure, Category="PlayKit|Context")
	bool IsEligibleForCompaction(UPlayKitNPCClient* NPC) const;

	/** Manually compact an NPC's conversation */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Context")
	void CompactConversation(UPlayKitNPCClient* NPC);

	/** Compact all eligible NPCs */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Context")
	int32 CompactAllEligible();

public:
	//========== Events ==========//

	/** Fired when an NPC conversation is compacted */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Context")
	FOnNPCCompacted OnNPCCompacted;

	/** Fired when compaction fails */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Context")
	FOnCompactionFailed OnCompactionFailed;

	/** Fired when player description changes */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Context")
	FOnPlayerDescriptionChanged OnPlayerDescriptionChanged;

public:
	//========== Configuration ==========//

	/** Model to use for compaction summaries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Context")
	FString FastModel = TEXT("gpt-4o-mini");

	/** Timeout in seconds before conversation is eligible for compaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Context")
	float AutoCompactTimeoutSeconds = 300.0f;

	/** Minimum messages before compaction is considered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Context")
	int32 AutoCompactMinMessages = 10;

private:
	void CheckAutoCompaction();

private:
	FString PlayerDescription;

	UPROPERTY()
	TMap<UPlayKitNPCClient*, FNPCConversationState> NPCStates;

	bool bAutoCompactEnabled = false;
	FTimerHandle AutoCompactTimerHandle;
};

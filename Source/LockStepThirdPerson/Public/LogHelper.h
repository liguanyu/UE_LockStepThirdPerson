#pragma once

#include "CoreMinimal.h"

inline void PrintLog(const FString& Str)
{
	UE_LOG(LogTemp, Log, TEXT("[LogHelper] %s"), *Str);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Yellow,
			Str);
	}
}

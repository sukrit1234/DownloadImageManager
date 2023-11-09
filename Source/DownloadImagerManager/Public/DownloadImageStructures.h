// ScrewUp

#pragma once

#include "CoreMinimal.h"
#include "DownloadImageStructures.generated.h"

USTRUCT(BlueprintType)
struct FByteArray
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnyWhere, BlueprintReadWrite)
	TArray<uint8> Bytes;

	FByteArray();

	static FByteArray Empty;
};
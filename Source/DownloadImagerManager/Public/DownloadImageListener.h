// ScrewUp

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DownloadImageListener.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UDownloadImageListener : public UInterface
{
	GENERATED_BODY()
};

class UTexture2DDynamic;
class DOWNLOADIMAGEMANAGER_API IDownloadImageListener
{
	GENERATED_BODY()
public:


	UFUNCTION(BlueprintNativeEvent)
	void Download_ImageSuccess(int32 Handle, const FString& URL, UTexture2DDynamic* Texture);

	UFUNCTION(BlueprintNativeEvent)
	void Download_ImageFailed(int32 Handle, const FString& URL);


	virtual void Download_OnImageSuccess(int32 Handle, const FString& URL, UTexture2DDynamic* Texture) = 0;
	virtual void Download_OnImageFailed(int32 Handle, const FString& URL) = 0;
};

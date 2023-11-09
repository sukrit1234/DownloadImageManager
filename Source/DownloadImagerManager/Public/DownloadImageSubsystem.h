#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DownloadImageStructures.h"
#include "Engine/Texture2DDynamic.h"
#include "Interfaces/IHttpRequest.h"
#include "HttpModule.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "DownloadImageSubsystem.generated.h"

class UTexture2DDynamic;


DECLARE_DYNAMIC_DELEGATE_ThreeParams(FDownloadImageSuccessSignature, int32, RequestID, const FString&, URL, UTexture2DDynamic*, Texture);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDownloadImageFailedSignature, int32, RequestID, const FString&, URL);

struct FDownloadImageDelegate
{
	FDownloadImageSuccessSignature OnSuccess;
	FDownloadImageFailedSignature OnFailed;
};

UCLASS()
class  UDownloadImageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	
	int32 RequestImage(const FString& URL, UObject* Listener = nullptr);
	int32 RequestImage(const FString& URL, const FDownloadImageSuccessSignature& OnSuccess, const FDownloadImageFailedSignature& OnFailed);

	UTexture2DDynamic* GetTextureByUrl(const FString& URL) const;

protected:

	int32 DoHttpRequest(const FString& URL);
	int32 DoInstanceInvoke(const FString& URL);

	UFUNCTION()
	void NextFrameInvokeListener(int32 Handle);

	void InvokeListener(bool bIsValidImage, int32 RequestID, const FString& ImageURL, UTexture2DDynamic* Texture);
	void InvokeCompletedDelegate(bool bIsValidImage, int32 RequestID, const FString& ImageURL, UTexture2DDynamic* Texture);
	UObject* GetLisnerByHandle(int32 RequestID) const;
	const FString& GetURLFromHandle(int32 RequestID) const;

	void HandleImageRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, int32 RequestID);

	int32 RequestHandleRunner = 0;

	TMap<int32, FDownloadImageDelegate> OnCompletedDelegates;

	UPROPERTY(Transient)
	TMap<int32, FTimerHandle> InstantSuccessInvokers;

	UPROPERTY(Transient)
	TMap<int32, UObject*> Listeners;


	int32 NextRequestHandle();

	UPROPERTY(Transient)
	TMap<int32, FString> RequestHandleToURLs;

	UPROPERTY(Transient)
	TMap<FString, UTexture2DDynamic*> CachedTextures;

	static FString EmptyURL;
	static FString GETMETHOD;
};

UCLASS()
class UDownloadImageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static UDownloadImageSubsystem* GetDownloadImageSystem(UObject* WorldContextObject);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Download Image")
	static int32 RequestDownloadImage(UObject* WorldContextObject, const FString& URL, UObject* Listener = nullptr);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Download Image")
	static int32 RequestDownloadImageCallBack(UObject* WorldContextObject, const FString& URL, const FDownloadImageSuccessSignature& OnSuccess, const FDownloadImageFailedSignature& OnFailed);

	UFUNCTION(BlueprintPure, Category = "Widget|Brush")
	static FSlateBrush MakeBrushFromTextureDynamic(UTexture2DDynamic* Texture, int32 Width = 0, int32 Height = 0);
};
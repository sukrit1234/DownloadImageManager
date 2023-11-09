// ScrewUp


#include "DownloadImageSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DDynamic.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpResponse.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "DownloadImageListener.h"
#include "Styling/SlateBrush.h"

#if !UE_SERVER

	static void WriteRawToTexture_RenderThread(FTexture2DDynamicResource* TextureResource, TArray64<uint8>* RawData, bool bUseSRGB = true)
{
	check(IsInRenderingThread());

	if (TextureResource)
	{
		FRHITexture2D* TextureRHI = TextureResource->GetTexture2DRHI();

		int32 Width = TextureRHI->GetSizeX();
		int32 Height = TextureRHI->GetSizeY();

		uint32 DestStride = 0;
		uint8* DestData = reinterpret_cast<uint8*>(RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false, false));

		for (int32 y = 0; y < Height; y++)
		{
			uint8* DestPtr = &DestData[((int64)Height - 1 - y) * DestStride];

			const FColor* SrcPtr = &((FColor*)(RawData->GetData()))[((int64)Height - 1 - y) * Width];
			for (int32 x = 0; x < Width; x++)
			{
				*DestPtr++ = SrcPtr->B;
				*DestPtr++ = SrcPtr->G;
				*DestPtr++ = SrcPtr->R;
				*DestPtr++ = SrcPtr->A;
				SrcPtr++;
			}
		}

		RHIUnlockTexture2D(TextureRHI, 0, false, false);
	}

	delete RawData;
}

#endif

	FString UDownloadImageSubsystem::EmptyURL = TEXT("");
	FString UDownloadImageSubsystem::GETMETHOD = TEXT("GET");

	UObject* UDownloadImageSubsystem::GetLisnerByHandle(int32 Handle) const
	{
		UObject*const* ListenerPtr = Listeners.Find(Handle);
		return (ListenerPtr != nullptr) ? (*ListenerPtr) : nullptr;
	}
	const FString& UDownloadImageSubsystem::GetURLFromHandle(int32 RequestID) const
	{
		const FString* URLPtr = RequestHandleToURLs.Find(RequestID);
		return (URLPtr != nullptr) ? (*URLPtr) : UDownloadImageSubsystem::EmptyURL;
	}
	UTexture2DDynamic* UDownloadImageSubsystem::GetTextureByUrl(const FString& URL) const
	{
		UTexture2DDynamic*const* TexturePtr = CachedTextures.Find(URL);
		return (TexturePtr != nullptr) ? (*TexturePtr) : nullptr;
	}
	int32 UDownloadImageSubsystem::NextRequestHandle()
	{
		RequestHandleRunner++;
		return (RequestHandleRunner);
	}
	int32 UDownloadImageSubsystem::DoInstanceInvoke(const FString& URL)
	{
		int32 NewHandle = INDEX_NONE;
		FTimerHandle TimerHandle;
		UWorld* TheWorld = GetWorld();
		if (IsValid(TheWorld))
		{
			NewHandle = NextRequestHandle();
			TheWorld->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &UDownloadImageSubsystem::NextFrameInvokeListener, NewHandle), 0.02f, false);
		}
		return NewHandle;
	}
	int32 UDownloadImageSubsystem::DoHttpRequest(const FString& URL)
	{
		int32 NewHandle = NextRequestHandle();
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UDownloadImageSubsystem::HandleImageRequest, NewHandle);
		HttpRequest->SetURL(URL);
		HttpRequest->SetVerb(GETMETHOD);
		HttpRequest->ProcessRequest();
		return NewHandle;
	}
	void UDownloadImageSubsystem::InvokeCompletedDelegate(bool bIsValidImage, int32 RequestID, const FString& ImageURL, UTexture2DDynamic* Texture)
	{
		FDownloadImageDelegate* DelegatePtr = OnCompletedDelegates.Find(RequestID);
		if (DelegatePtr != nullptr)
		{
			if (bIsValidImage)
				(*DelegatePtr).OnSuccess.ExecuteIfBound(RequestID, ImageURL, Texture);
			else
				(*DelegatePtr).OnFailed.ExecuteIfBound(RequestID, ImageURL);
		}
	}
	int32 UDownloadImageSubsystem::RequestImage(const FString& URL, UObject* Listener)
	{
		int32 NewHandle = INDEX_NONE;
#if !UE_SERVER
		UTexture2DDynamic* ImageTexture = GetTextureByUrl(URL);
		if (IsValid(ImageTexture))
			NewHandle = DoInstanceInvoke(URL);
		else
			NewHandle = DoHttpRequest(URL);
		if (NewHandle != INDEX_NONE)
		{
			Listeners.Add(NewHandle, Listener);
			RequestHandleToURLs.Add(NewHandle, URL);
		}
#endif
		return NewHandle;
	}
	int32 UDownloadImageSubsystem::RequestImage(const FString& URL, const FDownloadImageSuccessSignature& OnSuccess, const FDownloadImageFailedSignature& OnFailed)
	{
		int32 NewHandle = INDEX_NONE;
#if !UE_SERVER
		UTexture2DDynamic* ImageTexture = GetTextureByUrl(URL);
		if (IsValid(ImageTexture))
			NewHandle = DoInstanceInvoke(URL);
		else
			NewHandle = DoHttpRequest(URL);
		if (NewHandle != INDEX_NONE)
		{
			FDownloadImageDelegate Delegate;
			Delegate.OnSuccess = OnSuccess;
			Delegate.OnFailed = OnFailed;
			OnCompletedDelegates.Add(NewHandle,Delegate);

			RequestHandleToURLs.Add(NewHandle, URL);
		}
#endif
		return NewHandle;
	}
	void UDownloadImageSubsystem::NextFrameInvokeListener(int32 RequestID)
	{
		const FString& URL = GetURLFromHandle(RequestID);
		UTexture2DDynamic* ImageTexture = GetTextureByUrl(URL);
		InvokeListener(IsValid(ImageTexture) && !URL.IsEmpty(), RequestID, URL, ImageTexture);

		UWorld* TheWorld = GetWorld();
		if (IsValid(TheWorld))
		{
			FTimerHandle* TimerHandlePtr = InstantSuccessInvokers.Find(RequestID);
			if (TimerHandlePtr != nullptr)
				TheWorld->GetTimerManager().ClearTimer((*TimerHandlePtr));
		}
		InstantSuccessInvokers.Remove(RequestID);
		RequestHandleToURLs.Remove(RequestID);
	}
	void UDownloadImageSubsystem::InvokeListener(bool bIsValidImage, int32 RequestID, const FString& ImageURL, UTexture2DDynamic* Texture)
	{
		UObject* ListenerObject = GetLisnerByHandle(RequestID);
		if (IsValid(ListenerObject))
		{
			IDownloadImageListener* AsListener = Cast<IDownloadImageListener>(ListenerObject);
			if (AsListener != nullptr) {
				if (bIsValidImage)
					AsListener->Download_OnImageSuccess(RequestID, ImageURL, Texture);
				else
					AsListener->Download_OnImageFailed(RequestID, ImageURL);
			}

			bool bImplement = ListenerObject->GetClass()->ImplementsInterface(UDownloadImageListener::StaticClass());
			if (bImplement) {
				if (bIsValidImage)
					IDownloadImageListener::Execute_Download_ImageSuccess(ListenerObject, RequestID, ImageURL, Texture);
				else
					IDownloadImageListener::Execute_Download_ImageFailed(ListenerObject, RequestID, ImageURL);
			}
		}

		InvokeCompletedDelegate(bIsValidImage, RequestID, ImageURL, Texture);
		Listeners.Remove(RequestID);
		OnCompletedDelegates.Remove(RequestID);
	}
	void UDownloadImageSubsystem::HandleImageRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, int32 RequestID)
	{
#if !UE_SERVER

		const FString& ImageURL = GetURLFromHandle(RequestID);

		bool bIsValidImage = false;
		UTexture2DDynamic* Texture = nullptr;
		if (bSucceeded && HttpResponse.IsValid() && HttpResponse->GetContentLength() > 0)
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
			TSharedPtr<IImageWrapper> ImageWrappers[3] =
			{
				ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG),
				ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG),
				ImageWrapperModule.CreateImageWrapper(EImageFormat::BMP),
			};

			for (auto ImageWrapper : ImageWrappers)
			{
				if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(HttpResponse->GetContent().GetData(), HttpResponse->GetContentLength()))
				{
					TArray64<uint8>* RawData = new TArray64<uint8>();
					const ERGBFormat InFormat = ERGBFormat::BGRA;
					if (ImageWrapper->GetRaw(InFormat, 8, *RawData))
					{
						Texture = UTexture2DDynamic::Create(ImageWrapper->GetWidth(), ImageWrapper->GetHeight());
						if (IsValid(Texture))
						{
							Texture->SRGB = true;
							Texture->UpdateResource();

							FTexture2DDynamicResource* TextureResource = static_cast<FTexture2DDynamicResource*>(Texture->Resource);
							if (TextureResource)
							{
								ENQUEUE_RENDER_COMMAND(FWriteRawDataToTexture)(
									[TextureResource, RawData](FRHICommandListImmediate& RHICmdList)
								{
									WriteRawToTexture_RenderThread(TextureResource, RawData);
								});
								bIsValidImage = true;
								CachedTextures.Add(ImageURL, Texture);
							}
							else
								delete RawData;

							break;
						}
					}
				}
			}
		}
		
		InvokeListener(bIsValidImage, RequestID, ImageURL, Texture);
		RequestHandleToURLs.Remove(RequestID);
#endif
	}
	
	UDownloadImageSubsystem* UDownloadImageLibrary::GetDownloadImageSystem(UObject* WorldContextObject)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject))
		{
			UGameInstance* Gm = World->GetGameInstance();
			return IsValid(Gm) ? Gm->GetSubsystem<UDownloadImageSubsystem>() : nullptr;
		}
		return nullptr;
	}
	int32 UDownloadImageLibrary::RequestDownloadImage(UObject* WorldContextObject, const FString& URL, UObject* Listener)
	{
		UDownloadImageSubsystem* DownloadImageMgr = GetDownloadImageSystem(WorldContextObject);
		return IsValid(DownloadImageMgr) ? DownloadImageMgr->RequestImage(URL, Listener) : INDEX_NONE;
	}
	int32 UDownloadImageLibrary::RequestDownloadImageCallBack(UObject* WorldContextObject, const FString& URL, const FDownloadImageSuccessSignature& OnSuccess, const FDownloadImageFailedSignature& OnFailed)
	{
		UDownloadImageSubsystem* DownloadImageMgr = GetDownloadImageSystem(WorldContextObject);
		return IsValid(DownloadImageMgr) ? DownloadImageMgr->RequestImage(URL, OnSuccess, OnFailed) : INDEX_NONE;
	}

	FSlateBrush UDownloadImageLibrary::MakeBrushFromTextureDynamic(UTexture2DDynamic* Texture, int32 Width, int32 Height)
	{
		if (Texture)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Width = (Width > 0) ? Width : Texture->SizeX;
			Height = (Height > 0) ? Height : Texture->SizeY;
			Brush.ImageSize = FVector2D(Width, Height);
			return Brush;
		}

		return FSlateNoResource();
	}
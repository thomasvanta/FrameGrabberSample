// Fill out your copyright notice in the Description page of Project Settings.

#include "FrameGrabberActor.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "IAssetViewport.h"
#endif

// Sets default values
AFrameGrabberActor::AFrameGrabberActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AFrameGrabberActor::BeginPlay()
{
	Super::BeginPlay();
}

void AFrameGrabberActor::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseFrameGrabber();

	if (CaptureFrameTexture)
	{
		CaptureFrameTexture->ConditionalBeginDestroy();
		CaptureFrameTexture = nullptr;
	}
}

// Called every frame
void AFrameGrabberActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (FrameGrabber.IsValid())
	{
		Capture();
	}
}

void AFrameGrabberActor::Capture()
{
	if (FrameGrabber.IsValid() && CaptureFrameTexture)
	{
		FrameGrabber->CaptureThisFrame(FFramePayloadPtr());
		TArray<FCapturedFrameData> Frames = FrameGrabber->GetCapturedFrames();

		if (Frames.Num())
		{
			FCapturedFrameData& LastFrame = Frames.Last();

			void* TextureData = CaptureFrameTexture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, LastFrame.ColorBuffer.GetData(), LastFrame.ColorBuffer.Num() * 4);
			CaptureFrameTexture->PlatformData->Mips[0].BulkData.Unlock();
			CaptureFrameTexture->UpdateResource();
		}
	}

}

bool  AFrameGrabberActor::StartFrameGrab()
{
	TSharedPtr<FSceneViewport> SceneViewport;

	// Get SceneViewport
	// ( quoted from FRemoteSessionHost::OnCreateChannels() )
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				FSlatePlayInEditorInfo* SlatePlayInEditorSession = GEditor->SlatePlayInEditorMap.Find(Context.ContextHandle);
				if (SlatePlayInEditorSession)
				{
					if (SlatePlayInEditorSession->DestinationSlateViewport.IsValid())
					{
						TSharedPtr<IAssetViewport> DestinationLevelViewport = SlatePlayInEditorSession->DestinationSlateViewport.Pin();
						SceneViewport = DestinationLevelViewport->GetSharedActiveViewport();
					}
					else if (SlatePlayInEditorSession->SlatePlayInEditorWindowViewport.IsValid())
					{
						SceneViewport = SlatePlayInEditorSession->SlatePlayInEditorWindowViewport;
					}
				}
			}
		}
	}
	else
#endif
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		SceneViewport = GameEngine->SceneViewport;
	}
	if (!SceneViewport.IsValid())
	{
		return false;
	}


	EPixelFormat pixelFormat = GetPixelFormatFromRenderTargetFormat(TextureFormat);
	// Setup Texture
	CaptureFrameTexture = UTexture2D::CreateTransient(SceneViewport.Get()->GetSize().X, SceneViewport.Get()->GetSize().Y, pixelFormat);
	CaptureFrameTexture->UpdateResource();

	// Capture Start
	ReleaseFrameGrabber();
	FrameGrabber = MakeShareable(new FFrameGrabber(SceneViewport.ToSharedRef(), SceneViewport->GetSize(), pixelFormat));
	FrameGrabber->StartCapturingFrames();

	return true;
}

void  AFrameGrabberActor::StopFrameGrab()
{
	ReleaseFrameGrabber();
}

void AFrameGrabberActor::ReleaseFrameGrabber()
{
	if (FrameGrabber.IsValid())
	{
		FrameGrabber->StopCapturingFrames();
		FrameGrabber->Shutdown();
		FrameGrabber.Reset();
	}
}

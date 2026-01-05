#include "MyFileHelper.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

void UMyFileHelper::DownloadAndSaveGLB(const FString& URL, const FString& SavePath)
{
	// 创建 HTTP 下载请求
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("GET"));

	// 绑定下载完成后的回调逻辑
	Request->OnProcessRequestComplete().BindLambda([SavePath](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (bWasSuccessful && Response.IsValid())
			{
				const TArray<uint8>& BinaryData = Response->GetContent();

				// 物理路径处理：确保文件夹存在
				FString Directory = FPaths::GetPath(SavePath);
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				if (!PlatformFile.DirectoryExists(*Directory))
				{
					PlatformFile.CreateDirectoryTree(*Directory);
				}

				// 将原始二进制流写入 F 盘
				if (FFileHelper::SaveArrayToFile(BinaryData, *SavePath))
				{
					UE_LOG(LogTemp, Log, TEXT("File Saved Successfully to: %s"), *SavePath);
				}
			}
		});

	Request->ProcessRequest();
}
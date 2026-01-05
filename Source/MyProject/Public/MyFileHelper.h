#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Http.h"
#include "MyFileHelper.generated.h"

UCLASS()
class MYPROJECT_API UMyFileHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 这是一个静态函数，下载完成后会将成功的 SavePath 传给回调
	UFUNCTION(BlueprintCallable, Category = "TripoDownloader")
	static void DownloadAndSaveGLB(const FString& URL, const FString& SavePath);
};
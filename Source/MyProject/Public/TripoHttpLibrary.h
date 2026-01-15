#pragma once

// 1. CoreMinimal 必须放在最前面
#include "CoreMinimal.h" 

// 2. 蓝图函数库基类
#include "Kismet/BlueprintFunctionLibrary.h"

// 3. HTTP 模块引用
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

// 4. 生成的头文件 (必须在最后)
#include "TripoHttpLibrary.generated.h"

// --- 定义委托 (蓝图事件) ---

// 上传成功，返回 ImageToken
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnTripoUploadSuccess, FString, ImageToken);

// 失败回调，返回错误信息
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnTripoUploadFail, FString, ErrorMessage);

// 任务创建成功，返回 TaskID
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnTripoTaskSuccess, FString, TaskID);

// 🟢 [新增] 查询成功-任务还在进行中，返回进度(0.0-1.0)和状态字符串
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnTripoProgress, float, Progress, FString, Status);

// 🟢 [新增] 查询成功-任务已完成，返回模型下载链接 (.glb URL)
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnTripoModelReady, FString, ModelURL);


// 5. 类定义 (保持你的 MYPROJECT_API 不变)
UCLASS()
class MYPROJECT_API UTripoHttpLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ==========================================
	// 1. 上传图片 (Step 1)
	// ==========================================
	UFUNCTION(BlueprintCallable, Category = "Tripo|HTTP")
	static void UploadImageToTripo(FString FilePath, FString ApiKey, FOnTripoUploadSuccess OnSuccess, FOnTripoUploadFail OnFail);

	// ==========================================
	// 2. 发送生成任务 (Step 2)
	// ==========================================
	// 替代 VaRest，用 C++ 直接发送 JSON 请求，返回 TaskID
	UFUNCTION(BlueprintCallable, Category = "Tripo|HTTP")
	static void CreateTripoModel(FString ApiKey, FString ImageToken, FOnTripoTaskSuccess OnSuccess, FOnTripoUploadFail OnFail);

	// ==========================================
	// 3. 🟢 [新增] 查询任务状态 (Step 3)
	// ==========================================
	// 蓝图里的循环逻辑将调用此函数来检查进度
	UFUNCTION(BlueprintCallable, Category = "Tripo|HTTP")
	static void CheckTaskStatus(FString ApiKey, FString TaskID, FOnTripoModelReady OnFinished, FOnTripoProgress OnRunning, FOnTripoUploadFail OnFail);

	// ==========================================
	// 4. 截图裁剪工具
	// ==========================================
	UFUNCTION(BlueprintCallable, Category = "Tripo|Image")
	static bool CropAndSaveImage(FString SourcePath, FString SavePath, int32 StartX, int32 StartY, int32 Width, int32 Height);

private:
	// 内部回调：处理上传结果
	static void OnUploadComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnTripoUploadSuccess OnSuccess, FOnTripoUploadFail OnFail);

	// 内部回调：处理创建任务结果
	static void OnCreateTaskComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnTripoTaskSuccess OnSuccess, FOnTripoUploadFail OnFail);

	// 🟢 [新增] 内部回调：处理查询状态结果
	static void OnCheckStatusComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnTripoModelReady OnFinished, FOnTripoProgress OnRunning, FOnTripoUploadFail OnFail);
};
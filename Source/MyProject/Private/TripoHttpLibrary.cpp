#include "TripoHttpLibrary.h"
#include "Misc/FileHelper.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// 图片处理模块
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

// ==========================================
// 1. 上传图片 (Upload Image) - 强制 JPG
// ==========================================
void UTripoHttpLibrary::UploadImageToTripo(FString FilePath, FString ApiKey, FOnTripoUploadSuccess OnSuccess, FOnTripoUploadFail OnFail)
{
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        OnFail.ExecuteIfBound("Error: Could not load file: " + FilePath);
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    // 必须用 V2
    Request->SetURL("https://api.tripo3d.ai/v2/openapi/upload");
    Request->SetVerb("POST");
    Request->SetHeader("Authorization", "Bearer " + ApiKey);

    FString Boundary = "TripoBoundary_UE5_Client";
    Request->SetHeader("Content-Type", "multipart/form-data; boundary=" + Boundary);

    TArray<uint8> Payload;

    auto WriteString = [&](FString Str) {
        FTCHARToUTF8 Converted(*Str);
        Payload.Append((uint8*)Converted.Get(), Converted.Length());
        };

    FString BeginBoundary = "--" + Boundary + "\r\n";
    WriteString(BeginBoundary);

    // 强制声明为 JPG
    FString FileHeader = "Content-Disposition: form-data; name=\"file\"; filename=\"image.jpg\"\r\n";
    FileHeader += "Content-Type: image/jpeg\r\n\r\n";
    WriteString(FileHeader);

    Payload.Append(FileData);

    FString EndBoundary = "\r\n--" + Boundary + "--\r\n";
    WriteString(EndBoundary);

    Request->SetContent(Payload);
    Request->OnProcessRequestComplete().BindStatic(&UTripoHttpLibrary::OnUploadComplete, OnSuccess, OnFail);
    Request->ProcessRequest();
}

void UTripoHttpLibrary::OnUploadComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnTripoUploadSuccess OnSuccess, FOnTripoUploadFail OnFail)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        OnFail.ExecuteIfBound("Network Error: Request failed.");
        return;
    }

    FString ResponseStr = Response->GetContentAsString();
    UE_LOG(LogTemp, Warning, TEXT("Tripo Upload Raw Response: %s"), *ResponseStr);

    int32 ResponseCode = Response->GetResponseCode();
    if (ResponseCode != 200)
    {
        OnFail.ExecuteIfBound("HTTP Error " + FString::FromInt(ResponseCode) + ": " + ResponseStr);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        int32 Code = JsonObject->GetIntegerField("code");
        if (Code == 0)
        {
            const TSharedPtr<FJsonObject>* DataObj;
            if (JsonObject->TryGetObjectField("data", DataObj))
            {
                FString RealToken;
                if ((*DataObj)->TryGetStringField("image_token", RealToken))
                {
                    OnSuccess.ExecuteIfBound(RealToken);
                    return;
                }
            }
        }
        FString Msg = JsonObject->GetStringField("message");
        OnFail.ExecuteIfBound("Tripo API Error: " + Msg);
    }
}

// ==========================================
// 2. 创建生成任务 (Create Task) - 最终修正版
// ==========================================
void UTripoHttpLibrary::CreateTripoModel(FString ApiKey, FString ImageToken, FOnTripoTaskSuccess OnSuccess, FOnTripoUploadFail OnFail)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();

    Request->SetURL("https://api.tripo3d.ai/v2/openapi/task");
    Request->SetVerb("POST");
    Request->SetHeader("Authorization", "Bearer " + ApiKey);
    Request->SetHeader("Content-Type", "application/json");

    TSharedPtr<FJsonObject> RootObj = MakeShareable(new FJsonObject);
    RootObj->SetStringField("type", "image_to_model");

    // 1. 版本号必须是这个
    RootObj->SetStringField("model_version", "v2.0-20240919");

    TSharedPtr<FJsonObject> FileObj = MakeShareable(new FJsonObject);

    // 2. 类型必须是 "jpg" (对应成功的CMD命令)
    FileObj->SetStringField("type", "jpg");

    // 3. 字段名必须是 "file_token" (对应成功的CMD命令)
    FileObj->SetStringField("file_token", ImageToken);

    RootObj->SetObjectField("file", FileObj);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);

    UE_LOG(LogTemp, Warning, TEXT("Create Task Request Body: %s"), *RequestBody);

    Request->SetContentAsString(RequestBody);
    Request->OnProcessRequestComplete().BindStatic(&UTripoHttpLibrary::OnCreateTaskComplete, OnSuccess, OnFail);
    Request->ProcessRequest();
}

void UTripoHttpLibrary::OnCreateTaskComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnTripoTaskSuccess OnSuccess, FOnTripoUploadFail OnFail)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        OnFail.ExecuteIfBound("Network Error (Task Creation)");
        return;
    }

    FString ResponseStr = Response->GetContentAsString();
    UE_LOG(LogTemp, Log, TEXT("Create Task Response: %s"), *ResponseStr);

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        int32 Code = JsonObject->GetIntegerField("code");
        if (Code == 0)
        {
            const TSharedPtr<FJsonObject>* DataObj;
            if (JsonObject->TryGetObjectField("data", DataObj))
            {
                FString TaskID = (*DataObj)->GetStringField("task_id");
                OnSuccess.ExecuteIfBound(TaskID);
                return;
            }
        }
        FString Msg = JsonObject->GetStringField("message");
        OnFail.ExecuteIfBound("Task Error: " + Msg);
    }
}

// ==========================================
// 3. 裁剪图片 (Crop Image) - 强制转 JPG
// ==========================================
bool UTripoHttpLibrary::CropAndSaveImage(FString SourcePath, FString SavePath, int32 StartX, int32 StartY, int32 Width, int32 Height)
{
    TArray<uint8> RawFileData;
    if (!FFileHelper::LoadFileToArray(RawFileData, *SourcePath)) return false;

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

    EImageFormat DetectedFormat = ImageWrapperModule.DetectImageFormat(RawFileData.GetData(), RawFileData.Num());
    if (DetectedFormat == EImageFormat::Invalid) return false;

    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(DetectedFormat);
    if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num())) return false;

    TArray<uint8> UncompressedBGRA;
    if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA)) return false;

    int32 OrigW = ImageWrapper->GetWidth();
    int32 OrigH = ImageWrapper->GetHeight();

    if (StartX < 0) StartX = 0;
    if (StartY < 0) StartY = 0;
    if (StartX + Width > OrigW) Width = OrigW - StartX;
    if (StartY + Height > OrigH) Height = OrigH - StartY;

    if (Width <= 0 || Height <= 0) return false;

    TArray<uint8> CroppedData;
    CroppedData.AddUninitialized(Width * Height * 4);

    for (int32 y = 0; y < Height; y++)
    {
        int32 SourceIndex = ((StartY + y) * OrigW + StartX) * 4;
        int32 DestIndex = (y * Width) * 4;
        FMemory::Memcpy(&CroppedData[DestIndex], &UncompressedBGRA[SourceIndex], Width * 4);
    }

    // 强制保存为 JPEG (质量 90) 以去除透明度问题
    TSharedPtr<IImageWrapper> SaveWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
    SaveWrapper->SetRaw(CroppedData.GetData(), CroppedData.Num(), Width, Height, ERGBFormat::BGRA, 8);

    return FFileHelper::SaveArrayToFile(SaveWrapper->GetCompressed(90), *SavePath);
}
// ==========================================
// 4. 查询任务状态 (Check Status)
// ==========================================
void UTripoHttpLibrary::CheckTaskStatus(FString ApiKey, FString TaskID, FOnTripoModelReady OnFinished, FOnTripoProgress OnRunning, FOnTripoUploadFail OnFail)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();

    // 拼装查询 URL: https://api.tripo3d.ai/v2/openapi/task/{task_id}
    FString Url = "https://api.tripo3d.ai/v2/openapi/task/" + TaskID;

    Request->SetURL(Url);
    Request->SetVerb("GET"); // 查询必须用 GET
    Request->SetHeader("Authorization", "Bearer " + ApiKey);

    // 绑定回调
    Request->OnProcessRequestComplete().BindStatic(&UTripoHttpLibrary::OnCheckStatusComplete, OnFinished, OnRunning, OnFail);
    Request->ProcessRequest();
}

void UTripoHttpLibrary::OnCheckStatusComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnTripoModelReady OnFinished, FOnTripoProgress OnRunning, FOnTripoUploadFail OnFail)
{
    // 1. 网络错误检查
    if (!bWasSuccessful || !Response.IsValid())
    {
        OnFail.ExecuteIfBound("Network Error (Check Status)");
        return;
    }

    FString ResponseStr = Response->GetContentAsString();

    // 2. 解析 JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        int32 Code = JsonObject->GetIntegerField("code");
        if (Code == 0)
        {
            const TSharedPtr<FJsonObject>* DataObj;
            if (JsonObject->TryGetObjectField("data", DataObj))
            {
                // 获取状态
                FString Status = (*DataObj)->GetStringField("status");
                int32 ProgressInt = (*DataObj)->GetIntegerField("progress");
                float ProgressPercent = (float)ProgressInt / 100.0f;

                // 打印进度日志
                UE_LOG(LogTemp, Log, TEXT("Task Status: %s, Progress: %d%%"), *Status, ProgressInt);

                // --- 分支判断 ---
                if (Status == "success")
                {
                    // 🛑【关键修改】打印完整的成功 JSON，以便调试！
                    UE_LOG(LogTemp, Warning, TEXT("SUCCESS JSON BODY: %s"), *ResponseStr);

                    const TSharedPtr<FJsonObject>* OutputObj;
                    if ((*DataObj)->TryGetObjectField("output", OutputObj))
                    {
                        FString ModelURL;

                        // 🛑【关键修改】尝试多种可能的字段名
                        if ((*OutputObj)->TryGetStringField("model", ModelURL)) {}
                        else if ((*OutputObj)->TryGetStringField("pbr_model", ModelURL)) {} // V2 常见
                        else if ((*OutputObj)->TryGetStringField("base_model", ModelURL)) {}
                        else if ((*OutputObj)->TryGetStringField("glb", ModelURL)) {}

                        if (!ModelURL.IsEmpty())
                        {
                            OnFinished.ExecuteIfBound(ModelURL);
                        }
                        else
                        {
                            OnFail.ExecuteIfBound("JSON Error: Found 'output' but could not find model URL (tried model, pbr_model, base_model)");
                        }
                    }
                    else
                    {
                        OnFail.ExecuteIfBound("JSON Error: 'output' field missing");
                    }
                }
                else if (Status == "running" || Status == "queued")
                {
                    OnRunning.ExecuteIfBound(ProgressPercent, Status);
                }
                else if (Status == "failed" || Status == "cancelled")
                {
                    OnFail.ExecuteIfBound("Task Failed or Cancelled.");
                }
                else
                {
                    OnFail.ExecuteIfBound("Unknown Status: " + Status);
                }
                return;
            }
        }
        FString Msg = JsonObject->GetStringField("message");
        OnFail.ExecuteIfBound("API Error: " + Msg);
    }
    else
    {
        OnFail.ExecuteIfBound("JSON Parse Error");
    }
}
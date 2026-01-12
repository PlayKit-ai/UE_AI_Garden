// Copyright PlayKit. All Rights Reserved.

#include "PlayKitEditorLocalization.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Interfaces/IPluginManager.h"

TUniquePtr<FPlayKitEditorLocalization> FPlayKitEditorLocalization::Instance;

FPlayKitEditorLocalization& FPlayKitEditorLocalization::Get()
{
	if (!Instance.IsValid())
	{
		Instance = MakeUnique<FPlayKitEditorLocalization>();
	}
	return *Instance;
}

FPlayKitEditorLocalization::FPlayKitEditorLocalization()
{
	// Auto-detect language from system
	FString SystemLanguage = FPlatformMisc::GetDefaultLanguage();

	if (SystemLanguage.StartsWith(TEXT("zh-CN")) || SystemLanguage.StartsWith(TEXT("zh_CN")))
	{
		CurrentLanguage = EPlayKitLanguage::SimplifiedChinese;
	}
	else if (SystemLanguage.StartsWith(TEXT("zh-TW")) || SystemLanguage.StartsWith(TEXT("zh_TW")) ||
			 SystemLanguage.StartsWith(TEXT("zh-HK")) || SystemLanguage.StartsWith(TEXT("zh_HK")))
	{
		CurrentLanguage = EPlayKitLanguage::TraditionalChinese;
	}
	else if (SystemLanguage.StartsWith(TEXT("ja")))
	{
		CurrentLanguage = EPlayKitLanguage::Japanese;
	}
	else if (SystemLanguage.StartsWith(TEXT("ko")))
	{
		CurrentLanguage = EPlayKitLanguage::Korean;
	}
	else
	{
		CurrentLanguage = EPlayKitLanguage::English;
	}

	LoadLocalizationData();
}

FString FPlayKitEditorLocalization::GetLanguageCode() const
{
	switch (CurrentLanguage)
	{
	case EPlayKitLanguage::SimplifiedChinese:
		return TEXT("zh-CN");
	case EPlayKitLanguage::TraditionalChinese:
		return TEXT("zh-TW");
	case EPlayKitLanguage::Japanese:
		return TEXT("ja-JP");
	case EPlayKitLanguage::Korean:
		return TEXT("ko-KR");
	default:
		return TEXT("en-US");
	}
}

FString FPlayKitEditorLocalization::GetLanguageDisplayName(EPlayKitLanguage Language)
{
	switch (Language)
	{
	case EPlayKitLanguage::SimplifiedChinese:
		return TEXT("简体中文");
	case EPlayKitLanguage::TraditionalChinese:
		return TEXT("繁體中文");
	case EPlayKitLanguage::Japanese:
		return TEXT("日本語");
	case EPlayKitLanguage::Korean:
		return TEXT("한국어");
	default:
		return TEXT("English");
	}
}

TArray<EPlayKitLanguage> FPlayKitEditorLocalization::GetAvailableLanguages()
{
	return {
		EPlayKitLanguage::English,
		EPlayKitLanguage::SimplifiedChinese,
		EPlayKitLanguage::TraditionalChinese,
		EPlayKitLanguage::Japanese,
		EPlayKitLanguage::Korean
	};
}

void FPlayKitEditorLocalization::SetLanguage(EPlayKitLanguage Language)
{
	if (CurrentLanguage != Language)
	{
		CurrentLanguage = Language;
		LoadLocalizationData();
	}
}

void FPlayKitEditorLocalization::Reload()
{
	LoadLocalizationData();
}

void FPlayKitEditorLocalization::LoadLocalizationData()
{
	LocalizedStrings.Empty();

	// Default English strings (fallback)
	LocalizedStrings.Add(TEXT("window.title"), TEXT("PlayKit SDK Settings"));
	LocalizedStrings.Add(TEXT("header.title"), TEXT("PlayKit SDK"));

	// Authentication
	LocalizedStrings.Add(TEXT("auth.section"), TEXT("Developer Authentication"));
	LocalizedStrings.Add(TEXT("auth.status"), TEXT("Status:"));
	LocalizedStrings.Add(TEXT("auth.notLoggedIn"), TEXT("Not logged in"));
	LocalizedStrings.Add(TEXT("auth.loggedIn"), TEXT("Logged in"));
	LocalizedStrings.Add(TEXT("auth.authenticating"), TEXT("Authenticating..."));
	LocalizedStrings.Add(TEXT("auth.login"), TEXT("Login"));
	LocalizedStrings.Add(TEXT("auth.logout"), TEXT("Logout"));
	LocalizedStrings.Add(TEXT("auth.failed"), TEXT("Authentication failed"));
	LocalizedStrings.Add(TEXT("auth.instructions"), TEXT("Opening browser... Enter code: {Code}"));

	// Game Selection
	LocalizedStrings.Add(TEXT("game.section"), TEXT("Game Selection"));
	LocalizedStrings.Add(TEXT("game.select"), TEXT("Select Game:"));
	LocalizedStrings.Add(TEXT("game.placeholder"), TEXT("Select a game..."));
	LocalizedStrings.Add(TEXT("game.refresh"), TEXT("Refresh"));

	// AI Models
	LocalizedStrings.Add(TEXT("models.section"), TEXT("AI Model Defaults"));
	LocalizedStrings.Add(TEXT("models.chat"), TEXT("Chat Model:"));
	LocalizedStrings.Add(TEXT("models.image"), TEXT("Image Model:"));

	// Advanced
	LocalizedStrings.Add(TEXT("advanced.section"), TEXT("Advanced Settings"));
	LocalizedStrings.Add(TEXT("advanced.customUrl"), TEXT("Custom URL:"));
	LocalizedStrings.Add(TEXT("advanced.ignoreDevToken"), TEXT("Play as Player (ignore developer token)"));
	LocalizedStrings.Add(TEXT("advanced.debugLogging"), TEXT("Enable debug logging"));
	LocalizedStrings.Add(TEXT("advanced.clearPlayerToken"), TEXT("Clear Local Player Token"));

	// About
	LocalizedStrings.Add(TEXT("about.section"), TEXT("About"));
	LocalizedStrings.Add(TEXT("about.version"), TEXT("PlayKit SDK for Unreal Engine v1.0.0"));
	LocalizedStrings.Add(TEXT("about.docs"), TEXT("Documentation"));
	LocalizedStrings.Add(TEXT("about.website"), TEXT("Website"));
	LocalizedStrings.Add(TEXT("about.support"), TEXT("Support"));

	// Load language-specific overrides
	if (CurrentLanguage == EPlayKitLanguage::SimplifiedChinese)
	{
		LocalizedStrings.Add(TEXT("window.title"), TEXT("PlayKit SDK 设置"));
		LocalizedStrings.Add(TEXT("header.title"), TEXT("PlayKit SDK"));
		LocalizedStrings.Add(TEXT("auth.section"), TEXT("开发者认证"));
		LocalizedStrings.Add(TEXT("auth.status"), TEXT("状态："));
		LocalizedStrings.Add(TEXT("auth.notLoggedIn"), TEXT("未登录"));
		LocalizedStrings.Add(TEXT("auth.loggedIn"), TEXT("已登录"));
		LocalizedStrings.Add(TEXT("auth.authenticating"), TEXT("正在认证..."));
		LocalizedStrings.Add(TEXT("auth.login"), TEXT("登录"));
		LocalizedStrings.Add(TEXT("auth.logout"), TEXT("退出登录"));
		LocalizedStrings.Add(TEXT("auth.failed"), TEXT("认证失败"));
		LocalizedStrings.Add(TEXT("auth.instructions"), TEXT("正在打开浏览器... 请输入代码：{Code}"));
		LocalizedStrings.Add(TEXT("game.section"), TEXT("游戏选择"));
		LocalizedStrings.Add(TEXT("game.select"), TEXT("选择游戏："));
		LocalizedStrings.Add(TEXT("game.placeholder"), TEXT("请选择游戏..."));
		LocalizedStrings.Add(TEXT("game.refresh"), TEXT("刷新"));
		LocalizedStrings.Add(TEXT("models.section"), TEXT("AI 模型默认设置"));
		LocalizedStrings.Add(TEXT("models.chat"), TEXT("聊天模型："));
		LocalizedStrings.Add(TEXT("models.image"), TEXT("图像模型："));
		LocalizedStrings.Add(TEXT("advanced.section"), TEXT("高级设置"));
		LocalizedStrings.Add(TEXT("advanced.customUrl"), TEXT("自定义 URL："));
		LocalizedStrings.Add(TEXT("advanced.ignoreDevToken"), TEXT("以玩家身份运行（忽略开发者令牌）"));
		LocalizedStrings.Add(TEXT("advanced.debugLogging"), TEXT("启用调试日志"));
		LocalizedStrings.Add(TEXT("advanced.clearPlayerToken"), TEXT("清除本地玩家令牌"));
		LocalizedStrings.Add(TEXT("about.section"), TEXT("关于"));
		LocalizedStrings.Add(TEXT("about.version"), TEXT("PlayKit SDK for Unreal Engine v1.0.0"));
		LocalizedStrings.Add(TEXT("about.docs"), TEXT("文档"));
		LocalizedStrings.Add(TEXT("about.website"), TEXT("官网"));
		LocalizedStrings.Add(TEXT("about.support"), TEXT("技术支持"));
	}
	else if (CurrentLanguage == EPlayKitLanguage::TraditionalChinese)
	{
		LocalizedStrings.Add(TEXT("window.title"), TEXT("PlayKit SDK 設定"));
		LocalizedStrings.Add(TEXT("auth.section"), TEXT("開發者認證"));
		LocalizedStrings.Add(TEXT("auth.status"), TEXT("狀態："));
		LocalizedStrings.Add(TEXT("auth.notLoggedIn"), TEXT("未登入"));
		LocalizedStrings.Add(TEXT("auth.loggedIn"), TEXT("已登入"));
		LocalizedStrings.Add(TEXT("auth.authenticating"), TEXT("正在認證..."));
		LocalizedStrings.Add(TEXT("auth.login"), TEXT("登入"));
		LocalizedStrings.Add(TEXT("auth.logout"), TEXT("登出"));
		LocalizedStrings.Add(TEXT("auth.failed"), TEXT("認證失敗"));
		LocalizedStrings.Add(TEXT("game.section"), TEXT("遊戲選擇"));
		LocalizedStrings.Add(TEXT("game.select"), TEXT("選擇遊戲："));
		LocalizedStrings.Add(TEXT("game.placeholder"), TEXT("請選擇遊戲..."));
		LocalizedStrings.Add(TEXT("game.refresh"), TEXT("重新整理"));
		LocalizedStrings.Add(TEXT("models.section"), TEXT("AI 模型預設"));
		LocalizedStrings.Add(TEXT("models.chat"), TEXT("聊天模型："));
		LocalizedStrings.Add(TEXT("models.image"), TEXT("圖像模型："));
		LocalizedStrings.Add(TEXT("advanced.section"), TEXT("進階設定"));
		LocalizedStrings.Add(TEXT("advanced.customUrl"), TEXT("自訂 URL："));
		LocalizedStrings.Add(TEXT("advanced.ignoreDevToken"), TEXT("以玩家身份執行（忽略開發者令牌）"));
		LocalizedStrings.Add(TEXT("advanced.debugLogging"), TEXT("啟用除錯日誌"));
		LocalizedStrings.Add(TEXT("advanced.clearPlayerToken"), TEXT("清除本地玩家令牌"));
		LocalizedStrings.Add(TEXT("about.section"), TEXT("關於"));
		LocalizedStrings.Add(TEXT("about.docs"), TEXT("文件"));
		LocalizedStrings.Add(TEXT("about.website"), TEXT("官網"));
		LocalizedStrings.Add(TEXT("about.support"), TEXT("技術支援"));
	}
	else if (CurrentLanguage == EPlayKitLanguage::Japanese)
	{
		LocalizedStrings.Add(TEXT("window.title"), TEXT("PlayKit SDK 設定"));
		LocalizedStrings.Add(TEXT("auth.section"), TEXT("開発者認証"));
		LocalizedStrings.Add(TEXT("auth.status"), TEXT("ステータス："));
		LocalizedStrings.Add(TEXT("auth.notLoggedIn"), TEXT("未ログイン"));
		LocalizedStrings.Add(TEXT("auth.loggedIn"), TEXT("ログイン済み"));
		LocalizedStrings.Add(TEXT("auth.authenticating"), TEXT("認証中..."));
		LocalizedStrings.Add(TEXT("auth.login"), TEXT("ログイン"));
		LocalizedStrings.Add(TEXT("auth.logout"), TEXT("ログアウト"));
		LocalizedStrings.Add(TEXT("auth.failed"), TEXT("認証失敗"));
		LocalizedStrings.Add(TEXT("game.section"), TEXT("ゲーム選択"));
		LocalizedStrings.Add(TEXT("game.select"), TEXT("ゲームを選択："));
		LocalizedStrings.Add(TEXT("game.placeholder"), TEXT("ゲームを選択してください..."));
		LocalizedStrings.Add(TEXT("game.refresh"), TEXT("更新"));
		LocalizedStrings.Add(TEXT("models.section"), TEXT("AIモデルのデフォルト"));
		LocalizedStrings.Add(TEXT("models.chat"), TEXT("チャットモデル："));
		LocalizedStrings.Add(TEXT("models.image"), TEXT("画像モデル："));
		LocalizedStrings.Add(TEXT("advanced.section"), TEXT("詳細設定"));
		LocalizedStrings.Add(TEXT("advanced.customUrl"), TEXT("カスタムURL："));
		LocalizedStrings.Add(TEXT("advanced.ignoreDevToken"), TEXT("プレイヤーとして実行（開発者トークンを無視）"));
		LocalizedStrings.Add(TEXT("advanced.debugLogging"), TEXT("デバッグログを有効化"));
		LocalizedStrings.Add(TEXT("advanced.clearPlayerToken"), TEXT("ローカルプレイヤートークンをクリア"));
		LocalizedStrings.Add(TEXT("about.section"), TEXT("について"));
		LocalizedStrings.Add(TEXT("about.docs"), TEXT("ドキュメント"));
		LocalizedStrings.Add(TEXT("about.website"), TEXT("ウェブサイト"));
		LocalizedStrings.Add(TEXT("about.support"), TEXT("サポート"));
	}
	else if (CurrentLanguage == EPlayKitLanguage::Korean)
	{
		LocalizedStrings.Add(TEXT("window.title"), TEXT("PlayKit SDK 설정"));
		LocalizedStrings.Add(TEXT("auth.section"), TEXT("개발자 인증"));
		LocalizedStrings.Add(TEXT("auth.status"), TEXT("상태:"));
		LocalizedStrings.Add(TEXT("auth.notLoggedIn"), TEXT("로그인되지 않음"));
		LocalizedStrings.Add(TEXT("auth.loggedIn"), TEXT("로그인됨"));
		LocalizedStrings.Add(TEXT("auth.authenticating"), TEXT("인증 중..."));
		LocalizedStrings.Add(TEXT("auth.login"), TEXT("로그인"));
		LocalizedStrings.Add(TEXT("auth.logout"), TEXT("로그아웃"));
		LocalizedStrings.Add(TEXT("auth.failed"), TEXT("인증 실패"));
		LocalizedStrings.Add(TEXT("game.section"), TEXT("게임 선택"));
		LocalizedStrings.Add(TEXT("game.select"), TEXT("게임 선택:"));
		LocalizedStrings.Add(TEXT("game.placeholder"), TEXT("게임을 선택하세요..."));
		LocalizedStrings.Add(TEXT("game.refresh"), TEXT("새로고침"));
		LocalizedStrings.Add(TEXT("models.section"), TEXT("AI 모델 기본값"));
		LocalizedStrings.Add(TEXT("models.chat"), TEXT("채팅 모델:"));
		LocalizedStrings.Add(TEXT("models.image"), TEXT("이미지 모델:"));
		LocalizedStrings.Add(TEXT("advanced.section"), TEXT("고급 설정"));
		LocalizedStrings.Add(TEXT("advanced.customUrl"), TEXT("사용자 정의 URL:"));
		LocalizedStrings.Add(TEXT("advanced.ignoreDevToken"), TEXT("플레이어로 실행 (개발자 토큰 무시)"));
		LocalizedStrings.Add(TEXT("advanced.debugLogging"), TEXT("디버그 로깅 활성화"));
		LocalizedStrings.Add(TEXT("advanced.clearPlayerToken"), TEXT("로컬 플레이어 토큰 지우기"));
		LocalizedStrings.Add(TEXT("about.section"), TEXT("정보"));
		LocalizedStrings.Add(TEXT("about.docs"), TEXT("문서"));
		LocalizedStrings.Add(TEXT("about.website"), TEXT("웹사이트"));
		LocalizedStrings.Add(TEXT("about.support"), TEXT("지원"));
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Loaded localization for language: %s"), *GetLanguageCode());
}

FText FPlayKitEditorLocalization::GetText(const FString& Key) const
{
	const FString* Found = LocalizedStrings.Find(Key);
	if (Found)
	{
		return FText::FromString(*Found);
	}

	// Return key as fallback
	UE_LOG(LogTemp, Warning, TEXT("[PlayKit] Missing localization key: %s"), *Key);
	return FText::FromString(Key);
}

FText FPlayKitEditorLocalization::GetTextFormat(const FString& Key, const FFormatNamedArguments& Args) const
{
	FText BaseText = GetText(Key);
	return FText::Format(BaseText, Args);
}

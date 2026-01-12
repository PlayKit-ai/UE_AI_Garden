// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FPlayKitSDKEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OpenSettingsWindow();

	TSharedPtr<class FUICommandList> PluginCommands;
};

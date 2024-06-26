// UWP STORAGE MANAGER
// Copyright (c) 2023-2024 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

// Thanks to RetroArch/Libretro team for this idea 
// This is improved version of the original idea

#include "StorageAsync.h"

bool ActionPass(winrt::Windows::Foundation::IAsyncAction action)
{
	try {
		WaitTask(action);
		return true;
	}
	catch (...) {
		return false;
	}
}

// Async action such as 'Delete' file
// @action: async action
// return false when action failed
bool ExecuteTask(winrt::Windows::Foundation::IAsyncAction action)
{
	return ActionPass(action);
};

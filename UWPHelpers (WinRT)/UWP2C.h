// UWP STORAGE MANAGER
// Copyright (c) 2023-2024 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

// This is small bridge to invoke UWP Storage manager

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

	void* CreateFileUWP(const char* path, int accessMode, int shareMode, int openMode);
	int GetFileAttributesUWP(const void* name, void* lpFileInformation);
	int DeleteFileUWP(const void* name);

#ifdef __cplusplus
}
#endif

// UWP UI HELPER
// Copyright (c) 2023-2024 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

#include "StorageExtensions.h"

// Input Handler
void ShowInputKeyboard();
void HideInputKeyboard();


// Keys Status
bool IsCapsLockOn();
bool IsShiftOnHold();
bool IsCtrlOnHold();

// Notifications
void ShowToastNotification(std::string title, std::string message);

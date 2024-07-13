// UWP STORAGE MANAGER
// Copyright (c) 2023-2024 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

#pragma once

#include <string>

// Local settings
std::string GetDataFromLocalSettings(std::string key);
bool AddDataToLocalSettings(std::string key, std::string data, bool replace);

// Lookup list
void FillLookupList();

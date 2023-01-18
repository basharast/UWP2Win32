// UWP STORAGE MANAGER
// Copyright (c) 2023 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

// This code must keep support for lower builds (15063+)
// Try always to find possible way to keep that support

#pragma once

#include <string>
#include <list>
#include <vector>
#include <set>

#include "StorageInfo.h"
#include "Common/File/Path.h"
#include "Common/File/DirListing.h"

bool ItemsInfoUWPToFilesInfo(std::list<ItemInfoUWP> items,std::vector<File::FileInfo>* files, const char* filter, std::set<std::string> filters);
bool GetFileInfoUWP(std::string path, File::FileInfo* info);

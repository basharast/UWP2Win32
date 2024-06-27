// UWP STORAGE MANAGER
// Copyright (c) 2023 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

// This code must keep support for lower builds (15063+)
// Try always to find possible way to keep that support

#include "pch.h"
#include <collection.h>

#include "StorageConfig.h"
#include "StorageManager.h"
#include "StorageExtensions.h"
#include "StorageHandler.h"
#include "StorageAsync.h"
#include "StorageAccess.h"
#include "StorageItemW.h"
#include "StorageLog.h"

#include <vector>
#include <stdio.h>
#include <windows.h>
#include <strsafe.h>
#include <shlwapi.h>
#include <iostream>
#include <wincrypt.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <map>
#include <set>

using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::ApplicationModel;
using namespace Windows::System;
using namespace Windows::Storage::Streams;
using namespace Windows::Security::Cryptography;

extern std::list<StorageItemW> FutureAccessItems;

// Simply define `UWP_LEGACY` to force legacy APIs
#if _M_ARM || defined(UWP_LEGACY)
#define TARGET_IS_16299_OR_LOWER
#endif

std::string GetLastErrorAsString() {
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return "Unknown";
	}

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA( // Use FormatMessageA for ANSI
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string message(messageBuffer, size - 2); // -2 to strip \r\n appended at the end
	LocalFree(messageBuffer); // Free the buffer allocated by FormatMessage

	return message;
}

#pragma region Locations
std::string GetWorkingFolder() {
	if (AppWorkingFolder.empty()) {
		return GetLocalFolder();
	}
	else {
		return AppWorkingFolder;
	}
}
void SetWorkingFolder(std::string location) {
	AppWorkingFolder = location;
}
void SetWorkingFolder(std::wstring location) {
	SetWorkingFolder(convert(location));
}
std::string GetInstallationFolder() {
	return convert(Package::Current->InstalledLocation->Path);
}
StorageFolder^ GetLocalStorageFolder() {
	return ApplicationData::Current->LocalFolder;
}
std::string GetLocalFolder() {
	return convert(GetLocalStorageFolder()->Path);
}
std::string GetTempFolder() {
	return convert(ApplicationData::Current->TemporaryFolder->Path);
}
std::string GetTempFile(std::string name) {
	StorageFile^ tmpFile;
	ExecuteTask(tmpFile, ApplicationData::Current->TemporaryFolder->CreateFileAsync(convert(name), CreationCollisionOption::GenerateUniqueName));
	if (tmpFile != nullptr) {
		return convert(tmpFile->Path);
	}
	else {
		return "";
	}
}
std::string GetTempFile(std::wstring name) {
	return GetTempFile(convert(name));
}
std::string GetPicturesFolder() {
	// Requires 'picturesLibrary' capability
	return convert(KnownFolders::PicturesLibrary->Path);
}
std::string GetVideosFolder() {
	// Requires 'videosLibrary' capability
	return convert(KnownFolders::VideosLibrary->Path);
}
std::string GetDocumentsFolder() {
	// Requires 'documentsLibrary' capability
	return convert(KnownFolders::DocumentsLibrary->Path);
}
std::string GetMusicFolder() {
	// Requires 'musicLibrary' capability
	return convert(KnownFolders::MusicLibrary->Path);
}
std::string GetPreviewPath(std::string path) {
	std::string pathView = path;
	windowsPath(pathView);
	std::string appData = GetLocalFolder();
	replace(appData, "\\LocalState", "");
	replace(pathView, appData, "AppData");
	return pathView;
}
bool isLocalState(std::string path) {
	return iequals(GetPreviewPath(path), "LocalState");
}
bool isLocalState(std::wstring path) {
	return isLocalState(convert(path));
}
#pragma endregion

#pragma region Internal
PathUWP PathResolver(PathUWP path) {
	auto root = path.GetDirectory();
	auto newPath = path.ToString();
	if (path.IsRoot() || iequals(root, "/") || iequals(root, "\\")) {
		// System requesting file from app data
		replace(newPath, "/", (GetLocalFolder() + (path.size() > 1 ? "/" : "")));
	}
	path = PathUWP(newPath);
	return path;
}
PathUWP PathResolver(std::string path) {
	return PathResolver(PathUWP(path));
}

std::string ResolvePathUWP(std::string path) {
	return PathResolver(path).ToString();
}

// Return closer parent
StorageItemW GetStorageItemParent(PathUWP path) {
	path = PathResolver(path);
	StorageItemW parent;

	for (auto& fItem : FutureAccessItems) {
		if (isChild(fItem.GetPath(), path.ToString())) {
			if (fItem.IsDirectory()) {
				parent = fItem;
				break;
			}
		}
	}

	return parent;
}

StorageItemW GetStorageItem(PathUWP path, bool createIfNotExists = false, bool forceFolderType = false) {
	// Fill call will be ignored internally after the first call
	FillLookupList();

	path = PathResolver(path);
	StorageItemW item;

	// Look for match in FutureAccessItems
	for (auto& fItem : FutureAccessItems) {
		if (fItem.Equal(path)) {
			item = fItem;
			break;
		}
	}

	if (!item.IsValid()) {
		// Look for match inside FutureAccessFolders
		for (auto& fItem : FutureAccessItems) {
			if (fItem.IsDirectory()) {
				IStorageItem^ storageItem;
				if (fItem.Contains(path, storageItem)) {
					item = StorageItemW(storageItem);
					break;
				}
			}
		}
	}

	if (!item.IsValid() && createIfNotExists) {
		// Create and return new folder
		auto parent = GetStorageItemParent(path);
		if (parent.IsValid()) {
			if (!forceFolderType) {
				// File creation must be called in this case
				// Create folder usually will be called from 'CreateDirectory'
				item = StorageItemW(parent.CreateFile(path));
			}
			else {
				item = StorageItemW(parent.CreateFolder(path));
			}
		}
	}
	return item;
}

StorageItemW GetStorageItem(std::string path, bool createIfNotExists = false, bool forceFolderType = false) {
	return GetStorageItem(PathUWP(path), createIfNotExists, forceFolderType);
}

std::list<StorageItemW> GetStorageItemsByParent(PathUWP path) {
	path = PathResolver(path);
	std::list<StorageItemW> items;

	// Look for match in FutureAccessItems
	for (auto& fItem : FutureAccessItems) {
		if (isParent(path.ToString(), fItem.GetPath(), fItem.GetName())) {
			items.push_back(fItem);
		}
	}

	return items;
}

std::list<StorageItemW> GetStorageItemsByParent(std::string path) {
	return GetStorageItemsByParent(PathUWP(path));
}

bool IsContainsAccessibleItems(PathUWP path) {
	path = PathResolver(path);

	for (auto& fItem : FutureAccessItems) {
		if (isParent(path.ToString(), fItem.GetPath(), fItem.GetName())) {
			return true;
		}
	}

	return false;
}

bool IsContainsAccessibleItems(std::string path) {
	return IsContainsAccessibleItems(PathUWP(path));
}
bool IsContainsAccessibleItems(std::wstring path) {
	return IsContainsAccessibleItems(convert(path));
}

bool IsRootForAccessibleItems(PathUWP path, std::list<std::string>& subRoot, bool breakOnFirstMatch = false) {
	path = PathResolver(path);

	for (auto& fItem : FutureAccessItems) {
		if (isChild(path.ToString(), fItem.GetPath())) {
			if (breakOnFirstMatch) {
				// Just checking, we don't need to loop for each item
				return true;
			}
			auto sub = getSubRoot(path.ToString(), fItem.GetPath());

			// This check can be better, but that's how I can do it in C++
			if (!ends_with(sub, ":")) {
				bool alreadyAdded = false;
				for each (auto sItem in subRoot) {
					if (iequals(sItem, sub)) {
						alreadyAdded = true;
						break;
					}
				}
				if (!alreadyAdded) {
					subRoot.push_back(sub);
				}
			}
		}
	}
	return !subRoot.empty();
}

bool IsRootForAccessibleItems(std::string path, std::list<std::string>& subRoot, bool breakOnFirstMatch = false) {
	return IsRootForAccessibleItems(PathUWP(path), subRoot, breakOnFirstMatch);
}
bool IsRootForAccessibleItems(std::string path) {
	std::list<std::string> tmp;
	return IsRootForAccessibleItems(path, tmp, true);
}
bool IsRootForAccessibleItems(std::wstring path) {
	return IsRootForAccessibleItems(convert(path));
}
#pragma endregion

#pragma region Functions
bool CreateIfNotExists(int openMode) {
	switch (openMode)
	{
	case CREATE_ALWAYS:
	case OPEN_ALWAYS:
	case CREATE_NEW:
		return true;
	default:
		return false;
	}
}

HANDLE CreateFileAPI(std::string path, long accessMode, long shareMode, long openMode) {
#ifdef TARGET_IS_16299_OR_LOWER
	HANDLE hFile = CreateFile2(convertToLPCWSTR(path), accessMode, shareMode, openMode, nullptr);
#else
	// If the item was in access future list, this will work fine
	HANDLE hFile = CreateFile2FromAppW(convertToLPCWSTR(path), accessMode, shareMode, openMode, nullptr);
#endif
	return hFile;
}
HANDLE CreateFileUWP(std::string path, long accessMode, long shareMode, long openMode) {
	HANDLE handle = CreateFileAPI(path, accessMode, shareMode, openMode);
	if ((!handle || handle == INVALID_HANDLE_VALUE) && IsValidUWP(path)) {
		bool createIfNotExists = CreateIfNotExists(openMode);
		auto storageItem = GetStorageItem(path, createIfNotExists);

		if (storageItem.IsValid()) {
			UWP_DEBUG_LOG(UWPSMT, "Getting handle (%s)", path.c_str());
			HRESULT hr = storageItem.GetHandle(&handle, accessMode, shareMode);
			if (hr == E_FAIL) {
				handle = INVALID_HANDLE_VALUE;
			}
		}
		else {
			handle = INVALID_HANDLE_VALUE;
			UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", path.c_str());
		}
	}
	return handle;
}

HANDLE CreateFileUWP(std::wstring path, long accessMode, long shareMode, long openMode) {
	auto pathString = convert(path);
	return CreateFileUWP(pathString, accessMode, shareMode, openMode);
}

std::map<std::string, bool> accessState;
bool CheckDriveAccess(std::string driveName, bool checkIfContainsFutureAccessItems) {
	bool state = false;

	auto keyIter = accessState.find(driveName);
	if (keyIter != accessState.end()) {
		state = keyIter->second;
	}
	else {
		try {
			auto dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
			auto dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
			auto dwCreationDisposition = CREATE_ALWAYS;

			auto testFile = std::string(driveName);
			testFile.append("\\.UWPAccessCheck");
#if defined(_M_ARM)
			HANDLE h = CreateFile2(convertToLPCWSTR(testFile), dwDesiredAccess, dwShareMode, dwCreationDisposition, nullptr);
#else
			HANDLE h = CreateFile2FromAppW(convertToLPCWSTR(testFile), dwDesiredAccess, dwShareMode, dwCreationDisposition, nullptr);
#endif
			if (h != INVALID_HANDLE_VALUE) {
				state = true;
				CloseHandle(h);
#if defined(_M_ARM)
				DeleteFileW(convertToLPCWSTR(testFile));
#else
				DeleteFileFromAppW(convertToLPCWSTR(testFile));
#endif
			}
			accessState.insert(std::make_pair(driveName, state));
		}
		catch (...) {
		}
	}

	if (!state && checkIfContainsFutureAccessItems) {
		// Consider the drive accessible in case it contain files/folder selected before to avoid empty results
		state = IsRootForAccessibleItems(driveName) || IsContainsAccessibleItems(driveName);
	}
	return state;
}
bool CheckDriveAccess(std::wstring driveName, bool checkIfContainsFutureAccessItems) {
	return CheckDriveAccess(convert(driveName), checkIfContainsFutureAccessItems);
}

bool IsValidUWP(std::string path, bool allowForAppData) {
	auto p = PathResolver(path);

	//Check valid path
	if (p.Type() == PathTypeUWP::UNDEFINED || !p.IsAbsolute()) {
		// Nothing to do here
		UWP_VERBOSE_LOG(UWPSMT, "File is not valid (%s)", p.ToString().c_str());
		return false;
	}


	bool state = false;
	if (!allowForAppData) {
		auto resolvedPathStr = p.ToString();
		if (ends_with(resolvedPathStr, "LocalState") || ends_with(resolvedPathStr, "TempState") || ends_with(resolvedPathStr, "LocalCache")) {
			state = true;
		}
		else
			if (isChild(GetLocalFolder(), resolvedPathStr)) {
				state = true;
			}
			else if (isChild(GetInstallationFolder(), resolvedPathStr)) {
				state = true;
			}
			else if (isChild(GetTempFolder(), resolvedPathStr)) {
				state = true;
			}

		if (!state)
		{
			auto p = PathUWP(path);
			std::string driveName = p.GetRootVolume().ToString();
			state = CheckDriveAccess(driveName, false);
		}
	}
	return !state;
}
bool IsValidUWP(std::wstring path, bool allowForAppData) {
	return IsValidUWP(convert(path), allowForAppData);
}

bool IsExistsAPI(std::string path) {
	auto p = PathResolver(path);
	WIN32_FILE_ATTRIBUTE_DATA data{};
#ifdef TARGET_IS_16299_OR_LOWER
	if (GetFileAttributesExW(p.ToWString().c_str(), GetFileExInfoStandard, &data) || data.dwFileAttributes == INVALID_FILE_ATTRIBUTES) {
		return true;
	}
#else
	// If the item was in access future list, this will work fine
	if (GetFileAttributesExFromAppW(p.ToWString().c_str(), GetFileExInfoStandard, &data) || data.dwFileAttributes == INVALID_FILE_ATTRIBUTES) {
		return true;
	}
#endif
	if (!IsValidUWP(path)) {
		// If folder is not accessible but is part of accessible items
		// consider it exists
		std::list<std::string> tmp;
		if (IsRootForAccessibleItems(path, tmp, true)) {
			return true;
		}
	}
	return false;
}
bool IsExistsUWP(std::string path) {
	bool defaultState = IsExistsAPI(path);
	if (!defaultState && IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			return true;
		}

		// If folder is not accessible but contains accessible items
		// consider it exists
		if (IsContainsAccessibleItems(path)) {
			return true;
		}

		// If folder is not accessible but is part of accessible items
		// consider it exists
		std::list<std::string> tmp;
		if (IsRootForAccessibleItems(path, tmp, true)) {
			return true;
		}
	}
	// UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", path.c_str());
	return false;
}

bool IsExistsUWP(std::wstring path) {
	return IsExistsUWP(convert(path));
}

bool IsDirectoryAPI(std::string path) {
	auto p = PathResolver(path);
	WIN32_FILE_ATTRIBUTE_DATA data{};
#ifdef TARGET_IS_16299_OR_LOWER
	if (GetFileAttributesExW(p.ToWString().c_str(), GetFileExInfoStandard, &data) || data.dwFileAttributes == INVALID_FILE_ATTRIBUTES) {
		DWORD result = data.dwFileAttributes;
		return (result & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
	}
#else
	// If the item was in access future list, this will work fine
	if (GetFileAttributesExFromAppW(p.ToWString().c_str(), GetFileExInfoStandard, &data) || data.dwFileAttributes == INVALID_FILE_ATTRIBUTES) {
		DWORD result = data.dwFileAttributes;
		return (result & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
	}
#endif
	if (!IsValidUWP(path)) {
		// If folder is not accessible but is part of accessible items
		// consider it folder
		std::list<std::string> tmp;
		if (IsRootForAccessibleItems(path, tmp, true)) {
			return true;
		}
	}
	return false;
}
bool IsDirectoryUWP(std::string path) {
	bool defaultState = IsDirectoryAPI(path);
	if (!defaultState && IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			if (storageItem.IsDirectory()) {
				return true;
			}
		}
	}
	return false;
}

bool IsDirectoryUWP(std::wstring path) {
	return IsDirectoryUWP(convert(path));
}

FILE* GetFileStreamAPI(std::string path, const char* mode) {
	// Try it with fopen may work within the accessible places (installation folder, app data folder, maybe HDD/SSD with cap. added)
	FILE* file = fopen(path.c_str(), mode);
	return file;
}
FILE* GetFileStream(std::string path, const char* mode) {
	FILE* file = GetFileStreamAPI(path, mode);
	if (!file && IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			file = storageItem.GetStream(mode);
		}
		else {
			// Forward the request to parent folder
			auto p = PathUWP(path);
			auto itemName = p.GetFilename();
			auto rootPath = p.GetDirectory();
			if (IsValidUWP(rootPath)) {
				storageItem = GetStorageItem(rootPath);
				if (storageItem.IsValid()) {
					file = storageItem.GetFileStream(itemName, mode);
				}
				else {
					UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", rootPath.c_str());
					UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", path.c_str());
				}
			}
		}
	}

	return file;
}

FILE* GetFileStream(std::wstring path, const char* mode) {
	return GetFileStream(convert(path), mode);
}

FILE* GetFileStreamFromApp(std::string path, const char* mode) {

	FILE* file = GetFileStreamAPI(path, mode);
	if (!file) {
		auto pathResolved = PathUWP(ResolvePathUWP(path));
		HANDLE handle = INVALID_HANDLE_VALUE;

		auto fileMode = GetFileMode(mode);
		if (fileMode) {
			handle = CreateFile2(pathResolved.ToWString().c_str(), fileMode->dwDesiredAccess, fileMode->dwShareMode, fileMode->dwCreationDisposition, nullptr);
		}
		if (handle != INVALID_HANDLE_VALUE) {
			file = _fdopen(_open_osfhandle((intptr_t)handle, fileMode->flags), mode);
		}
	}
	return file;
}
FILE* GetFileStreamFromApp(std::wstring path, const char* mode) {
	return GetFileStreamFromApp(convert(path), mode);
}

#pragma region Content Helpers
ItemInfoUWP GetFakeFolderInfo(std::string folder) {
	ItemInfoUWP info;
	auto folderPath = PathUWP(folder);
	info.name = folderPath.GetFilename();
	info.fullName = folderPath.ToString();

	info.isDirectory = true;

	info.size = 1;
	info.lastAccessTime = 1000;
	info.lastWriteTime = 1000;
	info.changeTime = 1000;
	info.creationTime = 1000;

	info.attributes = FILE_ATTRIBUTE_DIRECTORY;

	return info;
}

#pragma endregion

// Not sure if we have UWP alternative API here?
std::list<ItemInfoUWP> GetFolderContents(std::string path, bool deepScan) {
	std::list<ItemInfoUWP> contents;

	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {

			// Files
			// deepScan is slow, try to avoid it
			auto rfiles = deepScan ? storageItem.GetAllFiles() : storageItem.GetFiles();
			for each (auto file in rfiles) {
				contents.push_back(file.GetFileInfo());
			}

			// Folders
			// deepScan is slow, try to avoid it
			auto rfolders = deepScan ? storageItem.GetAllFolders() : storageItem.GetFolders();
			for each (auto folder in rfolders) {
				contents.push_back(folder.GetFolderInfo());
			}
		}
		else {
			UWP_ERROR_LOG(UWPSMT, "Cannot get contents!, checking for other options.. (%s)", path.c_str());
		}
	}

	if (contents.size() == 0) {
		// Folder maybe not accessible or not exists
			// if not accessible, maybe some items inside it were selected before
			// and they already in our accessible list
		if (IsContainsAccessibleItems(path)) {
			UWP_DEBUG_LOG(UWPSMT, "Folder contains accessible items (%s)", path.c_str());

			// Check contents
			auto cItems = GetStorageItemsByParent(path);
			if (!cItems.empty()) {
				for each (auto item in cItems) {
					UWP_VERBOSE_LOG(UWPSMT, "Appending accessible item (%s)", item.GetPath().c_str());
					contents.push_back(item.GetItemInfo());
				}
			}
		}
		else
		{
			// Check if this folder is root for accessible item
			// then add fake folder as sub root to avoid empty results
			std::list<std::string> subRoot;
			if (IsRootForAccessibleItems(path, subRoot)) {
				UWP_DEBUG_LOG(UWPSMT, "Folder is root for accessible items (%s)", path.c_str());

				if (!subRoot.empty()) {
					for each (auto sItem in subRoot) {
						UWP_VERBOSE_LOG(UWPSMT, "Appending fake folder (%s)", sItem.c_str());
						contents.push_back(GetFakeFolderInfo(sItem));
					}
				}
			}
			else {
				UWP_ERROR_LOG(UWPSMT, "Cannot get any content!.. (%s)", path.c_str());
			}
		}
	}
	return contents;
}
std::list<ItemInfoUWP> GetFolderContents(std::wstring path, bool deepScan) {
	return GetFolderContents(convert(path), deepScan);
}

ItemInfoUWP GetItemInfoUWP(std::string path) {
	ItemInfoUWP info;
	info.size = -1;
	info.attributes = INVALID_FILE_ATTRIBUTES;

	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			info = storageItem.GetItemInfo();
		}
		else {
			UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", path.c_str());
		}
	}

	return info;
}

ItemInfoUWP GetItemInfoUWP(std::wstring path) {
	return GetItemInfoUWP(convert(path));
}
#pragma endregion

#pragma region Basics
int64_t GetSizeUWP(std::string path) {
	int64_t size = 0;
	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			size = storageItem.GetSize();
		}
		else {
			UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", path.c_str());
		}
	}
	return size;
}

int64_t GetSizeUWP(std::wstring path) {
	return GetSizeUWP(convert(path));
}

BOOL DeleteFileAPI(std::string path) {
	auto convertedPath = convertToLPCWSTR(path);
#ifdef TARGET_IS_16299_OR_LOWER
	return DeleteFileW(convertedPath) != 0;
#else
	// If the item was in access future list, this will work fine
	return DeleteFileFromAppW(convertedPath) != 0;
#endif
}
bool DeleteUWP(std::string path) {
	bool state = DeleteFileAPI(path);
	if (!state && IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			UWP_DEBUG_LOG(UWPSMT, "Delete (%s)", path.c_str());
			state = storageItem.Delete();
		}
		else {
			UWP_DEBUG_LOG(UWPSMT, "Couldn't find or access (%s)", path.c_str());
		}
	}

	return state;
}

bool DeleteUWP(std::wstring path) {
	return DeleteUWP(convert(path));
}

BOOL CreateDirectoryAPI(std::string path, bool replaceExisting) {
	auto convertedPath = convertToLPCWSTR(path);
#ifdef TARGET_IS_16299_OR_LOWER
	auto state = CreateDirectoryW(convertedPath, NULL);
#else
	// If the item was in access future list, this will work fine
	auto state = CreateDirectoryFromAppW(convertedPath, NULL);
#endif
	if (state == 0 && replaceExisting && GetLastError() == ERROR_ALREADY_EXISTS) {
		// Force replace
		UWP_WARN_LOG(UWPSMT, "Folder already exists, replace folder requested, %s", path.c_str());
		if (DeleteUWP(path)) {
#ifdef TARGET_IS_16299_OR_LOWER
			auto state = CreateDirectoryW(convertedPath, NULL);
#else
			auto state = CreateDirectoryFromAppW(convertedPath, NULL);
#endif
		}
		else {
			auto lastError = GetLastErrorAsString();
			UWP_ERROR_LOG(UWPSMT, "Cannot replace folder, %s: (%s)", lastError.c_str(), path.c_str());
		}
	}
	return state != 0;
}
bool CreateDirectoryUWP(std::string path, bool replaceExisting) {
	bool state = CreateDirectoryAPI(path, replaceExisting);
	if (!state && IsValidUWP(path)) {
		auto p = PathUWP(path);
		auto itemName = p.GetFilename();
		auto rootPath = p.GetDirectory();

		if (IsValidUWP(rootPath)) {
			auto storageItem = GetStorageItem(rootPath);
			if (storageItem.IsValid()) {
				UWP_DEBUG_LOG(UWPSMT, "Create new folder (%s)", path.c_str());
				state = storageItem.CreateFolder(itemName, replaceExisting);
			}
			else {
				UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", rootPath.c_str());
			}
		}
	}
	return state;
}

bool CreateDirectoryUWP(std::wstring path, bool replaceExisting) {
	return CreateDirectoryUWP(convert(path), replaceExisting);
}

// TODO: Add overwrite option
BOOL CopyAPI(std::string path, std::string dest) {
	auto convertedPath = convertToLPCWSTR(path);
	auto convertedDestPath = convertToLPCWSTR(dest);
#ifdef TARGET_IS_16299_OR_LOWER
	return CopyFileW(convertedPath, convertedDestPath, TRUE) != 0;
#else
	// If the item was in access future list, this will work fine
	return CopyFileFromAppW(convertedPath, convertedDestPath, TRUE) != 0;
#endif
}
bool CopyUWP(std::string path, std::string dest) {
	bool state = CopyAPI(path, dest);

	if (!state && IsValidUWP(path, true) && IsValidUWP(dest, true)) {
		auto srcStorageItem = GetStorageItem(path);
		if (srcStorageItem.IsValid()) {
			auto destDir = dest;
			auto srcName = srcStorageItem.GetName();
			auto dstPath = PathUWP(dest);
			auto dstName = dstPath.GetFilename();
			// Destination must be parent folder
			destDir = dstPath.GetDirectory();
			auto dstStorageItem = GetStorageItem(destDir, true, true);
			if (dstStorageItem.IsValid()) {
				UWP_DEBUG_LOG(UWPSMT, "Copy (%s) to (%s)", path.c_str(), dest.c_str());
				state = srcStorageItem.Copy(dstStorageItem, dstName);
			}
			else {
				UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", dest.c_str());
			}
		}
		else {
			UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", path.c_str());
		}
	}

	return state;
}

bool CopyUWP(std::wstring path, std::wstring dest) {
	return CopyUWP(convert(path), convert(dest));
}

// TODO: Add overwrite option
BOOL MoveAPI(std::string path, std::string dest) {
	auto convertedPath = convertToLPCWSTR(path);
	auto convertedDestPath = convertToLPCWSTR(dest);
#ifdef TARGET_IS_16299_OR_LOWER
	return MoveFileExW(convertedPath, convertedDestPath, NULL) != 0;
#else
	// If the item was in access future list, this will work fine
	return MoveFileFromAppW(convertedPath, convertedDestPath) != 0;
#endif
}
bool MoveUWP(std::string path, std::string dest) {
	bool state = MoveAPI(path, dest);

	if (!state && IsValidUWP(path, true) && IsValidUWP(dest, true)) {
		auto srcStorageItem = GetStorageItem(path);

		if (srcStorageItem.IsValid()) {
			auto destDir = dest;
			auto srcName = srcStorageItem.GetName();
			auto dstPath = PathUWP(dest);
			auto dstName = dstPath.GetFilename();
			// Destination must be parent folder
			destDir = dstPath.GetDirectory();
			auto dstStorageItem = GetStorageItem(destDir, true, true);
			if (dstStorageItem.IsValid()) {
				UWP_DEBUG_LOG(UWPSMT, "Move (%s) to (%s)", path.c_str(), dest.c_str());
				state = srcStorageItem.Move(dstStorageItem, dstName);
			}
			else {
				UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", dest.c_str());
			}
		}
		else {
			UWP_ERROR_LOG(UWPSMT, "Couldn't find or access (%s)", path.c_str());
		}
	}

	return state;
}

bool MoveUWP(std::wstring path, std::wstring dest) {
	return MoveUWP(convert(path), convert(dest));
}

bool RenameUWP(std::string oldname, std::string newname) {
	// Not sure about testing using Move API here?
	bool state = MoveAPI(oldname, newname);

	auto srcRoot = PathUWP(oldname).GetDirectory();
	auto dstRoot = PathUWP(newname).GetDirectory();
	// Check if system using rename to move
	if (iequals(srcRoot, dstRoot)) {
		auto srcStorageItem = GetStorageItem(oldname);
		if (srcStorageItem.IsValid()) {
			UWP_DEBUG_LOG(UWPSMT, "Rename (%s) to (%s)", oldname.c_str(), newname.c_str());
			state = srcStorageItem.Rename(newname);
		}
		else {
			UWP_DEBUG_LOG(UWPSMT, "Couldn't find or access (%s)", oldname.c_str());
		}
	}
	else {
		UWP_DEBUG_LOG(UWPSMT, " Rename used as move -> call move (%s) to (%s)", oldname.c_str(), newname.c_str());
		state = MoveUWP(oldname, newname);
	}

	return state;
}

bool RenameUWP(std::wstring oldname, std::wstring newname) {
	return RenameUWP(convert(oldname), convert(newname));
}
#pragma endregion

#pragma region File Content
std::string readFile(FILE* file) {
	if (!file) {
		std::cerr << "File pointer is null!" << std::endl;
		return "";
	}

	// Seek to the end to determine the size
	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	rewind(file);

	if (fileSize < 0) {
		std::cerr << "Failed to determine file size!" << std::endl;
		return "";
	}

	// Allocate memory for the file content
	std::vector<char> buffer(fileSize);

	// Read the file into the buffer
	size_t bytesRead = fread(buffer.data(), sizeof(char), fileSize, file);

	// Check for BOM and skip if present
	const unsigned char* uBuffer = reinterpret_cast<const unsigned char*>(buffer.data());
	if (bytesRead >= 3 && uBuffer[0] == 0xEF && uBuffer[1] == 0xBB && uBuffer[2] == 0xBF) {
		return std::string(buffer.data() + 3, bytesRead - 3);
	}

	// Ensure null-terminated string if needed
	buffer.push_back('\0');

	return std::string(buffer.data(), bytesRead);
}

std::string GetFileContent(std::string path, const char* mode) {
	std::string content;

	// Open the file using fopen
	FILE* file = GetFileStream(path, mode);
	if (file) {
		content = readFile(file);
	}
	else {
		UWP_ERROR_LOG(UWPSMT, "Cannot open file: %s", GetLastErrorAsString().c_str());
	}

	return content;
}
std::string GetFileContent(std::wstring path, const char* mode) {
	return GetFileContent(convert(path), mode);
}

// Helper function to generate a unique backup file name
void backupFile(std::string originalPath) {
	std::time_t t = std::time(nullptr);
	char buffer[64];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H-%M-%S", std::localtime(&t));

	auto filePath = PathUWP(originalPath);
	std::string originalName = filePath.GetFilename();
	std::string::size_type pos = originalName.find_last_of('.');
	std::string backupName = originalName.substr(0, pos) + " (" + buffer + ")" + originalName.substr(pos);

	std::string dataFolder = GetWorkingFolder() + "\\backups";
	CreateDirectoryUWP(dataFolder, false);

	std::string backupPath = dataFolder + "\\" + backupName;

	if (!CopyUWP(filePath.ToString(), backupPath)) {
		std::string errorMessage = "Cannot create backup folder: using native UWP";
		UWP_ERROR_LOG(UWPSMT, errorMessage.c_str());
	}
}

bool writeFile(const std::string& originalPath, FILE* file, const std::string& content, bool backup) {
	if (!file) {
		std::string errorMessage = "Invalid file pointer";
		UWP_ERROR_LOG(UWPSMT, errorMessage.c_str());
		return false;
	}

	if (backup) {
		// Generate a unique backup file name and save the backup
		backupFile(originalPath);
	}

	// Write content to the original file
	fseek(file, 0, SEEK_SET);
	size_t written = fwrite(content.c_str(), sizeof(char), content.size(), file);
	fflush(file); // Ensure data is written to the file

	return written == content.size();
}

bool writeFile(const std::string& originalPath, StorageFile^ file, const std::string& content, bool backup) {
	bool state = false;
	try {
		if (backup) {
			// Generate a unique backup file name and save the backup
			backupFile(originalPath);
		}

		// Write content to the original file
		if (!ExecuteTask(FileIO::WriteTextAsync(file, convert(content)))) {
			state = false;
		}
		else {
			state = true;
		}
	}
	catch (const std::exception& e) {
		std::string errorMessage = "Exception: ";
		errorMessage += std::string(e.what());
		UWP_ERROR_LOG(UWPSMT, errorMessage.c_str());
		state = false;
	}
	return state;
}

bool PutFileContents(std::string path, std::string content, const char* mode, bool backup) {
	bool state = false;
	// Open the file using fopen
	FILE* file = GetFileStream(path, mode);
	if (file) {
		state = writeFile(path, file, content, backup);
	}
	else {
		UWP_ERROR_LOG(UWPSMT, "Cannot open file: %s", GetLastErrorAsString().c_str());
	}

	return state;
}
bool PutFileContents(std::wstring path, std::wstring content, const char* mode, bool backup) {
	return PutFileContents(convert(path), convert(content), mode, backup);
}
#pragma endregion

#pragma region Helpers
bool OpenFile(std::string path) {
	bool state = false;

	auto storageItem = GetStorageItem(path);
	if (storageItem.IsValid()) {
		if (!storageItem.IsDirectory()) {
			ExecuteTask(state, Windows::System::Launcher::LaunchFileAsync(storageItem.GetStorageFile()), false);
		}
	}
	else {
		auto uri = ref new Windows::Foundation::Uri(convert(path));
		ExecuteTask(state, Windows::System::Launcher::LaunchUriAsync(uri), false);
	}
	return state;
}

bool OpenFile(std::wstring path) {
	return OpenFile(convert(path));
}

bool OpenFolder(std::string path) {
	bool state = false;
	PathUWP itemPath(path);
	Platform::String^ wString = ref new Platform::String(itemPath.ToWString().c_str());
	StorageFolder^ storageItem;
	ExecuteTask(storageItem, StorageFolder::GetFolderFromPathAsync(wString));
	if (storageItem != nullptr) {
		ExecuteTask(state, Windows::System::Launcher::LaunchFolderAsync(storageItem), false);
	}
	else {
		// Try as it's file
		PathUWP parent = PathUWP(itemPath.GetDirectory());
		Platform::String^ wParentString = ref new Platform::String(parent.ToWString().c_str());

		ExecuteTask(storageItem, StorageFolder::GetFolderFromPathAsync(wParentString));
		if (storageItem != nullptr) {
			ExecuteTask(state, Windows::System::Launcher::LaunchFolderAsync(storageItem), false);
		}
	}
	return state;
}

bool OpenFolder(std::wstring path) {
	return OpenFolder(convert(path));
}

bool GetDriveFreeSpace(PathUWP path, int64_t& space) {

	bool state = false;
	Platform::String^ wString = ref new Platform::String(path.ToWString().c_str());
	StorageFolder^ storageItem;
	ExecuteTask(storageItem, StorageFolder::GetFolderFromPathAsync(wString));
	if (storageItem != nullptr) {
		Platform::String^ freeSpaceKey = ref new Platform::String(L"System.FreeSpace");
		Platform::Collections::Vector<Platform::String^>^ propertiesToRetrieve = ref new Platform::Collections::Vector<Platform::String^>();
		propertiesToRetrieve->Append(freeSpaceKey);
		Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ result;
		ExecuteTask(result, storageItem->Properties->RetrievePropertiesAsync(propertiesToRetrieve));
		if (result != nullptr && result->Size > 0) {
			try {
				auto value = result->Lookup(L"System.FreeSpace");
				space = (uint64_t)value;
				state = true;
			}
			catch (...) {

			}
		}
	}

	return state;
}

bool IsFirstStart() {
	auto firstrun = GetDataFromLocalSettings("first_run");
	AddDataToLocalSettings("first_run", "done", true);
	return firstrun.empty();
}
#pragma endregion

#pragma region Logs
// Get log file name
std::string currentLogFile;
std::string getLogFileName() {
	//Initial new name each session/launch
	if (currentLogFile.empty() || currentLogFile.size() == 0) {
		std::time_t now = std::time(0);
		char mbstr[100];
		std::strftime(mbstr, 100, "ppsspp %d-%m-%Y (%T).txt", std::localtime(&now));
		std::string formatedDate(mbstr);
		std::replace(formatedDate.begin(), formatedDate.end(), ':', '-');
		currentLogFile = formatedDate;
	}

	return currentLogFile;
}

// Get current log file location
StorageFolder^ GetLogsStorageFolder() {
	// Ensure 'LOGS' folder is created
	auto workingFolder = GetStorageItem(GetWorkingFolder());
	StorageFolder^ logsFolder;
	if (workingFolder.IsValid()) {
		auto workingStorageFolder = workingFolder.GetStorageFolder();
		ExecuteTask(logsFolder, workingStorageFolder->CreateFolderAsync("LOGS", CreationCollisionOption::OpenIfExists));
	}
	return logsFolder;
}
std::string GetLogFile() {
	std::string logFilePath = "";

	// Ensure 'LOGS' folder is created
	StorageFolder^ logsFolder = GetLogsStorageFolder();

	if (logsFolder != nullptr) {
		auto logFileName = convert(getLogFileName());
		StorageFile^ logFile;
		ExecuteTask(logFile, logsFolder->CreateFileAsync(logFileName, CreationCollisionOption::OpenIfExists));

		if (logFile != nullptr) {
			logFilePath = convert(logFile->Path);
		}
	}

	return logFilePath;
}

// Save logs to folder selected by the user
bool SaveLogs() {
	try {
		auto folderPicker = ref new Windows::Storage::Pickers::FolderPicker();
		folderPicker->SuggestedStartLocation = Windows::Storage::Pickers::PickerLocationId::Desktop;
		folderPicker->FileTypeFilter->Append("*");

		StorageFolder^ saveFolder;
		ExecuteTask(saveFolder, folderPicker->PickSingleFolderAsync());

		if (saveFolder != nullptr) {
			StorageFolder^ logsFolder = GetLogsStorageFolder();

			if (logsFolder != nullptr) {
				StorageFolderW logsCache(logsFolder);
				logsCache.Copy(saveFolder);
			}
		}
	}
	catch (...) {
		return false;
	}
	return true;
}

void CleanupLogs() {
	StorageFolder^ logsFolder = GetLogsStorageFolder();
	if (logsFolder != nullptr) {
		StorageFolderW logsCache(logsFolder);
		std::list<StorageFileW> files = logsCache.GetFiles();
		if (!files.empty()) {
			for each (auto fItem in files) {
				if (fItem.GetSize() == 0) {
					fItem.Delete();
				}
			}
		}
	}
}
#pragma endregion

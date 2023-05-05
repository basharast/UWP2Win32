// UWP STORAGE MANAGER
// Copyright (c) 2023 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

// This code must keep support for lower builds (15063+)
// Try always to find possible way to keep that support

#pragma once 

#include "pch.h"

#include "StorageLog.h"
#include "StoragePath.h"
#include "StorageExtensions.h"
#include "StorageHandler.h"
#include "StorageAsync.h"
#include "StorageFileW.h"
#include "StorageInfo.h"

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Search.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Foundation.h>

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Storage::FileProperties;
using namespace winrt::Windows::Storage::Search;

class StorageFolderW {
public:
	StorageFolderW() :storageFolder(nullptr), properties(nullptr), folderSize(0) {
	}
	StorageFolderW(StorageFolder folder) : storageFolder(folder), properties(nullptr), folderSize(0) {
	}
	StorageFolderW(IStorageItem folder) : storageFolder(folder.as<StorageFolder>()), properties(nullptr), folderSize(0) {
	}
	~StorageFolderW() {
	}

	// Detect if storage folder is not null
	bool IsValid() {
		return (storageFolder != nullptr);
	}

	// Get folder path
	std::string GetPath() {
		return convert(storageFolder.Path());
	}

	// Delete folder
	bool Delete() {
		bool state = ExecuteTask(storageFolder.DeleteAsync());
		if (state) {
			storageFolder = nullptr;
		}
		return state;
	}

	// Compare folder with std::string
	bool Equal(std::string path) {
		std::string folderPath = GetPath();

		// Fix slashs back from '/' to '\'
		windowsPath(path);
		return iequals(folderPath, path);
	}

	// Compare folder with Platform::String
	bool Equal(winrt::hstring path) {
		return storageFolder.Path() == path;
	}

	// Compare folder with Path
	bool Equal(PathUWP path) {
		return Equal(path.ToString());
	}

	// Compare folder with StorageFolder
	bool Equal(StorageFolder folder) {
		return Equal(folder.Path());
	}

	// Get folder name
	std::string GetName() {
		return convert(storageFolder.Name());
	}

	// Create new folder
	bool CreateFolder(std::string name, bool replaceExisting = true) {
		bool state = false;
		StorageFolder newFolder(nullptr);
		ExecuteTask(newFolder, storageFolder.CreateFolderAsync(convert(name), replaceExisting ? CreationCollisionOption::ReplaceExisting : CreationCollisionOption::GenerateUniqueName));

		if (newFolder != nullptr) {
			state = true;
		}

		return state;
	}

	// Create new file
	bool CreateFile(std::string name, bool replaceExisting = true) {
		bool state = false;
		StorageFile newFile(nullptr);
		ExecuteTask(newFile, storageFolder.CreateFileAsync(convert(name), replaceExisting ? CreationCollisionOption::ReplaceExisting : CreationCollisionOption::GenerateUniqueName));

		if (newFile != nullptr) {
			state = true;
		}

		return state;
	}

	// Rename folder
	bool Rename(std::string name) {
		auto path = PathUWP(name);
		if (path.IsAbsolute()) {
			name = path.GetFilename();
		}
		return ExecuteTask(storageFolder.RenameAsync(convert(name)));
	}

	// Get files 1st level only, no deep scan
	std::list<StorageFileW> GetFiles() {
		std::list<StorageFileW> files;

		IVectorView<StorageFile> sFiles;
		UWP_VERBOSE_LOG(UWPSMT, "Getting files for %s", GetPath().c_str());

		ExecuteTask(sFiles, storageFolder.GetFilesAsync());
		if (sFiles != nullptr) {
			for (auto it = 0; it != sFiles.Size(); ++it) {
				auto sItem = sFiles.GetAt(it);
				if (sItem != nullptr) {
					files.push_back(StorageFileW(sItem));
				}
			}
		}
		UWP_VERBOSE_LOG(UWPSMT, "Total files added (%d) in (%s)", files.size(), GetPath().c_str());

		return files;
	}

	std::list<StorageFolderW> GetFolders() {
		std::list<StorageFolderW> folders;

		IVectorView<StorageFolder> sFolders;
		UWP_VERBOSE_LOG(UWPSMT, "Getting folders for %s", GetPath().c_str());

		ExecuteTask(sFolders, storageFolder.GetFoldersAsync());
		if (sFolders != nullptr) {
			UWP_VERBOSE_LOG(UWPSMT, "Sub folders founded (%d) in (%s)", sFolders.Size, GetPath().c_str());
			for (auto it = 0; it != sFolders.Size(); ++it) {
				auto sItem = sFolders.GetAt(it);
				if (sItem != nullptr) {
					folders.push_back(StorageFolderW(sItem));
				}
			}
		}
		UWP_VERBOSE_LOG(UWPSMT, "Total folders added (%d) in (%s)", folders.size(), GetPath().c_str());

		return folders;
	}

	// Get all files including files in sub folders (deep scan)
	std::list<StorageFileW> GetAllFiles(bool useWindowsIndexer = false) {
		std::list<StorageFileW> files; // No structure one-level list

		UWP_VERBOSE_LOG(UWPSMT, "Getting all files for %s", GetPath().c_str());
		IVectorView<StorageFile> sFiles;

		// Set query options to create groups of files within result
		QueryOptions queryOptions{ QueryOptions(CommonFolderQuery::DefaultQuery) };
		queryOptions.FolderDepth(FolderDepth::Deep); // Search in all levels

		// Windows indexer is very bad, it will return missing results if the files recently copied
		// It's better to search without it, even if it will be slower
		queryOptions.IndexerOption(useWindowsIndexer ? IndexerOption::UseIndexerWhenAvailable : IndexerOption::DoNotUseIndexer);
		StorageFileQueryResult filesResult = storageFolder.CreateFileQueryWithOptions(queryOptions);

		// Windows search query is slow but it's the only solution (or we need to build safe recursive function)
		// Regular 'StorageFolder.GetFilesAsync()' will not search in sub dirs
		ExecuteTask(sFiles, filesResult.GetFilesAsync());

		if (sFiles != nullptr) {
			for (auto it = 0; it != sFiles.Size(); ++it) {
				auto sItem = sFiles.GetAt(it);
				if (sItem != nullptr) {
					files.push_back(StorageFileW(sItem));
				}
			}
		}

		UWP_VERBOSE_LOG(UWPSMT, "Total files added (%d) in (%s)", files.size(), GetPath().c_str());

		return files;
	}

	// Get all sub folders (deep scan)
	std::list<StorageFolderW> GetAllFolders(bool useWindowsIndexer = false) {
		std::list<StorageFolderW> folders;

		IVectorView<StorageFolder> sFolders;
		UWP_VERBOSE_LOG(UWPSMT, "Getting all folders for %s", GetPath().c_str());

		// Set query options to create groups of files within result
		QueryOptions queryOptions{ QueryOptions(CommonFolderQuery::DefaultQuery) };
		queryOptions.FolderDepth(FolderDepth::Deep); // Search in all levels

		// Windows indexer is very bad, it will return missing results if the files recently copied
		// It's better to search without it, even if it will be slower
		queryOptions.IndexerOption(useWindowsIndexer ? IndexerOption::UseIndexerWhenAvailable : IndexerOption::DoNotUseIndexer);
		StorageFolderQueryResult foldersResult = storageFolder.CreateFolderQueryWithOptions(queryOptions);

		// Windows search query is slow but it's the only solution (or we need to build safe recursive function)
		// Regular 'StorageFolder.GetFoldersAsync()' will not search in sub dirs
		ExecuteTask(sFolders, foldersResult.GetFoldersAsync());
		if (sFolders != nullptr) {
			UWP_VERBOSE_LOG(UWPSMT, "Sub folders founded (%d) in (%s)", sFolders.Size, GetPath().c_str());
			for (auto it = 0; it != sFolders.Size(); ++it) {
				auto sItem = sFolders.GetAt(it);
				if (sItem != nullptr) {
					folders.push_back(StorageFolderW(sItem));
				}
			}
		}
		UWP_VERBOSE_LOG(UWPSMT, "Total folders added (%d) in (%s)", folders.size(), GetPath().c_str());

		return folders;
	}

	// Ensure item path without root
	std::string CleanItemPath(PathUWP& path) {
		std::string itemName = path.ToString();
		// Ensure slashs changed from '/' to '\'
		windowsPath(itemName);

		if (path.IsAbsolute()) {

			// If full path detected item in sub location,
			// root path must be removed
			replace(itemName, convert(storageFolder.Path() + L"\\"), "");
			replace(itemName, GetPath(), "");
		}

		// Do some fixes because 'TryGetItemAsync' is very sensetive 
		replace(itemName, "\\\\", "\\");
		replace(itemName, "//", "/");
		rtrim(itemName, ":"); // remove ':' at the end of the path (if any)

		path = PathUWP(itemName);

		return itemName;
	}

	// Check if folder contains item by name or path
	bool Contains(PathUWP path, IStorageItem& storageItem) {
		auto pathString = CleanItemPath(path);

		// If the path is for parent then ignore
		if (!path.IsAbsolute()) {
			UWP_VERBOSE_LOG(UWPSMT, "Looking for (%s) in (%s)", pathString.c_str(), GetPath().c_str());
			ExecuteTask(storageItem, storageFolder.TryGetItemAsync(convert(pathString)));
		}

		return storageItem != nullptr;
	}

	bool Contains(std::string path) {
		IStorageItem tempItem;
		return Contains(PathUWP(path), tempItem);
	}

	void BuildStructure(StorageFolder& folder, std::string path) {
		std::string folderName;
		std::vector<std::string> locationParts = split(path, '\\');
		for (auto dir : locationParts) {
			folderName.append(dir);
			// Create folder
			ExecuteTask(folder, storageFolder.CreateFolderAsync(convert(folderName), CreationCollisionOption::OpenIfExists));
			folderName.append("\\");
		}
	}

	void BuildStructure(StorageFolder& folder, std::string path, StorageFolder target) {
		IStorageItem test(nullptr);
		ExecuteTask(test, target.TryGetItemAsync(convert(path)));
		if (test == nullptr) {
			std::string folderName;
			std::vector<std::string> locationParts = split(path, '\\');
			for (auto dir : locationParts) {
				folderName.append(dir);
				// Create folder
				ExecuteTask(folder, target.CreateFolderAsync(convert(folderName), CreationCollisionOption::OpenIfExists));
				folderName.append("\\");
			}
		}
		else {
			folder = test.as<StorageFolder>();
		}
	}

	StorageFolder GetOrCreateFolder(PathUWP path) {
		StorageFolder folder(nullptr);
		auto pathString = CleanItemPath(path);
		BuildStructure(folder, pathString);

		return folder;
	}

	StorageFile GetOrCreateFile(PathUWP path) {
		StorageFile file(nullptr);
		StorageFolder folder(nullptr);
		auto name = path.GetFilename();
		auto dir = PathUWP(path.GetDirectory());
		auto dirString = CleanItemPath(dir);
		BuildStructure(folder, dirString);
		if (folder != nullptr) {
			ExecuteTask(file, folder.CreateFileAsync(convert(name), CreationCollisionOption::OpenIfExists));
		}
		return file;

	}

	// Copy to another folder
	bool Copy(StorageFolderW folder, bool move = false) {
		auto files = GetAllFiles();
		bool state = false;
		int failedCount = 0;
		if (!files.empty()) {
			auto destination = folder.GetStorageFolder();

			// Copy files one by one to avoid 'access violation' issues with deep-level tasks
			std::string rootName = convert(storageFolder.Name());
			std::string rootPath = PathUWP(GetPath()).GetDirectory();
			windowsPath(rootPath);

			if (destination != nullptr) {
				for (auto file : files) {
					auto fItem = file.GetStorageFile();

					// Get file full path
					std::string targetLocation = convert(fItem.Path());
					// Remove root path but keep the parent name
					replace(targetLocation, rootPath, "");
					replace(targetLocation, convert(L"\\" + fItem.Name()), "");
					ltrim(targetLocation, "\\");
					rtrim(targetLocation, "\\");

					// Build folder structure
					StorageFolder targetFolder(nullptr);
					BuildStructure(targetFolder, targetLocation, destination);

					if (targetFolder != nullptr) {
						// Copy file
						StorageFile testFile(nullptr);
						if (move) {
							ExecuteTask(fItem.MoveAsync((IStorageFolder)targetFolder, fItem.Name(), NameCollisionOption::ReplaceExisting));
							ExecuteTask(testFile, targetFolder.GetFileAsync(fItem.Name())); // testing, it can be ignored
						}
						else {
							ExecuteTask(testFile, fItem.CopyAsync((IStorageFolder)targetFolder, fItem.Name(), NameCollisionOption::ReplaceExisting));
						}

						if (testFile == nullptr) {
							// File failed to copy, we can handle this later
							failedCount++;
						}
					}
				}
				// Try to get the new folder
				IStorageItem newFolder;
				ExecuteTask(newFolder, destination.TryGetItemAsync(convert(rootName)));
				if (newFolder != nullptr) {
					if (move) {
						if (failedCount == 0) {
							// If all files moved, we can safely remove the folder
							ExecuteTask(storageFolder.DeleteAsync());
						}
						storageFolder = newFolder.as<StorageFolder>();
					}
					state = true;
				}
			}
		}
		return state;
	}

	// Copy to another folder using StorageFolder
	bool Copy(StorageFolder folder, bool move = false) {
		return Copy(StorageFolderW(folder), move);
	}

	// Move to another folder
	bool Move(StorageFolderW destination) {
		return Copy(destination, true);
	}

	// Get storage folder handle
	HRESULT GetHandle(HANDLE* handle, int accessMode = GENERIC_READ, int shareMode = FILE_SHARE_READ) {
		return GetFolderHandle(storageFolder, handle, GetAccessMode(accessMode), GetShareMode(shareMode));
	}

	// Get storage folder handle
	HRESULT GetHandleForFile(HANDLE* handle, std::string filename, int accessMode = GENERIC_READ, int shareMode = FILE_SHARE_READ, int openMode = OPEN_EXISTING) {
		return GetFileHandleFromFolder(storageFolder, filename, handle, GetAccessMode(accessMode), GetShareMode(shareMode), GetOpenMode(openMode));
	}

	// Get file stream
	FILE* GetFileStream(std::string name, const char* mode) {
		FILE* file{};

		auto fileMode = GetFileMode(mode);
		bool createIfNotExists = fileMode->isCreate;
		bool isAppend = fileMode->isAppend;

		StorageFile sfile(nullptr);

		if (createIfNotExists) {
			auto createMode = isAppend ? CreationCollisionOption::OpenIfExists : CreationCollisionOption::ReplaceExisting;
			ExecuteTask(sfile, storageFolder.CreateFileAsync(convert(name), createMode));
		}
		else {
			IStorageItem tempItem;
			ExecuteTask(tempItem, storageFolder.TryGetItemAsync(convert(name)));
			sfile = tempItem.as<StorageFile>();
		}

		StorageFileW storageFile(sfile);
		if (storageFile.IsValid()) {
			file = storageFile.GetStream(mode);
		}

		return file;
	}

	// Get folder size
	__int64 GetSize(bool updateCache = false) {
		if (folderSize == 0 || updateCache) {
			// Let's try getting size by handle first
			HANDLE handle;
			HRESULT hr = GetHandle(&handle);
			if (handle == INVALID_HANDLE_VALUE || hr != S_OK) {
				// We have no other option, fallback to UWP
				// This need to sum all files inside
				auto files = GetAllFiles(true);
				for (auto file : files) {
					folderSize += file.GetSize();
				}
			}
			else {
				LARGE_INTEGER size{ 0 };
				if (FALSE == GetFileSizeEx(handle, &size)) {
					LARGE_INTEGER end_offset;
					const LARGE_INTEGER zero{};
					if (SetFilePointerEx(handle, zero, &end_offset, FILE_END) == 0) {
						CloseHandle(handle);
					}
					else {
						folderSize = end_offset.QuadPart;
						SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN);
						CloseHandle(handle);
					}
				}
				else {
					folderSize = size.QuadPart;
					CloseHandle(handle);
				}
			}

		}
		return folderSize;
	}

	// Get folder basic properties
	FILE_BASIC_INFO* GetProperties() {
		HANDLE handle;
		HRESULT hr = GetHandle(&handle);

		size_t size = sizeof(FILE_BASIC_INFO);
		FILE_BASIC_INFO* information = (FILE_BASIC_INFO*)(malloc(size));
		if (hr == S_OK && handle != INVALID_HANDLE_VALUE && information) {
			information->FileAttributes = (uint64_t)storageFolder.Attributes();

			if (FALSE == GetFileInformationByHandleEx(handle, FileBasicInfo, information, (DWORD)size)) {
				// Fallback to UWP method (Slow)
				auto props = FetchProperties();
				information->ChangeTime.QuadPart = winrt::clock::to_file_time(props.DateModified()).value;
				information->CreationTime.QuadPart = winrt::clock::to_file_time(props.ItemDate()).value;
				information->LastAccessTime.QuadPart = winrt::clock::to_file_time(props.DateModified()).value;
				information->LastWriteTime.QuadPart = winrt::clock::to_file_time(props.DateModified()).value;
			}
			CloseHandle(handle);
		}

		return information;
	}

	// Get main storage folder
	StorageFolder GetStorageFolder() {
		return storageFolder;
	}

	time_t  filetime_to_timet(LARGE_INTEGER ull) const {
		return ull.QuadPart / 10000000ULL - 11644473600ULL;
	}
	ItemInfoUWP GetFolderInfo() {
		ItemInfoUWP info;
		info.name = GetName();
		info.fullName = GetPath();
		info.isDirectory = true;

		auto sProperties = GetProperties();

		info.size = (uint64_t)GetSize();
		info.lastAccessTime = (uint64_t)filetime_to_timet(sProperties->LastAccessTime);
		info.lastWriteTime = (uint64_t)filetime_to_timet(sProperties->LastWriteTime);
		info.changeTime = (uint64_t)filetime_to_timet(sProperties->ChangeTime);
		info.creationTime = (uint64_t)filetime_to_timet(sProperties->CreationTime);

		info.attributes = (uint64_t)sProperties->FileAttributes;

		return info;
	}

private:
	StorageFolder storageFolder;
	BasicProperties properties;
	__int64 folderSize = 0;

	BasicProperties FetchProperties() {
		if (properties == nullptr) {
			// Very bad and slow way in UWP to get size and other properties
			// not preferred to be used on big list of files
			ExecuteTask(properties, storageFolder.GetBasicPropertiesAsync());
		}
		return properties;
	}
};

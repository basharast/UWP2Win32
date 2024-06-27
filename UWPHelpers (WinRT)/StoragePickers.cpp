// UWP STORAGE MANAGER
// Copyright (c) 2023-2024 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan

#include "StorageLog.h"
#include "StorageExtensions.h"
#include "StorageAsync.h"
#include "StorageAccess.h"
#include "StoragePickers.h"

#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Storage.AccessCache.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Search.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt::Windows::Storage;

extern void AddItemToFutureList(IStorageItem folder);

// Call folder picker (the selected folder will be added to future list)
winrt::hstring PickSingleFolder()
{
	auto folderPicker{ Pickers::FolderPicker() };
	folderPicker.SuggestedStartLocation(Pickers::PickerLocationId::Desktop);
	folderPicker.FileTypeFilter().Append(L"*");

	auto folder = WaitTask(folderPicker.PickSingleFolderAsync());
	winrt::hstring path = {};
	if (folder != nullptr)
	{
		AddItemToFutureList(folder);
		path = folder.Path();
	}
	return path;
}

// Call file picker (the selected file will be added to future list)
winrt::hstring PickSingleFile(std::vector<std::string> exts)
{
	auto filePicker{ Pickers::FileOpenPicker() };
	filePicker.SuggestedStartLocation(Pickers::PickerLocationId::Desktop);
	filePicker.ViewMode(Pickers::PickerViewMode::List);

	if (exts.size() > 0) {
		for (auto ext : exts) {
			filePicker.FileTypeFilter().Append(convert(ext));
		}
	}
	else
	{
		filePicker.FileTypeFilter().Append(L"*");
	}
	auto file = WaitTask(filePicker.PickSingleFileAsync());

	winrt::hstring path = {};
	if (file != nullptr)
	{
		AddItemToFutureList(file);
		path = file.Path();
	}
	return path;
}


std::string ChooseFile(std::vector<std::string> exts) {
	return convert(PickSingleFile(exts));
}

std::string ChooseFolder() {
	return convert(PickSingleFolder());
}

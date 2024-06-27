# UWP Storage manager

This manager will help developers to deal with the storage in any **C++** UWP app
using only string path and not to aware about UWP side.

- **CX**
- **WinRT**

# Overview

There was big confusion since the new UWP APIs made such as `CreateFile2FromApp`

those were not able to deal with the future access list correctly in early builds 17134

even on Windows 10 they had many internal bugs, after build 22000 a lot of things got corrected

so on the recent builds on Windows once the folder/file is added to the future access list then:

## API direct access

it will be allowed for direct access using the API that made for UWP (those ends with `FromApp`)
this is allowed even without `FileSystem` enabled.

## Get StorageFile with full path

also functions such as `StorageFolder::GetFolderFromPathAsync` allowed to get files from outside local data
as long the target in the future access list

not to mention that by default APIs can be used in case:

- Accessing to AppData
- Accessing to AppFolder
- Accessing to USB (if capability added)

## Advanced

This will not be included in this project yet

but you can always resolve any DLL API to forward the request to your UWP replacement

you can check the nice idea made by Team Kodi at [this file](https://github.com/xbmc/xbmc/blob/8a90f175f53fa3f86415e6a376aed54d244cad0e/xbmc/cores/DllLoader/Win32DllLoader.cpp)

with this solution you can increase the app compatibility with UWP environment


# Usage & Structure

## Version 1.5

- Added check using API before UWP
- Added Get/Put file contents
- Implemented files into projects
- License changed to MIT
- Fix many build errors (WinRT)
- Fixed minor issues
- Added wstring version for functions

## Capabilities

Below can be done with `string path` you don't have to do major changes:

- Lookup locations for gaining access histoy without any extra action
- Extra lookup locations like `Documents`, `Videos`.etc can be activated
- Easy to use pickers file/folder (auto sync with lookup locations)
- Get file `HANDLE` or `FILE*`
- Get folder contents
- Perform many actions like `Delete`, `Copy`, `Move` and more
- libzip integration for extract will be detailed below (at the end)
- More will be detailed below


## Structure

- StorageConfig (Config) <- Main usage (`StorageConfig.h`)
- StorageManager (Engine) <- Main usage (`StorageManager.h`)
- StoragePickers (Pickers file/folders) <- Main usage (`StoragePickers.h`)
- StorageItemW (Wrapped `StorageItem`) [Main entry for File/Folder]
- StorageFileW (Wrapped `StorageFile`)
- StorageFolderW (Wrapped `StorageFolder`)
- StorageAccess (Future access)
- StorageAsync (Async operation/action helpers)
- StorageHandler (File `HANDLE` functions) [Internal usage]
- StorageExtensions (String helpers only)
- StoragePath (PathUWP class) [Internal usage]
- StorageInfo (item info struct)
- StorageLog (Log configuration)
- UWP2C (libzip helpers)


Your code will `#include` in general:

- `StorageManager.h`
- `StoragePickers.h` (if needed)
- `UWP2C.h` (for libzip)

The usage simplified to be similar to Win32 functions.

## Note 

Because I'm always aware for legacy support, some parts can be skipped

for recent Windows builds the call will mostly finished at UWP API level, 

rarely will use wrapped StorageItem or others.

in case you went into unneccesary delay, modify the code and disable the wrapped StorageItem fallback if you sure it's not needed

```cpp
// Example what fallback means
bool IsExistsUWP(std::string path) {
	bool defaultState = IsExistsAPI(path); // <-- Usually this succeed
	if (!defaultState && IsValidUWP(path)) {
		// ... Here the fallback ...
	}

	return false;
}
```


# Usage

## Tips

- To force legacy APIs define `UWP_LEGACY`
- By default legacy forced for ARM32 only
- Turn off precompiled header from your project
- Add preprocessors `_CRT_SECURE_NO_WARNINGS` and `NOMINMAX`
- For best results make things centralized in custom folder picked by the user, it's better in case the app removed and installed again


## Lookup lists

By default storage manager will append these locations:

- App installation folder
- App local data
- Picked files
- Picked folders

If you want to add extra locations such as `Documents`:

Open `StorageConfig.h` at the top you will find:

```c++
// Known locations
#define APPEND_APP_LOCALDATA_LOCATION 1 
#define APPEND_APP_INSTALLATION_LOCATION 1 
#define APPEND_DOCUMENTS_LOCATION 0
#define APPEND_VIDEOS_LOCATION 0
#define APPEND_MUSIC_LOCATION 0
#define APPEND_PICTURES_LOCATION 0
```

**BE CAREFUL** you must add the related capability or your app will crash.

### Known folders (capabilities)

- Documents: `documentsLibrary` <-(manually via XML editor)
- Videos: `videosLibrary`
- Pictures: `picturesLibrary`
- Music: `musicLibrary`

**Note:** try to avoid `documentsLibrary` 
it will throw exception in case you're trying to reach type is not declared in `appxmanifest` file

## Pick files/folders

Just `#include "StoragePickers.h"` in the target page

then you can invoke picker like this:

### File Picker

```c++
// Supported types
std::vector<std::string> supportedExtensions = { ".cso", ".bin", ".iso", ".elf", ".zip"};

// Call file picker
ChooseFile(supportedExtensions).then([](std::string filePath) {
	if (filePath.size() > 1) {
		// Do something here
	}
});
```

### Folder Picker

```c++
ChooseFolder().then([](std::string folderPath) {
	if (folderPath.size() > 1) {
		// Do something here
	}
});
```

When user cancel the picker empty string will be returned


# Management

All you need is to `#include "StorageManager.h"` in your code

then you can deal with the files using the following functions:


## Locations

```c++
std::string GetWorkingFolder();
void SetWorkingFolder(std::string location);
std::string GetInstallationFolder();
std::string GetLocalFolder();
std::string GetTempFolder();
std::string GetPicturesFolder();
std::string GetVideosFolder();
std::string GetDocumentsFolder();
std::string GetMusicFolder();
std::string GetPreviewPath(std::string path);
```

`GetWorkingFolder()` is optional, use it in case you have main folder for data

`GetPreviewPath()` for WorkingFolder it will return "App Local Data"

in case the working folder was in app local folder instead of long path


## Handling

Get `HANDLE` for file/folder:

```c++
// This call is almost similar to CreateFile2
HANDLE CreateFileUWP(std::string path, int accessMode, int shareMode, int openMode);
HANDLE CreateFileUWP(std::wstring path, int accessMode, int shareMode, int openMode);
```

Get `FILE*` stream for file:

```c++
// This call will be similar to fopen
FILE* GetFileStream(std::string path, const char* mode)
```

Get `FILE*` stream for file (DirectAPI):

This function is important to avoid using `fopen`, 

preferred to be used before `GetFileStream()`

it will call `CreateFile2FromAppW` with `mode` converted to it's args

```c++
// This call will be similar to fopen
FILE* GetFileStreamFromApp(std::string path, const char* mode)
```

## Restrictions

- You cannot use `FindFirstFile`, `FindNextFile` with folder `HANDLE`
- You must use the alternative function `GetFolderContents`


Get folder contents:

```c++
// Get list of file and folder
// be aware using 'deepScan' is slow
// because it will use WindowsQuery
std::list<ItemInfoUWP> GetFolderContents(std::string path, bool deepScan);
```

Get single file info:

```c++
// Get single file info
ItemInfoUWP GetItemInfoUWP(std::string path);
```


## ItemInfoUWP (Struct)

```c++
struct ItemInfoUWP {
	std::string name;
	std::string fullName;

	bool isDirectory = false;

	uint64_t size = 0;
	uint64_t lastAccessTime = 0;
	uint64_t lastWriteTime = 0;
	uint64_t changeTime = 0;
	uint64_t creationTime = 0;

	DWORD attributes = 0;
};
```

### Important

If you want to change `ItemInfoUWP` structure 

don't forget to apply the changes at `StorageManager.cpp`

it's used only in this file


## Validation

```c++
// Check if file/folder exists
bool IsExistsUWP(std::string path);

// Check if target is directory 
bool IsDirectoryUWP(std::string path);

// Check if drive is accessible 
// 'checkIfContainsFutureAccessItems' for listing purposes not real access, 'driveName' like C:
bool CheckDriveAccess(std::string driveName, bool checkIfContainsFutureAccessItems);
```


## Actions

```c++
int64_t GetSizeUWP(std::string path);
bool DeleteUWP(std::string path);
bool CreateDirectoryUWP(std::string path, bool replaceExisting = true);
bool RenameUWP(std::string path, std::string name);
bool CopyUWP(std::string path, std::string name);
bool MoveUWP(std::string path, std::string name);
```

## Extra

```c++
bool OpenFile(std::string path);
bool OpenFolder(std::string path);
bool IsFirstStart(); // Return true if first start
bool GetDriveFreeSpace(PathUWP path, int64_t& space) // Get drive free space by folder path (must be in FutureAccess)
```



# libzip Integration

As per my test on PPSSPP emulator 

I was able to make zip function working by doing the following:

- Open `zip_source_file_win32_utf16.c` file
- Do the following changes:

```c++
// Include headers
#ifdef MS_UWP
#include "UWP2C.h"
// For latest builds include 'fileapifromapp.h'
#include <fileapi.h>
#endif

// Replace `zip_win32_file_operations_t ops_utf16` by below
#ifdef MS_UWP
static BOOL __stdcall GetFileAttr(const void* name, GET_FILEEX_INFO_LEVELS info_level, void* lpFileInformation) {
	// For latest builds use 'GetFileAttributesExFromAppW'
	BOOL state = GetFileAttributesExW(name, info_level, lpFileInformation);
	if (state == FALSE) {
		// Ignore `info_level` not in use for now
		state = GetFileAttributesUWP(name, lpFileInformation);
	}
	return state;
}

static BOOL __stdcall DelFile(const void* name) {
	// For latest builds use 'DeleteFileFromAppW'
	BOOL state = DeleteFileW(name);
	if (state == FALSE) {
		state = DeleteFileUWP(name);
	}
	return state;
}

// You got the idea so you can add more if you want

zip_win32_file_operations_t ops_utf16 = {
	utf16_allocate_tempname,
	utf16_create_file, // Will invoke UWP Storage manager (If needed)
	DelFile, // Will invoke UWP Storage manager (If needed)
	GetFileAttributesW,
	GetFileAttr, // Will invoke UWP Storage manager (If needed)
	utf16_make_tempname,
	MoveFileExW,
	SetFileAttributesW,
	utf16_strdup
};
#else
 // Here goes the original `ops_utf16`
#endif

```

Now scroll to `utf16_create_file` function

and make the following changes:

```c++
#ifdef MS_UWP
	// For latest builds use 'CreateFile2FromAppW'
	HANDLE h = CreateFile2((const wchar_t*)name, access, share_mode, creation_disposition, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		h = CreateFileUWP(name, (int)access, (int)share_mode, (int)creation_disposition);
	}
    return h;
#else
    return CreateFileW((const wchar_t *)name, access, share_mode, security_attributes, creation_disposition, file_attributes, template_file);
#endif
```

Done.


# Logger

If you want to attach your own log function

just open `StorageLog.h` and do the required changes

using `StorageManager.h` you will be able to:

```c++
std::string GetLogFile();
bool SaveLogs(); // With picker
void CleanupLogs(); // Delete empty logs
```

Usually `LOGS` folder will be created in `WorkingFolder`

and inside it will be new file for each launch at the time `GetLogFile()` called


# Important

Be aware, `Contain` function is case senstive for now

try to use exact chars case in your path when requesting file/folder

I will consider to force lowercase in future


# Credits

- Bashar Astifan (Days of work to make in the best possible shape)
- Peter Torr - MSFT for Win32 `HANDLE` solution
- `PathUWP` based on `Path.h` type from **PPSSPP**
- Libretro/RetroArch team for `async` idea
- Team Kodi for `WaitTask` solution
- StackOverflow for various string helpers

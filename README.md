# UWP Storage manager

This manager will help developers to deal with the storage in any **C++** UWP app
using only string path and not to aware about UWP side.

## Available?
Yes very very soon (Final phase)..

Check this test on PPSSPP emulator and let me know what do you think?



## Why I made this?

Many projects lost legacy support when it comes to UWP

because they start using Microsoft's (latest) file APIs

and in general, gaining access directly to any file is againest UWP concept

also it will force the developer to use restricted caps like `boardFileSystem`

which will make the app useless if `File System` turned off in `Privacy` options.

Here where this storage manager come to solve the issue.


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

Each `.cpp`, `.h` file will contain list of functions at the top to make easier to get what you want.


# Usage

## Lookup lists

By default storage manager will append these locations:

- App installation folder
- App local data
- Picked files
- Picked folders

If you want to add extra locations such as `Documents`:

Open `StorageConfig.h` at the top you will find:

```
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


## Pick files/folders

Just `#include "StoragePickers.h"` in the target page

then you can invoke picker like this:

### File Picker

```
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

```
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

```
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

```
// This call is almost similar to CreateFile2
HANDLE CreateFileUWP(std::string path, int accessMode, int shareMode, int openMode);
HANDLE CreateFileUWP(std::wstring path, int accessMode, int shareMode, int openMode);
```

Get `FILE*` stream for file:

```
// This call will be similar to _wfopen
FILE* GetFileStream(std::string path, const char* mode)
```

## Restrictions

- You cannot use `FindFirstFile`, `FindNextFile` with folder `HANDLE`
- You must use the alternative function `GetFolderContents`


Get folder contents:

```
// Get list of file and folder
// be aware using 'deepScan' is slow
// because it will use WindowsQuery
std::list<ItemInfoUWP> GetFolderContents(std::string path, bool deepScan);
```

Get single file info:

```
// Get single file info
ItemInfoUWP GetItemInfoUWP(std::string path);
```


## ItemInfoUWP (Struct)

```
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

```
// Check if file/folder exists
bool IsExistsUWP(std::string path);

// Check if target is directory 
bool IsDirectoryUWP(std::string path);
```


## Actions

```
int64_t GetSizeUWP(std::string path);
bool DeleteUWP(std::string path);
bool CreateDirectoryUWP(std::string path, bool replaceExisting = true);
bool RenameUWP(std::string path, std::string name);
bool CopyUWP(std::string path, std::string name);
bool MoveUWP(std::string path, std::string name);
```

## Extra

```
bool OpenFile(std::string path);
bool OpenFolder(std::string path);
bool IsFirstStart(); // Return true if first start
```



# libzip Integration

As per my test on PPSSPP emulator 

I was able to make zip function working by doing the following:

- Open `zip_source_file_win32_utf16.c` file
- Do the following changes:

```
// Include this header
#include "UWP2C.h"

// Replace `zip_win32_file_operations_t ops_utf16` by below
#ifdef MS_UWP
static BOOL GetFileAttr(const void* name, GET_FILEEX_INFO_LEVELS info_level, void* lpFileInformation);
static BOOL __stdcall
GetFileAttr(const void* name, GET_FILEEX_INFO_LEVELS info_level, void* lpFileInformation) {
    // Ignore `info_level` it will not be used for now
	return GetFileAttributesUWP(name, lpFileInformation);
}

static BOOL DelFile(const void* name);
static BOOL __stdcall
DelFile(const void* name) {
	return DeleteFileUWP(name);
}

// You got the idea so you can add more if you want

zip_win32_file_operations_t ops_utf16 = {
	utf16_allocate_tempname,
	utf16_create_file, // Will invoke UWP Storage manager (If needed)
	DelFile, // Will invoke UWP Storage manager
	GetFileAttributesW, // Not implemented
	GetFileAttr, // Will invoke UWP Storage manager
	utf16_make_tempname,
	MoveFileExW, // Not implemented
	SetFileAttributesW,
	utf16_strdup
};
#else
 // Here goes the original `ops_utf16`
#endif

```

Now scroll to `utf16_create_file` function

and make the following changes:

```
#ifdef MS_UWP
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

```
std::string GetLogFile();
bool SaveLogs(); // With picker
void CleanupLogs(); // Delete empty logs
```

Usually `LOGS` folder will be created in `WorkingFolder`

and inside it will be new file for each launch at the time `GetLogFile()` called


# Tips

It's prefered to use file APIs first,

then forward the request to storage manager if APIs failed

the reason is: file APIs may work in many cases such as:

- Accessing to AppData
- Accessing to AppFolder
- Accessing to USB (if capability added)

and it will provide a bit faster performance in terms of fetching results


# Credits

- Bashar Astifan (Days of work to make in the best possible shape)
- Peter Torr - MSFT for Win32 `HANDLE` solution
- `PathUWP` based on `Path.h` type from **PPSSPP**
- Libretro/RetroArch team for `async` idea
- StackOverflow for various string helpers
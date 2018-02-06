#define _CRT_SECURE_NO_WARNINGS
#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS
#include <winternl.h>
#include <ntstatus.h>
#include <objbase.h>
#include <climits>

struct FILE_DIRECTORY_INFORMATION {
	ULONG NextEntryOffset;
	ULONG FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG FileAttributes;
	ULONG FileNameLength;
	WCHAR FileName[1];
};

struct FILE_LINK_INFORMATION {
	BOOLEAN ReplaceIfExists;
	HANDLE RootDirectory;
	ULONG FileNameLength;
	WCHAR FileName[1];
};

struct FILE_GET_EA_INFORMATION {
	ULONG NextEntryOffset;
	UCHAR EaNameLength;
	CHAR EaName[1];
};

struct FILE_FULL_EA_INFORMATION {
	ULONG NextEntryOffset;
	UCHAR Flags;
	UCHAR EaNameLength;
	USHORT EaValueLength;
	CHAR EaName[1];
};

extern "C" NTSYSAPI NTSTATUS NTAPI NtQueryDirectoryFile(
	_In_ HANDLE FileHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_Out_ PVOID FileInformation,
	_In_ ULONG Length,
	_In_ FILE_INFORMATION_CLASS FileInformationClass,
	_In_ BOOLEAN ReturnSingleEntry,
	_In_opt_ PUNICODE_STRING FileName,
	_In_ BOOLEAN RestartScan
);

extern "C" NTSYSAPI NTSTATUS NTAPI NtSetInformationFile(
	_In_ HANDLE FileHandle,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ PVOID FileInformation,
	_In_ ULONG Length,
	_In_ FILE_INFORMATION_CLASS FileInformationClass
);

extern "C" NTSYSAPI NTSTATUS NTAPI NtQueryEaFile(
	_In_ HANDLE FileHandle,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_Out_ PVOID Buffer,
	_In_ ULONG Length,
	_In_ BOOLEAN ReturnSingleEntry,
	_In_opt_ PVOID EaList,
	_In_ ULONG EaListLength,
	_In_opt_ PULONG EaIndex,
	_In_ BOOLEAN RestartScan
);

extern "C" NTSYSAPI NTSTATUS NTAPI NtSetEaFile(
	_In_ HANDLE FileHandle,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ PVOID EaBuffer,
	_In_ ULONG EaBufferSize
);

extern "C" __declspec(dllexport) HANDLE GetFileHandle(LPWSTR ntPath, bool directory, bool create, bool write) {
	UNICODE_STRING uniStr;
	RtlInitUnicodeString(&uniStr, ntPath);

	OBJECT_ATTRIBUTES objAttrs;
	InitializeObjectAttributes(&objAttrs, &uniStr, 0, 0, nullptr);

	HANDLE hFile;
	IO_STATUS_BLOCK status;
	return NtCreateFile(&hFile,
		write ? FILE_GENERIC_WRITE : FILE_GENERIC_READ,
		&objAttrs, &status, nullptr, 0, 0,
		create ? FILE_CREATE : FILE_OPEN,
		FILE_SYNCHRONOUS_IO_ALERT | (directory ? FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE),
		nullptr, 0
	) == STATUS_SUCCESS ? hFile : INVALID_HANDLE_VALUE;
}

extern "C" __declspec(dllexport) bool EnumerateDirectory(HANDLE hFile, LPWSTR *fileName, bool *directory) {
	const int fileInfoSize = (int)(sizeof(FILE_DIRECTORY_INFORMATION) + UCHAR_MAX * sizeof(wchar_t));
	static char fileInfoBuf[fileInfoSize];

	auto fileInfo = (FILE_DIRECTORY_INFORMATION *)fileInfoBuf;

	IO_STATUS_BLOCK status;
	switch (NtQueryDirectoryFile(hFile, nullptr, nullptr, nullptr, &status, fileInfo, fileInfoSize, FileDirectoryInformation, true, nullptr, false)) {
	case STATUS_NO_MORE_FILES: return true;
	case STATUS_SUCCESS:
		*fileName = nullptr;
		return false;
	}

	*fileName = (LPWSTR)CoTaskMemAlloc(fileInfo->FileNameLength * sizeof(wchar_t));
	wcscpy(*fileName, fileInfo->FileName);
	*directory = (fileInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0;
	return true;
}

extern "C" __declspec(dllexport) bool MakeHardLink(HANDLE hTarget, LPWSTR linkName) {
	const int linkNameLength = (int)(wcslen(linkName) * sizeof(wchar_t));
	const int linkInfoSize = (int)(sizeof(FILE_LINK_INFORMATION) + linkNameLength);
	auto linkInfo = (FILE_LINK_INFORMATION *)new char[linkInfoSize];
	linkInfo->ReplaceIfExists = false;
	linkInfo->RootDirectory = nullptr;
	linkInfo->FileNameLength = linkNameLength;
	wcscpy(linkInfo->FileName, linkName);

	IO_STATUS_BLOCK status;
	auto res = NtSetInformationFile(hTarget, &status, linkInfo, linkInfoSize, (FILE_INFORMATION_CLASS)11 /* FileLinkInformation */);

	delete[] linkInfo;
	return res == STATUS_SUCCESS;
}

const char *LxssEaName = "LXATTRB";
const int LxssEaNameLength = 7;
const int EaInfoSize = (int)(sizeof(FILE_FULL_EA_INFORMATION) + LxssEaNameLength + USHRT_MAX);

extern "C" __declspec(dllexport) bool CopyLxssEa(HANDLE hFrom, HANDLE hTo) {
	const int getEaInfoSize = (int)(sizeof(FILE_GET_EA_INFORMATION) + LxssEaNameLength);
	static char getEaBuf[getEaInfoSize];
	static char eaBuf[EaInfoSize];

	auto getEaInfo = (FILE_GET_EA_INFORMATION *)getEaBuf;
	getEaInfo->NextEntryOffset = 0;
	getEaInfo->EaNameLength = LxssEaNameLength;
	strcpy(getEaInfo->EaName, LxssEaName);

	auto eaInfo = (FILE_FULL_EA_INFORMATION *)eaBuf;
	IO_STATUS_BLOCK status;
	if (NtQueryEaFile(hFrom, &status, eaInfo, EaInfoSize, true, getEaInfo, getEaInfoSize, nullptr, true) != STATUS_SUCCESS) return false;
	if (NtSetEaFile(hTo, &status, eaInfo, EaInfoSize) != STATUS_SUCCESS) return false;

	return true;
}

extern "C" __declspec(dllexport) bool SetLxssEa(HANDLE hFile, char *data, int dataLength) {
	static char eaInfoBuf[EaInfoSize];

	auto eaInfo = (FILE_FULL_EA_INFORMATION *)eaInfoBuf;
	eaInfo->NextEntryOffset = 0;
	eaInfo->Flags = 0;
	eaInfo->EaNameLength = LxssEaNameLength;
	eaInfo->EaValueLength = dataLength;
	strcpy(eaInfo->EaName, LxssEaName);
	memcpy(eaInfo->EaName + LxssEaNameLength + 1, data, dataLength);

	IO_STATUS_BLOCK status;
	return NtSetEaFile(hFile, &status, eaInfo, EaInfoSize) == STATUS_SUCCESS;
}

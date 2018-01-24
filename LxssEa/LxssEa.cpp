#define _CRT_SECURE_NO_WARNINGS
#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS
#include <winternl.h>
#include <ntstatus.h>
#include <climits>

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

extern "C" NTSYSAPI BOOLEAN NTAPI RtlDosPathNameToNtPathName_U(
	_In_ PWSTR DosFileName,
	_Out_ PUNICODE_STRING NtFileName,
	_Out_opt_ PWSTR* FilePart,
	_Out_opt_ PVOID RelativeName
);

extern "C" __declspec(dllexport) HANDLE GetFileHandle(LPWSTR path, bool directory, bool create, bool write) {
	UNICODE_STRING ntPath;
	if (!RtlDosPathNameToNtPathName_U(path, &ntPath, nullptr, nullptr))
		return INVALID_HANDLE_VALUE;

	OBJECT_ATTRIBUTES objAttrs;
	InitializeObjectAttributes(&objAttrs, &ntPath, 0, 0, nullptr);

	HANDLE hFile;
	IO_STATUS_BLOCK status;
	auto res = NtCreateFile(&hFile,
		FILE_GENERIC_READ | (write ? FILE_GENERIC_WRITE : 0),
		&objAttrs, &status, nullptr, 0, 0,
		create ? FILE_CREATE : FILE_OPEN,
		FILE_SYNCHRONOUS_IO_ALERT | (directory ? FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE),
		nullptr, 0
	);

	return res == STATUS_SUCCESS ? hFile : INVALID_HANDLE_VALUE;
}

extern "C" __declspec(dllexport) bool CopyLxssEa(HANDLE hFrom, HANDLE hTo) {
	const char *lxssEaName = "LXATTRB";
	const int getEaInfoSize = (int)(sizeof(FILE_GET_EA_INFORMATION) + strlen(lxssEaName));
	const int eaInfoSize = getEaInfoSize + USHRT_MAX;

	auto getEaInfo = (FILE_GET_EA_INFORMATION *)new char[getEaInfoSize];
	getEaInfo->NextEntryOffset = 0;
	getEaInfo->EaNameLength = (int)strlen(lxssEaName);
	strcpy(getEaInfo->EaName, lxssEaName);

	auto eaInfo = (FILE_FULL_EA_INFORMATION *)new char[eaInfoSize];
	IO_STATUS_BLOCK status;
	auto res = NtQueryEaFile(hFrom, &status, eaInfo, eaInfoSize, true, getEaInfo, getEaInfoSize, nullptr, true);
	if (res != STATUS_SUCCESS) return false;
	res = NtSetEaFile(hTo, &status, eaInfo, eaInfoSize);
	if (res != STATUS_SUCCESS) return false;

	return true;
}

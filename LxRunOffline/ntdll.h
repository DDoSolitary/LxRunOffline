#pragma once
#pragma comment(lib, "ntdll.lib")
#include "stdafx.h"

#define FILE_CS_FLAG_CASE_SENSITIVE_DIR 0x00000001
#define FileCaseSensitiveInformation (FILE_INFORMATION_CLASS)71

struct FILE_CASE_SENSITIVE_INFORMATION {
	ULONG Flags;
};

struct FILE_GET_EA_INFORMATION {
	ULONG NextEntryOffset;
	UCHAR EaNameLength;
	CHAR EaName[1];
};

#ifndef __MINGW32__
struct FILE_FULL_EA_INFORMATION {
	ULONG NextEntryOffset;
	UCHAR Flags;
	UCHAR EaNameLength;
	USHORT EaValueLength;
	CHAR EaName[1];
};
#endif


extern "C" {
	NTSYSAPI NTSTATUS NTAPI NtQueryEaFile(
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

	NTSYSAPI NTSTATUS NTAPI NtSetEaFile(
		_In_ HANDLE FileHandle,
		_Out_ PIO_STATUS_BLOCK IoStatusBlock,
		_In_ PVOID EaBuffer,
		_In_ ULONG EaBufferSize
	);

	NTSYSAPI NTSTATUS NTAPI NtQueryInformationFile(
		_In_ HANDLE FileHandle,
		_Out_ PIO_STATUS_BLOCK IoStatusBlock,
		_Out_ PVOID FileInformation,
		_In_ ULONG Length,
		_In_ FILE_INFORMATION_CLASS FileInformationClass
	);

	NTSYSAPI NTSTATUS NTAPI NtSetInformationFile(
		_In_ HANDLE FileHandle,
		_Out_ PIO_STATUS_BLOCK IoStatusBlock,
		_In_ PVOID FileInformation,
		_In_ ULONG Length,
		_In_ FILE_INFORMATION_CLASS FileInformationClass
	);
}
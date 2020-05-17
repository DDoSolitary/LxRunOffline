#include "stdafx.h"
#include "error.h"

const wstr msg_table[] = {
	L"Couldn't open the file \"%1%\".",
	L"Couldn't open the directory \"%1%\".",
	L"Couldn't create the file \"%1%\".",
	L"Couldn't create the directory \"%1%\".",
	L"Couldn't delete the file \"%1%\".",
	L"Couldn't delete the directory \"%1%\".",
	L"Couldn't get contents of the directory \"%1%\".",
	L"Couldn't get information of the file \"%1%\".",
	L"Couldn't get size of the file \"%1%\".",
	L"Couldn't get the extended attribute \"%1%\" of the file or directory \"%2%\".",
	L"Couldn't set the extended attribute \"%1%\" of the file or directory \"%2%\".",
	L"The extended attribute \"%1%\" of the file or directory \"%2%\" is invalid.",
	L"Couldn't set the case sensitive attribute of the directory \"%1%\".",
	L"Couldn't get time information of the file or directory \"%1%\".",
	L"Couldn't set time information of the file or directory \"%1%\".",
	L"Couldn't get reparse information of the file \"%1%\".",
	L"Couldn't set reparse information of the file \"%1%\".",
	L"The symlink file \"%1%\" has an invalid length %2%.",
	L"Couldn't create the hard link from \"%1%\" to \"%2%\".",
	L"Couldn't read from the file \"%1%\".",
	L"Couldn't write to the file \"%1%\".",
	L"Couldn't recognize the path \"%1%\".",
	L"Couldn't convert the encoding of a string.",
	L"Error occurred while processing the archive: %1%",
	L"Couldn't get Windows version information. \"%1%\"",
	L"Windows 10 v%1% (v10.0.%2%) or later is required. Please upgrade your system.",
	L"Couldn't open or create the registry key \"%1%\".",
	L"Couldn't delete the registry key \"%1%\".",
	L"Couldn't get subkeys of the registry key \"%1%\".",
	L"Couldn't get the value \"%2%\" of the registry key \"%1%\".",
	L"Couldn't set the value \"%2%\" of the registry key \"%1%\".",
	L"Couldn't delete the value \"%2%\" of the registry key \"%1%\".",
	L"Couldn't create a GUID.",
	L"Couldn't convert a GUID to a string.",
	L"Couldn't find the distro \"%1%\".",
	L"The distro \"%1%\" already exists.",
	L"The distro \"%1%\" has running processes and can't be operated. \"wsl -t <name>\" or \"wsl --shutdown\" might help.",
	L"Couldn't find a valid default distribution.",
	L"No action is specified.",
	L"The action \"%1%\" is not recognized.",
	L"Couldn't load wslapi.dll. Please make sure that WSL has been installed.",
	L"Error occurred when trying to launch the distro \"%1%\".",
	L"Error occurred when creating the shortcut.",
	L"The environment variable \"%1%\" is invalid.",
	L"The environment variable \"%1%\" already exists.",
	L"Environment variable \"%1%\" not found.",
	L"Error occurred while processing the config file: %1%",
	L"Filesystem version %1% is not recognized.",
	L"Failed to detect filesystem version of the directory \"%1%\".",
	L"Installing to the root directory \"%1%\" is known to cause issues.",
	L"The configuration flags are invalid.",
	L"The action/argument \"%1%\" doesn't support WSL2.",
	L"Copying or moving into a subdirectory of the source directory is not allowed."
};

lro_error::lro_error(const err_msg msg_code, std::vector<wstr> msg_args, const HRESULT err_code)
	: msg_code(msg_code), msg_args(std::move(msg_args)), err_code(err_code) {}


lro_error lro_error::from_hresult(const err_msg msg_code, std::vector<wstr> msg_args, const HRESULT err_code) {
	return lro_error(msg_code, std::move(msg_args), err_code);

}

lro_error lro_error::from_win32(const err_msg msg_code, std::vector<wstr> msg_args, const uint32_t err_code) {
	return from_hresult(msg_code, std::move(msg_args), HRESULT_FROM_WIN32(err_code));
}

lro_error lro_error::from_win32_last(const err_msg msg_code, std::vector<wstr> msg_args) {
	return from_win32(msg_code, std::move(msg_args), GetLastError());
}

lro_error lro_error::from_nt(const err_msg msg_code, std::vector<wstr> msg_args, const NTSTATUS err_code) {
	return from_hresult(msg_code, std::move(msg_args), HRESULT_FROM_NT(err_code));
}

lro_error lro_error::from_other(const err_msg msg_code, std::vector<wstr> msg_args) {
	return from_hresult(msg_code, std::move(msg_args), S_OK);
}

wstr lro_error::format() const {
	std::wstringstream ss;

	auto fmt = boost::wformat(msg_table[static_cast<size_t>(msg_code)]);
	for (crwstr s : msg_args) fmt = fmt % s;
	ss << fmt << std::endl;

	if (err_code != 0) {
		ss << L"Reason: ";
		if ((err_code & FACILITY_NT_BIT) != 0) {
			auto stat = err_code & ~FACILITY_NT_BIT;
			wchar_t *buf = nullptr;
			auto f = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS;
			auto hm = LoadLibrary(L"ntdll.dll");
			auto ok = hm && FormatMessage(f, hm, stat, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), reinterpret_cast<wchar_t *>(&buf), 0, nullptr);
			if (hm) FreeLibrary(hm);
			if (ok) {
				ss << buf;
				LocalFree(buf);
			} else {
				ss << L"Unknown NTSTATUS: " << L"0x" << std::setfill(L'0') << std::setw(8) << std::hex << stat;
			}
		} else {
			_com_error ce(err_code);
			ss << ce.ErrorMessage();
		}
	}

	return ss.str();
}

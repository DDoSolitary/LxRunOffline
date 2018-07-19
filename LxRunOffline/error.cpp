#include "stdafx.h"
#include "error.h"
#include "utils.h"

const wstr msg_table[] = {
	L"Could't open the file \"%1%\".",
	L"Couldn't open the directory \"%1%\".",
	L"Couldn't create the file \"%1%\".",
	L"Couldn't create the directory \"%1%\".",
	L"Couldn't delete the file \"%1%\".",
	L"Couldn't delete the directory \"%1%\".",
	L"Couldn't get contents of the directory \"%1%\".",
	L"Couldn't get information of the file \"%1%\".",
	L"Couldn't get extended attributes of the file or directory \"%1%\".",
	L"Couldn't set extended attributes of the file or directory \"%1%\".",
	L"Couldn't set the case sensitive attribute of the directory \"%1%\".",
	L"Couldn't create the hard link from \"%1%\" to \"%2%\".",
	L"Couldn't read from the file \"%1%\".",
	L"Couldn't write to the file \"%1%\".",
	L"Couldn't recognize the path \"%1%\".",
	L"Couldn't convert a string from UTF-8 encoding to wide chars.",
	L"Error occurred while processing the archive.",
	L"Couldn't get Windows version information. \"%1%\"",
	L"Windows 10 v%1% (v10.0.%2%) or later is required. Please upgrade your system.",
	L"Couldn't open or create the registry key \"%1%\".",
	L"Couldn't delete the registry key \"%1%\".",
	L"Couldn't get subkeys of the registry key \"%1%\".",
	L"Couldn't copy the registry key \"%1%\" to \"%2%\".",
	L"Couldn't get the value \"%2%\" of the registry key \"%1%\".",
	L"Couldn't set the value \"%2%\" of the registry key \"%1%\".",
	L"Couldn't create a GUID.",
	L"Couldn't convert a GUID to a string.",
	L"Couldn't find the distro named \"%1%\".",
	L"A distro named \"%1%\" already exists.",
	L"No action is speicified.",
	L"The action \"%1%\" is not recognized.",
	L"Couldn't load wslapi.dll. Please make sure that WSL has been installed.",
};

err error_last(err_msg msg_code, const std::vector<wstr> &msg_args) {
	return err{ msg_code,msg_args,GetLastError(),L"" };
}

err error_code(err_msg msg_code, const std::vector<wstr> &msg_args, uint32_t err_code, bool from_nt) {
	return err{ msg_code,msg_args,err_code,from_nt ? L"ntdll.dll" : L"" };
}

err error_other(err_msg msg_code, const std::vector<wstr> &msg_args) {
	return err{ msg_code,msg_args,0,L"" };
}

wstr err::format() const {
	std::wstringstream ss;

	auto fmt = boost::wformat(msg_table[msg_code]);
	for (const auto &s : msg_args) fmt = fmt % s;
	ss << fmt << std::endl;

	if (err_code) {
		wchar_t *buf = nullptr;
		auto f = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
		HMODULE hm = 0;
		if (!mod.empty()) {
			hm = LoadLibrary(mod.c_str());
			f |= FORMAT_MESSAGE_FROM_HMODULE;
		}
		auto ok = (mod.empty() || hm) && FormatMessage(f, hm, err_code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (wchar_t *)&buf, 0, nullptr);
		if (hm) FreeLibrary(hm);
		if (ok) {
			ss << L"Reason: " << buf;
			LocalFree(buf);
		} else {
			ss << L"Unknown error code: " << L"0x" << std::setfill(L'0') << std::setw(8) << std::hex << err_code;
		}
	}

	return ss.str();
}

void err::push_if_empty(crwstr arg) {
	if (msg_args.empty()) msg_args.push_back(arg);
}

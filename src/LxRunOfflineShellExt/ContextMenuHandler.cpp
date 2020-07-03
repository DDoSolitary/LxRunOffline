#include "pch.h"
#include "dllmain.h"
#include "ContextMenuHandler.h"
#include "../lib/reg.h"

CContextMenuHandler::~CContextMenuHandler() {
	if (this->hsm) {
		DestroyMenu(hsm);
	}
}

IFACEMETHODIMP CContextMenuHandler::Initialize(
	const PCIDLIST_ABSOLUTE pidlFolder,
	IDataObject *const pdtobj,
	HKEY
) {
	if (pdtobj) {
		CComPtr<IShellItemArray> psia;
		auto hr = SHCreateShellItemArrayFromDataObject(
			pdtobj,
			IID_IShellItemArray,
			reinterpret_cast<void **>(&psia)
		);
		if (FAILED(hr)) {
			return hr;
		}
		DWORD cnt;
		hr = psia->GetCount(&cnt);
		if (FAILED(hr)) {
			return hr;
		}
		if (cnt != 1) {
			return E_FAIL;
		}
		CComPtr<IShellItem> psi;
		hr = psia->GetItemAt(0, &psi);
		if (FAILED(hr)) {
			return hr;
		}
		LPWSTR name;
		hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &name);
		if (FAILED(hr)) {
			return hr;
		}
		this->path.assign(name);
	} else if (pidlFolder) {
		wchar_t buf[MAX_PATH];
		if (!SHGetPathFromIDList(pidlFolder, buf)) {
			return E_FAIL;
		}
		path.assign(buf);
	} else {
		return E_FAIL;
	}
	try {
		this->distros = list_distros();
	} catch (...) {
		return E_FAIL;
	}
	return S_OK;
}

IFACEMETHODIMP CContextMenuHandler::QueryContextMenu(
	const HMENU hmenu,
	const UINT indexMenu,
	const UINT idCmdFirst,
	const UINT idCmdLast,
	const UINT uFlags
) {
	if (uFlags & CMF_DEFAULTONLY) {
		return S_OK;
	}
	const auto distro_size = static_cast<UINT>(this->distros.size());
	const auto menu_size = std::min(distro_size, idCmdLast - idCmdFirst + 1);
	this->hsm = CreatePopupMenu();
	if (!this->hsm) {
		return HRESULT_FROM_WIN32(GetLastError());
	}
	for (UINT i = 0; i < menu_size; i++) {
		MENUITEMINFO mi;
		mi.cbSize = sizeof(mi);
		mi.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
		mi.fType = MFT_STRING;
		mi.wID = idCmdFirst + i;
		mi.dwTypeData = const_cast<LPWSTR>(this->distros[i].c_str());
		if (!InsertMenuItem(this->hsm, i, TRUE, &mi)) {
			return HRESULT_FROM_WIN32(GetLastError());
		}
	}
	const auto disabled = distro_size == 0;
	static wchar_t item_name[] = L"LxRunOffline";
	static wchar_t item_name_disabled[] = L"LxRunOffline (no distros)";
	MENUITEMINFO mi;
	mi.cbSize = sizeof(mi);
	mi.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_SUBMENU;
	mi.fType = MFT_STRING;
	mi.fState = disabled ? MFS_DISABLED : MFS_ENABLED;
	mi.dwTypeData = disabled ? item_name_disabled : item_name;
	mi.hSubMenu = this->hsm;
	if (!InsertMenuItem(hmenu, indexMenu, TRUE, &mi)) {
		return HRESULT_FROM_WIN32(GetLastError());
	}
	return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, menu_size);
}

IFACEMETHODIMP CContextMenuHandler::GetCommandString(UINT_PTR, UINT, UINT *, LPSTR, UINT) {
	return E_NOTIMPL;
}

IFACEMETHODIMP CContextMenuHandler::InvokeCommand(CMINVOKECOMMANDINFO *const pici) {
	auto unicode = false;
	if (pici->cbSize == sizeof(CMINVOKECOMMANDINFOEX) && pici->fMask & CMIC_MASK_UNICODE) {
		unicode = true;
	}
	if (unicode && HIWORD(reinterpret_cast<CMINVOKECOMMANDINFOEX *const>(pici)->lpVerbW) || !unicode && HIWORD(pici->lpVerb)) {
		return E_INVALIDARG;
	}
	const auto id = LOWORD(pici->lpVerb);
	if (id >= this->distros.size()) {
		return E_INVALIDARG;
	}
	HMODULE hm;
	if (!GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&_AtlModule),
		&hm
	)) {
		return HRESULT_FROM_WIN32(GetLastError());
	}
	wchar_t dll_path[MAX_PATH];
	const auto path_len = GetModuleFileName(hm, dll_path, MAX_PATH);
	if (path_len == 0) {
		return HRESULT_FROM_WIN32(GetLastError());
	}
	const auto exe_path = std::filesystem::path(std::wstring(dll_path, path_len)).replace_filename(L"LxRunOffline.exe");
	std::error_code ec;
	if (!std::filesystem::exists(exe_path, ec)) {
		MessageBox(
			pici->hwnd,
			L"Unable to find the LxRunOffline executable. Please make sure it is in the same directory as the shell extension DLL file.",
			L"LxRunOffline",
			MB_OK | MB_ICONERROR
		);
		return E_FAIL;
	}
	std::wstringstream cmd_stream;
	cmd_stream << std::quoted(exe_path.wstring()) << L" run -n " << std::quoted(this->distros[id]);
	const auto cmd = cmd_stream.str();
	const auto cmd_size = cmd.size();
	const auto cmd_buf = std::make_unique<wchar_t[]>(cmd_size + 1);
	cmd.copy(cmd_buf.get(), cmd_size);
	cmd_buf[cmd_size] = 0;
	STARTUPINFO si =  {};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	if (!CreateProcess(
		nullptr, cmd_buf.get(),
		nullptr, nullptr, FALSE, 0, nullptr,
		this->path.c_str(),
		&si, &pi
	)) {
		const _com_error ce(HRESULT_FROM_WIN32(GetLastError()), nullptr);
		MessageBox(
			pici->hwnd,
			(std::wstring(L"Failed to launch LxRunOffline.exe:\n") + ce.ErrorMessage()).c_str(),
			L"LxRunOffline",
			MB_OK | MB_ICONERROR
		);
		return ce.Error();
	}
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return S_OK;
}

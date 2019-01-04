#include "stdafx.h"
#include "error.h"
#include "utils.h"

void create_shortcut(crwstr distro_name, crwstr file_path, crwstr icon_path) {
	auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) throw error_hresult(err_create_shortcut, {}, hr);
	IShellLink *psl;
	hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&psl);
	if (FAILED(hr)) throw error_hresult(err_create_shortcut, {}, hr);
	wchar_t ep[MAX_PATH];
	if (!GetModuleFileName(0, ep, MAX_PATH)) {
		error_win32_last(err_create_shortcut, {});
	}
	psl->SetPath(ep);
	psl->SetDescription((L"Launch the WSL distribution " + distro_name + L'.').c_str());
	psl->SetArguments((L"run -w -n \"" + distro_name + L"\"").c_str());
	if (!icon_path.empty()) psl->SetIconLocation(get_full_path(icon_path).c_str(), 0);
	IPersistFile *ppf;
	hr = psl->QueryInterface(IID_IPersistFile, (void **)&ppf);
	if (FAILED(hr)) throw error_hresult(err_create_shortcut, {}, hr);
	hr = ppf->Save(get_full_path(file_path).c_str(), true);
	if (FAILED(hr)) throw error_hresult(err_create_shortcut, {}, hr);
	ppf->Release();
	psl->Release();
}

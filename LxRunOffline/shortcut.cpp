#include "stdafx.h"
#include "error.h"
#include "utils.h"

template<typename T>
void release_interface(T *p) {
	p->Release();
}

void create_shortcut(crwstr distro_name, crwstr file_path, crwstr icon_path) {
	auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) throw error_hresult(err_create_shortcut, {}, hr);
	IShellLink *psl;
	hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
	if (FAILED(hr)) throw error_hresult(err_create_shortcut, {}, hr);
	unique_ptr_del<IShellLink *> upsl(psl, release_interface<IShellLink>);
	wchar_t ep[MAX_PATH];
	if (!GetModuleFileName(0, ep, MAX_PATH)) {
		error_win32_last(err_create_shortcut, {});
	}
	upsl->SetPath(ep);
	upsl->SetDescription((L"Launch the WSL distribution " + distro_name + L'.').c_str());
	upsl->SetArguments((L"run -w -n \"" + distro_name + L"\"").c_str());
	if (!icon_path.empty()) upsl->SetIconLocation(get_full_path(icon_path).c_str(), 0);
	IPersistFile *ppf;
	hr = upsl->QueryInterface(IID_PPV_ARGS(&ppf));
	if (FAILED(hr)) throw error_hresult(err_create_shortcut, {}, hr);
	unique_ptr_del<IPersistFile *> uppf(ppf, release_interface<IPersistFile>);
	hr = uppf->Save(get_full_path(file_path).c_str(), true);
	if (FAILED(hr)) throw error_hresult(err_create_shortcut, {}, hr);
}

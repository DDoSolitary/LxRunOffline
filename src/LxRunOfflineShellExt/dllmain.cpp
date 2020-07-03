#include "pch.h"
#include "dllmain.h"

CLxRunOfflineShellExtModule _AtlModule;

extern "C" BOOL WINAPI DllMain(const HINSTANCE hInstance, const DWORD dwReason, const LPVOID lpReserved) {
	return _AtlModule.DllMain(dwReason, lpReserved);
}

_Use_decl_annotations_
STDAPI DllCanUnloadNow() {
	return _AtlModule.DllCanUnloadNow();
}

_Use_decl_annotations_
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
	return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

_Use_decl_annotations_
STDAPI DllRegisterServer() {
	return _AtlModule.DllRegisterServer();
}

_Use_decl_annotations_
STDAPI DllUnregisterServer() {
	return _AtlModule.DllUnregisterServer();
}

STDAPI DllInstall(const BOOL bInstall, const LPCWSTR pszCmdLine) {
	HRESULT hr = E_FAIL;
	static const wchar_t szUserSwitch[] = L"user";

	if (pszCmdLine != nullptr) {
		if (_wcsnicmp(pszCmdLine, szUserSwitch, _countof(szUserSwitch)) == 0) {
			ATL::AtlSetPerUserRegistration(true);
		}
	}

	if (bInstall) {
		hr = DllRegisterServer();
		if (FAILED(hr)) {
			DllUnregisterServer();
		}
	} else {
		hr = DllUnregisterServer();
	}

	return hr;
}

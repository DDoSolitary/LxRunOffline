#pragma once
#include "pch.h"
#include "res/resources.h"
#include "LxRunOfflineShellExt_i.h"

class CLxRunOfflineShellExtModule : public ATL::CAtlDllModuleT<CLxRunOfflineShellExtModule> {
public:
	DECLARE_LIBID(LIBID_LxRunOfflineShellExtLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_LXRUNOFFLINESHELLEXT, "{110e1da1-3e28-4133-a05c-ca13d5e4a34a}")
};

extern CLxRunOfflineShellExtModule _AtlModule;

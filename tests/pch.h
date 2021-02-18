#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS

#include <ntstatus.h>
#include <objbase.h>
#include <CommCtrl.h>
#include <ShlObj.h>
#include <malloc.h>

#include <LxRunOffline/error.h>
#include <LxRunOffline/fs.h>
#include <LxRunOffline/path.h>
#include <LxRunOffline/reg.h>
#include <LxRunOffline/shortcut.h>
#include <LxRunOffline/utils.h>

#pragma once

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

#include <atlbase.h>
#include <atlcom.h>
#include <ShlObj.h>
#include <strsafe.h>

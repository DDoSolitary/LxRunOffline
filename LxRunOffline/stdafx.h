#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS
#include <winternl.h>
#include <ntstatus.h>
#include <comdef.h>
#include <ShlObj.h>

#undef _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include <archive.h>
#include <archive_entry.h>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <tinyxml2.h>

typedef std::wstring wstr;
typedef const std::wstring &crwstr;

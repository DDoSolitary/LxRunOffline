#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#undef _CRT_SECURE_NO_WARNINGS

#define WIN32_NO_STATUS
#ifdef _MSC_VER
#define NOMINMAX
#endif
#include <Windows.h>
#ifdef _MSC_VER
#undef MOMINMAX
#endif
#undef WIN32_NO_STATUS

#include <winternl.h>
#include <ntstatus.h>
#include <comdef.h>
#include <ShlObj.h>
#include <AclAPI.h>
#include <io.h>
#include <fcntl.h>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <archive.h>
#include <archive_entry.h>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <tinyxml2.h>

typedef std::wstring wstr;
typedef const std::wstring &crwstr;

template<typename T>
using unique_ptr_del = std::unique_ptr<typename std::remove_pointer<T>::type, std::function<void(T)>>;

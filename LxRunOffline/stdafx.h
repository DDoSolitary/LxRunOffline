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
#include <AclAPI.h>
#include <io.h>
#include <fcntl.h>

#undef _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <stack>
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

#pragma once
#include "pch.h"

uint32_t get_win_build();
void log_warning(crwstr msg);
void log_error(crwstr msg);
void print_progress(double progress);
wstr from_utf8(const char *s);
std::unique_ptr<char[]> to_utf8(wstr s);
wstr get_full_path(crwstr path);

// Flexible array member (FAM) is widely used in Windows SDK headers. However, it is not part of the C++ standard but
// supported as a Microsoft-specific compiler extension by Visual C++. As a result, it is impossible to use C++ language
// features like "new" and "make_unique" to create structs with FAM properly.
// Although it may seem possible to create a char array and cast the pointer, such code actually violates strict
// aliasing rules and results in undefined behavior. (See https://github.com/DDoSolitary/LxRunOffline/issues/112)
// So it seems that the only viable way to do so is to use the C memory allocator "malloc".
template<typename T>
unique_ptr_del<T *> create_fam_struct(const size_t size) {
	// FAMs in Windows SDK are defined as "type name[1];" so the additional byte should be removed from size;
	return unique_ptr_del<T *>(static_cast<T *>(malloc(size)), free);
}

template<typename TEle, typename TLen>
std::pair<std::unique_ptr<TEle[]>, TLen> probe_and_call(std::function<TLen(TEle *, TLen)> func) {
	auto n = func(nullptr, 0);
	if (n <= 0) return { nullptr, 0 };
	auto buf = std::make_unique<TEle[]>(n);
	n = func(buf.get(), n);
	if (n <= 0) return { nullptr, 0 };
	return std::make_pair(std::move(buf), n);
}

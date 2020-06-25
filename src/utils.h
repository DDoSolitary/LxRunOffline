#pragma once
#include "stdafx.h"

extern const uint32_t win_build;

void log_warning(crwstr msg);
void log_error(crwstr msg);
void print_progress(double progress);
wstr from_utf8(const char *s);
std::unique_ptr<char[]> to_utf8(wstr s);
wstr get_full_path(crwstr path);

template<typename TEle, typename TLen>
std::pair<std::unique_ptr<TEle[]>, TLen> probe_and_call(std::function<TLen(TEle *, TLen)> func) {
	auto n = func(nullptr, 0);
	if (n <= 0) return { nullptr, 0 };
	auto buf = std::make_unique<TEle[]>(n);
	n = func(buf.get(), n);
	if (n <= 0) return { nullptr, 0 };
	return std::make_pair(std::move(buf), n);
}

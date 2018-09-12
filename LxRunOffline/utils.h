#pragma once
#include "stdafx.h"

extern uint32_t win_build;

void log_warning(crwstr msg);
void log_error(crwstr msg);
void print_progress(double progress);
wstr from_utf8(const char *s);
std::unique_ptr<char[]> to_utf8(wstr s);
wstr get_full_path(crwstr path);

template<typename TEle, typename TLen>
std::pair<std::unique_ptr<TEle[]>, TLen> probe_and_call(std::function<TLen(TEle *, TLen)> func) {
	auto n = func(nullptr, 0);
	if (n <= 0) return { nullptr,0 };
	auto buf = std::make_unique<TEle[]>(n);
	n = func(buf.get(), n);
	if (n <= 0) return { nullptr,0 };
	return std::make_pair(std::move(buf), n);
}

template<typename T>
class unique_val {
	T val;
	std::function<void(T)> deleter;

	void move(unique_val &o) {
		val = o.val;
		deleter = o.deleter;
		o.deleter = nullptr;
	}

public:
	unique_val(std::function<void(T &)> val_func, std::function<void(T)> deleter)
		: deleter(deleter) {
		val_func(val);
	}

	unique_val(T val, std::function<void(T)> deleter)
		: val(val), deleter(deleter) {}

	unique_val(unique_val &&o) {
		move(o);
	}

	unique_val &operator=(unique_val &&o) {
		if (deleter) deleter(val);
		move(o);
		return *this;
	}

	~unique_val() {
		if (deleter) {
			deleter(val);
			deleter = nullptr;
		}
	}

	unique_val(const unique_val &) = delete;
	unique_val &operator=(const unique_val &) = delete;

	T get() const {
		return val;
	}
};

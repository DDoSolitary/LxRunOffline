#pragma once
#include "stdafx.h"

void log_warning(crwstr msg);
void log_error(crwstr msg);
void print_progress(double progress);

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
	void move(unique_val &o) {
		val = o.val;
		deleter = o.deleter;
		empty = o.empty;
		o.empty = true;
	}

public:
	T val;
	std::function<void(T)> deleter;
	bool empty;

	unique_val(std::function<void(T *)> val_func, std::function<void(T)> deleter)
		: deleter(deleter), empty(false) {
		val_func(&val);
	}


	unique_val(T val, std::function<void(T)> deleter)
		: val(val), deleter(deleter), empty(false) {}

	unique_val(unique_val &&o) {
		move(o);
	}

	unique_val &operator=(unique_val &&o) {
		if (!empty) deleter(val);
		move(o);
		return *this;
	}

	~unique_val() {
		if (!empty) {
			deleter(val);
			empty = true;
		}
	}

	unique_val(const unique_val &) = delete;
	unique_val &operator=(const unique_val &) = delete;
};

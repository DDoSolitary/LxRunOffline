#pragma once
#include "pch.h"

class fixture_tmp_dir {
	std::filesystem::path orig_path;
public:
	void setup();
	void teardown() const;
};

class fixture_lang_en {
	LANGID orig_lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
public:
	void setup();
	void teardown() const;
};

class fixture_com {
public:
	void setup() const;
	void teardown() const;
};

class fixture_tmp_reg {
	HKEY hk;
public:
	static const wchar_t *const PATH;
	void setup();
	void teardown() const;
	HKEY get_hkey() const;
};

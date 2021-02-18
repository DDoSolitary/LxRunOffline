#include <boost/test/unit_test.hpp>
#include "pch.h"
#include "fixtures.h"
#include "utils.h"

namespace fs = std::filesystem;

void fixture_tmp_dir::setup() {
	orig_path = fs::current_path();
	const auto tmp_path = fs::temp_directory_path() / new_guid();
	BOOST_TEST_REQUIRE(fs::create_directory(tmp_path));
	fs::current_path(tmp_path);
}

void fixture_tmp_dir::teardown() const {
	const auto tmp_path = fs::current_path();
	fs::current_path(orig_path);
	BOOST_TEST_REQUIRE(fs::remove_all(tmp_path) > 0);
}

void fixture_lang_en::setup() {
	orig_lang = GetThreadUILanguage();
	const auto lang = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
	BOOST_TEST_REQUIRE(SetThreadUILanguage(lang) == lang);
}

void fixture_lang_en::teardown() const {
	BOOST_TEST_REQUIRE(SetThreadUILanguage(orig_lang) == orig_lang);
}

void fixture_com::setup() const {
	BOOST_TEST_REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
}

void fixture_com::teardown() const {
	CoUninitialize();
}

const wchar_t *const fixture_tmp_reg::PATH = L"Software\\59dca4c7-6bc5-45be-980c-ef1bd5e70ad1\\";

void fixture_tmp_reg::setup() {
	BOOST_TEST_REQUIRE(RegCreateKeyEx(HKEY_CURRENT_USER, PATH, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hk, nullptr) == ERROR_SUCCESS);
	BOOST_TEST_REQUIRE(RegDeleteTree(hk, nullptr) == ERROR_SUCCESS);
}

void fixture_tmp_reg::teardown() const {
	BOOST_TEST_REQUIRE(RegCloseKey(hk) == ERROR_SUCCESS);
	BOOST_TEST_REQUIRE(RegDeleteTree(HKEY_CURRENT_USER, PATH) == ERROR_SUCCESS);
}

HKEY fixture_tmp_reg::get_hkey() const {
	return hk;
}

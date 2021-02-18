#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include "pch.h"
#include "fixtures.h"

using namespace boost::unit_test;

BOOST_AUTO_TEST_SUITE(test_shortcut)

BOOST_TEST_DECORATOR(*fixture<fixture_tmp_dir>())
BOOST_TEST_DECORATOR(*fixture<fixture_com>())
BOOST_DATA_TEST_CASE(test_create_shortcut, data::make({ L"bar", L"" }), icon_path) {
	create_shortcut(L"foo", L"test.lnk", icon_path);
	IShellLink *psl;
	BOOST_TEST_REQUIRE(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl))));
	IPersistFile *ppf;
	BOOST_TEST_REQUIRE(SUCCEEDED(psl->QueryInterface(IID_PPV_ARGS(&ppf))));
	BOOST_TEST_REQUIRE(SUCCEEDED(ppf->Load(L"test.lnk", 0)));
	const auto buf = std::make_unique<wchar_t[]>(INFOTIPSIZE);
	BOOST_TEST_REQUIRE(SUCCEEDED(psl->GetPath(buf.get(), INFOTIPSIZE, nullptr, SLGP_RAWPATH)));
	wchar_t mod_path[MAX_PATH];
	BOOST_TEST_REQUIRE(GetModuleFileName(nullptr, mod_path, MAX_PATH));
	BOOST_TEST(_wcsicmp(buf.get(), mod_path) == 0);
	BOOST_TEST_REQUIRE(SUCCEEDED(psl->GetDescription(buf.get(), INFOTIPSIZE)));
	BOOST_TEST(buf.get() == L"Launch the WSL distribution foo.");
	BOOST_TEST_REQUIRE(SUCCEEDED(psl->GetArguments(buf.get(), INFOTIPSIZE)));
	BOOST_TEST(buf.get() == L"run -w -n \"foo\"");
	int idx;
	BOOST_TEST_REQUIRE(SUCCEEDED(psl->GetIconLocation(buf.get(), INFOTIPSIZE, &idx)));
	if (icon_path[0] == 0) {
		BOOST_TEST(buf.get()[0] == 0);
	} else {
		BOOST_TEST(_wcsicmp(buf.get(), (std::filesystem::current_path() / icon_path).wstring().c_str()) == 0);
	}
	BOOST_TEST(idx == 0);
}

BOOST_TEST_DECORATOR(*fixture<fixture_tmp_dir>())
BOOST_AUTO_TEST_CASE(test_create_shortcut_unintialized) {
	create_shortcut(L"foo", L"test.lnk", L"bar");
}

BOOST_AUTO_TEST_SUITE_END()

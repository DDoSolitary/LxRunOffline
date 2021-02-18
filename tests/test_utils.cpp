#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include "pch.h"
#include "fixtures.h"

using namespace boost::unit_test;

BOOST_AUTO_TEST_SUITE(test_utils)

BOOST_AUTO_TEST_CASE(test_get_win_build) {
	const auto build = get_win_build();
	BOOST_TEST(build >= 10000u);
	BOOST_TEST(build <= 99999u);
}

BOOST_AUTO_TEST_CASE(test_utf8_conversion) {
	const auto u16_str = L"测试";
	const auto u8_str = u8"测试";
	BOOST_TEST(u16_str == from_utf8(u8_str).c_str());
	BOOST_TEST(to_utf8(u16_str).get() == u8_str);
	BOOST_CHECK_THROW(from_utf8("\xc2"), lro_error);
	BOOST_CHECK_THROW(to_utf8(L"\xd800"), lro_error);
}

BOOST_TEST_DECORATOR(*fixture<fixture_tmp_dir>())
BOOST_AUTO_TEST_CASE(test_get_full_path) {
	namespace fs = std::filesystem;
	const auto pwd = fs::current_path();
	const auto foo_path = pwd / "foo";
	BOOST_TEST(_wcsicmp(get_full_path(L"foo\\bar").c_str(), (foo_path / "bar").wstring().c_str()) == 0);
	BOOST_TEST_REQUIRE(fs::create_directory(foo_path));
	fs::current_path(foo_path);
	BOOST_TEST(_wcsicmp(get_full_path(L"..\\bar").c_str(), (pwd / "bar").wstring().c_str()) == 0);
	fs::current_path(pwd);
	BOOST_TEST(_wcsicmp(get_full_path(pwd.wstring()).c_str(), pwd.wstring().c_str()) == 0);
	BOOST_CHECK_THROW(get_full_path(L""), lro_error);
}

struct fam_struct {
	int x;
	char arr[1];
};

BOOST_AUTO_TEST_CASE(test_create_fam_struct) {
	const auto p = create_fam_struct<fam_struct>(42);
	BOOST_TEST(p.get() != nullptr);
	BOOST_TEST(_msize(p.get()) >= 42);
}

BOOST_DATA_TEST_CASE(test_probe_and_call,
	data::make({ 42, 42, 0 }) ^
	data::make({ 41, 0, 42 }) ^
	data::make({ 41, 0, 0 }) ^
	data::make({ 2, 2, 1 }),
	ret1, ret2, expected_ret, expected_count
) {
	auto call_count = 0;
	const auto [res_ptr, res_len] = probe_and_call<char, int>([&](const char *ptr, const int len) {
		call_count++;
		if (call_count == 1) {
			BOOST_TEST(ptr == nullptr);
			BOOST_TEST(len == 0);
			return ret1;
		}
		BOOST_TEST(ptr != nullptr);
		BOOST_TEST(len == ret1);
		return ret2;
	});
	BOOST_TEST(call_count == expected_count);
	if (expected_ret == 0) {
		BOOST_TEST(res_ptr.get() == nullptr);
	} else {
		BOOST_TEST(res_ptr.get() != nullptr);
		BOOST_TEST(_msize(res_ptr.get()) >= res_len);
	}
	BOOST_TEST(res_len == expected_ret);
}

BOOST_AUTO_TEST_SUITE_END()

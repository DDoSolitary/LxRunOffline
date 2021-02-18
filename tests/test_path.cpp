#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/mpl/list.hpp>
#include "pch.h"

using namespace boost::unit_test;

BOOST_AUTO_TEST_SUITE(test_path)

BOOST_AUTO_TEST_CASE(test_ctor_linux) {
	BOOST_TEST(linux_path(L"foobar", L"foo").data.empty());
	BOOST_TEST(linux_path(L"foo", L"foobar").data.empty());
	BOOST_TEST(linux_path(L"foo", L"foo").data.empty());
	BOOST_TEST(linux_path(L"foo/", L"foo").data.empty());
	BOOST_TEST(linux_path(L"foo", L"foo/").data.empty());
	BOOST_TEST(linux_path(L"foo/", L"foo/").data.empty());
	BOOST_TEST(linux_path(L"foo/bar", L"foo").data.c_str() == L"bar");
	BOOST_TEST(linux_path(L"foo/bar", L"foo/").data.c_str() == L"bar");
	BOOST_TEST(linux_path(L"foo", L"").data.c_str() == L"foo");
	BOOST_TEST(linux_path(L"/foo", L"").data.c_str() == L"foo");
	BOOST_TEST(linux_path(L"//foo", L"").data.c_str() == L"foo");
	BOOST_TEST(linux_path(L"foo//bar", L"").data.c_str() == L"foo/bar");
	BOOST_TEST(linux_path(L"./foo/./bar/./", L"").data.c_str() == L"foo/bar/");
	BOOST_TEST(linux_path(L"../foo", L"").data.c_str() == L"foo");
	BOOST_TEST(linux_path(L"foo/../bar", L"").data.c_str() == L"bar");
	BOOST_TEST(linux_path(L"foo/../../bar", L"").data.c_str() == L"bar");
	BOOST_TEST(linux_path(L"foo/bar/../", L"").data.c_str() == L"foo/");
}

BOOST_AUTO_TEST_CASE(test_clone_linux) {
	const linux_path path(L"foo", L"");
	const auto cloned_path = path.clone();
	BOOST_TEST(cloned_path->base_len == path.base_len);
	BOOST_TEST(cloned_path->data.c_str() == path.data.c_str());
}

typedef boost::mpl::list<wsl_v1_path, wsl_v2_path, wsl_legacy_path> wsl_path_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(test_ctor_wsl, T, wsl_path_types) {
	T path(L"foo");
	BOOST_TEST(_wcsicmp(path.data.c_str(), (L"\\\\?\\" + (std::filesystem::current_path() / L"foo\\").wstring()).c_str()) == 0);
	BOOST_TEST(path.base_len == path.data.size());
	path = T(L"C:\\foo");
	BOOST_TEST(_wcsicmp(path.data.c_str(), L"\\\\?\\C:\\foo\\") == 0);
	BOOST_TEST(path.base_len == path.data.size());
	path = T(L"\\\\?\\C:\\foo\\");
	BOOST_TEST(_wcsicmp(path.data.c_str(), L"\\\\?\\C:\\foo\\") == 0);
	BOOST_TEST(path.base_len == path.data.size());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_clone_wsl, T, wsl_path_types) {
	T path(L"foo");
	path.data += L"bar";
	const auto cloned_path = path.clone();
	BOOST_TEST(cloned_path->base_len == path.base_len);
	BOOST_TEST(cloned_path->data.c_str() == path.data.c_str());
}

static const wchar_t *const test_paths[][4] = {
	{
		L"foo/bar",
		L"\\\\?\\C:\\base_v1\\rootfs\\foo\\bar",
		L"\\\\?\\C:\\base_v2\\rootfs\\foo\\bar",
		L"\\\\?\\C:\\base_legacy\\rootfs\\foo\\bar"
	}, {
		L"root/foo",
		L"\\\\?\\C:\\base_v1\\rootfs\\root\\foo",
		L"\\\\?\\C:\\base_v2\\rootfs\\root\\foo",
		L"\\\\?\\C:\\base_legacy\\root\\foo"
	}, {
		L"home/foo",
		L"\\\\?\\C:\\base_v1\\rootfs\\home\\foo",
		L"\\\\?\\C:\\base_v2\\rootfs\\home\\foo",
		L"\\\\?\\C:\\base_legacy\\home\\foo"
	}, {
		L"mnt/foo",
		L"\\\\?\\C:\\base_v1\\rootfs\\mnt\\foo",
		L"\\\\?\\C:\\base_v2\\rootfs\\mnt\\foo",
		L"\\\\?\\C:\\base_legacy\\mnt\\foo"
	}, {
		L"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F<>:\"\\|*?#",
		L"\\\\?\\C:\\base_v1\\rootfs\\#0001#0002#0003#0004#0005#0006#0007#0008#0009#000A#000B#000C#000D#000E#000F#0010#0011#0012#0013#0014#0015#0016#0017#0018#0019#001A#001B#001C#001D#001E#001F#003C#003E#003A#0022#005C#007C#002A#003F#0023",
		L"\\\\?\\C:\\base_v2\\rootfs\\\xF001\xF002\xF003\xF004\xF005\xF006\xF007\xF008\xF009\xF00A\xF00B\xF00C\xF00D\xF00E\xF00F\xF010\xF011\xF012\xF013\xF014\xF015\xF016\xF017\xF018\xF019\xF01A\xF01B\xF01C\xF01D\xF01E\xF01F\xF03C\xF03E\xF03A\xF022\xF05C\xF07C\xF02A\xF03F#",
		L"\\\\?\\C:\\base_legacy\\rootfs\\#0001#0002#0003#0004#0005#0006#0007#0008#0009#000A#000B#000C#000D#000E#000F#0010#0011#0012#0013#0014#0015#0016#0017#0018#0019#001A#001B#001C#001D#001E#001F#003C#003E#003A#0022#005C#007C#002A#003F#0023"
	}
};

BOOST_DATA_TEST_CASE(test_path_conversion, data::xrange(static_cast<size_t>(0), std::extent<decltype(test_paths)>::value), i) {
	linux_path dst_linux;
	wsl_v1_path dst_wsl_v1(L"C:\\base_v1");
	wsl_v2_path dst_wsl_v2(L"C:\\base_v2");
	wsl_legacy_path dst_wsl_legacy(L"C:\\base_legacy");

	const linux_path src_linux(test_paths[i][0], L"");
	BOOST_TEST(src_linux.convert(dst_linux));
	BOOST_TEST(dst_linux.data.c_str() == test_paths[i][0]);
	BOOST_TEST(src_linux.convert(dst_wsl_v1));
	BOOST_TEST(dst_wsl_v1.data.c_str() == test_paths[i][1]);
	BOOST_TEST(src_linux.convert(dst_wsl_v2));
	BOOST_TEST(dst_wsl_v2.data.c_str() == test_paths[i][2]);
	BOOST_TEST(src_linux.convert(dst_wsl_legacy));
	BOOST_TEST(dst_wsl_legacy.data.c_str() == test_paths[i][3]);

	wsl_v1_path src_wsl_v1(L"C:\\base_v1");
	src_wsl_v1.data = test_paths[i][1];
	BOOST_TEST(src_wsl_v1.convert(dst_linux));
	BOOST_TEST(dst_linux.data.c_str() == test_paths[i][0]);
	BOOST_TEST(src_wsl_v1.convert(dst_wsl_v1));
	BOOST_TEST(dst_wsl_v1.data.c_str() == test_paths[i][1]);
	BOOST_TEST(src_wsl_v1.convert(dst_wsl_v2));
	BOOST_TEST(dst_wsl_v2.data.c_str() == test_paths[i][2]);
	BOOST_TEST(src_wsl_v1.convert(dst_wsl_legacy));
	BOOST_TEST(dst_wsl_legacy.data.c_str() == test_paths[i][3]);

	wsl_v2_path src_wsl_v2(L"C:\\base_v2");
	src_wsl_v2.data = test_paths[i][2];
	BOOST_TEST(src_wsl_v2.convert(dst_linux));
	BOOST_TEST(dst_linux.data.c_str() == test_paths[i][0]);
	BOOST_TEST(src_wsl_v2.convert(dst_wsl_v1));
	BOOST_TEST(dst_wsl_v1.data.c_str() == test_paths[i][1]);
	BOOST_TEST(src_wsl_v2.convert(dst_wsl_v2));
	BOOST_TEST(dst_wsl_v2.data.c_str() == test_paths[i][2]);
	BOOST_TEST(src_wsl_v2.convert(dst_wsl_legacy));
	BOOST_TEST(dst_wsl_legacy.data.c_str() == test_paths[i][3]);

	wsl_legacy_path src_wsl_legacy(L"C:\\base_legacy");
	src_wsl_legacy.data = test_paths[i][3];
	BOOST_TEST(src_wsl_legacy.convert(dst_linux));
	BOOST_TEST(dst_linux.data.c_str() == test_paths[i][0]);
	BOOST_TEST(src_wsl_legacy.convert(dst_wsl_v1));
	BOOST_TEST(dst_wsl_v1.data.c_str() == test_paths[i][1]);
	BOOST_TEST(src_wsl_legacy.convert(dst_wsl_v2));
	BOOST_TEST(dst_wsl_v2.data.c_str() == test_paths[i][2]);
	BOOST_TEST(src_wsl_legacy.convert(dst_wsl_legacy));
	BOOST_TEST(dst_wsl_legacy.data.c_str() == test_paths[i][3]);
}

BOOST_AUTO_TEST_CASE(test_conversion_failure) {
	linux_path dst_linux(L"", L"");
	wsl_legacy_path dst_wsl_legacy(L"C:\\base_legacy");

	wsl_v2_path src_wsl_v2(L"C:\\base_v2");
	src_wsl_v2.data += L"foo";
	BOOST_TEST(!src_wsl_v2.convert(dst_linux));

	src_wsl_v2.reset();
	src_wsl_v2.data += L"root\\";
	BOOST_TEST(!src_wsl_v2.convert(dst_wsl_legacy));
	src_wsl_v2.reset();
	src_wsl_v2.data += L"root";
	BOOST_TEST(!src_wsl_v2.convert(dst_wsl_legacy));
	src_wsl_v2.reset();
	src_wsl_v2.data += L"home\\";
	BOOST_TEST(!src_wsl_v2.convert(dst_wsl_legacy));
	src_wsl_v2.reset();
	src_wsl_v2.data += L"home";
	BOOST_TEST(!src_wsl_v2.convert(dst_wsl_legacy));
	src_wsl_v2.reset();
	src_wsl_v2.data += L"mnt\\";
	BOOST_TEST(!src_wsl_v2.convert(dst_wsl_legacy));
	src_wsl_v2.reset();
	src_wsl_v2.data += L"mnt";
	BOOST_TEST(!src_wsl_v2.convert(dst_wsl_legacy));

	wsl_legacy_path src_wsl_legacy(L"C:\\base_legacy");
	src_wsl_legacy.data += L"rootfs\\root\\";
	BOOST_TEST(!src_wsl_legacy.convert(dst_linux));
	src_wsl_legacy.reset();
	src_wsl_legacy.data += L"rootfs\\root";
	BOOST_TEST(!src_wsl_legacy.convert(dst_linux));
	src_wsl_legacy.reset();
	src_wsl_legacy.data += L"rootfs\\home\\";
	BOOST_TEST(!src_wsl_legacy.convert(dst_linux));
	src_wsl_legacy.reset();
	src_wsl_legacy.data += L"rootfs\\home";
	BOOST_TEST(!src_wsl_legacy.convert(dst_linux));
	src_wsl_legacy.reset();
	src_wsl_legacy.data += L"rootfs\\mnt\\";
	BOOST_TEST(!src_wsl_legacy.convert(dst_linux));
	src_wsl_legacy.reset();
	src_wsl_legacy.data += L"rootfs\\mnt";
	BOOST_TEST(!src_wsl_legacy.convert(dst_linux));
}

BOOST_AUTO_TEST_CASE(test_wsl_v2_reserved) {
	const linux_path src(L"\xF001", L"");
	wsl_v2_path dst(L"C:\\base_v2");
	src.convert(dst);
	BOOST_TEST(dst.data.c_str() == L"\\\\?\\C:\\base_v2\\rootfs\\\xF001");
}

BOOST_AUTO_TEST_SUITE_END()

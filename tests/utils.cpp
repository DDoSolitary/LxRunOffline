#include <boost/test/unit_test.hpp>
#include "pch.h"

std::wstring new_guid() {
	GUID guid;
	BOOST_TEST_REQUIRE(SUCCEEDED(CoCreateGuid(&guid)));
	const auto buf = std::make_unique<wchar_t[]>(39);
	BOOST_TEST_REQUIRE(StringFromGUID2(guid, buf.get(), 39) != 0);
	return buf.get();
}

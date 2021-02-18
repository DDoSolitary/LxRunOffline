#include <boost/test/unit_test.hpp>
#include "pch.h"
#include "fixtures.h"

using namespace boost::unit_test;

BOOST_AUTO_TEST_SUITE(test_error)

BOOST_TEST_DECORATOR(*fixture<fixture_lang_en>())
BOOST_AUTO_TEST_CASE(test_from_hresult) {
	const auto err = lro_error::from_hresult(
		err_msg::err_test,
		{ L"foo" },
		HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION)
	);
	BOOST_TEST(err.format().c_str() == L"Test error message: foo\nReason: Incorrect function.");
}

BOOST_TEST_DECORATOR(*fixture<fixture_lang_en>())
BOOST_AUTO_TEST_CASE(test_from_win32) {
	const auto err = lro_error::from_win32(
		err_msg::err_test,
		{ L"foo" },
		ERROR_INVALID_FUNCTION
	);
	BOOST_TEST(err.format().c_str() == L"Test error message: foo\nReason: Incorrect function.");
}

BOOST_TEST_DECORATOR(*fixture<fixture_lang_en>())
BOOST_AUTO_TEST_CASE(test_from_win32_last) {
	SetLastError(ERROR_INVALID_FUNCTION);
	const auto err = lro_error::from_win32_last(err_msg::err_test, { L"foo" });
	SetLastError(ERROR_SUCCESS);
	BOOST_TEST(err.format().c_str() == L"Test error message: foo\nReason: Incorrect function.");
}

BOOST_TEST_DECORATOR(*fixture<fixture_lang_en>())
BOOST_AUTO_TEST_CASE(test_from_nt) {
	const auto err = lro_error::from_nt(
		err_msg::err_test,
		{ L"foo" },
		STATUS_UNSUCCESSFUL
	);
	BOOST_TEST(err.format().c_str() == L"Test error message: foo\nReason: {Operation Failed}\r\nThe requested operation was unsuccessful.");
}

BOOST_AUTO_TEST_CASE(test_from_nt_unknown) {
	const auto err = lro_error::from_nt(
		err_msg::err_test,
		{ L"foo" },
		0xefffffff
	);
	BOOST_TEST(err.format().c_str() == L"Test error message: foo\nReason: Unknown NTSTATUS: 0xefffffff");
}

BOOST_AUTO_TEST_CASE(test_from_other) {
	const auto err = lro_error::from_other(err_msg::err_test, { L"foo" });
	BOOST_TEST(err.format().c_str() == L"Test error message: foo");
}

BOOST_AUTO_TEST_SUITE_END()

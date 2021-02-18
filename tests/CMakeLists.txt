add_executable(LxRunOfflineTest
	"main.cpp"
	"fixtures.cpp"
	"utils.cpp"
	"test_error.cpp"
	"test_path.cpp"
	"test_reg.cpp"
	"test_shortcut.cpp"
	"test_utils.cpp"
	"res/resources.rc")

target_link_libraries(LxRunOfflineTest LibLxRunOffline)
target_precompile_headers(LxRunOfflineTest PRIVATE pch.h)

find_package(Boost REQUIRED COMPONENTS unit_test_framework)
target_link_libraries(LxRunOfflineTest Boost::unit_test_framework)
if(NOT LXRUNOFFLINE_STATIC)
	target_compile_definitions(LxRunOfflineTest PRIVATE BOOST_TEST_DYN_LINK)
endif()

if(MSVC)
	target_link_options(LxRunOfflineTest PRIVATE /MANIFEST:NO)
endif()

add_test(NAME "\"LxRunOffline Unit Tests\"" COMMAND LxRunOfflineTest)

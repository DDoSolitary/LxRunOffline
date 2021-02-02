add_library(LibLxRunOffline STATIC
	"error.cpp"
	"fs.cpp"
	"path.cpp"
	"reg.cpp"
	"shortcut.cpp"
	"utils.cpp")

target_include_directories(LibLxRunOffline INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_precompile_headers(LibLxRunOffline PRIVATE pch.h)
target_link_libraries(LibLxRunOffline PUBLIC ntdll)
if(LXRUNOFFLINE_STATIC AND MSVC)
	target_link_libraries(LibLxRunOffline PUBLIC ws2_32 crypt32)
endif()

find_package(LibArchive REQUIRED)
target_link_libraries(LibLxRunOffline PUBLIC LibArchive::LibArchive)
if (LXRUNOFFLINE_STATIC AND MINGW)
	find_package(ZLIB REQUIRED)
	find_package(BZip2 REQUIRED)
	find_package(LibLZMA REQUIRED)
	find_package(EXPAT REQUIRED)
	find_package(Iconv REQUIRED)
	find_library(LZ4_LIBRARY lz4 REQUIRED)
	find_library(ZSTD_LIBRARY zstd REQUIRED)
	target_link_libraries(LibArchive::LibArchive INTERFACE
		ZLIB::ZLIB
		BZip2::BZip2
		LibLZMA::LibLZMA
		EXPAT::EXPAT
		Iconv::Iconv
		${LZ4_LIBRARY}
		${ZSTD_LIBRARY}
		bcrypt)
endif()

if(MSVC)
	find_package(tinyxml2 REQUIRED)
	target_link_libraries(LibLxRunOffline PUBLIC tinyxml2::tinyxml2)
else()
	# The config provided by tinyxml2 doesn't support static linking so we neeed to find it manually.
	find_library(TINYXML2_LIBRARY tinyxml2 REQUIRED)
	find_path(TINYXML2_INCLUDE_DIR tinyxml2.h REQUIRED)
	target_link_libraries(LibLxRunOffline PUBLIC ${TINYXML2_LIBRARY})
	target_include_directories(LibLxRunOffline PUBLIC ${TINYXML2_INCLUDE_DIR})
	if(CMAKE_BUILD_TYPE STREQUAL Debug)
		target_compile_definitions(LibLxRunOffline PUBLIC TINYXML2_DEBUG)
	endif()
endif()

find_package(Boost REQUIRED)
target_link_libraries(LibLxRunOffline PRIVATE Boost::headers)

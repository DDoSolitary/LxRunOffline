add_executable(LxRunOffline
	error.cpp
	fs.cpp
	main.cpp
	path.cpp
	reg.cpp
	shortcut.cpp
	utils.cpp
	res/resources.rc)

configure_file(config.h.in config.h)
target_include_directories(LxRunOffline PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_precompile_headers(LxRunOffline PRIVATE stdafx.h)
target_link_libraries(LxRunOffline PRIVATE ntdll)
if(MSVC)
	target_link_libraries(LxRunOffline PRIVATE ws2_32 crypt32)
	target_link_options(LxRunOffline PRIVATE /MANIFEST:NO)
endif()

find_package(LibArchive REQUIRED)
target_link_libraries(LxRunOffline PRIVATE LibArchive::LibArchive)
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
	target_link_libraries(LxRunOffline PRIVATE tinyxml2::tinyxml2)
else()
	# The config provided by tinyxml2 doesn't support static linking so we neeed to find it manually.
	find_library(TINYXML2_LIBRARY tinyxml2 REQUIRED)
	find_path(TINYXML2_INCLUDE_DIR tinyxml2.h REQUIRED)
	target_link_libraries(LxRunOffline PRIVATE ${TINYXML2_LIBRARY})
	target_include_directories(LxRunOffline PRIVATE ${TINYXML2_INCLUDE_DIR})
	if(CMAKE_BUILD_TYPE STREQUAL Debug)
		target_compile_definitions(LxRunOffline PRIVATE TINYXML2_DEBUG)
	endif()
endif()

find_package(Boost REQUIRED COMPONENTS program_options)
target_link_libraries(LxRunOffline PRIVATE Boost::headers Boost::program_options)

install(TARGETS LxRunOffline RUNTIME DESTINATION .)

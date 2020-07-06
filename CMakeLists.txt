cmake_minimum_required(VERSION 3.17)

execute_process(
	COMMAND git describe --tags
	OUTPUT_VARIABLE LxRunOffline_VERSION_STR
	RESULT_VARIABLE GIT_RESULT
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(GIT_RESULT EQUAL 0)
	string(SUBSTRING ${LxRunOffline_VERSION_STR} 1 -1 VERSION)
	string(FIND ${VERSION} - VERSION_SLASH_OFFSET)
	string(SUBSTRING ${VERSION} 0 ${VERSION_SLASH_OFFSET} VERSION)
else()
	message(WARNING "Unable to retrieve version using git.")
	set(LxRunOffline_VERSION_STR "unknown version")
	set(VERSION 0.0.0)
endif()

project(LxRunOffline VERSION ${VERSION})

option(LXRUNOFFLINE_STATIC "Link statically" ON)
option(BUILD_CHOCO_PKG "Package the files for Chocolatey" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)

if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
	# Build fails when linking Boost statically using MinGW.
	if(NOT LXRUNOFFLINE_STATIC OR NOT MINGW)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
	endif()
endif()

if(MSVC)
	add_compile_definitions(NOMINMAX)
	add_compile_options(/W3 /wd4068)
	set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
	if(NOT LXRUNOFFLINE_STATIC)
		string(APPEND CMAKE_MSVC_RUNTIME_LIBRARY DLL)
	endif()
elseif(MINGW)
	add_link_options(-municode)
	add_compile_options(-Wall -Wextra -Wpedantic -Wno-unknown-pragmas -Wno-parentheses)
	if(LXRUNOFFLINE_STATIC)
		add_link_options(-static -static-libgcc -static-libstdc++)
		list(PREPEND CMAKE_FIND_LIBRARY_SUFFIXES .a)
	endif()
else()
	message(WARNING "Only MinGW and MSVC compilers are supported.")
endif()

if(BUILD_CHOCO_PKG)
	if(MSVC)
		add_subdirectory(choco)
	else()
		message(FATAL_ERROR "Can't build Chocolatey package with non-MSVC compilers because the shell extension can't be built.")
	endif()
endif()
add_subdirectory(src)

install(FILES LICENSE LICENSE-3RD-PARTY DESTINATION .)

if(MSVC)
	set(ZIP_NAME_SUFFIX -msvc)
else()
	set(ZIP_NAME_SUFFIX -mingw)
endif()

set(CPACK_GENERATOR ZIP)
set(CPACK_STRIP_FILES ON)
set(CPACK_PACKAGE_FILE_NAME LxRunOffline-${LxRunOffline_VERSION_STR}${ZIP_NAME_SUFFIX})
include(CPack)

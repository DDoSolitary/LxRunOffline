add_subdirectory(lib)
add_subdirectory(LxRunOffline)

# MinGW doesn't support ATL.
if(MSVC)
	add_subdirectory(LxRunOfflineShellExt)
else()
	message(WARNING "Not using MSVC compiler. Shell extension will not be built.")
endif()

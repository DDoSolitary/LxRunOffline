add_subdirectory(lib)
add_subdirectory(LxRunOffline)

# MinGW doesn't support ATL.
if(MSVC)
	add_subdirectory(LxRunOfflineShellExt)
endif()

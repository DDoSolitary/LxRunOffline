set(IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/LxRunOfflineShellExt.idl)
set(IDL_HEADER_FILE ${CMAKE_CURRENT_BINARY_DIR}/LxRunOfflineShellExt_i.h)
set(IID_FILE ${CMAKE_CURRENT_BINARY_DIR}/LxRunOfflineShellExt_i.cpp)
set(TLB_FILE ${CMAKE_CURRENT_BINARY_DIR}/LxRunOfflineShellExt.tlb)

set(MIDL_COMMAND MIDL /h LxRunOfflineShellExt_i.h /iid LxRunOfflineShellExt_i.cpp /tlb LxRunOfflineShellExt.tlb /env x64 /target NT62)
if(CMAKE_BUILD_TYPE STREQUAL Debug)
	list(APPEND MIDL_COMMAND /D _DEBUG /W1)
endif()
list(APPEND MIDL_COMMAND ${IDL_FILE})

add_custom_command(
	OUTPUT ${IDL_HEADER_FILE} ${IID_FILE} ${TLB_FILE}
	COMMAND ${MIDL_COMMAND}
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS ${IDL_FILE}
	VERBATIM)

add_library(LxRunOfflineShellExt SHARED
	dllmain.cpp
	ContextMenuHandler.cpp
	${IID_FILE}
	LxRunOfflineShellExt.def
	res/resources.rc)

target_compile_definitions(LxRunOfflineShellExt PRIVATE _WINDLL _USRDLL)
target_include_directories(LxRunOfflineShellExt PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_precompile_headers(LxRunOfflineShellExt PRIVATE pch.h)

target_link_libraries(LxRunOfflineShellExt PRIVATE LibLxRunOffline)

install(TARGETS LxRunOfflineShellExt RUNTIME DESTINATION .)

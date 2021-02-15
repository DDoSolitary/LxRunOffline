set(HANDLER_CLSID_STR 10b70111-8421-4200-a46e-91e7ded88e5b)
configure_file(config.h.in config.h)
configure_file(res/ContextMenuHandler.rgs.in ContextMenuHandler.rgs)

set(IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/LxRunOfflineShellExt.idl)
set(IDL_HEADER_FILE ${CMAKE_CURRENT_BINARY_DIR}/LxRunOfflineShellExt_i.h)
set(IID_FILE ${CMAKE_CURRENT_BINARY_DIR}/LxRunOfflineShellExt_i.cpp)
set(TLB_FILE ${CMAKE_CURRENT_BINARY_DIR}/LxRunOfflineShellExt.tlb)

set(MIDL_COMMAND MIDL
	/I ${CMAKE_CURRENT_BINARY_DIR}
	/h LxRunOfflineShellExt_i.h
	/iid LxRunOfflineShellExt_i.cpp
	/tlb LxRunOfflineShellExt.tlb
	/env x64 /target NT62)
if(CMAKE_BUILD_TYPE STREQUAL Debug)
	list(APPEND MIDL_COMMAND /D _DEBUG /W1)
endif()
list(APPEND MIDL_COMMAND ${IDL_FILE})

add_custom_command(
	OUTPUT ${IDL_HEADER_FILE} ${IID_FILE} ${TLB_FILE}
	COMMAND ${MIDL_COMMAND}
	DEPENDS config.h ${IDL_FILE}
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	VERBATIM)

add_library(LxRunOfflineShellExt SHARED
	dllmain.cpp
	ContextMenuHandler.cpp
	${IDL_HEADER_FILE}
	${IID_FILE}
	${TLB_FILE}
	LxRunOfflineShellExt.def
	res/resources.rc)

target_compile_definitions(LxRunOfflineShellExt PRIVATE _WINDLL _USRDLL)
target_include_directories(LxRunOfflineShellExt PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_precompile_headers(LxRunOfflineShellExt PRIVATE pch.h)

target_link_libraries(LxRunOfflineShellExt PRIVATE LibLxRunOffline)

install(TARGETS LxRunOfflineShellExt RUNTIME DESTINATION .)

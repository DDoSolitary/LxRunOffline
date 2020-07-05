configure_file(lxrunoffline.nuspec.in pkg/lxrunoffline.nuspec)
configure_file(tools/chocolateyInstall.ps1 pkg/tools/chocolateyInstall.ps1 COPYONLY)
configure_file(tools/chocolateyUninstall.ps1 pkg/tools/chocolateyUninstall.ps1 COPYONLY)

set(PKG_DIR ${CMAKE_CURRENT_BINARY_DIR}/pkg)
set(PKG_PATH ${PKG_DIR}/lxrunoffline.${VERSION}.nupkg)

add_custom_target(choco ALL DEPENDS ${PKG_PATH})

add_custom_command(
	TARGET choco PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:LxRunOffline> ${PKG_DIR}/tools/)

add_custom_command(
	TARGET choco PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:LxRunOfflineShellExt> ${PKG_DIR}/tools/)

add_custom_command(
	OUTPUT ${PKG_PATH}
	COMMAND choco pack
	DEPENDS
		pkg/lxrunoffline.nuspec
		pkg/tools/chocolateyInstall.ps1
		pkg/tools/chocolateyUninstall.ps1
		$<TARGET_FILE:LxRunOffline>
		$<TARGET_FILE:LxRunOfflineShellExt>
	WORKING_DIRECTORY ${PKG_DIR})

install(FILES ${PKG_PATH} DESTINATION . COMPONENT choco EXCLUDE_FROM_ALL)

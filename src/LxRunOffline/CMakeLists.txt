add_executable(LxRunOffline
	main.cpp
	res/resources.rc)

configure_file(config.h.in config.h)
target_include_directories(LxRunOffline PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(LxRunOffline PRIVATE LibLxRunOffline)

find_package(Boost REQUIRED COMPONENTS program_options)
target_link_libraries(LxRunOffline PRIVATE Boost::program_options)

if(MSVC)
	target_link_options(LxRunOffline PRIVATE /MANIFEST:NO)
endif()

install(TARGETS LxRunOffline RUNTIME DESTINATION .)

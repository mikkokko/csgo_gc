project(steam_api)

add_library(steam_api INTERFACE)

set(SDK_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk)

target_include_directories(steam_api INTERFACE ${SDK_DIR}/public)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(STEAM_API_LIB ${SDK_DIR}/redistributable_bin/osx/libsteam_api.dylib)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	if (IS_32BIT)
		set(STEAM_API_LIB ${SDK_DIR}/redistributable_bin/linux32/libsteam_api.so)
	else()
		set(STEAM_API_LIB ${SDK_DIR}/redistributable_bin/linux64/libsteam_api.so)
	endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if (IS_32BIT)
		set(STEAM_API_LIB ${SDK_DIR}/redistributable_bin/steam_api.lib)
	else()
		set(STEAM_API_LIB ${SDK_DIR}/redistributable_bin/win64/steam_api64.lib)
	endif()
else()
	message(FATAL_ERROR "Unsupported system $[CMAKE_SYSTEM_NAME}")
endif()

target_link_libraries(steam_api INTERFACE ${STEAM_API_LIB})

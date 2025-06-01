project(csgo_gc)

add_library(csgo_gc SHARED
    case_opening.cpp
    config.cpp
    gc_client.cpp
    gc_message.cpp
    gc_server.cpp
    gc_shared.cpp
    graffiti.cpp
    inventory.cpp
    item_schema.cpp
    keyvalue.cpp
    main.cpp
    networking_client.cpp
    networking_server.cpp
    steam_hook.cpp)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(csgo_gc PRIVATE platform_windows.cpp)
else()
    target_sources(csgo_gc PRIVATE platform_unix.cpp)
endif()

target_precompile_headers(csgo_gc PRIVATE stdafx.h)

set_target_properties(csgo_gc PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(csgo_gc PRIVATE /W4 /wd4244)
    target_compile_definitions(csgo_gc PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(csgo_gc PRIVATE -Wall -Wextra)
endif()

file(GLOB PROTOBUFS ../protobufs/*.cc)
target_include_directories(csgo_gc PRIVATE ../protobufs)
target_sources(csgo_gc PRIVATE ${PROTOBUFS})
source_group("Protobufs" FILES ${PROTOBUFS})

target_link_libraries(csgo_gc PRIVATE
    cryptopp
    funchook-static
    libprotobuf-lite
    steam_api)

# apple message box
if (APPLE)
    target_link_libraries(csgo_gc PRIVATE "-framework Foundation")
endif()

if (OUTDIR)
    add_custom_command(TARGET csgo_gc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:csgo_gc>
        ${OUTDIR}/csgo_gc/${GC_LIB_DIR}/$<TARGET_FILE_NAME:csgo_gc>)
endif()

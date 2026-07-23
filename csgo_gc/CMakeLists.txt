project(csgo_gc)

find_package(MbedTLS CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# funchook and its deps get pulled in via FetchContent
# FIXME: consider alternatives!!! funchook is no longer
# maintained, and our hooks won't work reliably on x86_64
# macOS when using it
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

option(FUNCHOOK_BUILD_SHARED "build shared library" OFF)
option(FUNCHOOK_BUILD_TESTS "Build tests" OFF)
option(FUNCHOOK_INSTALL "Install Funchook" OFF)

FetchContent_Declare(funchook
	GIT_REPOSITORY https://github.com/kubo/funchook
	GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(funchook)

add_library(csgo_gc SHARED
    appid.cpp
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

set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../protobufs)
file(GLOB PROTOS ${PROTO_DIR}/*.proto)
set(PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_include_directories(csgo_gc PRIVATE ${PROTOC_OUT_DIR})

# bruhhhhhh
file(MAKE_DIRECTORY ${PROTOC_OUT_DIR})

protobuf_generate(
    TARGET csgo_gc
    PROTOS ${PROTOS}
    IMPORT_DIRS ${PROTO_DIR}
    PROTOC_OUT_DIR ${PROTOC_OUT_DIR})

source_group("Protobufs" REGULAR_EXPRESSION "\\.pb\\.(cc|h)$")

target_link_libraries(csgo_gc PRIVATE
    MbedTLS::mbedcrypto
    protobuf::libprotobuf-lite
    funchook-static
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

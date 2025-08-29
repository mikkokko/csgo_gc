cmake_minimum_required(VERSION 3.20)

project(csgo_gc)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(IS_32BIT ON)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if (IS_32BIT)
        set(GC_LIB_DIR ".")
        set(GC_EXE_SUFFIX "_osx")
    else()
        set(GC_LIB_DIR "osx64")
        set(GC_EXE_SUFFIX "_osx64")
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (IS_32BIT)
        set(GC_LIB_DIR ".")
        set(GC_EXE_SUFFIX "_linux")
    else()
        set(GC_LIB_DIR "linux64")
        set(GC_LIB_SUFFIX "_client")
        set(GC_EXE_SUFFIX "_linux64")
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (IS_32BIT)
        set(GC_LIB_DIR ".")
    else()
        set(GC_LIB_DIR "x64")
        set(GC_EXE_SUFFIX "_win64")
    endif()
else()
    message(FATAL_ERROR "Unsupported system $[CMAKE_SYSTEM_NAME}")
endif()

if (MSVC)
    # statically linked crt
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-fpic -fvisibility=hidden)

    if (NOT APPLE)
        add_link_options(-Wl,--no-undefined -z noexecstack)
    endif()
endif()

add_subdirectory(csgo_gc)
add_subdirectory(launcher)
add_subdirectory(steamworks)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

#-------------------------------------------------
# protobuf
#-------------------------------------------------

option(protobuf_INSTALL "Install protobuf binaries and files" OFF)
option(protobuf_BUILD_TESTS "Build tests" OFF)
option(protobuf_BUILD_PROTOC_BINARIES "Build libprotoc and protoc compiler" OFF)
option(protobuf_BUILD_SHARED_LIBS "Build Shared Libraries" OFF)
option(protobuf_WITH_ZLIB "Build with zlib support" OFF)

FetchContent_Declare(protobuf
	GIT_REPOSITORY https://github.com/protocolbuffers/protobuf
	GIT_TAG 2798a968c330de223b711e4fe504800280f333fb
	GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(protobuf)

#-------------------------------------------------
# cryptopp-cmake
#-------------------------------------------------

option(CRYPTOPP_BUILD_TESTING "Build library tests" OFF)
option(CRYPTOPP_INSTALL "Generate the install target for this project." OFF)

FetchContent_Declare(cryptopp-cmake
	GIT_REPOSITORY https://github.com/abdes/cryptopp-cmake
	GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(cryptopp-cmake)

if (MSVC)
    # shut the fuck up
    target_compile_definitions(cryptopp PRIVATE _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
endif()

#-------------------------------------------------
# funchook
#-------------------------------------------------

option(FUNCHOOK_BUILD_SHARED "build shared library" OFF)
option(FUNCHOOK_BUILD_TESTS "Build tests" OFF)
option(FUNCHOOK_INSTALL "Install Funchook" OFF)

FetchContent_Declare(funchook
	GIT_REPOSITORY https://github.com/kubo/funchook
	GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(funchook)

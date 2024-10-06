project(launcher)

# messy
function(launcher_target NAME DEDICATED)
    if (WIN32)
        add_executable(${NAME} WIN32 launcher_win.cpp launcher.rc)
    else()
        add_executable(${NAME} launcher_unix.cpp)
    endif()
    
    set_target_properties(${NAME} PROPERTIES OUTPUT_NAME ${NAME}${GC_EXE_SUFFIX})
    
    if (MSVC)
        # we need large address awareness and a larger stack to not crash
        target_link_options(${NAME} PRIVATE /LARGEADDRESSAWARE /STACK:1572864)
    endif()
    
    # apple message box
    if (APPLE)
        target_link_libraries(${NAME} PRIVATE "-framework Foundation")
    endif()

    if (DEDICATED)
        target_compile_definitions(${NAME} PRIVATE DEDICATED)
    endif()
    
    target_compile_definitions(${NAME} PRIVATE
        GC_LIB_DIR="${GC_LIB_DIR}"
        GC_LIB_SUFFIX="${GC_LIB_SUFFIX}"
        GC_LIB_EXTENSION="${CMAKE_SHARED_LIBRARY_SUFFIX}")
    
    if (OUTDIR)
        add_custom_command(TARGET ${NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${NAME}>
            ${OUTDIR}/$<TARGET_FILE_NAME:${NAME}>)
    endif()
endfunction()

launcher_target(csgo OFF)

if (WIN32 OR (IS_32BIT AND NOT APPLE))
    launcher_target(srcds ON)
endif()

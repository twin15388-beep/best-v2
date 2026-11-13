cmake_minimum_required(VERSION 3.20)
project(AvoidFreezeProxy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Standalone physics logic test (runs on any platform)
add_executable(test_avoid_physics
    tests/test_proxy_physics.cpp
    avoid_physics.cpp
    avoid_physics.h
)

enable_testing()
add_test(NAME PhysicsTest COMMAND test_avoid_physics)

# version.dll target for Windows
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(version SHARED
        dllmain.cpp
        proxy_version.cpp
        proxy_version.h
        proxy_version.def
        avoid_hook.cpp
        avoid_hook.h
        avoid_physics.cpp
        avoid_physics.h
    )

    set_target_properties(version PROPERTIES
        OUTPUT_NAME "version"
        PREFIX ""
        SUFFIX ".dll"
    )

    if(MSVC)
        target_link_options(version PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/proxy_version.def")
    endif()
endif()

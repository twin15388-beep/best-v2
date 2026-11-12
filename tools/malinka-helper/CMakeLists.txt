cmake_minimum_required(VERSION 3.24)
project(MalinkaHelper VERSION 0.1.1 LANGUAGES CXX)

# Independent build. Do NOT add this as a game-client component or installer.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(malinka-helper-core STATIC core.cpp core.h)
target_include_directories(malinka-helper-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
  target_compile_options(malinka-helper-core PRIVATE /W4 /WX /permissive- /utf-8)
else()
  target_compile_options(malinka-helper-core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()

option(MALINKA_BUILD_AVOID_PREVIEW "Build the independent, explicitly armed static-map Avoid preview" OFF)
if(MALINKA_BUILD_AVOID_PREVIEW)
  include("${CMAKE_CURRENT_SOURCE_DIR}/avoid/PinnedPhysics.cmake")
endif()

if(WIN32)
  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "MalinkaHelper requires an AMD64/x64 build")
  endif()
  enable_language(RC)
  add_executable(MalinkaHelper WIN32 main_win32.cpp win32.cpp win32.h helper.rc helper.manifest)
  target_compile_definitions(MalinkaHelper PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
  target_link_libraries(MalinkaHelper PRIVATE malinka-helper-core bcrypt user32 gdi32 comdlg32 shell32)
  if(MALINKA_BUILD_AVOID_PREVIEW)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      message(FATAL_ERROR "MalinkaAvoidPreview requires the measured MSVC x64 class/member-pointer ABI")
    endif()
    find_package(Git REQUIRED)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${MALINKA_PINNED_SOURCE}/ddnet-libs" rev-parse HEAD
      OUTPUT_VARIABLE MALINKA_DDNET_LIBS_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE MALINKA_DDNET_LIBS_GIT_RESULT)
    if(NOT MALINKA_DDNET_LIBS_GIT_RESULT EQUAL 0 OR NOT MALINKA_DDNET_LIBS_COMMIT STREQUAL "c0e6703fbcdbe03df2f26875427ec3951ce4ec21")
      message(FATAL_ERROR "MalinkaAvoidPreview requires ddnet-libs submodule c0e6703fbcdbe03df2f26875427ec3951ce4ec21")
    endif()
    set(MALINKA_UPSTREAM_INCLUDE_DIRS
      "${MALINKA_PINNED_SOURCE}/src/rust-bridge"
      "${MALINKA_PINNED_SOURCE}/ddnet-libs/sdl/include/windows"
      "${MALINKA_PINNED_SOURCE}/ddnet-libs/curl/include"
      "${MALINKA_PINNED_SOURCE}/ddnet-libs/sqlite3/include"
      "${MALINKA_PINNED_SOURCE}/ddnet-libs/zlib/include"
      "${MALINKA_PINNED_SOURCE}/ddnet-libs/discord/include")
    foreach(MALINKA_INCLUDE_DIR IN LISTS MALINKA_UPSTREAM_INCLUDE_DIRS)
      if(NOT IS_DIRECTORY "${MALINKA_INCLUDE_DIR}")
        message(FATAL_ERROR "MalinkaAvoidPreview is missing pinned header directory: ${MALINKA_INCLUDE_DIR}")
      endif()
    endforeach()
    if(NOT EXISTS "${MALINKA_PINNED_SOURCE}/ddnet-libs/sdl/include/windows/SDL_audio.h")
      message(FATAL_ERROR "MalinkaAvoidPreview needs the pinned SDL_audio.h header")
    endif()
    add_executable(MalinkaAvoidPreview avoid/preview_win32.cpp avoid/live_win32.cpp win32.cpp)
    target_compile_definitions(MalinkaAvoidPreview PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)
    target_include_directories(MalinkaAvoidPreview PRIVATE ${MALINKA_UPSTREAM_INCLUDE_DIRS})
    target_link_libraries(MalinkaAvoidPreview PRIVATE malinka-helper-core malinka-avoid-policy bcrypt user32)
    target_compile_options(MalinkaAvoidPreview PRIVATE /W4 /WX /permissive- /utf-8)
    target_link_options(MalinkaAvoidPreview PRIVATE /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /OPT:REF)
  endif()
  if(MSVC)
    target_compile_options(MalinkaHelper PRIVATE /W4 /WX /permissive- /utf-8)
    target_link_options(MalinkaHelper PRIVATE /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
  endif()
else()
  message(STATUS "Only the portable inspection core is built here. The Windows GUI is NOT built or tested by this target.")
endif()

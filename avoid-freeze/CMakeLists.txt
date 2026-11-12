cmake_minimum_required(VERSION 3.24)
project(AvoidFreeze VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(AF_PINNED_SOURCE "" CACHE PATH "Separate public BestClient cf192e3 source checkout (never the installed game)")
if(NOT IS_DIRECTORY "${AF_PINNED_SOURCE}/src")
  message(FATAL_ERROR "Set AF_PINNED_SOURCE to a separate extracted public source tree; see avoid-freeze/README.md")
endif()

find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_pinned_source.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/pinned-source-sha256.json" "${AF_PINNED_SOURCE}"
  RESULT_VARIABLE AF_VERIFY_RESULT OUTPUT_VARIABLE AF_VERIFY_OUTPUT ERROR_VARIABLE AF_VERIFY_ERROR)
if(NOT AF_VERIFY_RESULT EQUAL 0)
  message(FATAL_ERROR "${AF_VERIFY_OUTPUT}\n${AF_VERIFY_ERROR}")
endif()
message(STATUS "${AF_VERIFY_OUTPUT}")

set(AF_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/pinned-generated")
file(MAKE_DIRECTORY "${AF_GENERATED}/generated")
set(AF_GENERATOR_ARGS network_header content_types_header client_content_header client_content_source)
set(AF_GENERATOR_NAMES protocol.h data_types.h client_data.h client_data.cpp)
foreach(AF_INDEX RANGE 0 3)
  list(GET AF_GENERATOR_ARGS ${AF_INDEX} AF_ARGUMENT)
  list(GET AF_GENERATOR_NAMES ${AF_INDEX} AF_NAME)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONDONTWRITEBYTECODE=1 "${Python3_EXECUTABLE}" -B "datasrc/compile.py" "${AF_ARGUMENT}"
    WORKING_DIRECTORY "${AF_PINNED_SOURCE}"
    OUTPUT_FILE "${AF_GENERATED}/generated/${AF_NAME}"
    RESULT_VARIABLE AF_GENERATE_RESULT ERROR_VARIABLE AF_GENERATE_ERROR)
  if(NOT AF_GENERATE_RESULT EQUAL 0)
    message(FATAL_ERROR "Pinned generator ${AF_ARGUMENT} failed (${AF_GENERATE_RESULT}): ${AF_GENERATE_ERROR}")
  endif()
endforeach()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONDONTWRITEBYTECODE=1 "${Python3_EXECUTABLE}" -B -m datasrc.seven.compile network_header
  WORKING_DIRECTORY "${AF_PINNED_SOURCE}"
  OUTPUT_FILE "${AF_GENERATED}/generated/protocol7.h"
  RESULT_VARIABLE AF_GENERATE_RESULT ERROR_VARIABLE AF_GENERATE_ERROR)
if(NOT AF_GENERATE_RESULT EQUAL 0)
  message(FATAL_ERROR "Pinned seven protocol generator failed (${AF_GENERATE_RESULT}): ${AF_GENERATE_ERROR}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONDONTWRITEBYTECODE=1 "${Python3_EXECUTABLE}" -B -m datasrc.seven.compile client_content_header
  WORKING_DIRECTORY "${AF_PINNED_SOURCE}"
  OUTPUT_FILE "${AF_GENERATED}/generated/client_data7.h"
  RESULT_VARIABLE AF_GENERATE_RESULT ERROR_VARIABLE AF_GENERATE_ERROR)
if(NOT AF_GENERATE_RESULT EQUAL 0)
  message(FATAL_ERROR "Pinned seven client-content generator failed (${AF_GENERATE_RESULT}): ${AF_GENERATE_ERROR}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONDONTWRITEBYTECODE=1 "${Python3_EXECUTABLE}" -B "datasrc/crosscompile.py" map_header
  WORKING_DIRECTORY "${AF_PINNED_SOURCE}"
  OUTPUT_FILE "${AF_GENERATED}/generated/protocolglue.h"
  RESULT_VARIABLE AF_GENERATE_RESULT ERROR_VARIABLE AF_GENERATE_ERROR)
if(NOT AF_GENERATE_RESULT EQUAL 0)
  message(FATAL_ERROR "Pinned map-header generator failed (${AF_GENERATE_RESULT}): ${AF_GENERATE_ERROR}")
endif()

set(AF_SOURCE_FILES
  base/mem.cpp base/dbg.cpp base/str.cpp base/unicode/tolower.cpp base/unicode/tolower_data.cpp
  base/hash.cpp base/hash_libtomcrypt.cpp base/hash_bundled.cpp engine/external/md5/md5.c
  game/gamecore.cpp game/collision.cpp game/layers.cpp game/teamscore.cpp game/prng.cpp game/mapbugs.cpp
  game/client/laser_data.cpp game/client/projectile_data.cpp game/client/pickup_data.cpp
  game/client/prediction/gameworld.cpp game/client/prediction/entity.cpp
  game/client/prediction/entities/character.cpp game/client/prediction/entities/door.cpp
  game/client/prediction/entities/dragger.cpp game/client/prediction/entities/laser.cpp
  game/client/prediction/entities/pickup.cpp game/client/prediction/entities/plasma.cpp
  game/client/prediction/entities/projectile.cpp)
list(TRANSFORM AF_SOURCE_FILES PREPEND "${AF_PINNED_SOURCE}/src/")

enable_language(C)
add_library(avoid-freeze-pinned-physics STATIC
  ${AF_SOURCE_FILES}
  "${AF_GENERATED}/generated/client_data.cpp"
  src/physics/pinned_support.cpp)
target_include_directories(avoid-freeze-pinned-physics SYSTEM PUBLIC
  "${AF_PINNED_SOURCE}/src"
  "${AF_GENERATED}")
target_compile_definitions(avoid-freeze-pinned-physics PUBLIC
  CONF_VIDEORECORDER CONF_AUTOUPDATE CONF_DISCORD CONF_DISCORD_DYNAMIC
  _ITERATOR_DEBUG_LEVEL=0 NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
  target_compile_options(avoid-freeze-pinned-physics PRIVATE /W3 /Gy /utf-8)
else()
  target_compile_options(avoid-freeze-pinned-physics PRIVATE -ffunction-sections -fdata-sections)
endif()

add_library(avoid-freeze-policy STATIC
  src/physics/static_map.cpp
  src/policy/avoid_policy.cpp)
target_include_directories(avoid-freeze-policy PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(avoid-freeze-policy PUBLIC avoid-freeze-pinned-physics)
if(MSVC)
  target_compile_options(avoid-freeze-policy PRIVATE /W4 /WX /permissive- /utf-8)
else()
  target_compile_options(avoid-freeze-policy PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()

if(WIN32)
  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "The live process adapter is supported only on the measured Windows x64 MSVC ABI")
  endif()
  find_package(Git REQUIRED)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${AF_PINNED_SOURCE}/ddnet-libs" rev-parse HEAD
    OUTPUT_VARIABLE AF_DDNET_LIBS_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE AF_DDNET_LIBS_RESULT)
  if(NOT AF_DDNET_LIBS_RESULT EQUAL 0 OR NOT AF_DDNET_LIBS_COMMIT STREQUAL "c0e6703fbcdbe03df2f26875427ec3951ce4ec21")
    message(FATAL_ERROR "The Windows target requires pinned ddnet-libs c0e6703fbcdbe03df2f26875427ec3951ce4ec21")
  endif()
  set(AF_DEPENDENCY_INCLUDE_DIRS
    "${AF_PINNED_SOURCE}/src/rust-bridge"
    "${AF_PINNED_SOURCE}/ddnet-libs/sdl/include/windows"
    "${AF_PINNED_SOURCE}/ddnet-libs/curl/include"
    "${AF_PINNED_SOURCE}/ddnet-libs/sqlite3/include"
    "${AF_PINNED_SOURCE}/ddnet-libs/zlib/include"
    "${AF_PINNED_SOURCE}/ddnet-libs/discord/include")
  foreach(AF_INCLUDE_DIR IN LISTS AF_DEPENDENCY_INCLUDE_DIRS)
    if(NOT IS_DIRECTORY "${AF_INCLUDE_DIR}")
      message(FATAL_ERROR "Missing pinned dependency include directory: ${AF_INCLUDE_DIR}")
    endif()
  endforeach()
  if(NOT EXISTS "${AF_PINNED_SOURCE}/ddnet-libs/sdl/include/windows/SDL_audio.h")
    message(FATAL_ERROR "Missing pinned SDL_audio.h")
  endif()
  target_include_directories(avoid-freeze-pinned-physics SYSTEM PUBLIC ${AF_DEPENDENCY_INCLUDE_DIRS})
  add_executable(AvoidFreeze
    src/main.cpp
    src/platform/layout_check.cpp
    src/platform/win32_reader.cpp
    src/platform/win32_reader.h)
  target_include_directories(AvoidFreeze PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_definitions(AvoidFreeze PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)
  target_link_libraries(AvoidFreeze PRIVATE avoid-freeze-policy bcrypt user32)
  target_compile_options(AvoidFreeze PRIVATE /W4 /WX /permissive- /utf-8)
  target_link_options(AvoidFreeze PRIVATE /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /OPT:REF)
else()
  message(STATUS "Linux builds the isolated pinned-physics policy tests only; the Windows process adapter is not compiled here.")
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(avoid-freeze-tests tests/policy_tests.cpp)
  target_link_libraries(avoid-freeze-tests PRIVATE avoid-freeze-policy)
  if(MSVC)
    target_compile_options(avoid-freeze-tests PRIVATE /W4 /WX /permissive- /utf-8)
  else()
    target_compile_options(avoid-freeze-tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
  endif()
  add_test(NAME avoid-freeze-policy COMMAND avoid-freeze-tests)
endif()

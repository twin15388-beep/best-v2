# Optional, independent prediction dependency. No installed game directory is
# ever a build output, download target, or dependency to be patched.
set(MALINKA_PINNED_SOURCE "" CACHE PATH "Extracted public BestClient cf192e3 source (not the installed client)")
if(NOT IS_DIRECTORY "${MALINKA_PINNED_SOURCE}/src")
  message(FATAL_ERROR "Avoid preview needs the exact public source: set MALINKA_PINNED_SOURCE; see avoid/README.md")
endif()
enable_language(C)
find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/verify_upstream.py" "${CMAKE_CURRENT_LIST_DIR}/upstream-sha256.json" "${MALINKA_PINNED_SOURCE}"
  RESULT_VARIABLE VERIFY_EXIT OUTPUT_VARIABLE VERIFY_OUT ERROR_VARIABLE VERIFY_ERROR)
if(NOT VERIFY_EXIT EQUAL 0)
  message(FATAL_ERROR "${VERIFY_OUT}\n${VERIFY_ERROR}")
endif()
message(STATUS "${VERIFY_OUT}")
set(PINNED_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/pinned-generated")
file(MAKE_DIRECTORY "${PINNED_GENERATED}/generated")
set(GENERATOR_ARGS network_header content_types_header client_content_header client_content_source)
set(GENERATOR_NAMES protocol.h data_types.h client_data.h client_data.cpp)
foreach(I RANGE 0 3)
  list(GET GENERATOR_ARGS ${I} ARG)
  list(GET GENERATOR_NAMES ${I} NAME)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONDONTWRITEBYTECODE=1 "${Python3_EXECUTABLE}" -B datasrc/compile.py "${ARG}"
    WORKING_DIRECTORY "${MALINKA_PINNED_SOURCE}"
    OUTPUT_FILE "${PINNED_GENERATED}/generated/${NAME}"
    RESULT_VARIABLE GENERATE_EXIT ERROR_VARIABLE GENERATE_ERROR)
  if(NOT GENERATE_EXIT EQUAL 0)
    message(FATAL_ERROR "Pinned ${ARG} generation exit ${GENERATE_EXIT}: ${GENERATE_ERROR}")
  endif()
endforeach()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONDONTWRITEBYTECODE=1 "${Python3_EXECUTABLE}" -B -m datasrc.seven.compile network_header
  WORKING_DIRECTORY "${MALINKA_PINNED_SOURCE}"
  OUTPUT_FILE "${PINNED_GENERATED}/generated/protocol7.h"
  RESULT_VARIABLE GENERATE_EXIT ERROR_VARIABLE GENERATE_ERROR)
if(NOT GENERATE_EXIT EQUAL 0)
  message(FATAL_ERROR "Pinned protocol7.h generation exit ${GENERATE_EXIT}: ${GENERATE_ERROR}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONDONTWRITEBYTECODE=1 "${Python3_EXECUTABLE}" -B -m datasrc.seven.compile client_content_header
  WORKING_DIRECTORY "${MALINKA_PINNED_SOURCE}"
  OUTPUT_FILE "${PINNED_GENERATED}/generated/client_data7.h"
  RESULT_VARIABLE GENERATE_EXIT ERROR_VARIABLE GENERATE_ERROR)
if(NOT GENERATE_EXIT EQUAL 0)
  message(FATAL_ERROR "Pinned client_data7.h generation exit ${GENERATE_EXIT}: ${GENERATE_ERROR}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONDONTWRITEBYTECODE=1 "${Python3_EXECUTABLE}" -B datasrc/crosscompile.py map_header
  WORKING_DIRECTORY "${MALINKA_PINNED_SOURCE}"
  OUTPUT_FILE "${PINNED_GENERATED}/generated/protocolglue.h"
  RESULT_VARIABLE GENERATE_EXIT ERROR_VARIABLE GENERATE_ERROR)
if(NOT GENERATE_EXIT EQUAL 0)
  message(FATAL_ERROR "Pinned protocolglue.h generation exit ${GENERATE_EXIT}: ${GENERATE_ERROR}")
endif()
set(PINNED_PHYSICS_SOURCES
  base/mem.cpp base/dbg.cpp base/str.cpp base/unicode/tolower.cpp base/unicode/tolower_data.cpp
  base/hash.cpp base/hash_libtomcrypt.cpp base/hash_bundled.cpp engine/external/md5/md5.c
  game/gamecore.cpp game/collision.cpp game/layers.cpp game/teamscore.cpp game/prng.cpp game/mapbugs.cpp
  game/client/laser_data.cpp game/client/projectile_data.cpp game/client/pickup_data.cpp
  game/client/prediction/gameworld.cpp game/client/prediction/entity.cpp
  game/client/prediction/entities/character.cpp game/client/prediction/entities/door.cpp
  game/client/prediction/entities/dragger.cpp game/client/prediction/entities/laser.cpp
  game/client/prediction/entities/pickup.cpp game/client/prediction/entities/plasma.cpp
  game/client/prediction/entities/projectile.cpp)
list(TRANSFORM PINNED_PHYSICS_SOURCES PREPEND "${MALINKA_PINNED_SOURCE}/src/")
add_library(malinka-pinned-physics STATIC ${PINNED_PHYSICS_SOURCES} "${PINNED_GENERATED}/generated/client_data.cpp" "${CMAKE_CURRENT_LIST_DIR}/physics_support.cpp")
target_include_directories(malinka-pinned-physics SYSTEM PUBLIC "${MALINKA_PINNED_SOURCE}/src" "${PINNED_GENERATED}")
if(MSVC)
  target_compile_options(malinka-pinned-physics PRIVATE /W3 /Gy /utf-8)
  target_compile_definitions(malinka-pinned-physics PUBLIC CONF_VIDEORECORDER CONF_AUTOUPDATE CONF_DISCORD CONF_DISCORD_DYNAMIC _ITERATOR_DEBUG_LEVEL=0 NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  target_compile_options(malinka-pinned-physics PRIVATE -ffunction-sections -fdata-sections)
endif()
add_library(malinka-avoid-policy STATIC "${CMAKE_CURRENT_LIST_DIR}/policy.cpp" "${CMAKE_CURRENT_LIST_DIR}/policy.h")
target_link_libraries(malinka-avoid-policy PUBLIC malinka-pinned-physics)
target_include_directories(malinka-avoid-policy PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
if(MSVC)
  target_compile_options(malinka-avoid-policy PRIVATE /W4 /WX /permissive- /utf-8)
else()
  target_compile_options(malinka-avoid-policy PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()

# Ultima V Native -- Windows x64 desktop target (SDL3).
#
# One gameplay implementation: this target compiles the shared core
# (native/core, the same source list the T-Deck firmware builds) and the
# T-Deck application layer (native/targets/tdeck/main: AlphaRuntime, the
# Board presentation, saves, resources) from the SAME sources the firmware
# builds, over a Windows implementation of the small ESP-IDF surface they use
# (esp_compat/ and src/platform/). The few desktop needs in those shared
# sources (platform profile, desktop input entry, typed spell names) are
# behaviour-preserving for the T-Deck; see WINDOWS_PORT_PLAN.md, section 5.
#
#   cmake -S native/targets/windows -B build/windows -G "Visual Studio 18 2026" -A x64
#   cmake --build build/windows --config Release
#   ctest --test-dir build/windows -C Release
#
# SDL3 is fetched from its official release (pinned version + SHA-256) unless
# UV_USE_SYSTEM_SDL3=ON (find_package) or FETCHCONTENT_SOURCE_DIR_SDL3 points
# at an unpacked source tree (offline builds).
cmake_minimum_required(VERSION 3.24)
project(UltimaVNativeWindows VERSION 0.5.0 LANGUAGES C CXX)

if(NOT WIN32)
  message(FATAL_ERROR "native/targets/windows builds the Windows desktop target only.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "The Windows target is x64 only (configure with -A x64 or an x64 toolchain).")
endif()

option(UV_BUILD_TESTS "Build the Windows target's tests" ON)
option(UV_USE_SYSTEM_SDL3 "Use an installed SDL3 (find_package) instead of fetching the pinned release" OFF)
option(UV_ENABLE_DEVELOPER_TOOLS "Compile the Developer menu (Alt+D), as the T-Deck builds do" ON)
set(UV_VERSION_SUFFIX "dev" CACHE STRING "Pre-release suffix of the version string (empty for a release)")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# The static C/C++ runtime: the packaged executable needs no Visual C++
# redistributable, only SDL3.dll beside it.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

get_filename_component(UV_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(UV_CORE_DIR "${UV_REPO_ROOT}/native/core")
set(UV_TDECK_MAIN "${UV_REPO_ROOT}/native/targets/tdeck/main")
set(UV_WIN_DIR "${CMAKE_CURRENT_LIST_DIR}")

# ---------------------------------------------------------------------------
# Version and provenance
# ---------------------------------------------------------------------------
set(UV_VERSION "${PROJECT_VERSION}")
if(UV_VERSION_SUFFIX)
  string(APPEND UV_VERSION "-${UV_VERSION_SUFFIX}")
endif()
find_package(Git QUIET)
set(UV_GIT_COMMIT "unknown")
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${UV_REPO_ROOT}" OUTPUT_VARIABLE _uv_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _uv_rc)
  if(_uv_rc EQUAL 0 AND _uv_commit)
    set(UV_GIT_COMMIT "${_uv_commit}")
    # As the firmware does: a build of uncommitted tracked changes says so.
    execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
      WORKING_DIRECTORY "${UV_REPO_ROOT}" OUTPUT_VARIABLE _uv_changes
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_uv_changes)
      string(APPEND UV_GIT_COMMIT "-dirty")
    endif()
  endif()
endif()
message(STATUS "Ultima V Native for Windows ${UV_VERSION} (git ${UV_GIT_COMMIT})")

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------
if(UV_USE_SYSTEM_SDL3)
  find_package(SDL3 CONFIG REQUIRED)
else()
  include(FetchContent)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
    URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

# ---------------------------------------------------------------------------
# Common compile settings
# ---------------------------------------------------------------------------
set(UV_COMMON_DEFINITIONS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
if(UV_ENABLE_DEVELOPER_TOOLS)
  # Every translation unit must agree: alpha_runtime.h's layout depends on it.
  list(APPEND UV_COMMON_DEFINITIONS OPENU5_ENABLE_DEVELOPER_TOOLS=1)
endif()
if(MSVC)
  # /Zc:preprocessor: the ESP_LOG macros use __VA_OPT__. /utf-8: the sources are UTF-8.
  set(UV_COMMON_OPTIONS /utf-8 /Zc:preprocessor /Zc:__cplusplus /permissive- /EHsc /bigobj)
  set(UV_SHARED_WARNINGS /W3 /wd4244 /wd4267 /wd4305 /wd4996)
  set(UV_OWN_WARNINGS /W4 /WX)
else()
  set(UV_COMMON_OPTIONS)
  set(UV_SHARED_WARNINGS -Wall)
  set(UV_OWN_WARNINGS -Wall -Wextra -Werror)
endif()

# loot_names.inc, from the tracked JSON (see cmake/LootNames.cmake).
include("${UV_WIN_DIR}/cmake/LootNames.cmake")
set(UV_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${UV_GENERATED_DIR}")
uv_generate_loot_names("${UV_REPO_ROOT}/game/src/core/data/longEquipNames.json" "${UV_GENERATED_DIR}")
# dos_equipment_tables.inc, Original mode's (R)eady names and glyphs (cmake/DosTables.cmake).
include("${UV_WIN_DIR}/cmake/DosTables.cmake")
uv_generate_dos_tables("${UV_REPO_ROOT}/game/src/core/data/shortEquipNames.json"
                       "${UV_REPO_ROOT}/game/src/core/readyPicker.ts" "${UV_GENERATED_DIR}")

# ---------------------------------------------------------------------------
# 1. The shared core: the T-Deck firmware's own source list
# ---------------------------------------------------------------------------
set(OPENU5_ENABLE_DEVELOPER_TOOLS ${UV_ENABLE_DEVELOPER_TOOLS})
include("${UV_CORE_DIR}/sources.cmake")
add_library(uv_core STATIC ${OPENU5_CORE_SOURCES})
target_include_directories(uv_core PUBLIC "${UV_CORE_DIR}/include" PRIVATE "${UV_GENERATED_DIR}")
target_compile_definitions(uv_core PUBLIC ${UV_COMMON_DEFINITIONS})
target_compile_options(uv_core PRIVATE ${UV_COMMON_OPTIONS} ${UV_SHARED_WARNINGS})

# ---------------------------------------------------------------------------
# 2. The Windows platform layer: the ESP-IDF surface on Win32
# ---------------------------------------------------------------------------
set(UV_COMPAT_INCLUDES "${UV_WIN_DIR}/esp_compat")
if(MSVC)
  list(APPEND UV_COMPAT_INCLUDES "${UV_WIN_DIR}/esp_compat/msvc")
endif()
add_library(uv_platform STATIC
  src/platform/esp_runtime.cpp
  src/platform/log.cpp
  src/platform/storage.cpp
  src/platform/virtual_panel.cpp
  src/platform/sdlog_windows.cpp
  src/platform/device_glue.cpp)
target_include_directories(uv_platform PUBLIC ${UV_COMPAT_INCLUDES} "${UV_WIN_DIR}/src"
  PRIVATE "${UV_TDECK_MAIN}")
target_link_libraries(uv_platform PUBLIC uv_core)
target_compile_options(uv_platform PRIVATE ${UV_COMMON_OPTIONS} ${UV_OWN_WARNINGS})

# ---------------------------------------------------------------------------
# 3. The shared application layer: native/targets/tdeck/main (the firmware's sources)
# ---------------------------------------------------------------------------
# Every file here is portable application code the T-Deck firmware builds as
# well; the device-only files (main.cpp, tdeck_input.cpp, tdeck_audio.cpp,
# sd_diagnostic_logger.cpp, system_perf.cpp) are replaced by src/.
set(UV_SHARED_APP_SOURCES
  "${UV_TDECK_MAIN}/alpha_runtime.cpp"
  "${UV_TDECK_MAIN}/alpha_resources.cpp"
  "${UV_TDECK_MAIN}/asset_pack.cpp"
  "${UV_TDECK_MAIN}/alpha_dialogue.cpp"
  "${UV_TDECK_MAIN}/dungeon_art_cache.cpp"
  "${UV_TDECK_MAIN}/native_renderer.cpp"
  "${UV_TDECK_MAIN}/ui_input_adapter.cpp"
  "${UV_TDECK_MAIN}/input_controller.cpp"
  "${UV_TDECK_MAIN}/keyboard_matrix.cpp"
  "${UV_TDECK_MAIN}/native_movement.cpp"
  "${UV_TDECK_MAIN}/alpha_save.cpp"
  "${UV_TDECK_MAIN}/alpha_save_generation.cpp"
  "${UV_TDECK_MAIN}/alpha_audio.cpp"
  "${UV_TDECK_MAIN}/tdeck_board.cpp"
  "${UV_TDECK_MAIN}/device_smoke_tests.cpp"
  "${UV_TDECK_MAIN}/idle_service.cpp"
  # Desktop only (the firmware's CMakeLists does not list it): Original mode's
  # DOS 320 x 200 game screen, a part of the Board OPENU5_DESKTOP_DOS_LAYOUT
  # switches on below.
  "${UV_TDECK_MAIN}/board_dos_layout.cpp")
add_library(uv_shared_app STATIC ${UV_SHARED_APP_SOURCES})
target_include_directories(uv_shared_app PUBLIC ${UV_COMPAT_INCLUDES} "${UV_TDECK_MAIN}")
target_link_libraries(uv_shared_app PUBLIC uv_platform uv_core)
target_compile_definitions(uv_shared_app PRIVATE OPENU5_GIT_COMMIT="${UV_GIT_COMMIT}")
# PUBLIC: the Board's layout depends on it, so every file that includes
# tdeck_board.h must agree.
target_compile_definitions(uv_shared_app PUBLIC OPENU5_DESKTOP_DOS_LAYOUT=1)
# The device sources reach the "/sd" card through their own stdio/POSIX
# calls; the force-included redirect maps them to the data directory.
if(MSVC)
  target_compile_options(uv_shared_app PRIVATE ${UV_COMMON_OPTIONS} ${UV_SHARED_WARNINGS}
    "/FI${UV_WIN_DIR}/esp_compat/uv_storage_redirect.h")
else()
  target_compile_options(uv_shared_app PRIVATE ${UV_COMMON_OPTIONS} ${UV_SHARED_WARNINGS}
    -include "${UV_WIN_DIR}/esp_compat/uv_storage_redirect.h")
endif()

# ---------------------------------------------------------------------------
# 4. The desktop front end (SDL3) and the executable
# ---------------------------------------------------------------------------
configure_file(src/app/build_info.h.in "${UV_GENERATED_DIR}/build_info.h" @ONLY)
configure_file(src/app/build_info.h.rc.in "${UV_GENERATED_DIR}/build_info.h.rc" @ONLY)
add_library(uv_desktop STATIC
  src/app/application.cpp
  src/app/autotest.cpp
  src/app/data_paths.cpp
  src/app/desktop_input.cpp
  src/app/desktop_profile.cpp
  src/app/desktop_settings.cpp
  src/app/dos_text.cpp
  src/app/presentation_layout.cpp
  src/app/presenter.cpp
  src/app/sdl_audio_backend.cpp)
target_include_directories(uv_desktop PUBLIC "${UV_WIN_DIR}/src" "${UV_GENERATED_DIR}")
target_link_libraries(uv_desktop PUBLIC uv_shared_app SDL3::SDL3 shell32 ole32)
target_compile_options(uv_desktop PRIVATE ${UV_COMMON_OPTIONS} ${UV_OWN_WARNINGS})

add_executable(UltimaVNative WIN32 src/main.cpp src/app/resources.rc)
target_link_libraries(UltimaVNative PRIVATE uv_desktop)
target_compile_options(UltimaVNative PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${UV_COMMON_OPTIONS} ${UV_OWN_WARNINGS}>)
target_include_directories(UltimaVNative PRIVATE "${UV_GENERATED_DIR}" "${UV_WIN_DIR}/src/app")
if(MSVC)
  # resources.rc embeds src/app/UltimaVNative.manifest (DPI awareness, UTF-8 code page).
  target_link_options(UltimaVNative PRIVATE /MANIFEST:NO)
endif()
set_target_properties(UltimaVNative PROPERTIES OUTPUT_NAME "UltimaVNative")
# SDL3.dll beside the executable, so it runs from the build tree.
add_custom_command(TARGET UltimaVNative POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:UltimaVNative> $<TARGET_FILE_DIR:UltimaVNative>
  COMMAND_EXPAND_LISTS)

# ---------------------------------------------------------------------------
# 5. Tests
# ---------------------------------------------------------------------------
if(UV_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
  # End-to-end runs of the real executable (need the player's packs; see scenarios/).
  add_subdirectory(scenarios)
endif()

# Ultima V Native for Windows -- the Windows target's own tests.
#
# Plain C++20 executables, no framework (test_check.h): each prints one
# "PASS <name>" / "FAIL <name>: <detail>" line per check (SKIP for checks that
# need the player's own game files) and exits with the number of failures.
# They link the same libraries the game does (uv_core, uv_platform,
# uv_shared_app, uv_desktop, SDL3) and compile with the production options
# and definitions (UV_COMMON_OPTIONS / UV_COMMON_DEFINITIONS from the parent).
#
#   uv_desktop_input_test     DesktopInput::translate over hand-built SDL_Events
#   uv_desktop_settings_test  windows.json codec, clamping, files (non-ASCII paths)
#   uv_storage_test           the /sd mount, the redirect functions, the /FI macros
#   uv_virtual_panel_test     the virtual ST7789 by hand, then the real tdeck::Board
#   uv_save_layout_test       the 32-byte save commit record, settings.json
#   uv_audio_backend_test     SdlAudioBackend (offline, dummy, failed) + synth goldens
#
#   ctest --test-dir <build> --output-on-failure

function(uv_add_test name)
  cmake_parse_arguments(T "" "TIMEOUT" "SOURCES;LIBS;ARGS" ${ARGN})
  add_executable(${name} ${T_SOURCES})
  target_link_libraries(${name} PRIVATE ${T_LIBS})
  target_compile_definitions(${name} PRIVATE ${UV_COMMON_DEFINITIONS})
  target_compile_options(${name} PRIVATE ${UV_COMMON_OPTIONS} ${UV_OWN_WARNINGS})
  target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME ${name} COMMAND ${name} ${T_ARGS})
  if(NOT T_TIMEOUT)
    set(T_TIMEOUT 120)
  endif()
  set_tests_properties(${name} PROPERTIES TIMEOUT ${T_TIMEOUT} LABELS windows)
endfunction()

# SDL3.dll beside the executables that link SDL, so they run from the build tree.
function(uv_copy_runtime_dlls name)
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${name}> $<TARGET_FILE_DIR:${name}>
    COMMAND_EXPAND_LISTS)
endfunction()

# 1. The desktop keyboard.
uv_add_test(uv_desktop_input_test
  SOURCES desktop_input_test.cpp
  LIBS uv_desktop SDL3::SDL3)
uv_copy_runtime_dlls(uv_desktop_input_test)

# 2. windows.json.
uv_add_test(uv_desktop_settings_test
  SOURCES desktop_settings_test.cpp
  LIBS uv_desktop SDL3::SDL3)
uv_copy_runtime_dlls(uv_desktop_settings_test)

# 3. The /sd mount. storage_redirect_probe.cpp is compiled exactly like the
# shared device sources: with the redirect header force-included.
uv_add_test(uv_storage_test
  SOURCES storage_test.cpp storage_redirect_probe.cpp
  LIBS uv_platform uv_core)
if(MSVC)
  set_source_files_properties(storage_redirect_probe.cpp PROPERTIES
    COMPILE_OPTIONS "/FI${UV_WIN_DIR}/esp_compat/uv_storage_redirect.h")
else()
  set_source_files_properties(storage_redirect_probe.cpp PROPERTIES
    COMPILE_OPTIONS "-include;${UV_WIN_DIR}/esp_compat/uv_storage_redirect.h")
endif()

# 4. The virtual panel and the real Board on it.
uv_add_test(uv_virtual_panel_test
  SOURCES virtual_panel_test.cpp
  LIBS uv_shared_app uv_platform uv_core)

# 5. The on-card save layout.
uv_add_test(uv_save_layout_test
  SOURCES save_layout_test.cpp
  LIBS uv_shared_app uv_platform uv_core)

# 6. The audio backend and the synth goldens. The corpus goldens (G1-G4) need
# the player's own audio pack (native/assets/openu5-audio.bin, git-ignored,
# made by `npm run pack:audio`); without it they print SKIP. When that file is
# absent, UV_AUDIO_PACK in the environment may name the pack instead.
uv_add_test(uv_audio_backend_test
  SOURCES audio_backend_test.cpp
  LIBS uv_desktop SDL3::SDL3
  ARGS "${UV_REPO_ROOT}/native/assets/openu5-audio.bin"
  TIMEOUT 600)
target_include_directories(uv_audio_backend_test PRIVATE "${UV_CORE_DIR}/tests")
uv_copy_runtime_dlls(uv_audio_backend_test)

# The golden printer (not a test; built on request):
#   cmake --build <dir> --target uv_print_synth_fingerprints
# The same source builds with the host suite's GCC (see the file's header), which
# is how the GCC reference values in audio_backend_test.cpp were made.
add_executable(uv_print_synth_fingerprints EXCLUDE_FROM_ALL print_synth_fingerprints.cpp)
target_link_libraries(uv_print_synth_fingerprints PRIVATE uv_core)
target_compile_options(uv_print_synth_fingerprints PRIVATE ${UV_COMMON_OPTIONS} ${UV_OWN_WARNINGS})
target_include_directories(uv_print_synth_fingerprints PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${UV_CORE_DIR}/tests")

# KNOWN-BUG: SdlAudioBackend::render_offline() drops the rest of the last
# 128-frame block when `frames` is not a multiple of 128, so successive
# offline calls are not one continuous stream (src/app/sdl_audio_backend.cpp,
# StreamSink::deliver). Test hook only -- the SDL callback path keeps the
# remainder. The check (K1) runs with --known-bugs; enable this test once fixed.
add_test(NAME uv_audio_backend_known_bugs COMMAND uv_audio_backend_test --known-bugs)
set_tests_properties(uv_audio_backend_known_bugs PROPERTIES DISABLED TRUE LABELS "windows;known-bug")

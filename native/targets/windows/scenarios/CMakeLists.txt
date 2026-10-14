# End-to-end scenarios: the real UltimaVNative.exe, headless, driven by
# --autotest scripts through SDL events (see src/app/autotest.h).
#
# They need the player's resource packs, which are never in the repository:
# UV_SCENARIO_PACKS names a folder holding openu5-assets.bin and
# openu5-alpha1-resources.bin (optionally openu5-audio.bin). It defaults to
# native/assets, where `npm run pack:*` writes them. Without packs the
# scenarios are not registered. The PC-save scenario also needs an original
# SAVED.GAM/SAVED.OOL pair (UV_SCENARIO_PC_SAVES, default original/u5/ultima5).
set(UV_SCENARIO_PACKS "${UV_REPO_ROOT}/native/assets" CACHE PATH "Folder with openu5-*.bin packs for the scenarios")
set(UV_SCENARIO_PC_SAVES "${UV_REPO_ROOT}/original/u5/ultima5" CACHE PATH "Folder with an original SAVED.GAM/SAVED.OOL")

if(NOT EXISTS "${UV_SCENARIO_PACKS}/openu5-assets.bin" OR NOT EXISTS "${UV_SCENARIO_PACKS}/openu5-alpha1-resources.bin")
  message(STATUS "Scenarios skipped: no resource packs in UV_SCENARIO_PACKS (${UV_SCENARIO_PACKS})")
  return()
endif()

set(_runner "${CMAKE_CURRENT_SOURCE_DIR}/run_scenario.cmake")
set(_work "${CMAKE_CURRENT_BINARY_DIR}/work")

function(uv_scenario name)
  cmake_parse_arguments(S "DEVELOPER" "MODE;IMPORT;VIDEO" "EXPECT_FILES;EXPECT_SIZES" ${ARGN})
  add_test(NAME scenario_${name}
    COMMAND ${CMAKE_COMMAND}
      -DEXE=$<TARGET_FILE:UltimaVNative>
      -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/${name}.txt
      -DPACKS=${UV_SCENARIO_PACKS}
      -DWORK=${_work}/${name}
      -DMODE=${S_MODE}
      -DIMPORT=${S_IMPORT}
      -DVIDEO=${S_VIDEO}
      -DDEVELOPER=${S_DEVELOPER}
      "-DEXPECT_FILES=${S_EXPECT_FILES}"
      "-DEXPECT_SIZES=${S_EXPECT_SIZES}"
      -P ${_runner})
  set_tests_properties(scenario_${name} PROPERTIES LABELS scenario TIMEOUT 300)
endfunction()

uv_scenario(boot_title)
uv_scenario(new_game_original MODE original
  EXPECT_FILES "ultima5/saves/alpha1-g1.gam;ultima5/saves/alpha1-g1.ool;ultima5/saves/alpha1-g1.json;ultima5/saves/alpha1-g1.commit;ultima5/logs/windows.log")
uv_scenario(settings_modes MODE original EXPECT_FILES "ultima5/windows.json")
uv_scenario(typed_cast_original MODE original)
uv_scenario(list_cast_enhanced MODE enhanced)
uv_scenario(save_load_menu MODE original)
uv_scenario(quicksave_enhanced MODE enhanced)
uv_scenario(original_keys MODE original)
uv_scenario(original_interface MODE original)
uv_scenario(enhanced_interface MODE enhanced)
# The window as presented (Classic Framed, Whole pixels, Fit, the window's
# scale and fullscreen), read back from the renderer; F12's picture stays the
# 320x240 panel (230,454-byte BMP).
uv_scenario(presentation MODE original
  EXPECT_FILES "window-framed-3x.bmp;panel.bmp;window-play-1080.bmp;panel-play.bmp;ultima5/windows.json"
  EXPECT_SIZES "window-framed-3x.bmp=2322486;panel.bmp=230454;window-play-1080.bmp=6220854;panel-play.bmp=230454")
# The same kind of checks where the offscreen driver cannot look: a real
# window on SDL's own Windows driver, so the default renderer (Direct3D 11)
# samples the ring and a pixel-art Fit, and the window can be maximised. It
# opens a window on the desktop, so it is off by default: configure with
# -DUV_SCENARIO_DESKTOP=ON on a machine with a display (and leave the window
# alone while it runs).
option(UV_SCENARIO_DESKTOP "Also run scenario_presentation_desktop (opens a window on the desktop)" OFF)
if(UV_SCENARIO_DESKTOP)
  uv_scenario(presentation_desktop MODE original VIDEO windows)
endif()
# The Developer menu's Teleport page reaches the places a short script cannot
# walk to; its "teleport" / "fight" steps need the menu compiled in.
if(UV_ENABLE_DEVELOPER_TOOLS)
  uv_scenario(dungeon_walk MODE original DEVELOPER)
  uv_scenario(original_arena MODE original DEVELOPER)
  uv_scenario(original_camp MODE original DEVELOPER)
  uv_scenario(surface_combat MODE original DEVELOPER)
  uv_scenario(typed_cast_combat MODE original DEVELOPER)
  uv_scenario(town_talk MODE original DEVELOPER)
  # SPP1 (docs/SHARED_PRESERVATION_PARITY_SPP1.md): Ctrl+K / Ctrl+V, the dungeon Look and
  # D, and the arena's P / Y, in both interface modes.
  uv_scenario(spp1_original MODE original DEVELOPER)
  uv_scenario(spp1_original_arena MODE original DEVELOPER)
  uv_scenario(spp1_enhanced MODE enhanced DEVELOPER)
else()
  message(STATUS "Scenarios dungeon_walk, surface_combat, typed_cast_combat, town_talk skipped: UV_ENABLE_DEVELOPER_TOOLS is OFF")
endif()
if(EXISTS "${UV_SCENARIO_PC_SAVES}/SAVED.GAM" AND EXISTS "${UV_SCENARIO_PC_SAVES}/SAVED.OOL")
  uv_scenario(pc_import_export MODE original IMPORT "${UV_SCENARIO_PC_SAVES}"
    EXPECT_FILES "ultima5/export/slot1/SAVED.GAM;ultima5/export/slot1/SAVED.OOL;ultima5/export/slot1/EXPORT.TXT;ultima5/pc-import.txt"
    EXPECT_SIZES "ultima5/export/slot1/SAVED.GAM=4192;ultima5/export/slot1/SAVED.OOL=512")
else()
  message(STATUS "Scenario pc_import_export skipped: no SAVED.GAM/SAVED.OOL in ${UV_SCENARIO_PC_SAVES}")
endif()

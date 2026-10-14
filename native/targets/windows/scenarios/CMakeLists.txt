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
  cmake_parse_arguments(S "DEVELOPER" "MODE;IMPORT" "EXPECT_FILES;EXPECT_SIZES" ${ARGN})
  add_test(NAME scenario_${name}
    COMMAND ${CMAKE_COMMAND}
      -DEXE=$<TARGET_FILE:UltimaVNative>
      -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/${name}.txt
      -DPACKS=${UV_SCENARIO_PACKS}
      -DWORK=${_work}/${name}
      -DMODE=${S_MODE}
      -DIMPORT=${S_IMPORT}
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
# The Developer menu's Teleport page reaches the places a short script cannot walk to.
uv_scenario(dungeon_walk MODE original DEVELOPER)
uv_scenario(surface_combat MODE original DEVELOPER)
uv_scenario(town_talk MODE original DEVELOPER)
if(EXISTS "${UV_SCENARIO_PC_SAVES}/SAVED.GAM" AND EXISTS "${UV_SCENARIO_PC_SAVES}/SAVED.OOL")
  uv_scenario(pc_import_export MODE original IMPORT "${UV_SCENARIO_PC_SAVES}"
    EXPECT_FILES "ultima5/export/slot1/SAVED.GAM;ultima5/export/slot1/SAVED.OOL;ultima5/export/slot1/EXPORT.TXT;ultima5/pc-import.txt"
    EXPECT_SIZES "ultima5/export/slot1/SAVED.GAM=4192;ultima5/export/slot1/SAVED.OOL=512")
else()
  message(STATUS "Scenario pc_import_export skipped: no SAVED.GAM/SAVED.OOL in ${UV_SCENARIO_PC_SAVES}")
endif()

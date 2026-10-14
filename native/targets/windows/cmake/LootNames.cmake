# Writes loot_names.inc -- the long equipment names native/core/src/loot.cpp
# includes -- from the tracked game/src/core/data/longEquipNames.json, byte for
# byte what native/core/tools/generate-world-flow-fixtures.ts writes (its line
# 13: names joined with JSON.stringify). The core tree keeps the file
# git-ignored; this lets a clean checkout build the Windows target without
# Node or extracted game data. The generated copy lives in the build tree.
function(uv_generate_loot_names json_path out_dir)
  file(READ "${json_path}" json)
  string(JSON count LENGTH "${json}" names)
  set(items "")
  math(EXPR last "${count} - 1")
  foreach(i RANGE 0 ${last})
    string(JSON name GET "${json}" names ${i})
    string(REPLACE "\\" "\\\\" name "${name}")
    string(REPLACE "\"" "\\\"" name "${name}")
    list(APPEND items "\"${name}\"")
  endforeach()
  list(JOIN items "," joined)
  set(content "// Generated from game/src/core/data/longEquipNames.json.\nstatic const char *kLootNames[] = {${joined}};\n")
  set(target "${out_dir}/loot_names.inc")
  if(EXISTS "${target}")
    file(READ "${target}" existing)
  else()
    set(existing "")
  endif()
  if(NOT existing STREQUAL content)
    file(WRITE "${target}" "${content}")
  endif()
endfunction()

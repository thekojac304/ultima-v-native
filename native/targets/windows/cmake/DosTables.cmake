# Writes dos_equipment_tables.inc -- the two DATA.OVL equipment tables the 1988
# (R)eady picker prints (re/notes/ready-picker-panel.md): the 48 short names of
# DS 0x1962 and the 48 RUNES.CH class glyphs of DS 0x1ae8 -- from the copies the
# repository already tracks for the web port (game/src/core/data/shortEquipNames.json
# and READY_GLYPH_TABLE in game/src/core/readyPicker.ts). As with loot_names.inc,
# the generated file lives in the build tree only; Windows Original mode reads
# it through the platform profile (app/dos_text.cpp), the T-Deck never does.
function(uv_generate_dos_tables names_json glyph_ts out_dir)
  file(READ "${names_json}" json)
  string(JSON count LENGTH "${json}" names)
  if(NOT count EQUAL 48)
    message(FATAL_ERROR "${names_json}: expected 48 short equipment names, found ${count}")
  endif()
  set(items "")
  foreach(i RANGE 0 47)
    string(JSON name GET "${json}" names ${i})
    string(REPLACE "\\" "\\\\" name "${name}")
    string(REPLACE "\"" "\\\"" name "${name}")
    list(APPEND items "\"${name}\"")
  endforeach()
  list(JOIN items "," joined_names)

  file(READ "${glyph_ts}" ts)
  string(REGEX MATCH "READY_GLYPH_TABLE[^=]*=[ \t\r\n]*\\[([^]]*)\\]" _ "${ts}")
  set(body "${CMAKE_MATCH_1}")
  string(REGEX MATCHALL "0x[0-9a-fA-F][0-9a-fA-F]" glyphs "${body}")
  list(LENGTH glyphs glyph_count)
  if(NOT glyph_count EQUAL 48)
    message(FATAL_ERROR "${glyph_ts}: expected 48 READY_GLYPH_TABLE bytes, found ${glyph_count}")
  endif()
  list(JOIN glyphs "," joined_glyphs)

  set(content "// Generated from game/src/core/data/shortEquipNames.json (DS 0x1962) and\n// READY_GLYPH_TABLE in game/src/core/readyPicker.ts (DS 0x1ae8).\nstatic const char *const kDosEquipmentShort[48] = {${joined_names}};\nstatic const unsigned char kDosEquipmentGlyph[48] = {${joined_glyphs}};\n")
  set(target "${out_dir}/dos_equipment_tables.inc")
  if(EXISTS "${target}")
    file(READ "${target}" existing)
  else()
    set(existing "")
  endif()
  if(NOT existing STREQUAL content)
    file(WRITE "${target}" "${content}")
  endif()
endfunction()

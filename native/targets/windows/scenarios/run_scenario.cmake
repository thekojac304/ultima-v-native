# Runs one --autotest scenario against the real UltimaVNative.exe, headless.
#
#   cmake -DEXE=... -DSCRIPT=... -DPACKS=<folder with openu5-*.bin> -DWORK=<scratch>
#         [-DMODE=original|enhanced] [-DIMPORT=<folder with SAVED.GAM/SAVED.OOL>]
#         [-DVIDEO=<SDL video driver>] [-DARGS=<more options, ;-separated>]
#         [-DEXPECT_FILES=a;b] [-DEXPECT_SIZES=name=bytes;...] -P run_scenario.cmake
#
# A fresh data folder is made for every run (WORK is deleted first), so a
# scenario never sees another's saves and never touches the player's real
# data folder. SDL uses its offscreen video (unless VIDEO names another
# driver: "windows" opens a real window) and dummy audio drivers.
foreach(v EXE SCRIPT PACKS WORK)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_scenario.cmake: ${v} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/ultima5")
file(GLOB _packs "${PACKS}/openu5-*.bin")
if(NOT _packs)
  message(FATAL_ERROR "no openu5-*.bin packs in ${PACKS}")
endif()
file(COPY ${_packs} DESTINATION "${WORK}/ultima5")
if(DEFINED IMPORT AND NOT IMPORT STREQUAL "")
  file(MAKE_DIRECTORY "${WORK}/ultima5/import")
  file(COPY "${IMPORT}/SAVED.GAM" "${IMPORT}/SAVED.OOL" DESTINATION "${WORK}/ultima5/import")
endif()

if(DEFINED VIDEO AND NOT VIDEO STREQUAL "")
  set(ENV{SDL_VIDEODRIVER} "${VIDEO}")
else()
  set(ENV{SDL_VIDEODRIVER} "offscreen")
endif()
set(ENV{SDL_AUDIODRIVER} "dummy")
set(_args --data-dir "${WORK}" --autotest "${SCRIPT}")
if(DEFINED MODE AND NOT MODE STREQUAL "")
  list(APPEND _args --mode "${MODE}")
endif()
if(DEVELOPER)
  list(APPEND _args --developer)
endif()
if(DEFINED ARGS AND NOT ARGS STREQUAL "")
  list(APPEND _args ${ARGS})
endif()
# Run inside WORK, so a script's relative paths (window_screenshot) land there.
execute_process(COMMAND "${EXE}" ${_args} WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 240)
message("${_err}${_out}")
if(NOT _rc EQUAL 0)
  if(EXISTS "${WORK}/ultima5/logs/windows.log")
    file(READ "${WORK}/ultima5/logs/windows.log" _log)
    string(LENGTH "${_log}" _len)
    if(_len GREATER 6000)
      math(EXPR _start "${_len} - 6000")
      string(SUBSTRING "${_log}" ${_start} 6000 _log)
    endif()
    message("---- end of windows.log ----\n${_log}")
  endif()
  message(FATAL_ERROR "scenario failed: exit code ${_rc}")
endif()

foreach(f IN LISTS EXPECT_FILES)
  if(NOT EXISTS "${WORK}/${f}")
    message(FATAL_ERROR "expected file missing after the scenario: ${f}")
  endif()
  message("file present: ${f}")
endforeach()
foreach(pair IN LISTS EXPECT_SIZES)
  string(REPLACE "=" ";" _kv "${pair}")
  list(GET _kv 0 _name)
  list(GET _kv 1 _bytes)
  file(SIZE "${WORK}/${_name}" _actual)
  if(NOT _actual EQUAL _bytes)
    message(FATAL_ERROR "${_name}: ${_actual} bytes, expected ${_bytes}")
  endif()
  message("size ok: ${_name} = ${_bytes}")
endforeach()
message("scenario passed")

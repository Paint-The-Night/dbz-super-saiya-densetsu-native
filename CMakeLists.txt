cmake_minimum_required(VERSION 3.20)
project(dbz_native_port VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
option(DBZ_SANITIZE "Enable address and undefined behavior sanitizers" OFF)
option(DBZ_EXTENDED_TESTS "Enable longer ROM-dependent gameplay replay tests" OFF)
if(DBZ_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

set(LAKE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lakesnes/snes)
add_library(lakesnes_cpu OBJECT ${LAKE}/cpu.c)
target_compile_definitions(lakesnes_cpu PRIVATE cpu_runOpcode=lakesnes_cpu_runOpcode)
file(GLOB LAKE_SOURCES CONFIGURE_DEPENDS ${LAKE}/*.c)
list(REMOVE_ITEM LAKE_SOURCES ${LAKE}/cpu.c)
add_library(dbz_core STATIC ${LAKE_SOURCES} $<TARGET_OBJECTS:lakesnes_cpu>
  src/native.c src/rom.c src/i18n.c src/text_script.c)
target_include_directories(dbz_core PUBLIC src ${LAKE})
if(NOT MSVC)
  target_link_libraries(dbz_core PUBLIC m)
  set_source_files_properties(src/native.c src/rom.c src/i18n.c src/text_script.c PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
endif()

if(EMSCRIPTEN)
  add_executable(dbz-port src/web_main.c)
  target_link_libraries(dbz-port PRIVATE dbz_core)
  target_compile_definitions(dbz-port PRIVATE SDL_MAIN_HANDLED)
  target_compile_options(dbz-port PRIVATE -Wall -Wextra -Werror "SHELL:-sUSE_SDL=2")
  set_target_properties(dbz-port PROPERTIES OUTPUT_NAME "dbz" SUFFIX ".js")
  target_link_options(dbz-port PRIVATE
    "SHELL:-sUSE_SDL=2"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sINITIAL_MEMORY=67108864"
    "SHELL:-sSTACK_SIZE=1048576"
    "SHELL:-sFORCE_FILESYSTEM=1"
    "SHELL:-lidbfs.js"
    "SHELL:-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_dbz_web_load_rom','_dbz_web_load_prepared_rom','_dbz_web_expected_sha256','_dbz_web_set_button','_dbz_web_set_gesture_mask','_dbz_web_pulse_button','_dbz_web_set_turbo','_dbz_web_toggle_pause','_dbz_web_enter_lang_menu','_dbz_web_set_controls_mode','_dbz_web_get_controls_mode','_dbz_web_open_controls_menu','_dbz_web_canvas_tap','_dbz_web_skip_cutscene','_dbz_i18n_set','_dbz_i18n_get']"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','HEAPU8','FS']"
    "SHELL:-sNO_EXIT_RUNTIME=1"
    "SHELL:-sENVIRONMENT=web"
    "SHELL:-sASSERTIONS=0"
  )
else()
  find_package(SDL2 CONFIG REQUIRED)
  add_executable(dbz-port src/main.c)
  target_link_libraries(dbz-port PRIVATE dbz_core SDL2::SDL2)
  target_compile_definitions(dbz-port PRIVATE SDL_MAIN_HANDLED)
  if(NOT MSVC)
    target_compile_options(dbz-port PRIVATE -Wall -Wextra -Werror)
  endif()
endif()
enable_testing()
add_executable(test-native tests/test_native.c)
target_link_libraries(test-native PRIVATE dbz_core)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME progress_consistency COMMAND ${Python3_EXECUTABLE}
  "${CMAKE_CURRENT_SOURCE_DIR}/tools/progress.py" --check-readme)
add_test(NAME progress_tool COMMAND ${Python3_EXECUTABLE}
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_progress.py")
set(DBZ_TEST_ROM "" CACHE FILEPATH "Local unmodified Japanese Rev 1 ROM used by differential tests")
if(NOT DBZ_TEST_ROM AND DEFINED ENV{DBZ_ROM})
  set(DBZ_TEST_ROM "$ENV{DBZ_ROM}")
endif()
if(NOT DBZ_TEST_ROM)
  set(DBZ_DEFAULT_ROM "${CMAKE_CURRENT_SOURCE_DIR}/Backup/Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc")
  if(EXISTS "${DBZ_DEFAULT_ROM}")
    set(DBZ_TEST_ROM "${DBZ_DEFAULT_ROM}")
  endif()
endif()
if(DBZ_TEST_ROM AND EXISTS "${DBZ_TEST_ROM}")
  add_test(NAME native_equivalence COMMAND test-native "${DBZ_TEST_ROM}")
  add_test(NAME checkpoint_equivalence COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_checkpoint.py" $<TARGET_FILE:dbz-port> "${DBZ_TEST_ROM}")
  add_test(NAME scene_regression COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_scenes.py"
    --binary $<TARGET_FILE:dbz-port> --rom "${DBZ_TEST_ROM}")
  set_tests_properties(scene_regression PROPERTIES LABELS "scene;gameplay" TIMEOUT 600)
  if(DBZ_EXTENDED_TESTS)
    add_test(NAME battle_equivalence COMMAND ${Python3_EXECUTABLE}
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_battle.py" $<TARGET_FILE:dbz-port> "${DBZ_TEST_ROM}")
    set_tests_properties(battle_equivalence PROPERTIES LABELS "extended;gameplay" TIMEOUT 300)
  endif()
else()
  message(STATUS "DBZ_TEST_ROM not set; differential tests are omitted. Set DBZ_ROM or -DDBZ_TEST_ROM=/path/to/ROM and reconfigure.")
endif()

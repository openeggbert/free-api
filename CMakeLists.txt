cmake_minimum_required(VERSION 3.23)

project(free-api
        VERSION 0.1.0
        DESCRIPTION "Minimal WinAPI reimplementation in C++"
        LANGUAGES CXX)

include(GNUInstallDirs)

# --- SDL3 acquisition --------------------------------------------------------
# free-api needs SDL3::SDL3, SDL3_image::SDL3_image and SDL3_mixer::SDL3_mixer
# as an internal backend detail. free-api never vendors SDL3 itself; it only
# ever does one of the following, in order:
#   1) Reuse targets a parent project already created before this repo was
#      added via add_subdirectory() (e.g. ../free-eggbert or ../planetblupi
#      building free-api as part of their own tree).
#   2) Find a system-installed SDL3/SDL3_image/SDL3_mixer via find_package()
#      when configured with -DFREE_API_USE_SYSTEM_SDL3=ON.
# A third "sibling-vendored" tier (reusing a sibling game's own SDL3-vendoring
# CMake helper when free-api itself is the top-level project) previously
# existed here but was removed (TASK-24H-0001): that helper always resolves
# its vendored-dependency root relative to CMAKE_SOURCE_DIR, which is
# free-api's own root in exactly the situation where this fallback could ever
# be reached -- so it could never actually succeed, and only produced a
# confusing, wrongly-attributed "missing submodule" error instead of the
# clear guidance below. See docs/cmake-options.md for details.
option(FREE_API_USE_SYSTEM_SDL3
        "Find SDL3/SDL3_image/SDL3_mixer via find_package() instead of relying on a parent project"
        OFF)

if(NOT TARGET SDL3::SDL3 AND FREE_API_USE_SYSTEM_SDL3)
    find_package(SDL3 REQUIRED)
    find_package(SDL3_image REQUIRED)
    find_package(SDL3_mixer REQUIRED)
endif()

if(NOT TARGET SDL3::SDL3 OR NOT TARGET SDL3_image::SDL3_image OR NOT TARGET SDL3_mixer::SDL3_mixer)
    message(FATAL_ERROR
            "free-api requires SDL3::SDL3, SDL3_image::SDL3_image and SDL3_mixer::SDL3_mixer targets, "
            "none of which were found. Either:\n"
            "  - configure with -DFREE_API_USE_SYSTEM_SDL3=ON (requires SDL3/SDL3_image/SDL3_mixer dev packages), or\n"
            "  - build free-api as a subdirectory of ../free-eggbert or ../planetblupi (either already provides SDL3), or\n"
            "  - define the SDL3::SDL3 / SDL3_image::SDL3_image / SDL3_mixer::SDL3_mixer targets yourself "
            "before add_subdirectory(free-api).")
endif()

# --- Real UI string-table extraction (LoadStringA) --------------------------
# Both target games source all on-screen UI text through LoadStringA, but
# neither compiles a .rc file into its build, so the real STRINGTABLE data
# already sitting in their own resource/*.rc files is otherwise unused.
# cmake/ExtractStringTable.cmake is a narrow, STRINGTABLE-only extractor (not
# a general .rc/.res compiler) that turns that data into a small generated
# .cpp compiled into free-api, so LoadStringA can return real text instead of
# a placeholder. Building free-api standalone (neither sibling game present)
# simply yields an empty generated table, and LoadStringA falls back to its
# documented placeholder behavior -- see docs/cmake-options.md.

# Explicit override for which game is authoritative, on top of the
# CMAKE_PROJECT_NAME auto-detection below -- see docs/cmake-options.md.
# Kept intentionally tiny (four fixed values, no further sub-options): this
# is a targeted override for the two known games plus a forced-off switch,
# not a general build-configuration framework.
set(FREE_API_TARGET_GAME "auto" CACHE STRING
        "Which target game's resources drive LoadStringA's real-text table \
and fail-loud verification. 'auto' (default) detects via CMAKE_PROJECT_NAME. \
'free-eggbert'/'planetblupi' force that game (and its REQUIRE_STRINGS/known-ID/ \
used-ID gating) even when CMAKE_PROJECT_NAME doesn't currently say so -- falls \
back to the ../<game> sibling layout if we are not actually that game's own \
subdirectory. 'standalone' forces the ungated placeholder-fallback path even \
if CMAKE_PROJECT_NAME would otherwise select a game.")
set_property(CACHE FREE_API_TARGET_GAME PROPERTY STRINGS auto free-eggbert planetblupi standalone)

if(NOT FREE_API_TARGET_GAME MATCHES "^(auto|free-eggbert|planetblupi|standalone)$")
    message(FATAL_ERROR "free-api: FREE_API_TARGET_GAME must be one of: auto, free-eggbert, planetblupi, standalone (got '${FREE_API_TARGET_GAME}')")
endif()

set(_free_api_string_table_rc "")
set(_free_api_string_table_encoding "UTF-8")
set(_free_api_string_table_headers "")
set(_free_api_target_game "")
set(_free_api_verify_id "")
set(_free_api_verify_text "")
set(_free_api_used_ids_file "")

# Resolve which game (if any) is authoritative for this build. Default
# (FREE_API_TARGET_GAME=auto) prefers detecting which game is ACTUALLY
# building free-api right now via the top-level project name
# (CMAKE_PROJECT_NAME), not just "does a sibling directory happen to exist
# next to free-api" -- both target games are always siblings of each other
# on a typical development machine, so sibling *existence* alone cannot
# tell planetblupi's own build from free-eggbert's: whichever one is the
# top-level project is unambiguous, since CMake sets CMAKE_PROJECT_NAME
# from the first project() call in the whole configure run.
# FREE_API_TARGET_GAME=free-eggbert/planetblupi/standalone overrides that
# detection explicitly (see the CACHE STRING docstring above).
#
# _free_api_target_game is also the fail-loud switch: when set, a missing
# .rc file, a zero-string extraction, a known-ID mismatch, or a missing
# used-ID (docs/used-string-ids.md) below is a configure-time FATAL_ERROR --
# shipped game UI must never silently fall back to placeholder/RES_<id>
# text. It stays unset for standalone free-api builds (including the
# sibling-lookup convenience path below), which keep the placeholder-
# fallback behavior documented in docs/cmake-options.md.
if(FREE_API_TARGET_GAME STREQUAL "standalone")
    set(_free_api_resolved_game "")
elseif(FREE_API_TARGET_GAME STREQUAL "free-eggbert")
    set(_free_api_resolved_game "free-eggbert")
elseif(FREE_API_TARGET_GAME STREQUAL "planetblupi")
    set(_free_api_resolved_game "planetblupi")
elseif(CMAKE_PROJECT_NAME STREQUAL "SPEEDY_BLUPI_WINDOWS")
    set(_free_api_resolved_game "free-eggbert")
elseif(CMAKE_PROJECT_NAME STREQUAL "PLANET_BLUPI_WINDOWS")
    set(_free_api_resolved_game "planetblupi")
else()
    set(_free_api_resolved_game "")
endif()

if(_free_api_resolved_game STREQUAL "free-eggbert")
    set(_free_api_target_game "free-eggbert")
    if(CMAKE_PROJECT_NAME STREQUAL "SPEEDY_BLUPI_WINDOWS")
        # free-eggbert is building us directly; CMAKE_SOURCE_DIR is its own root.
        set(_free_api_game_root "${CMAKE_SOURCE_DIR}")
    else()
        # FREE_API_TARGET_GAME explicitly forced free-eggbert, but we are not
        # actually being built as free-eggbert's own subdirectory right now
        # -- fall back to the sibling-directory layout instead.
        set(_free_api_game_root "${CMAKE_CURRENT_SOURCE_DIR}/../free-eggbert")
    endif()
    set(_free_api_string_table_rc "${_free_api_game_root}/resource/Eggbert2.rc")
    set(_free_api_string_table_encoding "UTF-16LE")
    set(_free_api_string_table_headers "${_free_api_game_root}/resource/resource.h")
    # TX_BUTTON_QUITTER (id 106) is a known STRINGTABLE entry in Eggbert2.rc
    # that must resolve to "Quit BLUPI" -- catches silent parsing regressions.
    set(_free_api_verify_id "106")
    set(_free_api_verify_text "Quit BLUPI")
    set(_free_api_used_ids_file "${CMAKE_CURRENT_SOURCE_DIR}/cmake/used-string-ids/free-eggbert.txt")
    unset(_free_api_game_root)
elseif(_free_api_resolved_game STREQUAL "planetblupi")
    set(_free_api_target_game "planetblupi")
    if(CMAKE_PROJECT_NAME STREQUAL "PLANET_BLUPI_WINDOWS")
        # planetblupi is building us directly; CMAKE_SOURCE_DIR is its own root.
        set(_free_api_game_root "${CMAKE_SOURCE_DIR}")
    else()
        # FREE_API_TARGET_GAME explicitly forced planetblupi, but we are not
        # actually being built as planetblupi's own subdirectory right now --
        # fall back to the sibling-directory layout instead.
        set(_free_api_game_root "${CMAKE_CURRENT_SOURCE_DIR}/../planetblupi")
    endif()
    set(_free_api_string_table_rc "${_free_api_game_root}/resource/blupi-e.rc")
    set(_free_api_string_table_encoding "UTF-8")
    set(_free_api_string_table_headers
            "${_free_api_game_root}/include/resource.h;${_free_api_game_root}/include/resrc1.h")
    # TX_BUTTON_QUITTER (id 106) is a known STRINGTABLE entry in blupi-e.rc
    # that must resolve to "Quit BLUPI" -- catches silent parsing regressions.
    set(_free_api_verify_id "106")
    set(_free_api_verify_text "Quit BLUPI")
    set(_free_api_used_ids_file "${CMAKE_CURRENT_SOURCE_DIR}/cmake/used-string-ids/planetblupi.txt")
    unset(_free_api_game_root)
else()
    # Standalone free-api build (FREE_API_TARGET_GAME=standalone, or auto
    # with neither game's CMAKE_PROJECT_NAME detected): fall back to a
    # developer-convenience sibling lookup, purely for local iteration --
    # checked in order, first match wins. Never fails configure over
    # missing/empty resources: _free_api_target_game stays unset, so
    # REQUIRE_STRINGS/VERIFY_ID/USED_IDS_FILE are never passed below.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../free-eggbert/resource/Eggbert2.rc")
        set(_free_api_string_table_rc "${CMAKE_CURRENT_SOURCE_DIR}/../free-eggbert/resource/Eggbert2.rc")
        set(_free_api_string_table_encoding "UTF-16LE")
        set(_free_api_string_table_headers "${CMAKE_CURRENT_SOURCE_DIR}/../free-eggbert/resource/resource.h")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../planetblupi/resource/blupi-e.rc")
        set(_free_api_string_table_rc "${CMAKE_CURRENT_SOURCE_DIR}/../planetblupi/resource/blupi-e.rc")
        set(_free_api_string_table_encoding "UTF-8")
        set(_free_api_string_table_headers
                "${CMAKE_CURRENT_SOURCE_DIR}/../planetblupi/include/resource.h;${CMAKE_CURRENT_SOURCE_DIR}/../planetblupi/include/resrc1.h")
    endif()
endif()
unset(_free_api_resolved_game)

set(FREE_API_GENERATED_STRINGS_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/FreeApiGeneratedStrings.cpp")

set(_free_api_extract_extra_args "")
if(_free_api_target_game)
    list(APPEND _free_api_extract_extra_args
            -DREQUIRE_STRINGS=ON
            "-DTARGET_GAME_NAME=${_free_api_target_game}"
            "-DVERIFY_ID=${_free_api_verify_id}"
            "-DVERIFY_TEXT=${_free_api_verify_text}"
            "-DUSED_IDS_FILE=${_free_api_used_ids_file}")
endif()

if(_free_api_string_table_rc)
    execute_process(
            COMMAND ${CMAKE_COMMAND}
                -DOUTPUT_CPP=${FREE_API_GENERATED_STRINGS_CPP}
                -DRC_FILE=${_free_api_string_table_rc}
                -DRC_ENCODING=${_free_api_string_table_encoding}
                "-DRESOURCE_HEADERS=${_free_api_string_table_headers}"
                ${_free_api_extract_extra_args}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtractStringTable.cmake
            RESULT_VARIABLE _free_api_string_table_result
    )
else()
    execute_process(
            COMMAND ${CMAKE_COMMAND}
                -DOUTPUT_CPP=${FREE_API_GENERATED_STRINGS_CPP}
                ${_free_api_extract_extra_args}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtractStringTable.cmake
            RESULT_VARIABLE _free_api_string_table_result
    )
endif()

if(NOT _free_api_string_table_result EQUAL 0)
    message(FATAL_ERROR "free-api: ExtractStringTable.cmake failed (result=${_free_api_string_table_result}) -- see messages above for the specific STRINGTABLE extraction/verification failure")
endif()

# Kept (not unset) so tests/test_loadstring_regressions.cpp's build-mode
# compile definitions below can branch on which table, if any, is real.
set(FREE_API_DETECTED_TARGET_GAME "${_free_api_target_game}")

unset(_free_api_string_table_rc)
unset(_free_api_string_table_encoding)
unset(_free_api_string_table_headers)
unset(_free_api_string_table_result)
unset(_free_api_target_game)
unset(_free_api_verify_id)
unset(_free_api_verify_text)
unset(_free_api_used_ids_file)
unset(_free_api_extract_extra_args)

add_library(free-api STATIC)
add_library(free-api::free-api ALIAS free-api)

set_target_properties(free-api PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Compatibility headers: windows.h, io.h, direct.h, winbase.h...
# Must NOT be globally visible to SDL or other third_party libraries.
if(NOT TARGET freeapi_compat_headers)
    add_library(freeapi_compat_headers INTERFACE)
    add_library(free-api::compat_headers ALIAS freeapi_compat_headers)

    target_include_directories(freeapi_compat_headers INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(NOT WIN32)
        target_include_directories(freeapi_compat_headers INTERFACE
                ${CMAKE_CURRENT_SOURCE_DIR}/include_non_windows
        )
    endif()
endif()

target_sources(free-api PRIVATE
        src/winapi.cpp

        src/internal/FreeApiDiagnostics.cpp
        src/internal/FreeApiGdi.cpp
        src/internal/FreeApiWindowRegistry.cpp
        src/internal/FreeApiMessageQueue.cpp
        src/internal/FreeApiTimers.cpp
        src/internal/FreeApiPath.cpp
        src/internal/FreeApiSdlVideo.cpp
        src/internal/FreeApiStringTable.cpp
        ${FREE_API_GENERATED_STRINGS_CPP}

        src/winbase.cpp
        src/winbase_file.cpp

        src/winuser_window.cpp
        src/winuser_message.cpp
        src/winuser_cursor.cpp
        src/winuser_timer.cpp
        src/winuser_misc.cpp

        src/wingdi_bitmap.cpp
        src/wingdi_dc.cpp
        src/wingdi_blit.cpp
        src/wingdi_misc.cpp

        src/winmm.cpp

        src/crt_io.cpp
        src/crt_direct.cpp

        src/winmain_bridge.cpp

        src/MidiMusic.cpp

        src/platform/PlatformProcessInfo.cpp
)

target_include_directories(free-api PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(free-api
        PUBLIC
        freeapi_compat_headers
        PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
)

if(EMSCRIPTEN)
    option(FREE_API_BUILD_TESTS "Build free-api tests" OFF)
else()
    option(FREE_API_BUILD_TESTS "Build free-api tests" ON)
endif()

if(FREE_API_BUILD_TESTS)
    enable_testing()

    add_executable(basic_test tests/basic_test.cpp)
    set_target_properties(basic_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(basic_test PRIVATE free-api SDL3::SDL3)
    add_test(NAME basic_test COMMAND basic_test)

    # TASK-0016: fresh-clone smoke check preventing a regression of the
    # hardcoded-absolute-path bug TASK-0011 fixed.
    #
    # Must use CMAKE_CURRENT_SOURCE_DIR, not CMAKE_SOURCE_DIR: when free-api
    # is built as a subdirectory of either target game,
    # CMAKE_SOURCE_DIR refers to *that game's* root (the top-level
    # project), not free-api's own -- CMAKE_CURRENT_SOURCE_DIR always
    # resolves to the directory of the CMakeLists.txt currently being
    # processed (free-api's own root), regardless of nesting.
    add_test(NAME check_no_hardcoded_paths
             COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckNoHardcodedPaths.cmake")

    add_executable(test_input_pipeline tests/test_input_pipeline.cpp)
    set_target_properties(test_input_pipeline PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_input_pipeline PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_input_pipeline COMMAND test_input_pipeline)

    add_executable(test_header_compile tests/test_header_compile.cpp)
    set_target_properties(test_header_compile PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_header_compile PRIVATE freeapi_compat_headers)
    add_test(NAME test_header_compile COMMAND test_header_compile)

    add_executable(test_winuser_regressions tests/test_winuser_regressions.cpp)
    set_target_properties(test_winuser_regressions PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_winuser_regressions PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_winuser_regressions COMMAND test_winuser_regressions)

    add_executable(test_file_regressions tests/test_file_regressions.cpp)
    set_target_properties(test_file_regressions PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_file_regressions PRIVATE free-api)
    add_test(NAME test_file_regressions COMMAND test_file_regressions)

    add_executable(test_file_paths tests/test_file_paths.cpp)
    set_target_properties(test_file_paths PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_file_paths PRIVATE free-api)
    add_test(NAME test_file_paths COMMAND test_file_paths)

    add_executable(test_loadstring_regressions tests/test_loadstring_regressions.cpp)
    set_target_properties(test_loadstring_regressions PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_loadstring_regressions PRIVATE free-api)
    # Tells the test which build mode it's running in, so it can assert the
    # right contract: standalone builds may legitimately return the
    # "RES_<id>" placeholder, but target-game builds must never return it
    # for a known game string ID (see docs/out-of-scope.md's "RES_<id>" note).
    if(FREE_API_DETECTED_TARGET_GAME STREQUAL "free-eggbert")
        target_compile_definitions(test_loadstring_regressions PRIVATE FREE_API_TARGET_GAME_FREE_EGGBERT=1)
    elseif(FREE_API_DETECTED_TARGET_GAME STREQUAL "planetblupi")
        target_compile_definitions(test_loadstring_regressions PRIVATE FREE_API_TARGET_GAME_PLANETBLUPI=1)
    endif()
    add_test(NAME test_loadstring_regressions COMMAND test_loadstring_regressions)

    add_executable(test_resources tests/test_resources.cpp)
    set_target_properties(test_resources PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_resources PRIVATE free-api)
    add_test(NAME test_resources COMMAND test_resources)

    add_executable(test_joystick_regressions tests/test_joystick_regressions.cpp)
    set_target_properties(test_joystick_regressions PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_joystick_regressions PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_joystick_regressions COMMAND test_joystick_regressions)

    add_executable(test_mci_avivideo_regressions tests/test_mci_avivideo_regressions.cpp)
    set_target_properties(test_mci_avivideo_regressions PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_mci_avivideo_regressions PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_mci_avivideo_regressions COMMAND test_mci_avivideo_regressions)

    add_executable(test_gdi_regressions tests/test_gdi_regressions.cpp)
    set_target_properties(test_gdi_regressions PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_gdi_regressions PRIVATE free-api)
    add_test(NAME test_gdi_regressions COMMAND test_gdi_regressions)

    add_executable(test_timer_regressions tests/test_timer_regressions.cpp)
    set_target_properties(test_timer_regressions PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_timer_regressions PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_timer_regressions COMMAND test_timer_regressions)

    add_executable(test_planetblupi_loop tests/test_planetblupi_loop.cpp)
    set_target_properties(test_planetblupi_loop PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_planetblupi_loop PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_planetblupi_loop COMMAND test_planetblupi_loop)

    add_executable(test_eggbert_loop tests/test_eggbert_loop.cpp)
    set_target_properties(test_eggbert_loop PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_eggbert_loop PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_eggbert_loop COMMAND test_eggbert_loop)

    add_executable(test_mci_sequences tests/test_mci_sequences.cpp)
    set_target_properties(test_mci_sequences PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_mci_sequences PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_mci_sequences COMMAND test_mci_sequences)

    if(NOT WIN32)
        add_executable(test_timeb tests/test_timeb.cpp)
        set_target_properties(test_timeb PROPERTIES
                CXX_STANDARD 20
                CXX_STANDARD_REQUIRED YES
                CXX_EXTENSIONS NO
        )
        target_link_libraries(test_timeb PRIVATE freeapi_compat_headers)
        add_test(NAME test_timeb COMMAND test_timeb)
    endif()
endif()

# --- Examples ----------------------------------------------------------------
# Standalone, single-file demonstrations of specific WinAPI-compatible
# functionality (see examples/README.md). Unlike tests, these open a real
# window and expect a real display/audio backend, so they are opt-in and not
# built by default when free-api is consumed as a sibling library by either
# target game.
if(EMSCRIPTEN)
    option(FREE_API_BUILD_EXAMPLES "Build free-api examples" OFF)
else()
    option(FREE_API_BUILD_EXAMPLES "Build free-api examples" OFF)
endif()

if(FREE_API_BUILD_EXAMPLES)
    foreach(_free_api_example IN ITEMS
            01_window_and_message_loop
            02_timers
            03_input_and_cursor
            04_gdi_minimap
            05_midi_playback)
        add_executable(${_free_api_example} examples/${_free_api_example}.cpp)
        set_target_properties(${_free_api_example} PROPERTIES
                CXX_STANDARD 20
                CXX_STANDARD_REQUIRED YES
                CXX_EXTENSIONS NO
        )
        target_link_libraries(${_free_api_example} PRIVATE free-api SDL3::SDL3)
    endforeach()
    unset(_free_api_example)
endif()

install(TARGETS free-api freeapi_compat_headers
        EXPORT free-api-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
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
    # TASK-24H-1237: version floors matching what this project's testing has
    # actually been validated against in its active development environment
    # (SDL3/SDL3_image 3.4.0, SDL3_mixer 3.2.0, confirmed via each package's
    # installed *ConfigVersion.cmake). free-api links against whatever
    # SDL3-family version the consuming game or system provides, with no
    # other build-time compatibility guard (unlike TinySoundFont/
    # TinyMidiLoader, which are vendored source and therefore effectively
    # pinned) -- these floors catch an accidentally-much-older SDL3 install
    # at configure time instead of surfacing as an obscure runtime/link
    # failure. Raise the floor if a newer version is validated; do not lower
    # it without re-validating against the older version first.
    find_package(SDL3 3.4.0 REQUIRED)
    find_package(SDL3_image 3.4.0 REQUIRED)
    find_package(SDL3_mixer 3.2.0 REQUIRED)
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
subdirectory. 'standalone' disables the fail-loud REQUIRE_STRINGS/VERIFY_ID/ \
USED_IDS_FILE verification gating even if CMAKE_PROJECT_NAME would \
otherwise select a game -- it does NOT guarantee an empty/placeholder \
string table: the same developer-convenience sibling .rc lookup 'auto' \
uses still opportunistically runs and extracts real text if a sibling \
game's .rc happens to be present on disk, just unverified (TASK-24H-0008).")
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

# TASK-24H-0014: directory-scoped, not per-target -- every target below
# (free-api itself, plus all test/example targets) previously repeated an
# identical 5-line set_target_properties(... CXX_STANDARD 20
# CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO) block (19 copies, ~90 lines
# of pure repetition). Placed here (after the SDL3-acquisition block above,
# before the first add_library()) so it does not retroactively change the
# C++ standard used to build any add_subdirectory()'d SDL3/SDL3_image/
# SDL3_mixer vendored subprojects, which are configured earlier during SDL3
# acquisition and must keep their own build settings unchanged. Matches
# ../free-direct/CMakeLists.txt's existing directory-scoped form.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

add_library(free-api STATIC)
add_library(free-api::free-api ALIAS free-api)

set_target_properties(free-api PROPERTIES
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

# --- Optional sanitizer instrumentation (opt-in, off by default) ------------
# TASK-24H-0506: gives mechanical, sanitizer-verified confidence that
# FreeApiMmTimerBridge (src/winmm.cpp) doesn't race timeKillEvent, instead of
# resting on manual code review of a genuinely subtle cross-thread ordering
# alone. Purely additive: the default value ("") leaves every existing build
# mode -- including both target games' own default desktop builds -- exactly
# as before. PUBLIC so tests/examples linking free-api automatically pick up
# matching compile/link flags (required for sanitizer runtimes to work), but
# nothing enables this except an explicit -DFREE_API_SANITIZE=... by hand.
set(FREE_API_SANITIZE "" CACHE STRING
        "Instrument free-api (and anything that links it, e.g. its test \
binaries) with a Clang/GCC sanitizer for local verification. '' (default, \
off) leaves the build unmodified. 'thread' enables ThreadSanitizer. \
'address' enables AddressSanitizer.")
set_property(CACHE FREE_API_SANITIZE PROPERTY STRINGS "" thread address)

if(FREE_API_SANITIZE)
    if(NOT FREE_API_SANITIZE MATCHES "^(thread|address)$")
        message(FATAL_ERROR "free-api: FREE_API_SANITIZE must be '', 'thread', or 'address' (got '${FREE_API_SANITIZE}')")
    endif()
    if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        message(FATAL_ERROR "free-api: FREE_API_SANITIZE=${FREE_API_SANITIZE} requires Clang or GCC (got '${CMAKE_CXX_COMPILER_ID}')")
    endif()
    target_compile_options(free-api PUBLIC -fsanitize=${FREE_API_SANITIZE} -fno-omit-frame-pointer -g)
    target_link_options(free-api PUBLIC -fsanitize=${FREE_API_SANITIZE})
endif()

if(EMSCRIPTEN)
    option(FREE_API_BUILD_TESTS "Build free-api tests" OFF)
else()
    option(FREE_API_BUILD_TESTS "Build free-api tests" ON)
endif()

if(FREE_API_BUILD_TESTS)
    enable_testing()

    add_executable(basic_test tests/basic_test.cpp)
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

    # TASK-24H-0011: proves the checker's own pattern-matching logic still
    # actually fires on genuinely bad input (and doesn't false-positive on
    # clean input) -- the test above only ever exercises the "nothing
    # found" branch against the current, already-clean repo.
    add_test(NAME check_no_hardcoded_paths_self_test
             COMMAND ${CMAKE_COMMAND}
                     -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-fixtures-out
                     -DCHECKER_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckNoHardcodedPaths.cmake
                     -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckNoHardcodedPathsSelfTest.cmake")

    # TASK-24H-1238: a lightweight guard against docs/scope.md's citation
    # rule eroding silently -- fails loudly (naming the offending symbol) if
    # include/*.h or include_non_windows/*.h gains a new public declaration
    # not yet listed in cmake/known-public-symbols.txt. Same
    # CMAKE_CURRENT_SOURCE_DIR rationale as check_no_hardcoded_paths above.
    add_test(NAME check_public_surface_baseline
             COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckPublicSurfaceBaseline.cmake")

    # TASK-24H-1238: proves the extractor's regex set still actually finds
    # every declaration shape it's meant to (and excludes include guards/
    # comments), and that the baseline-diff logic fires on a genuinely new
    # symbol -- the real check above only ever exercises the "nothing new
    # found" branch against the current, already-clean header set.
    add_test(NAME check_public_surface_baseline_self_test
             COMMAND ${CMAKE_COMMAND}
                     -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-fixtures-out
                     -DCHECKER_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckPublicSurfaceBaseline.cmake
                     -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckPublicSurfaceBaselineSelfTest.cmake")

    # TASK-24H-0801/0802: negative-path tests proving ExtractStringTable.cmake's
    # fail-loud gates (REQUIRE_STRINGS, VERIFY_ID/VERIFY_TEXT, USED_IDS_FILE's
    # missing-ID and duplicate-ID checks) actually fire on broken input,
    # rather than being trusted by code-reading alone. Each uses the small
    # cmake/test-fixtures/minimal-stringtable.rc fixture (one STRINGTABLE
    # entry, IDS_TEST_STRING=42="Hello Test") and CTest's WILL_FAIL property
    # to assert the underlying `cmake -P` invocation fails (non-zero exit /
    # FATAL_ERROR), which CTest then reports as this *test* PASSING.
    set(_free_api_st_fixture_rc "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test-fixtures/minimal-stringtable.rc")
    set(_free_api_st_fixture_header "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test-fixtures/minimal-resource.h")

    # Positive control: proves the fixture itself is valid and the extractor
    # succeeds end-to-end (correct VERIFY_ID/VERIFY_TEXT, correct
    # USED_IDS_FILE) -- so the negative tests below are known to be failing
    # for the *specific broken parameter under test*, not some unrelated
    # fixture problem.
    add_test(NAME extract_string_table_fixture_positive_control
             COMMAND ${CMAKE_COMMAND}
                     -DOUTPUT_CPP=${CMAKE_CURRENT_BINARY_DIR}/test-fixtures-out/positive.cpp
                     -DRC_FILE=${_free_api_st_fixture_rc}
                     -DRC_ENCODING=UTF-8
                     -DRESOURCE_HEADERS=${_free_api_st_fixture_header}
                     -DREQUIRE_STRINGS=ON -DTARGET_GAME_NAME=test-fixture
                     -DVERIFY_ID=42 "-DVERIFY_TEXT=Hello Test"
                     -DUSED_IDS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/test-fixtures/used-ids-valid.txt
                     -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtractStringTable.cmake")

    # REQUIRE_STRINGS: a missing .rc file must be a FATAL_ERROR, not a
    # silent empty-table fallback, when REQUIRE_STRINGS is set.
    add_test(NAME extract_string_table_require_strings_fails_on_missing_rc
             COMMAND ${CMAKE_COMMAND}
                     -DOUTPUT_CPP=${CMAKE_CURRENT_BINARY_DIR}/test-fixtures-out/missing-rc.cpp
                     -DRC_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/test-fixtures/does-not-exist.rc
                     -DRC_ENCODING=UTF-8
                     -DRESOURCE_HEADERS=${_free_api_st_fixture_header}
                     -DREQUIRE_STRINGS=ON -DTARGET_GAME_NAME=test-fixture
                     -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtractStringTable.cmake")
    set_tests_properties(extract_string_table_require_strings_fails_on_missing_rc PROPERTIES WILL_FAIL TRUE)

    # VERIFY_ID/VERIFY_TEXT: a known-ID/text mismatch must be a FATAL_ERROR.
    add_test(NAME extract_string_table_verify_id_fails_on_text_mismatch
             COMMAND ${CMAKE_COMMAND}
                     -DOUTPUT_CPP=${CMAKE_CURRENT_BINARY_DIR}/test-fixtures-out/verify-mismatch.cpp
                     -DRC_FILE=${_free_api_st_fixture_rc}
                     -DRC_ENCODING=UTF-8
                     -DRESOURCE_HEADERS=${_free_api_st_fixture_header}
                     -DREQUIRE_STRINGS=ON -DTARGET_GAME_NAME=test-fixture
                     -DVERIFY_ID=42 "-DVERIFY_TEXT=This Is Not The Real Text"
                     -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtractStringTable.cmake")
    set_tests_properties(extract_string_table_verify_id_fails_on_text_mismatch PROPERTIES WILL_FAIL TRUE)

    # USED_IDS_FILE missing-ID gate: a manifest listing an ID absent from
    # the extracted table must be a FATAL_ERROR.
    add_test(NAME extract_string_table_used_ids_fails_on_missing_id
             COMMAND ${CMAKE_COMMAND}
                     -DOUTPUT_CPP=${CMAKE_CURRENT_BINARY_DIR}/test-fixtures-out/missing-id.cpp
                     -DRC_FILE=${_free_api_st_fixture_rc}
                     -DRC_ENCODING=UTF-8
                     -DRESOURCE_HEADERS=${_free_api_st_fixture_header}
                     -DREQUIRE_STRINGS=ON -DTARGET_GAME_NAME=test-fixture
                     -DUSED_IDS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/test-fixtures/used-ids-missing.txt
                     -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtractStringTable.cmake")
    set_tests_properties(extract_string_table_used_ids_fails_on_missing_id PROPERTIES WILL_FAIL TRUE)

    # USED_IDS_FILE duplicate-ID gate: a manifest listing the same ID twice
    # must be a FATAL_ERROR, checked before the missing-ID check.
    add_test(NAME extract_string_table_used_ids_fails_on_duplicate_id
             COMMAND ${CMAKE_COMMAND}
                     -DOUTPUT_CPP=${CMAKE_CURRENT_BINARY_DIR}/test-fixtures-out/duplicate-id.cpp
                     -DRC_FILE=${_free_api_st_fixture_rc}
                     -DRC_ENCODING=UTF-8
                     -DRESOURCE_HEADERS=${_free_api_st_fixture_header}
                     -DREQUIRE_STRINGS=ON -DTARGET_GAME_NAME=test-fixture
                     -DUSED_IDS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/test-fixtures/used-ids-duplicate.txt
                     -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtractStringTable.cmake")
    set_tests_properties(extract_string_table_used_ids_fails_on_duplicate_id PROPERTIES WILL_FAIL TRUE)

    add_executable(test_input_pipeline tests/test_input_pipeline.cpp)
    target_link_libraries(test_input_pipeline PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_input_pipeline COMMAND test_input_pipeline)

    add_executable(test_header_compile tests/test_header_compile.cpp)
    target_link_libraries(test_header_compile PRIVATE freeapi_compat_headers)
    add_test(NAME test_header_compile COMMAND test_header_compile)

    # TASK-24H-1113: source-scan guard against future ungated SDL_Log
    # additions. Must use CMAKE_CURRENT_SOURCE_DIR, not CMAKE_SOURCE_DIR --
    # see the check_no_hardcoded_paths comment above for why.
    add_executable(test_sdl_log_gating tests/test_sdl_log_gating.cpp)
    target_compile_definitions(test_sdl_log_gating PRIVATE
        FREE_API_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_sdl_log_gating COMMAND test_sdl_log_gating)

    add_executable(test_winuser_regressions tests/test_winuser_regressions.cpp)
    target_link_libraries(test_winuser_regressions PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_winuser_regressions COMMAND test_winuser_regressions)

    add_executable(test_file_regressions tests/test_file_regressions.cpp)
    target_link_libraries(test_file_regressions PRIVATE free-api)
    add_test(NAME test_file_regressions COMMAND test_file_regressions)

    add_executable(test_file_paths tests/test_file_paths.cpp)
    target_link_libraries(test_file_paths PRIVATE free-api)
    add_test(NAME test_file_paths COMMAND test_file_paths)

    add_executable(test_loadstring_regressions tests/test_loadstring_regressions.cpp)
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
    target_link_libraries(test_resources PRIVATE free-api)
    add_test(NAME test_resources COMMAND test_resources)

    add_executable(test_joystick_regressions tests/test_joystick_regressions.cpp)
    target_link_libraries(test_joystick_regressions PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_joystick_regressions COMMAND test_joystick_regressions)

    add_executable(test_mci_avivideo_regressions tests/test_mci_avivideo_regressions.cpp)
    target_link_libraries(test_mci_avivideo_regressions PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_mci_avivideo_regressions COMMAND test_mci_avivideo_regressions)

    add_executable(test_gdi_regressions tests/test_gdi_regressions.cpp)
    target_link_libraries(test_gdi_regressions PRIVATE free-api)
    add_test(NAME test_gdi_regressions COMMAND test_gdi_regressions)

    add_executable(test_timer_regressions tests/test_timer_regressions.cpp)
    target_link_libraries(test_timer_regressions PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_timer_regressions COMMAND test_timer_regressions)

    add_executable(test_planetblupi_loop tests/test_planetblupi_loop.cpp)
    target_link_libraries(test_planetblupi_loop PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_planetblupi_loop COMMAND test_planetblupi_loop)

    add_executable(test_eggbert_loop tests/test_eggbert_loop.cpp)
    target_link_libraries(test_eggbert_loop PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_eggbert_loop COMMAND test_eggbert_loop)

    add_executable(test_mci_sequences tests/test_mci_sequences.cpp)
    target_link_libraries(test_mci_sequences PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_mci_sequences COMMAND test_mci_sequences)

    # TASK-24H-1109: deliberately a separate binary from test_mci_sequences --
    # see tests/test_midi_backend_failure.cpp's doc comment for why (it trips
    # a permanent-for-the-process failure latch that would break every other
    # MCI test sharing the same binary).
    add_executable(test_midi_backend_failure tests/test_midi_backend_failure.cpp)
    target_link_libraries(test_midi_backend_failure PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_midi_backend_failure COMMAND test_midi_backend_failure)

    # TASK-24H-1223: FreeApiRunWinMain (src/winmain_bridge.cpp) is the real
    # process-bootstrap bridge both target games launch through, but had
    # zero automated coverage -- test_eggbert_loop/test_planetblupi_loop
    # both bypass it with a hand-written main().
    add_executable(test_winmain_bridge tests/test_winmain_bridge.cpp)
    target_link_libraries(test_winmain_bridge PRIVATE free-api)
    add_test(NAME test_winmain_bridge COMMAND test_winmain_bridge)

    if(NOT WIN32)
        add_executable(test_timeb tests/test_timeb.cpp)
        target_link_libraries(test_timeb PRIVATE freeapi_compat_headers)
        add_test(NAME test_timeb COMMAND test_timeb)
    endif()

    # TASK-24H-0010: LABELS grouping tests by subsystem, for `ctest -L
    # <label>` iteration on one area without running the full suite. Opt-in
    # tooling only -- a plain `ctest` with no -L filter still runs
    # everything, unaffected. Only labels tests that actually exist in this
    # configuration (guards mirror each test's own registration above).
    set_tests_properties(test_winuser_regressions test_input_pipeline
        PROPERTIES LABELS "winuser")
    set_tests_properties(test_gdi_regressions PROPERTIES LABELS "gdi")
    set_tests_properties(test_mci_sequences test_mci_avivideo_regressions
        test_midi_backend_failure test_joystick_regressions
        PROPERTIES LABELS "winmm")
    set_tests_properties(test_timer_regressions PROPERTIES LABELS "winuser;winmm")
    set_tests_properties(test_file_regressions test_file_paths
        PROPERTIES LABELS "file")
    set_tests_properties(test_resources test_loadstring_regressions
        PROPERTIES LABELS "resources")
    set_tests_properties(test_planetblupi_loop test_eggbert_loop
        PROPERTIES LABELS "integration")
    set_tests_properties(basic_test check_no_hardcoded_paths
        check_no_hardcoded_paths_self_test
        check_public_surface_baseline check_public_surface_baseline_self_test
        test_header_compile test_sdl_log_gating test_winmain_bridge
        PROPERTIES LABELS "hygiene")
    if(NOT WIN32)
        set_tests_properties(test_timeb PROPERTIES LABELS "hygiene")
    endif()
    set_tests_properties(
        extract_string_table_fixture_positive_control
        extract_string_table_require_strings_fails_on_missing_rc
        extract_string_table_verify_id_fails_on_text_mismatch
        extract_string_table_used_ids_fails_on_missing_id
        extract_string_table_used_ids_fails_on_duplicate_id
        PROPERTIES LABELS "hygiene")

    # TASK-24H-0506 follow-up (post-session-3 audit item E): when
    # FREE_API_SANITIZE is set, resolve the actual sanitizer runtime .so and
    # auto-LD_PRELOAD it for every real test-binary CTest entry, so a plain
    # `ctest` inside a sanitizer-configured build tree just works -- no more
    # manually resolving `gcc -print-file-name=...` and setting LD_PRELOAD by
    # hand every time (see docs/cmake-options.md's "Sanitizer-instrumented
    # test builds" section for the previously-manual version of this step).
    # Entirely gated behind FREE_API_SANITIZE (off by default): a default,
    # non-sanitized build never touches this block or any test's ENVIRONMENT.
    # Skips the cmake -P-based extract_string_table_* tests (they don't run
    # any sanitized code, and LD_PRELOAD-ing a sanitizer runtime into the
    # `cmake` binary itself has no benefit and only wastes their fast
    # negative-path runtime).
    if(FREE_API_SANITIZE)
        if(FREE_API_SANITIZE STREQUAL "thread")
            set(_free_api_sanitizer_soname "libtsan.so")
        else() # "address"
            set(_free_api_sanitizer_soname "libasan.so")
        endif()
        execute_process(
                COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${_free_api_sanitizer_soname}
                OUTPUT_VARIABLE _free_api_sanitizer_lib
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        get_filename_component(_free_api_sanitizer_lib_real "${_free_api_sanitizer_lib}" REALPATH)
        # TASK-24H-1247: this used to blanket-disable LeakSanitizer for the
        # entire ctest suite (`ASAN_OPTIONS=detect_leaks=0`) on the theory
        # that SDL3 itself holds long-lived global allocations that read as
        # false-positive "leaks" at exit. That meant `ctest` in a
        # FREE_API_SANITIZE=address build tree could never catch a real
        # leak in free-api's own code through the standard, documented
        # workflow -- confirmed to be a real gap, not a hypothetical one: a
        # genuine test-file resource leak (TASK-24H-1229's verification)
        # went undetected through every prior ctest-based ASan pass across
        # multiple sessions, only found by manually running a test binary
        # directly (bypassing this ENVIRONMENT override). Re-verified with
        # detect_leaks=1 across every real test binary in this build
        # (16/16, dummy SDL video/audio drivers) and found zero leaks, real
        # or false-positive, from SDL3 or anything else -- the blanket
        # disable is not needed in this project's actual configuration, so
        # leak detection is left at its default (enabled) rather than
        # speculatively suppressed. If a genuine SDL3 false-positive leak
        # is ever observed in some other configuration, prefer a scoped
        # LSAN_OPTIONS=suppressions=<file> naming SDL3's specific
        # allocation stacks over reintroducing a blanket disable.
        set(_free_api_sanitizer_env "LD_PRELOAD=${_free_api_sanitizer_lib_real}")
        if(EXISTS "${_free_api_sanitizer_lib_real}")
            get_property(_free_api_all_tests DIRECTORY PROPERTY TESTS)
            foreach(_free_api_test_name IN LISTS _free_api_all_tests)
                if(NOT _free_api_test_name MATCHES "^extract_string_table_")
                    set_tests_properties(${_free_api_test_name} PROPERTIES
                            ENVIRONMENT "${_free_api_sanitizer_env}")
                endif()
            endforeach()
            message(STATUS "free-api: FREE_API_SANITIZE=${FREE_API_SANITIZE} -- auto-LD_PRELOADing ${_free_api_sanitizer_lib_real} for all test binaries")
            unset(_free_api_test_name)
            unset(_free_api_all_tests)
        else()
            message(WARNING "free-api: FREE_API_SANITIZE=${FREE_API_SANITIZE} set, but could not resolve ${_free_api_sanitizer_soname} via the compiler -- tests will need LD_PRELOAD set manually (see docs/cmake-options.md)")
        endif()
        unset(_free_api_sanitizer_soname)
        unset(_free_api_sanitizer_lib)
        unset(_free_api_sanitizer_lib_real)
        unset(_free_api_sanitizer_env)
    endif()
endif()

# --- Examples ----------------------------------------------------------------
# Standalone, single-file demonstrations of specific WinAPI-compatible
# functionality (see examples/README.md). Unlike tests, these open a real
# window and expect a real display/audio backend, so they are opt-in and not
# built by default when free-api is consumed as a sibling library by either
# target game.
option(FREE_API_BUILD_EXAMPLES "Build free-api examples" OFF)

if(FREE_API_BUILD_EXAMPLES)
    foreach(_free_api_example IN ITEMS
            01_window_and_message_loop
            02_timers
            03_input_and_cursor
            04_gdi_minimap
            05_midi_playback)
        add_executable(${_free_api_example} examples/${_free_api_example}.cpp)
        target_link_libraries(${_free_api_example} PRIVATE free-api SDL3::SDL3)
    endforeach()
    unset(_free_api_example)
endif()

# TASK-24H-0007: no `install(EXPORT free-api-targets ...)` call exists (and
# none is added here) -- neither target game nor free-direct consumes
# free-api via find_package(); both use add_subdirectory(). The EXPORT
# clause below was previously a no-op (targets were tagged into an export
# set that was never installed, so find_package(free-api CONFIG) could
# never have worked); removed rather than completed, since a real
# find_package()-consumable package (generated Config/ConfigVersion files)
# has no evidenced consumer to justify the added surface.
install(TARGETS free-api freeapi_compat_headers
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# TASK-24H-0007: include_non_windows/ is added to freeapi_compat_headers'
# public include path whenever NOT WIN32 (see the target_include_directories
# call above) -- an installed non-Windows copy needs these headers too, or
# its own build (which depends on them) would fail.
if(NOT WIN32)
    install(DIRECTORY include_non_windows/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()
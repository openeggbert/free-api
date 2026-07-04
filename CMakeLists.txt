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
#   3) As a developer convenience only, reuse one of the two target games'
#      existing SDL3-vendoring CMake helper if that game happens to be
#      checked out next to this repo (no absolute/machine-specific path).
# See docs/cmake-options.md for details.
option(FREE_API_USE_SYSTEM_SDL3
        "Find SDL3/SDL3_image/SDL3_mixer via find_package() instead of a sibling game project's vendoring script"
        OFF)

if(NOT TARGET SDL3::SDL3 AND FREE_API_USE_SYSTEM_SDL3)
    find_package(SDL3 REQUIRED)
    find_package(SDL3_image REQUIRED)
    find_package(SDL3_mixer REQUIRED)
endif()

if(NOT TARGET SDL3::SDL3)
    # Developer convenience only: if free-api is checked out next to one of
    # its two target games, reuse that game's own SDL3 vendoring script
    # instead of duplicating it here. This is optional; it is skipped
    # entirely if neither sibling directory is present.
    foreach(_free_api_sibling IN ITEMS free-eggbert planetblupi)
        set(_free_api_sibling_sdl_cmake
                "${CMAKE_CURRENT_SOURCE_DIR}/../${_free_api_sibling}/cmake/ThirdPartySDL.cmake")
        if(EXISTS "${_free_api_sibling_sdl_cmake}")
            include("${_free_api_sibling_sdl_cmake}")
            configure_vendored_sdl()
            break()
        endif()
    endforeach()
    unset(_free_api_sibling_sdl_cmake)
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
set(_free_api_string_table_rc "")
set(_free_api_string_table_encoding "UTF-8")
set(_free_api_string_table_headers "")

# Prefer detecting which game is ACTUALLY building free-api right now via
# the top-level project name (CMAKE_PROJECT_NAME), not just "does a sibling
# directory happen to exist next to free-api" -- both target games are
# always siblings of each other on a typical development machine, so sibling
# *existence* alone cannot tell planetblupi's own build from free-eggbert's:
# whichever one is the top-level project is unambiguous, since CMake sets
# CMAKE_PROJECT_NAME from the first project() call in the whole configure run.
if(CMAKE_PROJECT_NAME STREQUAL "SPEEDY_BLUPI_WINDOWS")
    # free-eggbert is building us directly; CMAKE_SOURCE_DIR is its own root.
    set(_free_api_string_table_rc "${CMAKE_SOURCE_DIR}/resource/Eggbert2.rc")
    set(_free_api_string_table_encoding "UTF-16LE")
    set(_free_api_string_table_headers "${CMAKE_SOURCE_DIR}/resource/resource.h")
elseif(CMAKE_PROJECT_NAME STREQUAL "PLANET_BLUPI_WINDOWS")
    # planetblupi is building us directly; CMAKE_SOURCE_DIR is its own root.
    set(_free_api_string_table_rc "${CMAKE_SOURCE_DIR}/resource/blupi-e.rc")
    set(_free_api_string_table_encoding "UTF-8")
    set(_free_api_string_table_headers
            "${CMAKE_SOURCE_DIR}/include/resource.h;${CMAKE_SOURCE_DIR}/include/resrc1.h")
else()
    # Standalone free-api build (its own project() call is the top-level
    # one): fall back to a developer-convenience sibling lookup, purely for
    # local iteration -- checked in order, first match wins.
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

set(FREE_API_GENERATED_STRINGS_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/FreeApiGeneratedStrings.cpp")

if(_free_api_string_table_rc)
    execute_process(
            COMMAND ${CMAKE_COMMAND}
                -DOUTPUT_CPP=${FREE_API_GENERATED_STRINGS_CPP}
                -DRC_FILE=${_free_api_string_table_rc}
                -DRC_ENCODING=${_free_api_string_table_encoding}
                "-DRESOURCE_HEADERS=${_free_api_string_table_headers}"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtractStringTable.cmake
            RESULT_VARIABLE _free_api_string_table_result
    )
else()
    execute_process(
            COMMAND ${CMAKE_COMMAND}
                -DOUTPUT_CPP=${FREE_API_GENERATED_STRINGS_CPP}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtractStringTable.cmake
            RESULT_VARIABLE _free_api_string_table_result
    )
endif()

if(NOT _free_api_string_table_result EQUAL 0)
    message(FATAL_ERROR "free-api: ExtractStringTable.cmake failed (result=${_free_api_string_table_result})")
endif()

unset(_free_api_string_table_rc)
unset(_free_api_string_table_encoding)
unset(_free_api_string_table_headers)
unset(_free_api_string_table_result)

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

    add_executable(test_loadstring_regressions tests/test_loadstring_regressions.cpp)
    set_target_properties(test_loadstring_regressions PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_loadstring_regressions PRIVATE free-api)
    add_test(NAME test_loadstring_regressions COMMAND test_loadstring_regressions)

    add_executable(test_mci_avivideo_regressions tests/test_mci_avivideo_regressions.cpp)
    set_target_properties(test_mci_avivideo_regressions PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    target_link_libraries(test_mci_avivideo_regressions PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_mci_avivideo_regressions COMMAND test_mci_avivideo_regressions)

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

install(TARGETS free-api freeapi_compat_headers
        EXPORT free-api-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
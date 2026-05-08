cmake_minimum_required(VERSION 3.23)

project(free-api
    VERSION 0.1.0
    DESCRIPTION "Minimal WinAPI reimplementation in C++"
    LANGUAGES CXX)

include(GNUInstallDirs)

if(NOT TARGET SDL3::SDL3 OR NOT TARGET SDL3_image::SDL3_image OR NOT TARGET SDL3_mixer::SDL3_mixer)
    message(FATAL_ERROR
        "SDL targets are missing. Configure vendored SDL first (see free-eggbert/cmake/ThirdPartySDL.cmake), "
        "or provide SDL targets from the parent project.")
endif()

add_library(free-api STATIC)
add_library(free-api::free-api ALIAS free-api)

set_target_properties(free-api PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

target_include_directories(free-api PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(NOT WIN32)
    target_include_directories(free-api PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include_non_windows
    )
endif()
target_link_libraries(free-api PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer)

target_sources(free-api PRIVATE
    # Legacy stub (intentionally empty — content split into modules below)
    src/winapi.cpp

    # Internal modules
    src/internal/FreeApiDiagnostics.cpp
    src/internal/FreeApiGdi.cpp
    src/internal/FreeApiWindowRegistry.cpp
    src/internal/FreeApiMessageQueue.cpp
    src/internal/FreeApiTimers.cpp
    src/internal/FreeApiPath.cpp
    src/internal/FreeApiSdlVideo.cpp

    # WinBase
    src/winbase.cpp
    src/winbase_file.cpp

    # WinUser
    src/winuser_window.cpp
    src/winuser_message.cpp
    src/winuser_cursor.cpp
    src/winuser_timer.cpp
    src/winuser_misc.cpp

    # WinGDI
    src/wingdi_bitmap.cpp
    src/wingdi_dc.cpp
    src/wingdi_blit.cpp
    src/wingdi_misc.cpp

    # WinMM
    src/winmm.cpp

    # CRT compatibility
    src/crt_io.cpp

    # Startup bridge
    src/winmain_bridge.cpp

    # MIDI backend
    src/MidiMusic.cpp)

target_include_directories(free-api PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_include_directories(free-api PRIVATE external)

option(FREE_API_BUILD_TESTS "Build free-api tests" ON)

if(FREE_API_BUILD_TESTS)
    enable_testing()
    add_executable(basic_test tests/basic_test.cpp)
    target_link_libraries(basic_test PRIVATE free-api)
    add_test(NAME basic_test COMMAND basic_test)

    # End-to-end input pipeline test: injects SDL events and verifies WinAPI translation
    add_executable(test_input_pipeline tests/test_input_pipeline.cpp)
    set_target_properties(test_input_pipeline PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES)
    target_link_libraries(test_input_pipeline PRIVATE free-api SDL3::SDL3)
    add_test(NAME test_input_pipeline COMMAND test_input_pipeline)
endif()

install(TARGETS free-api
        EXPORT free-api-targets
        DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
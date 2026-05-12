cmake_minimum_required(VERSION 3.23)

project(free-api
        VERSION 0.1.0
        DESCRIPTION "Minimal WinAPI reimplementation in C++"
        LANGUAGES CXX)

include(GNUInstallDirs)

if(NOT TARGET SDL3::SDL3)
    include("/rv/data/development/github.com/openeggbert/free-eggbert/cmake/ThirdPartySDL.cmake")
    configure_vendored_sdl()
endif()

if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR "Missing SDL3::SDL3 target even after configure_vendored_sdl().")
endif()

if(NOT TARGET SDL3_image::SDL3_image)
    message(FATAL_ERROR "Missing SDL3_image::SDL3_image target even after configure_vendored_sdl().")
endif()

if(NOT TARGET SDL3_mixer::SDL3_mixer)
    message(FATAL_ERROR "Missing SDL3_mixer::SDL3_mixer target even after configure_vendored_sdl().")
endif()

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
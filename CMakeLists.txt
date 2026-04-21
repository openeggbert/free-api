cmake_minimum_required(VERSION 3.23)

project(free-api
    VERSION 0.1.0
    DESCRIPTION "Minimal WinAPI reimplementation in C++"
    LANGUAGES CXX)

include(GNUInstallDirs)

add_library(free-api)
add_library(free-api::free-api ALIAS free-api)

set_target_properties(free-api PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

target_include_directories(free-api PUBLIC include)

target_sources(free-api PRIVATE src/winapi.cpp)

enable_testing()
add_executable(basic_test tests/basic_test.cpp)
target_link_libraries(basic_test PRIVATE free-api)
add_test(NAME basic_test COMMAND basic_test)

install(TARGETS free-api
        EXPORT free-api-targets
        DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
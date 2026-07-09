# TASK-0016 (plan.md): a lightweight fresh-clone smoke check preventing a
# regression of the exact bug TASK-0011 fixed -- a hardcoded, machine-
# specific absolute path silently breaking every clone but the original
# author's. Run via `cmake -P cmake/CheckNoHardcodedPaths.cmake` from the
# repo root, or through the `check_no_hardcoded_paths` CTest test.
#
# Deliberately narrow: this greps CMakeLists.txt/cmake/*.cmake text for a
# short list of suspicious absolute-path prefixes. It is not a general
# static-analysis tool.

# TASK-24H-0011: the actual detection logic lives in this function, not
# inline in the script body below, so cmake/CheckNoHardcodedPathsSelfTest.cmake
# can `include()` this file and exercise the real pattern list/matching
# logic against a controlled temp file -- not a duplicated copy that could
# silently drift out of sync with the real thing. Deliberately does not
# call message(FATAL_ERROR) itself, so it's safely reusable/testable
# without terminating the calling `cmake -P` process.
function(free_api_check_no_hardcoded_paths files_to_check out_found_issue)
    set(_suspicious_patterns
        "/home/"
        "/rv/"
        "/Users/"
        "C:\\\\"
    )

    set(_found_issue FALSE)
    foreach(_file IN LISTS files_to_check)
        file(STRINGS "${_file}" _lines)
        foreach(_line IN LISTS _lines)
            foreach(_pattern IN LISTS _suspicious_patterns)
                if(_line MATCHES "${_pattern}")
                    message(WARNING "CheckNoHardcodedPaths: suspicious absolute path in ${_file}: ${_line}")
                    set(_found_issue TRUE)
                endif()
            endforeach()
        endforeach()
    endforeach()

    set(${out_found_issue} ${_found_issue} PARENT_SCOPE)
endfunction()

# Only run the real repo-wide check when this file is invoked directly
# (`cmake -P`), not when it's `include()`d by the self-test above.
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    set(_free_api_root "${CMAKE_CURRENT_LIST_DIR}/..")
    set(_files_to_check "${_free_api_root}/CMakeLists.txt")
    file(GLOB _cmake_scripts "${_free_api_root}/cmake/*.cmake")
    foreach(_script IN LISTS _cmake_scripts)
        get_filename_component(_script_name "${_script}" NAME)
        # This check script (and its self-test) are themselves under
        # cmake/ -- exclude them from the scan (they necessarily contain
        # these patterns as string literals).
        if(NOT _script_name STREQUAL "CheckNoHardcodedPaths.cmake" AND
           NOT _script_name STREQUAL "CheckNoHardcodedPathsSelfTest.cmake")
            list(APPEND _files_to_check "${_script}")
        endif()
    endforeach()

    free_api_check_no_hardcoded_paths("${_files_to_check}" _found_issue)

    if(_found_issue)
        message(FATAL_ERROR "CheckNoHardcodedPaths: one or more machine-specific absolute paths found -- see warnings above (this is the exact class of bug TASK-0011 fixed).")
    else()
        message(STATUS "CheckNoHardcodedPaths: no suspicious absolute paths found.")
    endif()
endif()

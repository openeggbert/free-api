# TASK-24H-0011: proves cmake/CheckNoHardcodedPaths.cmake's own detection
# logic (free_api_check_no_hardcoded_paths(), _suspicious_patterns) still
# actually fires on genuinely bad input, and does NOT fire on clean input --
# the real `check_no_hardcoded_paths` test only ever exercises the "nothing
# found" branch against the current, already-clean repo, so a future edit
# that narrows or breaks the pattern list (e.g. an accidental regex-escaping
# change) would otherwise pass that test forever.
#
# Run via `cmake -DOUTPUT_DIR=<dir> -DCHECKER_SCRIPT=<path> -P
# cmake/CheckNoHardcodedPathsSelfTest.cmake`, or through the
# `check_no_hardcoded_paths_self_test` CTest test.

if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "CheckNoHardcodedPathsSelfTest: OUTPUT_DIR not set")
endif()
if(NOT DEFINED CHECKER_SCRIPT)
    message(FATAL_ERROR "CheckNoHardcodedPathsSelfTest: CHECKER_SCRIPT not set")
endif()

# Pulls in the real, current free_api_check_no_hardcoded_paths() function --
# not a duplicated copy of its pattern list/matching logic.
include("${CHECKER_SCRIPT}")

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(_bad_fixture "${OUTPUT_DIR}/self-test-bad.cmake")
file(WRITE "${_bad_fixture}" "# set(_x \"/home/someuser/free-api\")\n")

set(_clean_fixture "${OUTPUT_DIR}/self-test-clean.cmake")
file(WRITE "${_clean_fixture}" "# set(_x \"a perfectly ordinary relative path, cmake/foo.cmake\")\n")

free_api_check_no_hardcoded_paths("${_bad_fixture}" _bad_found_issue)
if(NOT _bad_found_issue)
    message(FATAL_ERROR "CheckNoHardcodedPathsSelfTest: FAILED -- a known-bad fixture containing '/home/' was NOT flagged. The checker's pattern list or matching logic is broken.")
endif()

free_api_check_no_hardcoded_paths("${_clean_fixture}" _clean_found_issue)
if(_clean_found_issue)
    message(FATAL_ERROR "CheckNoHardcodedPathsSelfTest: FAILED -- a clean fixture with no suspicious patterns WAS flagged (false positive).")
endif()

file(REMOVE "${_bad_fixture}" "${_clean_fixture}")

message(STATUS "CheckNoHardcodedPathsSelfTest: PASSED -- the checker correctly flags a known-bad pattern and does not flag clean input.")

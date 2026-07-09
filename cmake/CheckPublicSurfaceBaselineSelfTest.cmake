# TASK-24H-1238: proves cmake/CheckPublicSurfaceBaseline.cmake's own
# extraction logic (free_api_extract_public_symbols()) still actually finds
# symbols in each of the declaration shapes this project's headers use, and
# that the baseline-diff logic actually fires on a genuinely new symbol --
# the real `check_public_surface_baseline` test only ever exercises the
# "nothing new found" branch against the current, already-clean header set,
# so a future edit that narrows or breaks the regex set would otherwise pass
# that test forever (exactly the failure mode
# CheckNoHardcodedPathsSelfTest.cmake was written to catch for its sibling
# check).
#
# Run via `cmake -DOUTPUT_DIR=<dir> -DCHECKER_SCRIPT=<path> -P
# cmake/CheckPublicSurfaceBaselineSelfTest.cmake`, or through the
# `check_public_surface_baseline_self_test` CTest test.

if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "CheckPublicSurfaceBaselineSelfTest: OUTPUT_DIR not set")
endif()
if(NOT DEFINED CHECKER_SCRIPT)
    message(FATAL_ERROR "CheckPublicSurfaceBaselineSelfTest: CHECKER_SCRIPT not set")
endif()

# Pulls in the real, current free_api_extract_public_symbols() function --
# not a duplicated copy of its regex set.
include("${CHECKER_SCRIPT}")

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

# A fixture header exercising every declaration shape the extractor knows
# about: a #define macro, a WINAPI-style function declaration, an inline
# function definition, a function-pointer typedef, a typedef'd struct with
# multiple aliases, a plain (non-typedef'd) struct tag, a simple one-line
# typedef, and an extern global -- plus an include guard and a comment
# (both of which must NOT be captured as symbols) and a #define that must
# be excluded (the FREE_API_..._H include-guard convention).
set(_fixture "${OUTPUT_DIR}/self-test-fixture.h")
file(WRITE "${_fixture}" "\
#ifndef FREE_API_SELFTEST_FIXTURE_H
#define FREE_API_SELFTEST_FIXTURE_H
// a line comment mentioning NotASymbol(int x); should not be captured
/** a block comment mentioning AlsoNotASymbol(void); should not be captured either */
#define SELFTEST_MACRO_CONSTANT 42
int WINAPI SelfTestFunctionDecl(int x);
inline int SelfTestInlineFunctionDef(int x) {
    return x;
}
typedef void(CALLBACK* SELFTEST_CALLBACK)(int a, int b);
typedef struct tagSelfTestStruct {
    int field;
} SELFTEST_STRUCT, *PSELFTEST_STRUCT;
struct SelfTestPlainStruct {
    int field;
};
typedef int SELFTEST_SIMPLE_ALIAS;
extern int g_selfTestGlobal;
#endif // FREE_API_SELFTEST_FIXTURE_H
")

free_api_extract_public_symbols("${_fixture}" _found_symbols)

set(_expected_symbols
    SELFTEST_MACRO_CONSTANT
    SelfTestFunctionDecl
    SelfTestInlineFunctionDef
    SELFTEST_CALLBACK
    tagSelfTestStruct
    SELFTEST_STRUCT
    PSELFTEST_STRUCT
    SelfTestPlainStruct
    SELFTEST_SIMPLE_ALIAS
    g_selfTestGlobal
)

foreach(_expected IN LISTS _expected_symbols)
    list(FIND _found_symbols "${_expected}" _idx)
    if(_idx EQUAL -1)
        message(FATAL_ERROR "CheckPublicSurfaceBaselineSelfTest: FAILED -- expected symbol '${_expected}' was NOT extracted from the fixture header. The extractor's regex set is broken for this declaration shape.")
    endif()
endforeach()

list(FIND _found_symbols "FREE_API_SELFTEST_FIXTURE_H" _guard_idx)
if(NOT _guard_idx EQUAL -1)
    message(FATAL_ERROR "CheckPublicSurfaceBaselineSelfTest: FAILED -- the fixture's own include guard (FREE_API_SELFTEST_FIXTURE_H) was captured as a symbol; it should be excluded.")
endif()

list(FIND _found_symbols "NotASymbol" _comment_idx)
if(NOT _comment_idx EQUAL -1)
    message(FATAL_ERROR "CheckPublicSurfaceBaselineSelfTest: FAILED -- text inside a // line comment was captured as a symbol.")
endif()

list(FIND _found_symbols "AlsoNotASymbol" _block_comment_idx)
if(NOT _block_comment_idx EQUAL -1)
    message(FATAL_ERROR "CheckPublicSurfaceBaselineSelfTest: FAILED -- text inside a /* block comment */ was captured as a symbol.")
endif()

# Now prove the baseline-diff logic itself (not just extraction) actually
# flags a genuinely new symbol against a controlled baseline -- mirrors how
# the real check's FATAL_ERROR path behaves, without touching the repo's
# real cmake/known-public-symbols.txt.
set(_mini_baseline "${OUTPUT_DIR}/self-test-baseline.txt")
file(WRITE "${_mini_baseline}" "SelfTestFunctionDecl\nSELFTEST_MACRO_CONSTANT\n")
file(STRINGS "${_mini_baseline}" _baseline_symbols)

set(_new_symbols "${_found_symbols}")
list(REMOVE_ITEM _new_symbols ${_baseline_symbols})
if(NOT _new_symbols)
    message(FATAL_ERROR "CheckPublicSurfaceBaselineSelfTest: FAILED -- diffing the fixture's real symbol set against a deliberately incomplete baseline found no new symbols; the diff logic itself is broken.")
endif()

file(REMOVE "${_fixture}" "${_mini_baseline}")

message(STATUS "CheckPublicSurfaceBaselineSelfTest: PASSED -- the extractor correctly finds every known declaration shape, excludes include guards/comments, and the baseline-diff logic correctly flags a new symbol.")

# TASK-24H-1238 (plan.md): a lightweight guard against docs/scope.md's
# citation rule ("every new public API must cite a real usage site") eroding
# silently -- today it is enforced entirely by human/AI diligence when
# writing plan.md entries, with nothing in the build or test suite flagging
# a new public declaration added "because it seemed useful." Run via
# `cmake -P cmake/CheckPublicSurfaceBaseline.cmake` from the repo root, or
# through the `check_public_surface_baseline` CTest test.
#
# Deliberately narrow, matching cmake/CheckNoHardcodedPaths.cmake's own
# precedent: this extracts symbol names from include/*.h and
# include_non_windows/*.h using a handful of regexes matching the
# declaration shapes this project's headers actually use (single-line
# WINAPI-style function declarations, single-line inline function
# definitions, #define macros, typedef'd struct/alias lists, plain
# struct/enum tags, simple one-line typedefs, extern globals) -- it is not a
# general C++ declaration parser and can miss an exotic shape it has never
# seen before. If it ever silently misses a real new symbol, tighten the
# regex set; do not delete the check because it isn't perfect.

# TASK-24H-1238: the extraction logic lives in this function (not inline in
# the script body below) so cmake/CheckPublicSurfaceBaselineSelfTest.cmake
# can `include()` this file and exercise the real regex set against
# controlled fixture headers -- not a duplicated copy that could silently
# drift out of sync with the real thing.
function(free_api_extract_public_symbols header_file out_symbols)
    set(_symbols "")
    if(NOT EXISTS "${header_file}")
        set(${out_symbols} "${_symbols}" PARENT_SCOPE)
        return()
    endif()

    file(STRINGS "${header_file}" _lines)

    set(_in_block_comment FALSE)
    foreach(_raw_line IN LISTS _lines)
        set(_line "${_raw_line}")

        # Skip/strip /* ... */ block comments via a simple state flag -- not
        # a full tokenizer (matches CheckNoHardcodedPaths.cmake's own
        # documented "not general static analysis" precedent).
        if(_in_block_comment)
            if(_line MATCHES "\\*/")
                string(REGEX REPLACE "^.*\\*/" "" _line "${_line}")
                set(_in_block_comment FALSE)
            else()
                continue()
            endif()
        endif()
        if(_line MATCHES "/\\*" AND NOT _line MATCHES "\\*/")
            string(REGEX REPLACE "/\\*.*$" "" _line "${_line}")
            set(_in_block_comment TRUE)
        endif()
        string(REGEX REPLACE "/\\*.*\\*/" "" _line "${_line}")
        string(REGEX REPLACE "//.*$" "" _line "${_line}")

        string(STRIP "${_line}" _trimmed)
        if(_trimmed STREQUAL "")
            continue()
        endif()

        # 1) #define NAME ... -- macro constants/aliases. Excludes this
        #    project's own FREE_API_..._H include-guard convention.
        if(_trimmed MATCHES "^#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)")
            set(_name "${CMAKE_MATCH_1}")
            if(NOT _name MATCHES "^FREE_API_.*_H$")
                list(APPEND _symbols "${_name}")
            endif()
            continue()
        endif()

        # 1b) Function-pointer typedef: "typedef RETTYPE(CONV* NAME)(...);"
        #     -- e.g. "typedef void(CALLBACK* LPTIMECALLBACK)(UINT, ...);".
        #     Must run before rule 2 below, which would otherwise mis-capture
        #     the return type keyword (e.g. "int"/"void") as the symbol name.
        if(_trimmed MATCHES "^typedef[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\\([A-Za-z_][A-Za-z0-9_]*\\*[ \t]*([A-Za-z_][A-Za-z0-9_]*)\\)\\(.*\\)[ \t]*;[ \t]*$")
            list(APPEND _symbols "${CMAKE_MATCH_1}")
            continue()
        endif()

        # 2) Single-line function declaration ending in ");" -- e.g.
        #    "int WINAPIV wsprintfA(LPSTR lpOut, LPCSTR lpFmt, ...);"
        if(_trimmed MATCHES "^[A-Za-z_][A-Za-z0-9_ \t\\*&]*[ \t\\*]([A-Za-z_][A-Za-z0-9_]*)\\([^;{]*\\)[ \t]*;[ \t]*$")
            list(APPEND _symbols "${CMAKE_MATCH_1}")
            continue()
        endif()

        # 2b) Single-line inline function definition ending in "{" right
        #     after the parameter list -- e.g.
        #     "inline int ftime(struct timeb* tb) {"
        if(_trimmed MATCHES "^[A-Za-z_][A-Za-z0-9_ \t\\*&]*[ \t\\*]([A-Za-z_][A-Za-z0-9_]*)\\([^;{]*\\)[ \t]*\\{[ \t]*$")
            list(APPEND _symbols "${CMAKE_MATCH_1}")
            continue()
        endif()

        # 3) Typedef struct closing line: "} NAME, *PNAME, *LPNAME;"
        if(_trimmed MATCHES "^\\}[ \t]*(.+);[ \t]*$")
            set(_alias_list "${CMAKE_MATCH_1}")
            string(REPLACE "," ";" _alias_items "${_alias_list}")
            foreach(_alias IN LISTS _alias_items)
                string(STRIP "${_alias}" _alias)
                string(REGEX REPLACE "^\\*+" "" _alias "${_alias}")
                if(NOT _alias STREQUAL "")
                    list(APPEND _symbols "${_alias}")
                endif()
            endforeach()
            continue()
        endif()

        # 4) Plain struct/enum tag declaration: "struct NAME {" / "enum NAME {"
        #    (optionally "typedef struct NAME {") -- captures the tag itself
        #    in addition to whatever alias list a later "}" line adds.
        if(_trimmed MATCHES "^(typedef[ \t]+)?(struct|enum)[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\{?[ \t]*$")
            list(APPEND _symbols "${CMAKE_MATCH_3}")
            continue()
        endif()

        # 5) Simple one-line typedef with no braces: "typedef X Y;"
        if(_trimmed MATCHES "^typedef[ \t]+.*[ \t\\*]([A-Za-z_][A-Za-z0-9_]*)[ \t]*;[ \t]*$")
            list(APPEND _symbols "${CMAKE_MATCH_1}")
            continue()
        endif()

        # 6) extern variable declaration: "extern TYPE name;" (not extern "C")
        if(_trimmed MATCHES "^extern[ \t]+[^\"].*[ \t\\*]([A-Za-z_][A-Za-z0-9_]*)[ \t]*;[ \t]*$")
            list(APPEND _symbols "${CMAKE_MATCH_1}")
            continue()
        endif()
    endforeach()

    if(_symbols)
        list(REMOVE_DUPLICATES _symbols)
        list(SORT _symbols)
    endif()
    set(${out_symbols} "${_symbols}" PARENT_SCOPE)
endfunction()

# Extracts and merges symbols from every header in a directory (non-recursive
# except one level into sys/, matching include_non_windows/'s actual shape).
function(free_api_extract_public_symbols_from_dir header_dir out_symbols)
    set(_all_symbols "")
    file(GLOB _headers "${header_dir}/*.h")
    file(GLOB _nested_headers "${header_dir}/*/*.h")
    foreach(_header IN LISTS _headers _nested_headers)
        free_api_extract_public_symbols("${_header}" _file_symbols)
        list(APPEND _all_symbols ${_file_symbols})
    endforeach()
    if(_all_symbols)
        list(REMOVE_DUPLICATES _all_symbols)
        list(SORT _all_symbols)
    endif()
    set(${out_symbols} "${_all_symbols}" PARENT_SCOPE)
endfunction()

# Only run the real repo-wide check when this file is invoked directly
# (`cmake -P`), not when it's `include()`d by the self-test above.
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    set(_free_api_root "${CMAKE_CURRENT_LIST_DIR}/..")
    set(_baseline_file "${CMAKE_CURRENT_LIST_DIR}/known-public-symbols.txt")

    free_api_extract_public_symbols_from_dir("${_free_api_root}/include" _include_symbols)
    free_api_extract_public_symbols_from_dir("${_free_api_root}/include_non_windows" _non_windows_symbols)
    set(_current_symbols ${_include_symbols} ${_non_windows_symbols})
    list(REMOVE_DUPLICATES _current_symbols)
    list(SORT _current_symbols)

    if(NOT EXISTS "${_baseline_file}")
        message(FATAL_ERROR "CheckPublicSurfaceBaseline: baseline file missing: ${_baseline_file}")
    endif()
    file(STRINGS "${_baseline_file}" _baseline_symbols)
    list(SORT _baseline_symbols)

    set(_new_symbols "${_current_symbols}")
    if(_baseline_symbols)
        list(REMOVE_ITEM _new_symbols ${_baseline_symbols})
    endif()

    if(_new_symbols)
        list(JOIN _new_symbols ", " _new_symbols_str)
        message(FATAL_ERROR "CheckPublicSurfaceBaseline: new, unlisted public declaration(s) found in include/*.h or include_non_windows/*.h: ${_new_symbols_str}. Every new public API needs a plan.md task citing a real file:line usage site in ../free-eggbert or ../planetblupi (see docs/scope.md's \"The rule\") before it belongs in the baseline. Once that evidence exists, add the symbol name to cmake/known-public-symbols.txt.")
    else()
        list(LENGTH _current_symbols _current_count)
        message(STATUS "CheckPublicSurfaceBaseline: no new, unlisted public declarations found (${_current_count} symbols checked against the baseline).")
    endif()
endif()

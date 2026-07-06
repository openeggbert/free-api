# Narrow, single-purpose extractor for Win32 STRINGTABLE blocks inside a
# classic .rc resource script -- this is NOT a general .rc/.res compiler.
# It only understands lines shaped like:
#
#   SYMBOL   "quoted text"
#
# inside STRINGTABLE ... BEGIN ... END blocks, and resolves SYMBOL to a
# numeric ID via one or more "#define SYMBOL <number>" C/C++ headers.
# Everything else in the .rc file (icons, dialogs, version info, menus,
# bitmaps, ...) is ignored entirely.
#
# Required variable:
#   OUTPUT_CPP        - path to the generated .cpp file to write.
#
# Optional variables (RC_FILE and RESOURCE_HEADERS must both be set to
# extract real data; if either is unset/missing, an empty table is written
# so LoadStringA falls back to its placeholder behavior):
#   RC_FILE           - path to the .rc file containing STRINGTABLE blocks.
#   RC_ENCODING       - encoding of RC_FILE: "UTF-8" or "UTF-16LE" (default UTF-8).
#   RESOURCE_HEADERS  - list of headers with "#define SYMBOL <number>" lines
#                       resolving RC_FILE's STRINGTABLE symbols.
#
# Optional hardening variables, used only by target-game builds (free-eggbert,
# planetblupi) so a broken/missing .rc file fails CMake configure loudly
# instead of silently degrading to an empty table -- standalone free-api
# builds never set these and keep the placeholder-fallback behavior above:
#   REQUIRE_STRINGS   - if set/true, a missing RC_FILE or zero extracted
#                       strings is a FATAL_ERROR instead of a WARNING.
#   TARGET_GAME_NAME  - human-readable game name, used only in error text.
#   VERIFY_ID         - a known numeric string ID that must be present in the
#                       extracted table (known-ID regression check).
#   VERIFY_TEXT       - if VERIFY_ID is set, the exact text it must resolve
#                       to; mismatch is a FATAL_ERROR.
#   USED_IDS_FILE     - path to a manifest file (see cmake/used-string-ids/)
#                       listing every numeric STRINGTABLE ID a target game
#                       actually calls LoadString(A)/LoadString with, proven
#                       by source evidence (docs/used-string-ids.md). One
#                       bare integer or inclusive "A-B" range per line;
#                       '#' comments and blank lines ignored. Every ID it
#                       lists must be present in this run's extracted table,
#                       or configure fails loudly listing the missing IDs --
#                       this is what catches a *specific* used string going
#                       missing, not just "the whole table came out empty"
#                       (which REQUIRE_STRINGS/VERIFY_ID already catch).

if(NOT DEFINED OUTPUT_CPP)
    message(FATAL_ERROR "ExtractStringTable.cmake: OUTPUT_CPP is required")
endif()

if(NOT DEFINED REQUIRE_STRINGS)
    set(REQUIRE_STRINGS FALSE)
endif()

set(entries "")
set(extracted_ids "")
set(_verify_found FALSE)
set(_verify_actual "")

if(DEFINED RC_FILE AND DEFINED RESOURCE_HEADERS)
    if(NOT EXISTS "${RC_FILE}")
        if(REQUIRE_STRINGS)
            message(FATAL_ERROR "ExtractStringTable.cmake: required .rc file not found for target game '${TARGET_GAME_NAME}': ${RC_FILE}")
        endif()
        message(WARNING "ExtractStringTable.cmake: RC_FILE not found: ${RC_FILE} -- generating an empty string table")
    else()
        # Resolve symbol -> numeric id from all given resource headers.
        set(symbol_names "")
        set(symbol_values "")
        foreach(header IN LISTS RESOURCE_HEADERS)
            if(EXISTS "${header}")
                file(STRINGS "${header}" header_lines ENCODING UTF-8)
                foreach(hline IN LISTS header_lines)
                    if(hline MATCHES "^#define[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]+([0-9]+)[ \t]*$")
                        list(APPEND symbol_names "${CMAKE_MATCH_1}")
                        list(APPEND symbol_values "${CMAKE_MATCH_2}")
                    endif()
                endforeach()
            endif()
        endforeach()

        if(NOT RC_ENCODING)
            set(RC_ENCODING "UTF-8")
        endif()
        file(STRINGS "${RC_FILE}" rc_lines ENCODING ${RC_ENCODING})

        set(in_table FALSE)
        foreach(rline IN LISTS rc_lines)
            string(REGEX REPLACE "\t" " " normalized "${rline}")
            string(STRIP "${normalized}" trimmed)

            if(NOT in_table)
                if(trimmed STREQUAL "STRINGTABLE" OR trimmed MATCHES "^STRINGTABLE[ ]")
                    set(in_table TRUE)
                endif()
                continue()
            endif()

            if(trimmed STREQUAL "BEGIN")
                continue()
            endif()
            if(trimmed STREQUAL "END")
                set(in_table FALSE)
                continue()
            endif()

            # This parser only understands plain ANSI "..." string literals.
            # An L"..." wide-string entry would silently fail to match below
            # and be dropped with no explanation -- warn instead.
            if(trimmed MATCHES "^([A-Za-z_][A-Za-z0-9_]*)[ \t]+L\"")
                message(WARNING "ExtractStringTable.cmake: ${RC_FILE}: unsupported L\"...\" wide-string STRINGTABLE entry ignored (ANSI-only parser): ${trimmed}")
                continue()
            endif()

            if(trimmed MATCHES "^([A-Za-z_][A-Za-z0-9_]*) +\"(.*)\"$")
                set(sym "${CMAKE_MATCH_1}")
                set(text "${CMAKE_MATCH_2}")

                # An embedded escaped double-quote (\") within the string text
                # is not distinguished from the closing quote by the regex
                # above, and could silently mis-parse the entry -- warn.
                string(FIND "${text}" "\\\"" escaped_quote_idx)
                if(NOT escaped_quote_idx EQUAL -1)
                    message(WARNING "ExtractStringTable.cmake: ${RC_FILE}: STRINGTABLE entry '${sym}' contains an embedded escaped double-quote, which this ANSI-only parser may mis-parse: ${trimmed}")
                endif()

                list(FIND symbol_names "${sym}" sym_idx)
                if(sym_idx GREATER -1)
                    list(GET symbol_values ${sym_idx} numeric_id)
                    # Escape a literal double-quote; leave existing C-style
                    # escapes (e.g. "\n") untouched so they keep their
                    # meaning in the generated C++ string literal.
                    string(REPLACE "\"" "\\\"" text_escaped "${text}")
                    list(APPEND entries "    {${numeric_id}u, \"${text_escaped}\"},")
                    list(APPEND extracted_ids "${numeric_id}")

                    if(DEFINED VERIFY_ID AND numeric_id STREQUAL "${VERIFY_ID}")
                        set(_verify_found TRUE)
                        set(_verify_actual "${text}")
                    endif()
                endif()
            endif()
        endforeach()
    endif()
endif()

list(LENGTH entries entry_count)

if(REQUIRE_STRINGS AND EXISTS "${RC_FILE}" AND entry_count EQUAL 0)
    message(FATAL_ERROR "ExtractStringTable.cmake: zero strings extracted from ${RC_FILE} for target game '${TARGET_GAME_NAME}' -- STRINGTABLE parsing is broken or the file no longer contains STRINGTABLE data")
endif()

if(DEFINED VERIFY_ID)
    if(NOT _verify_found)
        message(FATAL_ERROR "ExtractStringTable.cmake: known-ID verification failed for target game '${TARGET_GAME_NAME}': ID ${VERIFY_ID} was not found in the table extracted from ${RC_FILE}")
    elseif(DEFINED VERIFY_TEXT AND NOT _verify_actual STREQUAL "${VERIFY_TEXT}")
        message(FATAL_ERROR "ExtractStringTable.cmake: known-ID verification failed for target game '${TARGET_GAME_NAME}': ID ${VERIFY_ID} resolved to \"${_verify_actual}\", expected \"${VERIFY_TEXT}\"")
    else()
        message(STATUS "ExtractStringTable.cmake: known-ID verification passed for '${TARGET_GAME_NAME}': ID ${VERIFY_ID} -> \"${_verify_actual}\"")
    endif()
endif()

if(DEFINED USED_IDS_FILE)
    if(NOT EXISTS "${USED_IDS_FILE}")
        message(FATAL_ERROR "ExtractStringTable.cmake: USED_IDS_FILE not found for target game '${TARGET_GAME_NAME}': ${USED_IDS_FILE}")
    endif()

    file(STRINGS "${USED_IDS_FILE}" _used_ids_lines ENCODING UTF-8)
    set(_used_ids_missing "")
    set(_used_ids_checked 0)
    foreach(_used_line IN LISTS _used_ids_lines)
        # Strip a trailing '#...' comment (the whole line if it starts with
        # one), then surrounding whitespace; skip what's left over if blank.
        string(REGEX REPLACE "#.*$" "" _used_line "${_used_line}")
        string(STRIP "${_used_line}" _used_line)
        if(_used_line STREQUAL "")
            continue()
        endif()

        if(_used_line MATCHES "^([0-9]+)-([0-9]+)$")
            set(_range_lo "${CMAKE_MATCH_1}")
            set(_range_hi "${CMAKE_MATCH_2}")
            foreach(_used_id RANGE ${_range_lo} ${_range_hi})
                math(EXPR _used_ids_checked "${_used_ids_checked}+1")
                list(FIND extracted_ids "${_used_id}" _used_id_idx)
                if(_used_id_idx EQUAL -1)
                    list(APPEND _used_ids_missing "${_used_id}")
                endif()
            endforeach()
        elseif(_used_line MATCHES "^[0-9]+$")
            math(EXPR _used_ids_checked "${_used_ids_checked}+1")
            list(FIND extracted_ids "${_used_line}" _used_id_idx)
            if(_used_id_idx EQUAL -1)
                list(APPEND _used_ids_missing "${_used_line}")
            endif()
        else()
            message(FATAL_ERROR "ExtractStringTable.cmake: USED_IDS_FILE ${USED_IDS_FILE}: unparseable line (expected a bare integer or 'A-B' range): ${_used_line}")
        endif()
    endforeach()

    if(_used_ids_missing)
        list(LENGTH _used_ids_missing _used_ids_missing_count)
        message(FATAL_ERROR "ExtractStringTable.cmake: ${_used_ids_missing_count} of ${_used_ids_checked} used string ID(s) for target game '${TARGET_GAME_NAME}' (per ${USED_IDS_FILE}) are MISSING from the table extracted from ${RC_FILE} -- these would silently fall back to the \"RES_<id>\" placeholder at runtime: ${_used_ids_missing}")
    else()
        message(STATUS "ExtractStringTable.cmake: all ${_used_ids_checked} used string ID(s) for '${TARGET_GAME_NAME}' verified present (${USED_IDS_FILE})")
    endif()
endif()

set(content "// Auto-generated by cmake/ExtractStringTable.cmake. Do not edit by hand.\n")
string(APPEND content "// Extracted from: ${RC_FILE}\n")
string(APPEND content "#include \"internal/FreeApiGeneratedStrings.hpp\"\n\n")
string(APPEND content "namespace FreeApi::Internal {\n\n")
string(APPEND content "const GeneratedStringEntry g_generatedStringTable[] = {\n")
foreach(entry IN LISTS entries)
    string(APPEND content "${entry}\n")
endforeach()
if(entry_count EQUAL 0)
    string(APPEND content "    {0u, nullptr},\n")
endif()
string(APPEND content "};\n\n")
string(APPEND content "const std::size_t g_generatedStringTableCount = ${entry_count};\n\n")
string(APPEND content "} // namespace FreeApi::Internal\n")

file(WRITE "${OUTPUT_CPP}" "${content}")

message(STATUS "ExtractStringTable.cmake: wrote ${entry_count} string(s) to ${OUTPUT_CPP}")

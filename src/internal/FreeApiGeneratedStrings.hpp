#pragma once

#include <cstddef>

namespace FreeApi::Internal {

/**
 * @brief One resolved STRINGTABLE entry: a numeric resource ID and its text.
 *
 * Populated by a generated .cpp produced at CMake configure time by
 * cmake/ExtractStringTable.cmake from whichever target game (../free-eggbert
 * or ../planetblupi) is building free-api as a sibling. In a standalone
 * build with neither sibling present, the generated table is empty and
 * LoadStringA falls back to its placeholder behavior.
 */
struct GeneratedStringEntry {
    unsigned int id;
    const char*  text;
};

// Defined in the generated .cpp (see cmake/ExtractStringTable.cmake).
extern const GeneratedStringEntry g_generatedStringTable[];
extern const std::size_t          g_generatedStringTableCount;

/**
 * @brief Looks up a resource ID in the generated string table.
 * @return The real string text, or nullptr if not found (e.g. the table is
 *         empty, or this specific ID has no STRINGTABLE entry in the
 *         source .rc file).
 */
const char* FindGeneratedString(unsigned int id);

} // namespace FreeApi::Internal

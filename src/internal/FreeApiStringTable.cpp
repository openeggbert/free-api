#include "internal/FreeApiGeneratedStrings.hpp"

namespace FreeApi::Internal {

const char* FindGeneratedString(unsigned int id)
{
    for (std::size_t i = 0; i < g_generatedStringTableCount; ++i) {
        if (g_generatedStringTable[i].id == id) {
            return g_generatedStringTable[i].text;
        }
    }
    return nullptr;
}

} // namespace FreeApi::Internal

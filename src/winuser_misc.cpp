#include "windows.h"
#include "internal/FreeApiGeneratedStrings.hpp"

#include <cstdio>
#include <cstdlib>

extern "C" {

int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    (void)hWnd;
    (void)uType;

    const char* caption = lpCaption ? lpCaption : "Message";
    const char* text    = lpText ? lpText : "";
    fprintf(stderr, "[MessageBoxA] %s: %s\n", caption, text);
    return 1;
}

int WINAPI LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax)
{
    (void)hInstance;
    if (!lpBuffer || cchBufferMax <= 0) {
        return 0;
    }

    // Both target games source all on-screen UI text through LoadStringA.
    // Real text comes from a generated STRINGTABLE lookup (see
    // cmake/ExtractStringTable.cmake); if this ID isn't found there --
    // either a standalone build with no sibling game's .rc extracted, or an
    // ID with no STRINGTABLE entry in the source .rc -- fall back to a
    // placeholder rather than silently returning empty/garbage text.
    const char* realText = FreeApi::Internal::FindGeneratedString(uID);

    int written;
    if (realText) {
        written = snprintf(lpBuffer, static_cast<size_t>(cchBufferMax), "%s", realText);
    } else {
        written = snprintf(lpBuffer, static_cast<size_t>(cchBufferMax), "RES_%u", uID);
    }

    if (written < 0) {
        lpBuffer[0] = '\0';
        return 0;
    }
    if (written >= cchBufferMax) {
        return cchBufferMax - 1;
    }
    return written;
}

HBRUSH WINAPI GetStockBrush(int fnObject)
{
    return reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(fnObject + 1));
}

int WINAPI GetSystemMetrics(int nIndex)
{
    if (nIndex == SM_CXSCREEN) {
        return 1024;
    }

    if (nIndex == SM_CYSCREEN) {
        return 768;
    }

    if (nIndex == SM_CYCAPTION) {
        return 24;
    }

    return 0;
}

BOOL WINAPI AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    (void)dwStyle;
    (void)bMenu;

    // Intentional identity transform: see the Doxygen comment on this
    // function's declaration in include/winuser.h for why leaving lpRect
    // unchanged is the correct behavior given how CreateWindowExA and
    // GetClientRect define "window size" in this implementation.
    return lpRect ? TRUE : FALSE;
}

} // extern "C"

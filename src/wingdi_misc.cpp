#include "windows.h"

extern "C" {

int WINAPI GetDeviceCaps(HDC hdc, int index)
{
    (void)hdc;
    (void)index;
    // Real Win32 only reports a nonzero SIZEPALETTE for an actual
    // hardware-palette (<=8bpp) device; both games' TrueColor-vs-palette
    // branching (pixmap.cpp in each) depends on 0 meaning "not a palette
    // device", so a modern host must report 0 here, not a placeholder like
    // 256 (which used to force both games onto their legacy palette path).
    return 0;
}

UINT WINAPI GetSystemPaletteEntries(HDC hdc, UINT iStartIndex, UINT nEntries, LPVOID lppe)
{
    (void)hdc;
    if (!lppe) {
        return 0;
    }

    PALETTEENTRY* entries = reinterpret_cast<PALETTEENTRY*>(lppe);
    for (UINT i = 0; i < nEntries; ++i) {
        UINT value       = (iStartIndex + i) & 0xFFu;
        entries[i].peRed   = static_cast<BYTE>(value);
        entries[i].peGreen = static_cast<BYTE>(value);
        entries[i].peBlue  = static_cast<BYTE>(value);
        entries[i].peFlags = 0;
    }
    return nEntries;
}

} // extern "C"

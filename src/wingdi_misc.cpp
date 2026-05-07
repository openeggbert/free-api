#include "windows.h"

extern "C" {

int WINAPI GetDeviceCaps(HDC hdc, int index)
{
    (void)hdc;
    if (index == SIZEPALETTE) {
        return 256;
    }
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

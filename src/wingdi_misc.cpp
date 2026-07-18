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
    // TASK-24H-0604: index is ignored entirely, so every other index also
    // returns 0 -- an unintended side effect of this shape, not a
    // deliberate per-index decision. Confirmed correct only for
    // SIZEPALETTE (the one index either game queries).
    return 0;
}

UINT WINAPI GetSystemPaletteEntries(HDC hdc, UINT iStartIndex, UINT nEntries, LPVOID lppe)
{
    (void)hdc;
    if (!lppe) {
        return 0;
    }

    // A real system palette (always an 8-bit-or-less hardware palette
    // device) never has more than 256 entries -- clamp here rather than
    // trusting an arbitrary caller-supplied nEntries unconditionally
    // (found by this session's edge-case audit). Both real call sites in
    // free-eggbert/planetblupi already pass exactly 256, matching a
    // fixed-size PALETTEENTRY[256] buffer, so this is a no-op for them.
    if (nEntries > 256u) {
        nEntries = 256u;
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

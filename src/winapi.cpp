/**
 * @file winapi.cpp
 * @brief Legacy monolithic file — content has been split into separate modules.
 *
 * This file is intentionally empty. All implementations have been moved to:
 *
 * Internal modules (src/internal/):
 *   - FreeApiDiagnostics.hpp/.cpp  — diagnostics counters and helpers
 *   - FreeApiGdi.hpp/.cpp          — GDI types (CompatBitmap, CompatDC) and helpers
 *   - FreeApiWindowRegistry.hpp/.cpp — window/class registries
 *   - FreeApiMessageQueue.hpp/.cpp — message queue, PushMessage, PumpSdlEvents
 *   - FreeApiTimers.hpp/.cpp       — WinAPI and multimedia timer state
 *   - FreeApiPath.hpp/.cpp         — path helpers (NormalizePath, BuildCommandLine)
 *   - FreeApiSdlVideo.hpp/.cpp     — SDL video subsystem management
 *
 * Public source files (src/):
 *   - winbase.cpp          — Sleep, GetTickCount, GlobalMemoryStatus, etc.
 *   - winbase_file.cpp     — file/resource functions (_lopen, DeleteFileA, etc.)
 *   - winuser_window.cpp   — window creation/management
 *   - winuser_message.cpp  — message queue functions
 *   - winuser_cursor.cpp   — cursor/mouse functions
 *   - winuser_timer.cpp    — SetTimer/KillTimer
 *   - winuser_misc.cpp     — MessageBoxA, LoadStringA, GetSystemMetrics, etc.
 *   - wingdi_bitmap.cpp    — bitmap creation and management
 *   - wingdi_dc.cpp        — device context functions
 *   - wingdi_blit.cpp      — StretchBlt, GetPixel, SetPixel
 *   - wingdi_misc.cpp      — GetDeviceCaps, GetSystemPaletteEntries
 *   - winmm.cpp            — multimedia timer and MIDI/MCI functions
 *   - crt_io.cpp           — CRT file enumeration (_findfirst, etc.)
 *   - winmain_bridge.cpp   — FreeApiRunWinMain and WinMain bridge
 */

/**
 * @file digitalv.h
 * @brief MCI Digital Video / MIDI sequencer compatibility subset.
 *
 * Exposes the WinMM MCI command interface (mciSendCommand / mciGetDeviceID)
 * and the supporting MCI parameter structures.
 *
 * Supported device types:
 * - "sequencer" : MIDI playback via TinySoundFont + TinyMidiLoader (MCI_OPEN,
 *   MCI_PLAY, MCI_CLOSE, MCI_SET).
 *
 * Unsupported device types:
 * - "cdaudio": gracefully declined with MCIERR_UNSUPPORTED_FUNCTION.
 * - "avivideo" (digital video / movie playback): intentionally, permanently
 *   declined with MCIERR_UNSUPPORTED_FUNCTION. This is not a placeholder --
 *   both target games' CMovie::Create() disables movie playback when this
 *   open fails, and their StartMovie()/MovieToStart() then transition
 *   straight to the post-movie phase, exactly as a completed movie would.
 *   Cutscenes are silently and safely skipped end-to-end; there is no
 *   crash, hang, or visible error to fix. See plan.md section 10 / NEXT.md
 *   for the investigation, and tests/test_mci_avivideo_regressions.cpp for
 *   the regression test locking this in.
 *
 * MCI_NOTIFY is supported for MCI_PLAY: MM_MCINOTIFY is posted to the
 * callback HWND when playback completes.
 *
 * MCI_PLAY looping is not implemented (TODO).
 *
 * @note The implementation lives in src/winapi.cpp and src/MidiMusic.cpp.
 * @note Status: PARTIAL (MIDI/sequencer); "avivideo" is a deliberate,
 *       evidenced permanent decline, not a gap -- see above.
 */
#ifndef FREE_API_DIGITALV_H
#define FREE_API_DIGITALV_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MCI_OPEN 0x0803
#define MCI_CLOSE 0x0804
#define MCI_PLAY 0x0806
#define MCI_STATUS 0x0814
#define MCI_PAUSE 0x0809

#define MCI_NOTIFY 0x00000001L
#define MCI_WAIT 0x00000002L

#define MCI_OPEN_TYPE 0x00002000L
#define MCI_OPEN_ELEMENT 0x00000200L

#define MCI_STATUS_ITEM 0x00000100L

#define MCI_DGV_OPEN_PARENT 0x00002000L
#define MCI_DGV_OPEN_WS 0x00010000L
#define MCI_DGV_STATUS_HWND 0x00004001L
#define MCI_DGV_PLAY_REVERSE 0x00010000L

typedef struct tagMCI_GENERIC_PARMS {
    DWORD_PTR dwCallback;
} MCI_GENERIC_PARMS, *LPMCI_GENERIC_PARMS;

typedef struct tagMCI_STATUS_PARMS {
    DWORD_PTR dwCallback;
    DWORD_PTR dwReturn;
    DWORD_PTR dwItem;
    DWORD_PTR dwTrack;
} MCI_STATUS_PARMS, *LPMCI_STATUS_PARMS;

typedef struct tagMCI_DGV_OPEN_PARMSA {
    DWORD_PTR dwCallback;
    MCIDEVICEID wDeviceID;
    LPCSTR lpstrDeviceType;
    LPCSTR lpstrElementName;
    LPCSTR lpstrAlias;
    DWORD_PTR dwStyle;
    HWND hWndParent;
} MCI_DGV_OPEN_PARMSA, *LPMCI_DGV_OPEN_PARMSA;

typedef MCI_DGV_OPEN_PARMSA MCI_DGV_OPEN_PARMS;
typedef LPMCI_DGV_OPEN_PARMSA LPMCI_DGV_OPEN_PARMS;

typedef struct tagMCI_DGV_WINDOW_PARMSA {
    DWORD_PTR dwCallback;
    HWND hWnd;
    UINT nCmdShow;
    LPCSTR lpstrText;
} MCI_DGV_WINDOW_PARMSA, *LPMCI_DGV_WINDOW_PARMSA;

typedef MCI_DGV_WINDOW_PARMSA MCI_DGV_WINDOW_PARMS;
typedef LPMCI_DGV_WINDOW_PARMSA LPMCI_DGV_WINDOW_PARMS;

typedef struct tagMCI_DGV_STATUS_PARMSA {
    DWORD_PTR dwCallback;
    DWORD_PTR dwReturn;
    DWORD_PTR dwItem;
    DWORD_PTR dwTrack;
    LPCSTR lpstrDrive;
    DWORD_PTR dwReference;
} MCI_DGV_STATUS_PARMSA, *LPMCI_DGV_STATUS_PARMSA;

typedef MCI_DGV_STATUS_PARMSA MCI_DGV_STATUS_PARMS;
typedef LPMCI_DGV_STATUS_PARMSA LPMCI_DGV_STATUS_PARMS;

typedef struct tagMCI_DGV_PLAY_PARMS {
    DWORD_PTR dwCallback;
    DWORD_PTR dwFrom;
    DWORD_PTR dwTo;
    DWORD_PTR dwSpeed;
} MCI_DGV_PLAY_PARMS, *LPMCI_DGV_PLAY_PARMS;

// TASK-24H-0107: this guard never actually fires in practice. <windows.h>
// (included at the top of this file) already includes mmsystem.h, which
// defines FREE_API_MCI_PLAY_PARMS_DEFINED with a *different*-shaped
// MCI_PLAY_PARMS (dwCallback/dwFrom/dwTo, matching both games' live
// "sequencer" MIDI calls), before this line is ever reached -- so
// mmsystem.h's shape always wins, not this MCI_DGV_PLAY_PARMS alias.
// Confirmed safe: neither game's AVI-probe code (movie.cpp) ever uses the
// bare MCI_PLAY_PARMS alias for its (always-unreached, since "avivideo"
// never actually opens) MCI_PLAY call -- it explicitly declares
// MCI_DGV_PLAY_PARMS and casts to LPMCI_DGV_PLAY_PARMS by name, same
// pattern as the MCI_OPEN case above.
#ifndef FREE_API_MCI_PLAY_PARMS_DEFINED
#define FREE_API_MCI_PLAY_PARMS_DEFINED
typedef MCI_DGV_PLAY_PARMS MCI_PLAY_PARMS;
typedef LPMCI_DGV_PLAY_PARMS LPMCI_PLAY_PARMS;
#endif

typedef struct tagMCI_DGV_PAUSE_PARMS {
    DWORD_PTR dwCallback;
} MCI_DGV_PAUSE_PARMS, *LPMCI_DGV_PAUSE_PARMS;

// TASK-24H-0107: same include-order situation as MCI_PLAY_PARMS above --
// <windows.h>'s transitive mmsystem.h include already defines
// FREE_API_MCI_OPEN_PARMS_DEFINED with the differently-shaped, real
// sequencer-call MCI_OPEN_PARMS before this line is reached, so this
// MCI_DGV_OPEN_PARMS alias never actually wins. Confirmed safe: both
// games' AVI-probe code (../free-eggbert/src/movie.cpp:34,103;
// ../planetblupi/src/movie.cpp:31,100) explicitly declares
// MCI_DGV_OPEN_PARMS and casts to LPMCI_DGV_OPEN_PARMS by name for its
// MCI_OPEN calls, never the bare MCI_OPEN_PARMS alias.
#ifndef FREE_API_MCI_OPEN_PARMS_DEFINED
#define FREE_API_MCI_OPEN_PARMS_DEFINED
typedef MCI_DGV_OPEN_PARMS MCI_OPEN_PARMS;
typedef LPMCI_DGV_OPEN_PARMS LPMCI_OPEN_PARMS;
#endif

/**
 * @brief Sends an MCI command to a device.
 *
 * Only "sequencer" MIDI devices are handled; other types are declined.
 * @note Status: PARTIAL
 */
MCIERROR WINAPI mciSendCommandA(MCIDEVICEID mciId, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam);

/**
 * @brief Returns the device ID for a named MCI device.
 *
 * Not implemented; always returns 0.
 * @note Status: STUB
 */
MCIDEVICEID WINAPI mciGetDeviceIDA(LPCSTR lpszDevice);

#define mciSendCommand mciSendCommandA
#define mciGetDeviceID mciGetDeviceIDA

#ifdef __cplusplus
}
#endif

#endif // FREE_API_DIGITALV_H
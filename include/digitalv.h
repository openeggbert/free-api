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
 * - All other video/digital-video device types: declined.
 *
 * MCI_NOTIFY is supported for MCI_PLAY: MM_MCINOTIFY is posted to the
 * callback HWND when playback completes.
 *
 * MCI_PLAY looping is not implemented (TODO).
 *
 * @note The implementation lives in src/winapi.cpp and src/MidiMusic.cpp.
 * @note Status: STUB (for digital video); PARTIAL (for MIDI/sequencer)
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

#ifndef FREE_API_MCI_PLAY_PARMS_DEFINED
#define FREE_API_MCI_PLAY_PARMS_DEFINED
typedef MCI_DGV_PLAY_PARMS MCI_PLAY_PARMS;
typedef LPMCI_DGV_PLAY_PARMS LPMCI_PLAY_PARMS;
#endif

typedef struct tagMCI_DGV_PAUSE_PARMS {
    DWORD_PTR dwCallback;
} MCI_DGV_PAUSE_PARMS, *LPMCI_DGV_PAUSE_PARMS;

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
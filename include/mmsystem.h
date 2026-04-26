#ifndef FREE_API_MMSYSTEM_H
#define FREE_API_MMSYSTEM_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// WinMM subset - Status: PARTIAL
typedef UINT MMRESULT;
typedef UINT MCIDEVICEID;
typedef DWORD MCIERROR;

typedef struct tagJOYINFOEX {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwXpos;
    DWORD dwYpos;
    DWORD dwZpos;
    DWORD dwRpos;
    DWORD dwUpos;
    DWORD dwVpos;
    DWORD dwButtons;
    DWORD dwButtonNumber;
    DWORD dwPOV;
    DWORD dwReserved1;
    DWORD dwReserved2;
} JOYINFOEX, *PJOYINFOEX, *LPJOYINFOEX;

typedef struct _WAVEFORMAT {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
} WAVEFORMAT, *PWAVEFORMAT, *LPWAVEFORMAT;

#ifndef FREE_API_MCI_OPEN_PARMS_DEFINED
#define FREE_API_MCI_OPEN_PARMS_DEFINED
typedef struct _MCI_OPEN_PARMSA {
    DWORD_PTR dwCallback;
    MCIDEVICEID wDeviceID;
    LPCSTR lpstrDeviceType;
    LPCSTR lpstrElementName;
    LPCSTR lpstrAlias;
} MCI_OPEN_PARMSA, *LPMCI_OPEN_PARMSA;

typedef MCI_OPEN_PARMSA MCI_OPEN_PARMS;
typedef LPMCI_OPEN_PARMSA LPMCI_OPEN_PARMS;
#endif

#ifndef FREE_API_MCI_PLAY_PARMS_DEFINED
#define FREE_API_MCI_PLAY_PARMS_DEFINED
typedef struct _MCI_PLAY_PARMS {
    DWORD_PTR dwCallback;
    DWORD_PTR dwFrom;
    DWORD_PTR dwTo;
} MCI_PLAY_PARMS, *LPMCI_PLAY_PARMS;
#endif

typedef struct _MCI_SET_PARMS {
    DWORD_PTR dwCallback;
    DWORD dwTimeFormat;
    DWORD dwAudio;
} MCI_SET_PARMS, *LPMCI_SET_PARMS;

typedef HANDLE HMIDIOUT;
typedef HMIDIOUT* LPHMIDIOUT;

#define TIME_PERIODIC 0x0001

#define MM_MCINOTIFY 0x03B9
#define MCI_NOTIFY_SUCCESSFUL 0x0001

#define JOY_BUTTON1 0x0001
#define JOY_BUTTON2 0x0002
#define JOY_BUTTON3 0x0004
#define JOY_BUTTON4 0x0008

#define MMSYSERR_NOERROR 0

#define WAVE_FORMAT_PCM 1

#define MCI_OPEN 0x0803
#define MCI_CLOSE 0x0804
#define MCI_PLAY 0x0806
#define MCI_SET 0x080D
#define MCI_NOTIFY 0x00000001L
#define MCI_WAIT 0x00000002L
#define MCI_OPEN_TYPE 0x00002000L
#define MCI_OPEN_TYPE_ID 0x00001000L
#define MCI_OPEN_ELEMENT 0x00000200L
#define MCI_SET_TIME_FORMAT 0x00000400L
#define MCI_FORMAT_TMSF 10
#define MCI_TRACK 0x00000010L

#define mmioFOURCC(ch0, ch1, ch2, ch3) \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))

typedef void(CALLBACK* LPTIMECALLBACK)(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);

MMRESULT WINAPI timeSetEvent(UINT uDelay,
                             UINT uResolution,
                             LPTIMECALLBACK lpTimeProc,
                             DWORD_PTR dwUser,
                             UINT fuEvent); // Status: STUB

MMRESULT WINAPI timeKillEvent(UINT uTimerID); // Status: STUB

MMRESULT WINAPI joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji); // Status: STUB
UINT WINAPI joyGetNumDevs(void); // Status: STUB

UINT WINAPI midiOutGetNumDevs(void); // Status: STUB
MMRESULT WINAPI midiOutOpen(LPHMIDIOUT phmo, UINT uDeviceID, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen); // Status: STUB
MMRESULT WINAPI midiOutSetVolume(HMIDIOUT hmo, DWORD dwVolume); // Status: STUB
MMRESULT WINAPI midiOutClose(HMIDIOUT hmo); // Status: STUB

MCIERROR WINAPI mciSendCommandA(MCIDEVICEID mciId, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam); // Status: STUB
BOOL WINAPI mciGetErrorStringA(MCIERROR mcierr, LPSTR pszText, UINT cchText); // Status: STUB

#define mciSendCommand mciSendCommandA
#define mciGetErrorString mciGetErrorStringA

#ifdef __cplusplus
}
#endif

#endif // FREE_API_MMSYSTEM_H
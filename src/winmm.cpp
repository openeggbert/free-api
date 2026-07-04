#include "windows.h"
#include "mmsystem.h"
#include "digitalv.h"
#include "MidiMusic.h"
#include "internal/FreeApiTimers.hpp"
#include "internal/FreeApiDiagnostics.hpp"

#include <SDL3/SDL.h>

using namespace FreeApi::Internal;

/**
 * @brief SDL3 timer callback bridge that invokes the user-supplied LPTIMECALLBACK.
 *
 * SDL_AddTimer fires this callback on a private SDL timer thread; the user
 * callback (commonly TimerStep in legacy WinAPI games) typically calls
 * PostMessage which queues a WM_* message. The message queue is therefore
 * mutex-protected (see g_messageQueueMutex).
 *
 * @param userdata Pointer to MmTimerEntry registered in timeSetEvent.
 * @param sdlTimerId SDL timer id (unused, we already store it).
 * @param interval Current interval in ms; returning the same value reschedules.
 * @return Same interval to keep the periodic timer running.
 *
 * @note Status: IMPLEMENTED
 */
static Uint32 SDLCALL FreeApiMmTimerBridge(void* userdata, SDL_TimerID sdlTimerId, Uint32 interval)
{
    (void)sdlTimerId;
    LPTIMECALLBACK cb = nullptr;
    UINT mmId = 0;
    DWORD_PTR user = 0;
    {
        std::lock_guard<std::mutex> lock(g_mmTimerMutex);
        const auto* entry = static_cast<const MmTimerEntry*>(userdata);
        if (!entry) return 0;
        // Verify the entry is still alive in the map (avoid use-after-free if killed).
        auto it = g_mmTimers.find(entry->mmId);
        if (it == g_mmTimers.end()) return 0;
        cb   = it->second.callback;
        mmId = it->second.mmId;
        user = it->second.user;
    }
    if (cb) {
        cb(mmId, 0, static_cast<DWORD>(user), 0, 0);
    }
    return interval;
}

extern "C" {

/**
 * @brief Starts a periodic multimedia timer using SDL3.
 *
 * Implemented via SDL_AddTimer; the registered callback is invoked roughly every
 * @p uDelay milliseconds on a private SDL timer thread. The legacy fuEvent
 * parameter is honored only for TIME_PERIODIC; one-shot is treated as periodic.
 *
 * @note Status: IMPLEMENTED
 */
MMRESULT WINAPI timeSetEvent(UINT uDelay,
                             UINT uResolution,
                             LPTIMECALLBACK lpTimeProc,
                             DWORD_PTR dwUser,
                             UINT fuEvent)
{
    (void)uResolution;
    (void)fuEvent;

    if (uDelay == 0 || lpTimeProc == nullptr) {
        SDL_Log("free-api timeSetEvent: invalid args (uDelay=%u, lpTimeProc=%p)", uDelay, (void*)(uintptr_t)lpTimeProc);
        return 0;
    }

    if (!SDL_InitSubSystem(SDL_INIT_EVENTS)) {
        SDL_Log("free-api timeSetEvent: SDL_INIT_EVENTS failed: %s", SDL_GetError());
    }

    const UINT timerId = g_nextTimerId.fetch_add(1);
    MmTimerEntry* entryPtr = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mmTimerMutex);
        auto& entry     = g_mmTimers[timerId];
        entry.mmId      = timerId;
        entry.callback  = lpTimeProc;
        entry.user      = dwUser;
        entry.sdlId     = 0;
        entryPtr        = &entry;
    }

    SDL_TimerID sdlId = SDL_AddTimer(uDelay, FreeApiMmTimerBridge, entryPtr);
    if (sdlId == 0) {
        SDL_Log("free-api timeSetEvent: SDL_AddTimer failed: %s", SDL_GetError());
        std::lock_guard<std::mutex> lock(g_mmTimerMutex);
        g_mmTimers.erase(timerId);
        return 0;
    }

    {
        std::lock_guard<std::mutex> lock(g_mmTimerMutex);
        auto it = g_mmTimers.find(timerId);
        if (it != g_mmTimers.end()) it->second.sdlId = sdlId;
    }
    g_activeTimerIds.insert(timerId);
    SDL_Log("free-api timeSetEvent: mmId=%u sdlId=%u delay=%u ms", timerId, sdlId, uDelay);
    FreeApiDiagSnapshot("mm-timer-set");
    return timerId;
}

/**
 * @brief Stops a multimedia timer started with timeSetEvent.
 *
 * @note Status: IMPLEMENTED
 */
MMRESULT WINAPI timeKillEvent(UINT uTimerID)
{
    if (uTimerID == 0) {
        return 1;
    }
    SDL_TimerID sdlId = 0;
    {
        std::lock_guard<std::mutex> lock(g_mmTimerMutex);
        auto it = g_mmTimers.find(uTimerID);
        if (it == g_mmTimers.end()) {
            SDL_Log("free-api timeKillEvent: unknown timer id %u", uTimerID);
            return 1;
        }
        sdlId = it->second.sdlId;
        g_mmTimers.erase(it);
    }
    if (sdlId != 0) {
        SDL_RemoveTimer(sdlId);
    }
    g_activeTimerIds.erase(uTimerID);
    FreeApiDiagSnapshot("mm-timer-kill");
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji)
{
    (void)uJoyID;
    if (!pji) {
        return 1;
    }

    if (pji->dwSize >= sizeof(JOYINFOEX)) {
        memset(pji, 0, sizeof(JOYINFOEX));
        pji->dwSize = sizeof(JOYINFOEX);
    }

    return 1;
}

UINT WINAPI joyGetNumDevs(void)
{
    return 0;
}

UINT WINAPI midiOutGetNumDevs(void)
{
    return MidiMusicGetNumDevs();
}

MMRESULT WINAPI midiOutOpen(LPHMIDIOUT phmo, UINT uDeviceID, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen)
{
    (void)uDeviceID;
    (void)dwCallback;
    (void)dwInstance;
    (void)fdwOpen;
    return MidiMusicOutOpen(phmo);
}

MMRESULT WINAPI midiOutSetVolume(HMIDIOUT hmo, DWORD dwVolume)
{
    (void)hmo;
    return MidiMusicSetVolume(dwVolume);
}

MMRESULT WINAPI midiOutClose(HMIDIOUT hmo)
{
    (void)hmo;
    return MidiMusicOutClose();
}

MCIERROR WINAPI mciSendCommandA(MCIDEVICEID mciId, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    // MCI_OPEN with MCI_OPEN_TYPE only (no MCI_OPEN_ELEMENT) is an "open
    // device class" call, used by CMovie::initAVI() in both target games
    // for "avivideo". AVI digital-video playback is intentionally not
    // implemented (see plan.md section 10 / NEXT.md for the investigation):
    // both games' CMovie::Create() sets m_bEnable=FALSE when this open
    // fails, and their CEvent::StartMovie()/MovieToStart() then immediately
    // transition to the same phase a completed movie would -- cutscenes are
    // silently and safely skipped end-to-end, with no crash or hang. This
    // is a deliberate, permanent, evidenced decision, not a placeholder.
    // Locked in by tests/test_mci_avivideo_regressions.cpp.
    //
    // IMPORTANT: Planet Blupi truncates the struct pointer to DWORD when passing
    // it to this function, which makes the pointer invalid on 64-bit Linux.
    // We must NOT dereference dwParam here when only MCI_OPEN_TYPE is set.

    if (uMsg == MCI_OPEN && (fdwCommand & MCI_OPEN_TYPE) && !(fdwCommand & MCI_OPEN_ELEMENT)) {
        SDL_Log("free-api mciSendCommandA: MCI_OPEN device-type-only (avivideo) — "
                "video playback not implemented, returning MCIERR_UNSUPPORTED_FUNCTION");
        return MCIERR_UNSUPPORTED_FUNCTION;
    }

    return MidiMusicSendCommand(mciId, uMsg, fdwCommand, dwParam);
}

MCIDEVICEID WINAPI mciGetDeviceIDA(LPCSTR lpszDevice)
{
    (void)lpszDevice;
    return 1;
}

BOOL WINAPI mciGetErrorStringA(MCIERROR mcierr, LPSTR pszText, UINT cchText)
{
    return MidiMusicGetErrorString(mcierr, pszText, cchText);
}

} // extern "C"

#include "windows.h"
#include "mmsystem.h"
#include "digitalv.h"
#include "MidiMusic.h"
#include "internal/FreeApiTimers.hpp"
#include "internal/FreeApiDiagnostics.hpp"

#include <SDL3/SDL.h>
#include <mutex>
#include <unordered_map>

using namespace FreeApi::Internal;

// TASK-0103: real joystick backend, SDL_Joystick-based (not the higher-level
// Gamepad API -- free-eggbert's own usage is a generic 2-axis/4-button
// joystick, matching the raw joystick model directly, and SDL's virtual
// joystick test API attaches as a plain joystick without needing a gamepad
// mapping-database entry).
namespace {

std::mutex g_joystickMutex;
std::unordered_map<UINT, SDL_Joystick*> g_openJoysticks; // keyed by Win32-style 0-based device index

bool EnsureJoystickSubsystem()
{
    if (SDL_WasInit(SDL_INIT_JOYSTICK)) return true;
    return SDL_InitSubSystem(SDL_INIT_JOYSTICK);
}

// Resolves a Win32-style 0-based joystick index to an already-open (cached)
// SDL_Joystick*, opening it on first use. Returns nullptr if uJoyID is out
// of range or the device can't be opened.
SDL_Joystick* ResolveJoystick(UINT uJoyID)
{
    std::lock_guard<std::mutex> lock(g_joystickMutex);

    auto it = g_openJoysticks.find(uJoyID);
    if (it != g_openJoysticks.end()) {
        return it->second;
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (!ids) return nullptr;

    SDL_Joystick* joystick = nullptr;
    if (static_cast<int>(uJoyID) < count) {
        joystick = SDL_OpenJoystick(ids[uJoyID]);
    }
    SDL_free(ids);

    if (joystick) {
        g_openJoysticks[uJoyID] = joystick;
    }
    return joystick;
}

} // namespace

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
    if (!pji) {
        return JOYERR_PARMS;
    }

    const DWORD originalSize = pji->dwSize;
    if (originalSize >= sizeof(JOYINFOEX)) {
        memset(pji, 0, sizeof(JOYINFOEX));
        pji->dwSize = sizeof(JOYINFOEX);
    }

    if (!EnsureJoystickSubsystem()) {
        return JOYERR_UNPLUGGED;
    }

    SDL_UpdateJoysticks(); // refresh state without requiring the caller to have pumped events this frame
    SDL_Joystick* joystick = ResolveJoystick(uJoyID);
    if (!joystick) {
        return JOYERR_UNPLUGGED;
    }

    if (originalSize < sizeof(JOYINFOEX)) {
        // Caller didn't provide a big enough struct to write into -- real
        // Win32 would reject this too small a request.
        return JOYERR_PARMS;
    }

    // SDL axes are signed 16-bit (-32768..32767, center 0); real Win32
    // joystick axes are unsigned 16-bit (0..65535, center 32768) -- both
    // free-eggbert's thresholds (<16384 / >49152) assume this convention.
    const int numAxes = SDL_GetNumJoystickAxes(joystick);
    if (numAxes >= 1) {
        pji->dwXpos = static_cast<DWORD>(static_cast<int>(SDL_GetJoystickAxis(joystick, 0)) + 32768);
    } else {
        pji->dwXpos = 32768; // centered: no X axis reported
    }
    if (numAxes >= 2) {
        pji->dwYpos = static_cast<DWORD>(static_cast<int>(SDL_GetJoystickAxis(joystick, 1)) + 32768);
    } else {
        pji->dwYpos = 32768;
    }

    // Real Win32 JOY_BUTTON1-4 are bits 0-3 of dwButtons, matching the first
    // four physical buttons in device order -- free-eggbert's only usage
    // (event.cpp:2069-2127) reads exactly these four.
    DWORD buttons = 0;
    const int numButtons = SDL_GetNumJoystickButtons(joystick);
    for (int i = 0; i < 4 && i < numButtons; ++i) {
        if (SDL_GetJoystickButton(joystick, i)) {
            buttons |= (1u << i); // JOY_BUTTON1..JOY_BUTTON4
        }
    }
    pji->dwButtons = buttons;

    return JOYERR_NOERROR;
}

UINT WINAPI joyGetNumDevs(void)
{
    if (!EnsureJoystickSubsystem()) {
        return 0;
    }
    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (ids) SDL_free(ids);
    return static_cast<UINT>(count);
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

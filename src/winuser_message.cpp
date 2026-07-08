#include "windows.h"
#include "internal/FreeApiMessageQueue.hpp"
#include "internal/FreeApiWindowRegistry.hpp"
#include "internal/FreeApiTimers.hpp"
#include "internal/FreeApiDiagnostics.hpp"

#include <SDL3/SDL.h>
#include <vector>
#include <cstdarg>
#include <cstdio>

using namespace FreeApi::Internal;

// Forward declaration (defined in winuser_window.cpp)
extern "C" BOOL WINAPI DestroyWindow(HWND hWnd);

extern "C" {

BOOL WINAPI PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    PushMessage(hWnd, Msg, wParam, lParam);
    return TRUE;
}

BOOL WINAPI PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    // hWnd/wMsgFilterMin/wMsgFilterMax are intentionally ignored -- every
    // call behaves as an unfiltered peek across the whole queue, regardless
    // of what's passed. Two independent full-source usage sweeps confirm
    // neither ../free-eggbert nor ../planetblupi ever passes a non-zero
    // filter or a specific hWnd, so this is harmless today. Do not
    // implement real filtering without new evidence of a caller that needs
    // it (TASK-24H-0201/1210; see docs/out-of-scope.md).
    (void)hWnd;
    (void)wMsgFilterMin;
    (void)wMsgFilterMax;
    FreeApiDiagTick();

    if (!lpMsg) {
        return FALSE;
    }

    // IMPORTANT: PeekMessageA must NEVER sleep or block. Real WinAPI returns
    // immediately with FALSE if no message is available. Sleeping here (e.g.
    // SDL_Delay(1)) inside a hot game loop adds ~1ms of latency on every poll
    // and produces the rubbery / laggy feel the game suffered from. Blocking
    // is only allowed in GetMessageA / WaitMessage which are documented to
    // wait for a message.

    // First check the internal message queue. If we already have queued
    // messages, drain them before calling SDL_PumpEvents/SDL_PollEvent. This
    // avoids the previous behaviour where SDL was pumped on every Peek even
    // while the queue still had unprocessed messages, which both wasted CPU
    // and could starve queued messages by reordering work.
    bool needPump = false;
    {
        std::lock_guard<std::mutex> lock(g_messageQueueMutex);
        needPump = g_messageQueue.empty();
    }

    if (needPump) {
        PumpSdlEvents();

        // Generate WM_TIMER messages for elapsed WinAPI timers (also only
        // when the queue was empty — pending timer messages would be coalesced
        // anyway, but doing this only when needed avoids redundant work).
        std::vector<MSG> pendingTimers;
        {
            std::lock_guard<std::mutex> timerLock(g_winTimerMutex);
            const uint64_t now = SDL_GetTicks();
            for (auto& [id, wt] : g_winTimers) {
                if (now - wt.lastFireTick >= wt.intervalMs) {
                    wt.lastFireTick = now;
                    MSG timerMsg{};
                    timerMsg.hwnd    = wt.hwnd;
                    timerMsg.message = WM_TIMER;
                    timerMsg.wParam  = static_cast<WPARAM>(wt.id);
                    timerMsg.lParam  = 0;
                    pendingTimers.push_back(timerMsg);
                }
            }
        }
        for (auto& m : pendingTimers) {
            // Route through PushMessage so WM_TIMER coalescing applies.
            PushMessage(m.hwnd, m.message, m.wParam, m.lParam);
        }
    }

    std::lock_guard<std::mutex> lock(g_messageQueueMutex);
    if (g_messageQueue.empty()) {
        // No message — return immediately. Do NOT sleep here.
        return FALSE;
    }

    *lpMsg = g_messageQueue.front();
    if ((wRemoveMsg & PM_REMOVE) != 0) {
        g_messageQueue.pop_front();
        if (lpMsg->message == kDiagWmUpdate) {
            g_updateMessagePending.store(false, std::memory_order_release);
            if (FreeApiDiagnosticsFastEnabled()) {
                g_diagWmUpdatePending.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    }

    return TRUE;
}

BOOL WINAPI GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
    if (!lpMsg) {
        return FALSE;
    }

    while (true) {
        if (PeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, PM_REMOVE)) {
            if (lpMsg->message == WM_QUIT) {
#if defined(__ANDROID__)
                SDL_Log("FREEAPI_ANDROID: GetMessage returning FALSE (WM_QUIT wParam=%u)",
                        (unsigned)lpMsg->wParam);
#endif
                return FALSE;
            }
#if defined(__ANDROID__)
            // FREEAPI_ANDROID_PERF: throttled once-per-second diagnostics — not per-message.
            {
                static uint64_t s_calls = 0;
                static uint64_t s_timers = 0;
                static uint64_t s_lastMs = 0;
                s_calls++;
                if (lpMsg->message == WM_TIMER) s_timers++;
                const uint64_t nowMs = SDL_GetTicks();
                if (s_lastMs == 0) s_lastMs = nowMs;
                if (nowMs - s_lastMs >= 1000) {
                    SDL_Log("FREEAPI_ANDROID_PERF: GetMessage calls/s=%llu wm_timer/s=%llu wait_ms=10",
                            (unsigned long long)s_calls,
                            (unsigned long long)s_timers);
                    s_calls = 0;
                    s_timers = 0;
                    s_lastMs = nowMs;
                }
            }
#endif
            return TRUE;
        }

#if defined(__ANDROID__)
        // On Android use SDL_WaitEventTimeout so the Android event system gets
        // proper CPU time and lifecycle events are delivered promptly.
        // SDL_Delay(1) in a tight spin-loop is not appropriate for Android.
        SDL_WaitEventTimeout(NULL, 10);
#else
        SDL_Delay(1);
#endif
    }
}

BOOL WINAPI TranslateMessage(const MSG* lpMsg)
{
    return lpMsg ? TRUE : FALSE;
}

LRESULT WINAPI DispatchMessageA(const MSG* lpMsg)
{
    if (!lpMsg) {
        return 0;
    }

    if (lpMsg->message == WM_QUIT) {
        return 0;
    }

    if (FreeApiDiagnosticsFastEnabled()) {
        g_diagMessagesDispatched.fetch_add(1, std::memory_order_relaxed);
        if (lpMsg->message == kDiagWmUpdate) {
            g_diagWmUpdateDispatched.fetch_add(1, std::memory_order_relaxed);
        }
        FreeApiDiagTick();
    }

    InputLog("DISPATCH hwnd=%p msg=0x%04X wParam=0x%X lParam=0x%X",
        (void*)lpMsg->hwnd, lpMsg->message, (unsigned)lpMsg->wParam, (unsigned)lpMsg->lParam);

    const auto it = g_windowProcedures.find(lpMsg->hwnd);
    if (it != g_windowProcedures.end() && it->second) {
        InputLog("DISPATCH -> WndProc=%p", (void*)(uintptr_t)it->second);
        return it->second(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
    }

    // Fallback: if hwnd not found but there is exactly one registered window, use it.
    // This handles cases where the message was pushed with a NULL or mismatched hwnd.
    if (!g_windowProcedures.empty()) {
        auto& [fwnd, fproc] = *g_windowProcedures.begin();
        if (fproc && lpMsg->hwnd == NULL) {
            InputLog("DISPATCH fallback hwnd=%p -> WndProc=%p", (void*)fwnd, (void*)(uintptr_t)fproc);
            return fproc(fwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
        }
    }

    InputLog("DISPATCH -> DefWindowProc (no WndProc found for hwnd=%p)", (void*)lpMsg->hwnd);
    return DefWindowProcA(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
}

LRESULT WINAPI DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    (void)wParam;
    (void)lParam;

    if (Msg == WM_CLOSE) {
#if defined(__ANDROID__)
        SDL_Log("FREEAPI_ANDROID: DefWindowProcA WM_CLOSE -> DestroyWindow hwnd=%p",
                (void*)hWnd);
#endif
        // DestroyWindow() now synchronously dispatches WM_DESTROY to the
        // window's own procedure before returning, so quitting is posted
        // from the WM_DESTROY handling below (or by the app's own WM_DESTROY
        // handler, as both target games have) -- not redundantly here too.
        DestroyWindow(hWnd);
        return 0;
    }

    if (Msg == WM_DESTROY) {
#if defined(__ANDROID__)
        SDL_Log("FREEAPI_ANDROID: DefWindowProcA WM_DESTROY -> PostQuitMessage hwnd=%p",
                (void*)hWnd);
#endif
        PostQuitMessage(0);
        return 0;
    }

    return 0;
}

void WINAPI PostQuitMessage(int nExitCode)
{
#if defined(__ANDROID__)
    SDL_Log("FREEAPI_ANDROID: PostQuitMessage called nExitCode=%d", nExitCode);
#endif
    PushMessage(NULL, WM_QUIT, static_cast<WPARAM>(nExitCode), 0);
}

// Polling-based approximation, not a true OS-level blocking wait (no
// condition variable woken by PushMessage). Checks the queue, pumps SDL
// events once, checks again, and if still empty sleeps ~1ms (or yields via
// SDL_WaitEventTimeout on Android) before one final pump -- then returns
// TRUE **unconditionally**, even if the queue is still empty after that
// final check. This is adequate for both games' current idle-loop usage
// (they only need a non-busy-spin wait, not a guarantee that a message is
// actually ready), but a caller expecting true Win32 blocking semantics
// would be surprised. Do not implement a real blocking wait without new
// evidence either game's behavior depends on it (TASK-24H-0204/1211; see
// docs/out-of-scope.md).
BOOL WINAPI WaitMessage(void)
{
    {
        std::lock_guard<std::mutex> lock(g_messageQueueMutex);
        if (!g_messageQueue.empty()) return TRUE;
    }

    PumpSdlEvents();
    {
        std::lock_guard<std::mutex> lock(g_messageQueueMutex);
        if (!g_messageQueue.empty()) return TRUE;
    }

#if defined(__ANDROID__)
    // On Android, yield to the event system via SDL_WaitEventTimeout instead
    // of a blind sleep, then pump any newly arrived events.
    SDL_WaitEventTimeout(NULL, 10);
#else
    SDL_Delay(1);
#endif
    PumpSdlEvents();
    return TRUE;
}

int WINAPIV wsprintfA(LPSTR lpOut, LPCSTR lpFmt, ...)
{
    if (!lpOut || !lpFmt) {
        return 0;
    }

    va_list args;
    va_start(args, lpFmt);
    int written = vsnprintf(lpOut, 1024, lpFmt, args);
    va_end(args);
    return written;
}

} // extern "C"

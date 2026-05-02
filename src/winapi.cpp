/**
 * @file winapi.cpp
 * @brief Main WinAPI / WinMM / GDI-like compatibility layer — implementation.
 *
 * This file implements the public API declared in include/windows.h and
 * include/mmsystem.h using SDL3 and POSIX.  It is the core of the Free API
 * compatibility library.  No WinAPI symbols are emitted; all public functions
 * are C linkage wrappers around SDL/POSIX calls.
 *
 * @par Internal Structure
 * All private state is in an anonymous namespace:
 *
 * - @b CompatBitmap / @b CompatDC — GDI-like in-memory bitmaps and device
 *   contexts.  Only a small RGBA32 pixel buffer subset is implemented.
 *   Handles (HBITMAP, HDC) are pointers to these structs cast through void*.
 *   Magic-number validation guards against stale handles.
 *
 * - @b Message Queue — a global std::queue<MSG> protected by
 *   @c g_messageQueueMutex.  The multimedia timer (timeSetEvent) fires its
 *   callback on an SDL thread, which may call PostMessageA; the mutex prevents
 *   concurrent queue corruption.
 *
 * - @b WinTimers — WinAPI polling timers (SetTimer/KillTimer).  Timers are
 *   stored in @c g_winTimers and checked in PeekMessageA; they produce
 *   WM_TIMER messages when their interval has elapsed.  There is no separate
 *   timer thread.
 *
 * - @b MmTimers — WinMM multimedia timers (timeSetEvent/timeKillEvent).
 *   Backed by SDL_AddTimer (fires on a dedicated SDL timer thread).  The
 *   callback receives MMTIM-style parameters and is called from that thread.
 *   @c g_mmTimerMutex protects the id→entry map.
 *
 * - @b Window Map — HWND is an SDL_Window* cast to void*.  @c g_windowProcedures
 *   maps HWND→WNDPROC; @c g_registeredClasses maps class name string→WNDPROC.
 *
 * - @b Diagnostics — optional memory/message diagnostic snapshots controlled
 *   by the @c FREE_API_DIAGNOSTICS=1 environment variable.  Also see
 *   FreeApiDiagSnapshot() and FreeApiDiagTick().
 *
 * @par SDL Event Translation
 * PumpSdlEvents() processes SDL3 events and converts them to WinAPI messages
 * pushed into the global message queue:
 *  - SDL_EVENT_QUIT → WM_QUIT
 *  - SDL_EVENT_KEY_DOWN/UP → WM_KEYDOWN / WM_KEYUP (via SdlScancodeToVK)
 *  - SDL_EVENT_MOUSE_MOTION → WM_MOUSEMOVE
 *  - SDL_EVENT_MOUSE_BUTTON_DOWN/UP → WM_LBUTTONDOWN etc.
 *  - SDL_EVENT_MOUSE_WHEEL → WM_MOUSEWHEEL (high word = delta * WHEEL_DELTA)
 *  - SDL_EVENT_WINDOW_CLOSE_REQUESTED → WM_CLOSE
 *
 * @par Threading Model
 * - Main thread: runs the message loop, PeekMessageA/GetMessageA/DispatchMessageA.
 * - SDL timer thread: fires timeSetEvent callbacks.
 * - MIDI mixer thread: runs in MidiMusic.cpp.
 *
 * @par Unsupported Areas
 * Real GDI drawing (lines, rectangles, brushes, fonts), Win32 resources,
 * common dialogs, Unicode APIs, real security descriptors, COM/OLE, Winsock.
 *
 * @note See also: src/MidiMusic.cpp for the MIDI/MCI backend.
 * @note See also: src/winmain_bridge.cpp for the WinMain startup shim.
 */
#include "windows.h"
#include "io.h"
#include "mmsystem.h"
#include "digitalv.h"
#include "MidiMusic.h"
#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
#include <queue>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdarg>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

namespace {
    constexpr uint32_t kCompatBitmapMagic = 0x504d5442u; // 'BTMP'
    constexpr uint32_t kCompatDcMagic = 0x30434446u;     // 'FDC0'

    enum class CompatDcKind {
        Memory,
        Surface
    };

    struct CompatBitmap {
        uint32_t magic = kCompatBitmapMagic;
        int width = 0;
        int height = 0;
        int pitch = 0;
        int bitsPerPixel = 32;
        std::vector<uint8_t> pixels;
    };

    struct CompatDC {
        uint32_t magic = kCompatDcMagic;
        CompatDcKind kind = CompatDcKind::Memory;
        CompatBitmap* selectedBitmap = nullptr;
        uint8_t* surfacePixels = nullptr;
        int surfaceWidth = 0;
        int surfaceHeight = 0;
        int surfacePitch = 0;
        int surfaceBitsPerPixel = 32;
    };

    std::unordered_map<std::string, WNDPROC> g_registeredClasses;
    std::unordered_map<HWND, WNDPROC> g_windowProcedures;
    std::queue<MSG> g_messageQueue;
    // Mutex protecting g_messageQueue. The multimedia timer (timeSetEvent)
    // dispatches its callback on a separate SDL thread, where the user code
    // is allowed to call PostMessage. Without locking, concurrent push/pop
    // would corrupt the std::queue.
    std::mutex g_messageQueueMutex;
    std::unordered_set<UINT> g_activeTimerIds;
    // WinAPI-style timers registered via SetTimer()
    struct WinTimer {
        HWND hwnd = NULL;
        UINT_PTR id = 0;
        UINT intervalMs = 0;
        uint64_t lastFireTick = 0; // SDL_GetTicks() when last fired
    };
    std::unordered_map<UINT_PTR, WinTimer> g_winTimers;
    std::mutex g_winTimerMutex;
    // Map from MMTIMER id (we expose) to SDL_TimerID that actually drives it,
    // plus the user callback parameters that must be passed to LPTIMECALLBACK.
    struct MmTimerEntry {
        SDL_TimerID sdlId = 0;
        LPTIMECALLBACK callback = nullptr;
        DWORD_PTR user = 0;
        UINT mmId = 0;
    };
    std::unordered_map<UINT, MmTimerEntry> g_mmTimers;
    std::mutex g_mmTimerMutex;
    bool g_videoInitialized = false;
    std::atomic<UINT> g_nextTimerId{1};
    HWND g_focusWindow = NULL;
    constexpr UINT kDiagWmUpdate = WM_USER + 1;

    std::atomic<uint64_t> g_diagMessagesPosted{0};
    std::atomic<uint64_t> g_diagMessagesDispatched{0};
    std::atomic<uint64_t> g_diagSdlEventsProcessed{0};
    std::atomic<uint64_t> g_diagWmUpdatePosted{0};
    std::atomic<uint64_t> g_diagWmUpdateDispatched{0};
    std::atomic<int64_t> g_diagWmUpdatePending{0};
    std::atomic<int64_t> g_diagSdlSurfaces{0};
    std::atomic<int64_t> g_diagSdlSurfacesEver{0};
    std::atomic<int64_t> g_diagSdlSurfacesDestroyed{0};
    std::atomic<int64_t> g_diagCompatBitmaps{0};
    std::atomic<int64_t> g_diagCompatBitmapsEver{0};
    std::atomic<int64_t> g_diagCompatBitmapsDestroyed{0};
    std::atomic<int64_t> g_diagCompatDcs{0};
    std::atomic<int64_t> g_diagCompatDcsEver{0};
    std::atomic<int64_t> g_diagCompatDcsDestroyed{0};
    std::atomic<int64_t> g_diagCompatBitmapPixelCapacityBytes{0};
    std::atomic<int64_t> g_diagCompatBitmapPixelCapacityHighWaterBytes{0};

    // Mouse button state tracked for MK_* wParam in WM_MOUSEMOVE
    WPARAM g_mouseButtons = 0;

    // Optional debug logging for input translation (set FREE_API_DEBUG_INPUT=1 at runtime)
    bool g_debugInput = false;

    void InputLog(const char* fmt, ...)
    {
        if (!g_debugInput) return;
        char buf[256];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        SDL_Log("[free-api input] %s", buf);
    }

    void AdjustDiagLiveBytes(std::atomic<int64_t>& liveCounter, std::atomic<int64_t>& highWaterCounter, const int64_t delta)
    {
        const int64_t value = liveCounter.fetch_add(delta, std::memory_order_relaxed) + delta;
        if (delta <= 0) return;

        int64_t highWater = highWaterCounter.load(std::memory_order_relaxed);
        while (value > highWater &&
               !highWaterCounter.compare_exchange_weak(highWater, value, std::memory_order_relaxed)) {
        }
    }

    void FreeApiDiagSnapshot(const char* tag);

    bool FreeApiDiagnosticsEnabled()
    {
        static int cached = -1;
        if (cached < 0) {
            const char* direct = SDL_getenv("FREE_DIRECT_DIAGNOSTICS");
            const char* api = SDL_getenv("FREE_API_DIAGNOSTICS");
            cached = ((direct && *direct && std::strcmp(direct, "0") != 0)
                   || (api && *api && std::strcmp(api, "0") != 0)) ? 1 : 0;
            if (cached) {
                std::atexit([]() { FreeApiDiagSnapshot("atexit"); });
                FreeApiDiagSnapshot("startup");
            }
        }
        return cached != 0;
    }

    long FreeApiReadRssKB()
    {
#if defined(__linux__)
        int fd = open("/proc/self/status", O_RDONLY);
        if (fd < 0) return 0;
        char buffer[4096];
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);
        if (bytes <= 0) return 0;
        buffer[bytes] = '\0';
        long rss = 0;
        const char* line = buffer;
        while (*line) {
            if (std::strncmp(line, "VmRSS:", 6) == 0) {
                std::sscanf(line + 6, "%ld", &rss);
                break;
            }
            const char* next = std::strchr(line, '\n');
            if (!next) break;
            line = next + 1;
        }
        return rss;
#else
        return 0;
#endif
    }

    void FreeApiDiagSnapshot(const char* tag)
    {
        if (!FreeApiDiagnosticsEnabled()) return;

        size_t queueSize = 0;
        {
            std::lock_guard<std::mutex> lock(g_messageQueueMutex);
            queueSize = g_messageQueue.size();
        }

        size_t winTimerCount = 0;
        {
            std::lock_guard<std::mutex> lock(g_winTimerMutex);
            winTimerCount = g_winTimers.size();
        }

        size_t mmTimerCount = 0;
        {
            std::lock_guard<std::mutex> lock(g_mmTimerMutex);
            mmTimerCount = g_mmTimers.size();
        }

        const long rssKb = FreeApiReadRssKB();
        SDL_Log("[FREE_API_DIAG][%s] rss=%ldKB rssMB=%.1f queue=%zu activeTimers=%zu winTimers=%zu mmTimers=%zu "
                "wmUpdate=posted:%llu dispatched:%llu pending:%lld messages=posted:%llu dispatched:%llu sdlEvents=%llu "
                "sdlSurface=%lld/%lld/%lld compatBitmap=%lld/%lld/%lld compatDC=%lld/%lld/%lld "
                "bytes: compatBitmapPixels=%lldKB(hw=%lldKB) cache: freeApi=0",
                tag ? tag : "snapshot",
                rssKb,
                static_cast<double>(rssKb) / 1024.0,
                queueSize,
                winTimerCount + mmTimerCount,
                winTimerCount,
                mmTimerCount,
                static_cast<unsigned long long>(g_diagWmUpdatePosted.load()),
                static_cast<unsigned long long>(g_diagWmUpdateDispatched.load()),
                static_cast<long long>(g_diagWmUpdatePending.load()),
                static_cast<unsigned long long>(g_diagMessagesPosted.load()),
                static_cast<unsigned long long>(g_diagMessagesDispatched.load()),
                static_cast<unsigned long long>(g_diagSdlEventsProcessed.load()),
                static_cast<long long>(g_diagSdlSurfaces.load()),
                static_cast<long long>(g_diagSdlSurfacesEver.load()),
                static_cast<long long>(g_diagSdlSurfacesDestroyed.load()),
                static_cast<long long>(g_diagCompatBitmaps.load()),
                static_cast<long long>(g_diagCompatBitmapsEver.load()),
                static_cast<long long>(g_diagCompatBitmapsDestroyed.load()),
                static_cast<long long>(g_diagCompatDcs.load()),
                static_cast<long long>(g_diagCompatDcsEver.load()),
                static_cast<long long>(g_diagCompatDcsDestroyed.load()),
                static_cast<long long>(g_diagCompatBitmapPixelCapacityBytes.load() / 1024),
                static_cast<long long>(g_diagCompatBitmapPixelCapacityHighWaterBytes.load() / 1024));
    }

    void FreeApiDiagTick()
    {
        if (!FreeApiDiagnosticsEnabled()) return;
        static std::atomic<uint64_t> lastNs{0};
        const uint64_t now = SDL_GetTicksNS();
        uint64_t prev = lastNs.load(std::memory_order_relaxed);
        if (prev != 0 && now - prev < 5000000000ULL) return;
        if (!lastNs.compare_exchange_strong(prev, now)) return;
        FreeApiDiagSnapshot("periodic5s");
    }

    // Returns the active window for input routing.
    // Prefers the SDL-focus window; falls back to the first registered window so that
    // mouse/keyboard events arriving before the first FOCUS_GAINED are not silently
    // dropped (SDL can deliver motion/button events before the focus event).
    HWND GetActiveWindow()
    {
        if (g_focusWindow) return g_focusWindow;
        // Fallback: use the first (and usually only) registered window.
        if (!g_windowProcedures.empty()) {
            HWND hw = g_windowProcedures.begin()->first;
            InputLog("GetActiveWindow fallback -> hwnd=%p (no focus window set yet)", (void*)hw);
            return hw;
        }
        return NULL;
    }

    // Minimal SDL scancode → WinAPI VK_* mapping
    static WPARAM SdlScancodeToVK(SDL_Scancode sc)
    {
        switch (sc) {
            // Letters: SDL_SCANCODE_A=4 .. Z=29, WinAPI 'A'=0x41 .. 'Z'=0x5A
            case SDL_SCANCODE_A: return 'A';
            case SDL_SCANCODE_B: return 'B';
            case SDL_SCANCODE_C: return 'C';
            case SDL_SCANCODE_D: return 'D';
            case SDL_SCANCODE_E: return 'E';
            case SDL_SCANCODE_F: return 'F';
            case SDL_SCANCODE_G: return 'G';
            case SDL_SCANCODE_H: return 'H';
            case SDL_SCANCODE_I: return 'I';
            case SDL_SCANCODE_J: return 'J';
            case SDL_SCANCODE_K: return 'K';
            case SDL_SCANCODE_L: return 'L';
            case SDL_SCANCODE_M: return 'M';
            case SDL_SCANCODE_N: return 'N';
            case SDL_SCANCODE_O: return 'O';
            case SDL_SCANCODE_P: return 'P';
            case SDL_SCANCODE_Q: return 'Q';
            case SDL_SCANCODE_R: return 'R';
            case SDL_SCANCODE_S: return 'S';
            case SDL_SCANCODE_T: return 'T';
            case SDL_SCANCODE_U: return 'U';
            case SDL_SCANCODE_V: return 'V';
            case SDL_SCANCODE_W: return 'W';
            case SDL_SCANCODE_X: return 'X';
            case SDL_SCANCODE_Y: return 'Y';
            case SDL_SCANCODE_Z: return 'Z';
            // Digits
            case SDL_SCANCODE_0: return '0';
            case SDL_SCANCODE_1: return '1';
            case SDL_SCANCODE_2: return '2';
            case SDL_SCANCODE_3: return '3';
            case SDL_SCANCODE_4: return '4';
            case SDL_SCANCODE_5: return '5';
            case SDL_SCANCODE_6: return '6';
            case SDL_SCANCODE_7: return '7';
            case SDL_SCANCODE_8: return '8';
            case SDL_SCANCODE_9: return '9';
            // Navigation
            case SDL_SCANCODE_LEFT:      return 0x25; // VK_LEFT
            case SDL_SCANCODE_UP:        return 0x26; // VK_UP
            case SDL_SCANCODE_RIGHT:     return 0x27; // VK_RIGHT
            case SDL_SCANCODE_DOWN:      return 0x28; // VK_DOWN
            case SDL_SCANCODE_HOME:      return 0x24; // VK_HOME
            case SDL_SCANCODE_END:       return 0x23; // VK_END
            case SDL_SCANCODE_PAGEUP:    return 0x21; // VK_PRIOR
            case SDL_SCANCODE_PAGEDOWN:  return 0x22; // VK_NEXT
            case SDL_SCANCODE_INSERT:    return 0x2D; // VK_INSERT
            case SDL_SCANCODE_DELETE:    return 0x2E; // VK_DELETE
            // Control keys
            case SDL_SCANCODE_RETURN:    return 0x0D; // VK_RETURN
            case SDL_SCANCODE_KP_ENTER:  return 0x0D; // VK_RETURN
            case SDL_SCANCODE_ESCAPE:    return 0x1B; // VK_ESCAPE
            case SDL_SCANCODE_SPACE:     return 0x20; // VK_SPACE
            case SDL_SCANCODE_TAB:       return 0x09; // VK_TAB
            case SDL_SCANCODE_BACKSPACE: return 0x08; // VK_BACK
            case SDL_SCANCODE_LSHIFT:
            case SDL_SCANCODE_RSHIFT:    return 0x10; // VK_SHIFT
            case SDL_SCANCODE_LCTRL:
            case SDL_SCANCODE_RCTRL:     return 0x11; // VK_CONTROL
            case SDL_SCANCODE_LALT:
            case SDL_SCANCODE_RALT:      return 0x12; // VK_MENU
            case SDL_SCANCODE_PAUSE:     return 0x13; // VK_PAUSE
            // Function keys
            case SDL_SCANCODE_F1:  return 0x70;
            case SDL_SCANCODE_F2:  return 0x71;
            case SDL_SCANCODE_F3:  return 0x72;
            case SDL_SCANCODE_F4:  return 0x73;
            case SDL_SCANCODE_F5:  return 0x74;
            case SDL_SCANCODE_F6:  return 0x75;
            case SDL_SCANCODE_F7:  return 0x76;
            case SDL_SCANCODE_F8:  return 0x77;
            case SDL_SCANCODE_F9:  return 0x78;
            case SDL_SCANCODE_F10: return 0x79;
            case SDL_SCANCODE_F11: return 0x7A;
            case SDL_SCANCODE_F12: return 0x7B;
            default: return 0;
        }
    }

    CompatBitmap* AsCompatBitmap(HGDIOBJ object)
    {
        auto* bitmap = reinterpret_cast<CompatBitmap*>(object);
        if (!bitmap || bitmap->magic != kCompatBitmapMagic) {
            return nullptr;
        }
        return bitmap;
    }

    CompatDC* AsCompatDC(HDC dc)
    {
        auto* compatDc = reinterpret_cast<CompatDC*>(dc);
        if (!compatDc || compatDc->magic != kCompatDcMagic) {
            return nullptr;
        }
        return compatDc;
    }

    std::string NormalizePath(const char* path)
    {
        std::string normalized = path ? path : "";
        for (char& ch : normalized) {
            if (ch == '\\') {
                ch = '/';
            }
        }
        return normalized;
    }

    CompatBitmap* CreateCompatBitmapFromSurface(SDL_Surface* surface)
    {
        if (!surface) {
            return nullptr;
        }

        SDL_Surface* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (!rgbaSurface) {
            SDL_Log("free-api LoadImageA: SDL_ConvertSurface failed: %s", SDL_GetError());
            return nullptr;
        }
        g_diagSdlSurfaces.fetch_add(1, std::memory_order_relaxed);
        g_diagSdlSurfacesEver.fetch_add(1, std::memory_order_relaxed);

        auto* bitmap = new CompatBitmap{};
        g_diagCompatBitmaps.fetch_add(1, std::memory_order_relaxed);
        g_diagCompatBitmapsEver.fetch_add(1, std::memory_order_relaxed);
        bitmap->width = rgbaSurface->w;
        bitmap->height = rgbaSurface->h;
        bitmap->bitsPerPixel = 32;
        bitmap->pitch = bitmap->width * 4;
        bitmap->pixels.resize(static_cast<size_t>(bitmap->pitch) * static_cast<size_t>(bitmap->height));
        AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                            g_diagCompatBitmapPixelCapacityHighWaterBytes,
                            static_cast<int64_t>(bitmap->pixels.capacity()));

        for (int y = 0; y < bitmap->height; ++y) {
            const auto* srcRow = static_cast<const uint8_t*>(rgbaSurface->pixels) + static_cast<size_t>(y) * static_cast<size_t>(rgbaSurface->pitch);
            auto* dstRow = bitmap->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(bitmap->pitch);
            memcpy(dstRow, srcRow, static_cast<size_t>(bitmap->pitch));
        }

        SDL_DestroySurface(rgbaSurface);
        g_diagSdlSurfaces.fetch_sub(1, std::memory_order_relaxed);
        g_diagSdlSurfacesDestroyed.fetch_add(1, std::memory_order_relaxed);
        return bitmap;
    }

    void ScaleCompatBitmap(CompatBitmap& bitmap, const int targetWidth, const int targetHeight)
    {
        if (targetWidth <= 0 || targetHeight <= 0 || (targetWidth == bitmap.width && targetHeight == bitmap.height)) {
            return;
        }

        const size_t oldCapacity = bitmap.pixels.capacity();
        std::vector<uint8_t> scaled(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4u, 0);
        const int srcWidth = bitmap.width;
        const int srcHeight = bitmap.height;
        const int srcPitch = bitmap.pitch;
        const uint8_t* srcData = bitmap.pixels.data();

        for (int y = 0; y < targetHeight; ++y) {
            const int srcY = (y * srcHeight) / targetHeight;
            auto* dstRow = scaled.data() + static_cast<size_t>(y) * static_cast<size_t>(targetWidth) * 4u;
            for (int x = 0; x < targetWidth; ++x) {
                const int srcX = (x * srcWidth) / targetWidth;
                const auto* srcPixel = srcData + static_cast<size_t>(srcY) * static_cast<size_t>(srcPitch) + static_cast<size_t>(srcX) * 4u;
                auto* dstPixel = dstRow + static_cast<size_t>(x) * 4u;
                dstPixel[0] = srcPixel[0];
                dstPixel[1] = srcPixel[1];
                dstPixel[2] = srcPixel[2];
                dstPixel[3] = srcPixel[3];
            }
        }

        bitmap.width = targetWidth;
        bitmap.height = targetHeight;
        bitmap.pitch = targetWidth * 4;
        bitmap.pixels.swap(scaled);
        const auto delta = static_cast<int64_t>(bitmap.pixels.capacity()) - static_cast<int64_t>(oldCapacity);
        AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                            g_diagCompatBitmapPixelCapacityHighWaterBytes,
                            delta);
    }

    bool EnsureVideoSubsystem()
    {
        if (g_videoInitialized) {
            return true;
        }

        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            SDL_Log("free-api EnsureVideoSubsystem: SDL_INIT_VIDEO failed: %s", SDL_GetError());
            return false;
        }

        g_videoInitialized = true;
        SDL_Log("free-api EnsureVideoSubsystem: SDL video initialized");
        // Initialize debug input flag from environment
        const char* dbgInput = SDL_getenv("FREE_API_DEBUG_INPUT");
        const char* dbgMouse = SDL_getenv("FREE_API_DEBUG_MOUSE");
        const char* dbgReal  = SDL_getenv("FREE_API_DEBUG_REAL_INPUT");
        g_debugInput = (dbgInput && dbgInput[0] == '1')
            || (dbgMouse && dbgMouse[0] == '1')
            || (dbgReal  && dbgReal[0]  == '1');
        return true;
    }

    HWND FindWindowById(const SDL_WindowID windowId)
    {
        for (const auto& [hwnd, proc] : g_windowProcedures) {
            auto* sdlWindow = reinterpret_cast<SDL_Window*>(hwnd);
            if (sdlWindow && SDL_GetWindowID(sdlWindow) == windowId) {
                return hwnd;
            }
        }
        return NULL;
    }

    void PushMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        g_diagMessagesPosted.fetch_add(1, std::memory_order_relaxed);
        if (message == kDiagWmUpdate) {
            g_diagWmUpdatePosted.fetch_add(1, std::memory_order_relaxed);
            g_diagWmUpdatePending.fetch_add(1, std::memory_order_relaxed);
        }
        MSG msg{};
        msg.hwnd = hwnd;
        msg.message = message;
        msg.wParam = wParam;
        msg.lParam = lParam;
        msg.time = static_cast<DWORD>(SDL_GetTicks());
        msg.pt = {0, 0};
        size_t qsize;
        {
            std::lock_guard<std::mutex> lock(g_messageQueueMutex);
            g_messageQueue.push(msg);
            qsize = g_messageQueue.size();
        }
        InputLog("ENQUEUE hwnd=%p msg=0x%04X wParam=0x%X lParam=0x%X qsize=%d",
            (void*)hwnd, message, (unsigned)wParam, (unsigned)lParam, (int)qsize);
        FreeApiDiagTick();
    }

    void PumpSdlEvents()
    {
        // Make sure SDL has consumed pending OS events before we poll.
        SDL_PumpEvents();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            g_diagSdlEventsProcessed.fetch_add(1, std::memory_order_relaxed);
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    InputLog("SDL_EVENT_QUIT -> WM_QUIT");
                    PushMessage(NULL, WM_QUIT, 0, 0);
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                    HWND hwnd = FindWindowById(event.window.windowID);
                    InputLog("SDL_EVENT_WINDOW_CLOSE_REQUESTED hwnd=%p -> WM_CLOSE", (void*)hwnd);
                    PushMessage(hwnd, WM_CLOSE, 0, 0);
                    break;
                }
                case SDL_EVENT_WINDOW_FOCUS_GAINED: {
                    HWND hwnd = FindWindowById(event.window.windowID);
                    if (hwnd) g_focusWindow = hwnd;
                    InputLog("SDL_EVENT_WINDOW_FOCUS_GAINED hwnd=%p -> WM_ACTIVATEAPP(1)", (void*)hwnd);
                    PushMessage(hwnd, WM_ACTIVATEAPP, 1, 0);
                    break;
                }
                case SDL_EVENT_WINDOW_FOCUS_LOST: {
                    // Do NOT send WM_ACTIVATEAPP(0) — sending it causes the game to set
                    // g_bActive=FALSE which stops rendering and input processing.
                    // SDL focus-lost events may fire spuriously on startup or in certain
                    // desktop environments. The game window should remain active as long as
                    // it is visible. If real deactivation is needed in the future, add a
                    // configurable delay or a user-controlled flag.
                    HWND hwnd = FindWindowById(event.window.windowID);
                    InputLog("SDL_EVENT_WINDOW_FOCUS_LOST hwnd=%p -> suppressed WM_ACTIVATEAPP(0)", (void*)hwnd);
                    (void)hwnd;
                    break;
                }
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP: {
                    // Ignore repeat keydowns (SDL sends repeat events; pass them through
                    // as WM_KEYDOWN repeats matching Windows behavior)
                    HWND hwnd = GetActiveWindow();
                    if (!hwnd) break;
                    WPARAM vk = SdlScancodeToVK(event.key.scancode);
                    if (vk == 0) break; // unmapped key
                    UINT msg = (event.type == SDL_EVENT_KEY_DOWN) ? WM_KEYDOWN : WM_KEYUP;
                    // lParam: simplified — repeat count=1, scan code in bits 16-23
                    LPARAM lp = (LPARAM)(event.key.scancode << 16) | 1;
                    if (event.type == SDL_EVENT_KEY_UP) {
                        lp |= (1u << 30) | (1u << 31); // transition/previous-state bits
                    }
                    // Map F10 as WM_SYSKEYDOWN/UP to match Windows behavior
                    if (vk == 0x79 /* VK_F10 */) {
                        msg = (event.type == SDL_EVENT_KEY_DOWN) ? WM_SYSKEYDOWN : WM_SYSKEYUP;
                    }
                    InputLog("%s vk=0x%02X hwnd=%p -> msg=0x%04X",
                        event.type == SDL_EVENT_KEY_DOWN ? "KEY_DOWN" : "KEY_UP",
                        (unsigned)vk, (void*)hwnd, msg);
                    PushMessage(hwnd, msg, vk, lp);
                    break;
                }
                case SDL_EVENT_TEXT_INPUT: {
                    // Feed WM_CHAR for text input (name entry screens)
                    HWND hwnd = GetActiveWindow();
                    if (!hwnd) break;
                    const char* text = event.text.text;
                    while (*text) {
                        unsigned char ch = (unsigned char)*text++;
                        if (ch < 128) {
                            InputLog("TEXT_INPUT char=0x%02X hwnd=%p -> WM_CHAR", ch, (void*)hwnd);
                            PushMessage(hwnd, WM_CHAR, (WPARAM)ch, 1);
                        }
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION: {
                    HWND hwnd = FindWindowById(event.motion.windowID);
                    if (!hwnd) hwnd = GetActiveWindow();
                    InputLog("SDL_EVENT_MOUSE_MOTION windowID=%u resolvedHwnd=%p",
                        (unsigned)event.motion.windowID, (void*)hwnd);
                    if (!hwnd) break;
                    int x = (int)event.motion.x;
                    int y = (int)event.motion.y;
                    // lParam encodes client-area x/y
                    LPARAM lp = (LPARAM)(((WORD)(DWORD_PTR)y << 16) | ((WORD)(DWORD_PTR)x));
                    InputLog("MOUSE_MOTION x=%d y=%d wParam=0x%X -> WM_MOUSEMOVE",
                        x, y, (unsigned)g_mouseButtons);
                    PushMessage(hwnd, WM_MOUSEMOVE, g_mouseButtons, lp);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    HWND hwnd = FindWindowById(event.button.windowID);
                    if (!hwnd) hwnd = GetActiveWindow();
                    InputLog("SDL_EVENT_MOUSE_BUTTON windowID=%u resolvedHwnd=%p",
                        (unsigned)event.button.windowID, (void*)hwnd);
                    if (!hwnd) break;
                    int x = (int)event.button.x;
                    int y = (int)event.button.y;
                    LPARAM lp = (LPARAM)(((WORD)(DWORD_PTR)y << 16) | ((WORD)(DWORD_PTR)x));
                    bool isDown = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                    UINT msg = 0;
                    WPARAM mk = 0;
                    switch (event.button.button) {
                        case SDL_BUTTON_LEFT:
                            msg = isDown ? WM_LBUTTONDOWN : WM_LBUTTONUP;
                            mk = MK_LBUTTON;
                            break;
                        case SDL_BUTTON_RIGHT:
                            msg = isDown ? WM_RBUTTONDOWN : WM_RBUTTONUP;
                            mk = MK_RBUTTON;
                            break;
                        case SDL_BUTTON_MIDDLE:
                            msg = isDown ? WM_MBUTTONDOWN : WM_MBUTTONUP;
                            mk = MK_MBUTTON;
                            break;
                        default:
                            break;
                    }
                    if (!msg) break;
                    if (isDown) {
                        g_mouseButtons |= mk;
                    } else {
                        g_mouseButtons &= ~mk;
                    }
                    // Build wParam: current button state + keyboard modifiers
                    const bool* keys = SDL_GetKeyboardState(nullptr);
                    WPARAM wp = g_mouseButtons;
                    if (keys && (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]))   wp |= MK_SHIFT;
                    if (keys && (keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL]))    wp |= MK_CONTROL;
                    InputLog("MOUSE_BTN %s btn=%d x=%d y=%d wParam=0x%X -> msg=0x%04X",
                        isDown ? "DOWN" : "UP", event.button.button, x, y, (unsigned)wp, msg);
                    PushMessage(hwnd, msg, wp, lp);
                    break;
                }
                default:
                    if (g_debugInput) {
                        InputLog("SDL_EVENT type=0x%X (unhandled)", event.type);
                    }
                    break;
            }
        }
    }

    std::string BuildCommandLine(const int argc, char** argv)
    {
        std::string cmdLine;
        for (int i = 1; i < argc; ++i) {
            if (!cmdLine.empty()) {
                cmdLine += ' ';
            }
            if (argv[i]) {
                cmdLine += argv[i];
            }
        }
        return cmdLine;
    }
}

extern "C" {

char* _pgmptr = nullptr;

void WINAPI Sleep(DWORD dwMilliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
}

DWORD WINAPI GetTickCount(void) {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<DWORD>(ms.count());
}

void WINAPI GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer)
{
    if (!lpBuffer) {
        return;
    }

    lpBuffer->dwLength = sizeof(MEMORYSTATUS);
    lpBuffer->dwMemoryLoad = 25;
    lpBuffer->dwTotalPhys = 512u * 1024u * 1024u;
    lpBuffer->dwAvailPhys = 256u * 1024u * 1024u;
    lpBuffer->dwTotalPageFile = 1024u * 1024u * 1024u;
    lpBuffer->dwAvailPageFile = 512u * 1024u * 1024u;
    lpBuffer->dwTotalVirtual = 1024u * 1024u * 1024u;
    lpBuffer->dwAvailVirtual = 512u * 1024u * 1024u;
}

BOOL WINAPI CloseHandle(HANDLE hObject) {
    // placeholder for now
    return TRUE;
}

void WINAPI OutputDebugStringA(LPCSTR lpOutputString) {
    if (lpOutputString) {
        printf("%s", lpOutputString);
    }
}

void WINAPI OutputDebugStringW(LPCWSTR lpOutputString) {

    if (lpOutputString) {
        while (*lpOutputString) {
            printf("%c", (char)*lpOutputString++);
        }
    }
}

ATOM WINAPI RegisterClassA(const WNDCLASSA* lpWndClass)
{
    if (!lpWndClass || !lpWndClass->lpszClassName || !lpWndClass->lpfnWndProc) {
        return 0;
    }

    g_registeredClasses[lpWndClass->lpszClassName] = lpWndClass->lpfnWndProc;
    return 1;
}

HWND WINAPI CreateWindowExA(const DWORD dwExStyle,
                            LPCSTR lpClassName,
                            LPCSTR lpWindowName,
                            const DWORD dwStyle,
                            const int X,
                            const int Y,
                            const int nWidth,
                            const int nHeight,
                            HWND hWndParent,
                            HMENU hMenu,
                            HINSTANCE hInstance,
                            LPVOID lpParam)
{
    (void)dwExStyle;
    (void)hWndParent;
    (void)hMenu;
    (void)hInstance;
    (void)lpParam;

    SDL_Log("free-api CreateWindowExA: class=%s title=%s style=0x%08lx exStyle=0x%08lx pos=(%d,%d) size=%dx%d", 
            lpClassName ? lpClassName : "<null>",
            lpWindowName ? lpWindowName : "<null>",
            static_cast<unsigned long>(dwStyle),
            static_cast<unsigned long>(dwExStyle),
            X,
            Y,
            nWidth,
            nHeight);

    if (!lpClassName) {
        SDL_Log("free-api CreateWindowExA: missing class name");
        return NULL;
    }

    const auto classIt = g_registeredClasses.find(lpClassName);
    if (classIt == g_registeredClasses.end()) {
        SDL_Log("free-api CreateWindowExA: class not registered: %s", lpClassName);
        return NULL;
    }

    if (!EnsureVideoSubsystem()) {
        return NULL;
    }

    const int width = nWidth > 0 ? nWidth : 640;
    const int height = nHeight > 0 ? nHeight : 480;
    // Do NOT use SDL_WINDOW_RESIZABLE by default: on some Wayland/X11 compositors
    // a resizable popup window immediately receives a WM_CLOSE from the compositor.
    // Planet Blupi uses WS_POPUPWINDOW|WS_CAPTION (popup with title bar, fixed size).
    Uint32 flags = 0;

    if ((dwStyle & WS_VISIBLE) == 0) {
        flags |= SDL_WINDOW_HIDDEN;
    }

    if ((dwStyle & WS_POPUP) != 0 && (dwStyle & WS_CAPTION) == 0) {
        flags |= SDL_WINDOW_BORDERLESS;
    }

    SDL_Log("free-api SDL_CreateWindow: title=%s width=%d height=%d flags=0x%08x", 
            lpWindowName ? lpWindowName : lpClassName,
            width,
            height,
            static_cast<unsigned>(flags));
    auto* sdlWindow = SDL_CreateWindow(lpWindowName ? lpWindowName : lpClassName, width, height, flags);
    if (!sdlWindow) {
        SDL_Log("free-api SDL_CreateWindow failed: %s", SDL_GetError());
        return NULL;
    }

    SDL_Log("free-api SDL_CreateWindow result: window=%p id=%u", static_cast<void*>(sdlWindow), static_cast<unsigned>(SDL_GetWindowID(sdlWindow)));

    const int posX = (X < 0) ? SDL_WINDOWPOS_CENTERED : X;
    const int posY = (Y < 0) ? SDL_WINDOWPOS_CENTERED : Y;
    SDL_SetWindowPosition(sdlWindow, posX, posY);
    SDL_Log("free-api SDL_SetWindowPosition: window=%p x=%d y=%d", static_cast<void*>(sdlWindow), posX, posY);

    HWND hwnd = reinterpret_cast<HWND>(sdlWindow);
    g_windowProcedures[hwnd] = classIt->second;
    g_focusWindow = hwnd;

    CREATESTRUCTA createStruct{};
    createStruct.lpCreateParams = lpParam;
    createStruct.hInstance = hInstance;
    createStruct.hMenu = hMenu;
    createStruct.hwndParent = hWndParent;
    createStruct.cy = height;
    createStruct.cx = width;
    createStruct.y = Y;
    createStruct.x = X;
    createStruct.style = static_cast<LONG>(dwStyle);
    createStruct.lpszName = lpWindowName;
    createStruct.lpszClass = lpClassName;
    createStruct.dwExStyle = dwExStyle;
    classIt->second(hwnd, WM_CREATE, 0, reinterpret_cast<LPARAM>(&createStruct));

    SDL_Log("free-api CreateWindowExA result: hwnd=%p visible=%s popup=%s caption=%s", 
            hwnd,
            ((dwStyle & WS_VISIBLE) != 0) ? "yes" : "no",
            ((dwStyle & WS_POPUP) != 0) ? "yes" : "no",
            ((dwStyle & WS_CAPTION) != 0) ? "yes" : "no");

    return hwnd;
}

HWND WINAPI CreateWindowA(LPCSTR lpClassName,
                          LPCSTR lpWindowName,
                          DWORD dwStyle,
                          int X,
                          int Y,
                          int nWidth,
                          int nHeight,
                          HWND hWndParent,
                          HMENU hMenu,
                          HINSTANCE hInstance,
                          LPVOID lpParam)
{
    return CreateWindowExA(0,
                           lpClassName,
                           lpWindowName,
                           dwStyle,
                           X,
                           Y,
                           nWidth,
                           nHeight,
                           hWndParent,
                           hMenu,
                           hInstance,
                           lpParam);
}

BOOL WINAPI DestroyWindow(HWND hWnd)
{
    if (!hWnd) {
        return FALSE;
    }

    g_windowProcedures.erase(hWnd);
    SDL_DestroyWindow(reinterpret_cast<SDL_Window*>(hWnd));

    if (g_windowProcedures.empty() && g_videoInitialized) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        g_videoInitialized = false;
    }

    return TRUE;
}

BOOL WINAPI ShowWindow(HWND hWnd, int nCmdShow)
{
    if (!hWnd) {
        return FALSE;
    }

    auto* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    SDL_Log("free-api ShowWindow: hwnd=%p cmd=%d window=%p", hWnd, nCmdShow, static_cast<void*>(sdlWindow));
    switch (nCmdShow) {
        case SW_HIDE:
            SDL_HideWindow(sdlWindow);
            break;
        case 2: // SW_SHOWMINIMIZED
        case 6: // SW_MINIMIZE
            SDL_MinimizeWindow(sdlWindow);
            break;
        case 3: // SW_SHOWMAXIMIZED
            SDL_MaximizeWindow(sdlWindow);
            SDL_ShowWindow(sdlWindow);
            SDL_RaiseWindow(sdlWindow);
            break;
        case 9: // SW_RESTORE
            SDL_RestoreWindow(sdlWindow);
            SDL_ShowWindow(sdlWindow);
            SDL_RaiseWindow(sdlWindow);
            break;
        default:
            SDL_ShowWindow(sdlWindow);
            SDL_RaiseWindow(sdlWindow);
            break;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);
    SDL_Log("free-api ShowWindow applied: window=%p size=%dx%d", static_cast<void*>(sdlWindow), windowWidth, windowHeight);

    return TRUE;
}

BOOL WINAPI MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    (void)bRepaint;
    if (!hWnd) {
        return FALSE;
    }

    SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    SDL_SetWindowPosition(sdlWindow, X, Y);
    SDL_SetWindowSize(sdlWindow, nWidth, nHeight);
    return TRUE;
}

BOOL WINAPI InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase)
{
    (void)hWnd;
    (void)lpRect;
    (void)bErase;
    return TRUE;
}

BOOL WINAPI UpdateWindow(HWND hWnd)
{
    if (!hWnd) {
        return FALSE;
    }

    auto* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    SDL_RaiseWindow(sdlWindow);
    SDL_Log("free-api UpdateWindow: hwnd=%p raised window=%p", hWnd, static_cast<void*>(sdlWindow));
    return TRUE;
}

BOOL WINAPI SetWindowTextA(HWND hWnd, LPCSTR lpString)
{
    if (!hWnd) {
        return FALSE;
    }

    SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
    SDL_SetWindowTitle(sdlWindow, lpString ? lpString : "");
    return TRUE;
}

BOOL WINAPI PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    PushMessage(hWnd, Msg, wParam, lParam);
    return TRUE;
}

int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    (void)hWnd;
    (void)uType;

    const char* caption = lpCaption ? lpCaption : "Message";
    const char* text = lpText ? lpText : "";
    fprintf(stderr, "[MessageBoxA] %s: %s\n", caption, text);
    return 1;
}

BOOL WINAPI GetCursorPos(LPPOINT lpPoint)
{
    if (!lpPoint) {
        return FALSE;
    }

    float x = 0.0f;
    float y = 0.0f;
    SDL_GetGlobalMouseState(&x, &y);
    lpPoint->x = static_cast<LONG>(x);
    lpPoint->y = static_cast<LONG>(y);
    InputLog("GetCursorPos -> screen=(%d,%d)", (int)lpPoint->x, (int)lpPoint->y);
    return TRUE;
}

BOOL WINAPI ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
    if (!hWnd || !lpPoint) {
        return FALSE;
    }

    int x = 0;
    int y = 0;
    if (!SDL_GetWindowPosition(reinterpret_cast<SDL_Window*>(hWnd), &x, &y)) {
        return FALSE;
    }

    LONG inX = lpPoint->x;
    LONG inY = lpPoint->y;
    lpPoint->x -= x;
    lpPoint->y -= y;
    InputLog("ScreenToClient hwnd=%p win=(%d,%d) screen=(%d,%d) -> client=(%d,%d)",
        (void*)hWnd, x, y, (int)inX, (int)inY, (int)lpPoint->x, (int)lpPoint->y);
    return TRUE;
}

HCURSOR WINAPI SetCursor(HCURSOR hCursor)
{
    return hCursor;
}

int WINAPI ShowCursor(BOOL bShow)
{
    (void)bShow;
    return 0;
}

BOOL WINAPI ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
    if (!hWnd || !lpPoint) {
        return FALSE;
    }

    int x = 0;
    int y = 0;
    if (!SDL_GetWindowPosition(reinterpret_cast<SDL_Window*>(hWnd), &x, &y)) {
        return FALSE;
    }

    lpPoint->x += x;
    lpPoint->y += y;
    return TRUE;
}

BOOL WINAPI SetCursorPos(int X, int Y)
{
    return SDL_WarpMouseGlobal(static_cast<float>(X), static_cast<float>(Y));
}

int WINAPI LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax)
{
    (void)hInstance;
    if (!lpBuffer || cchBufferMax <= 0) {
        return 0;
    }

    int written = snprintf(lpBuffer, static_cast<size_t>(cchBufferMax), "RES_%u", uID);
    if (written < 0) {
        lpBuffer[0] = '\0';
        return 0;
    }
    if (written >= cchBufferMax) {
        return cchBufferMax - 1;
    }
    return written;
}

HCURSOR WINAPI LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    (void)hInstance;
    (void)lpCursorName;
    return reinterpret_cast<HCURSOR>(static_cast<uintptr_t>(1));
}

HICON WINAPI LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
    (void)hInstance;
    (void)lpIconName;
    return reinterpret_cast<HICON>(static_cast<uintptr_t>(1));
}

HBRUSH WINAPI GetStockBrush(int fnObject)
{
    return reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(fnObject + 1));
}

HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName)
{
    (void)lpModuleName;
    return reinterpret_cast<HMODULE>(static_cast<uintptr_t>(1));
}

HDC FreeApiCreateSurfaceDC(void* pixels, int width, int height, int pitch, int bitsPerPixel)
{
    if (!pixels || width <= 0 || height <= 0 || pitch <= 0 || bitsPerPixel != 32) {
        return NULL;
    }

    auto* dc = new CompatDC{};
    g_diagCompatDcs.fetch_add(1, std::memory_order_relaxed);
    g_diagCompatDcsEver.fetch_add(1, std::memory_order_relaxed);
    dc->kind = CompatDcKind::Surface;
    dc->surfacePixels = static_cast<uint8_t*>(pixels);
    dc->surfaceWidth = width;
    dc->surfaceHeight = height;
    dc->surfacePitch = pitch;
    dc->surfaceBitsPerPixel = bitsPerPixel;
    return reinterpret_cast<HDC>(dc);
}

BOOL FreeApiDestroySurfaceDC(HDC hdc)
{
    auto* dc = AsCompatDC(hdc);
    if (!dc) {
        return FALSE;
    }

    delete dc;
    g_diagCompatDcs.fetch_sub(1, std::memory_order_relaxed);
    g_diagCompatDcsDestroyed.fetch_add(1, std::memory_order_relaxed);
    return TRUE;
}

HANDLE WINAPI LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad)
{
    (void)hInst;

    if (type != IMAGE_BITMAP || !name) {
        return NULL;
    }

    if ((fuLoad & LR_LOADFROMFILE) == 0) {
        SDL_Log("free-api LoadImageA: resource bitmap loading is not implemented for '%s'", name);
        return NULL;
    }

    std::string normalizedPath = NormalizePath(name);
    SDL_Surface* loaded = SDL_LoadBMP(normalizedPath.c_str());
    if (!loaded) {
        SDL_Log("free-api LoadImageA: SDL_LoadBMP failed for '%s': %s", normalizedPath.c_str(), SDL_GetError());
        return NULL;
    }
    g_diagSdlSurfaces.fetch_add(1, std::memory_order_relaxed);
    g_diagSdlSurfacesEver.fetch_add(1, std::memory_order_relaxed);

    CompatBitmap* bitmap = CreateCompatBitmapFromSurface(loaded);
    SDL_DestroySurface(loaded);
    g_diagSdlSurfaces.fetch_sub(1, std::memory_order_relaxed);
    g_diagSdlSurfacesDestroyed.fetch_add(1, std::memory_order_relaxed);
    if (!bitmap) {
        return NULL;
    }

    if (cx > 0 && cy > 0) {
        ScaleCompatBitmap(*bitmap, cx, cy);
    }

    SDL_Log("free-api LoadImageA: loaded bitmap '%s' -> %dx%d", normalizedPath.c_str(), bitmap->width, bitmap->height);
    return reinterpret_cast<HANDLE>(bitmap);
}

int WINAPI GetObjectA(HANDLE h, int c, LPVOID pv)
{
    if (!h || !pv || c <= 0) {
        return 0;
    }

    CompatBitmap* bitmap = AsCompatBitmap(reinterpret_cast<HGDIOBJ>(h));
    if (!bitmap) {
        return 0;
    }

    BITMAP info{};
    info.bmType = 0;
    info.bmWidth = bitmap->width;
    info.bmHeight = bitmap->height;
    info.bmWidthBytes = bitmap->pitch;
    info.bmPlanes = 1;
    info.bmBitsPixel = static_cast<WORD>(bitmap->bitsPerPixel);
    info.bmBits = bitmap->pixels.data();

    const int copySize = c < static_cast<int>(sizeof(BITMAP)) ? c : static_cast<int>(sizeof(BITMAP));
    memcpy(pv, &info, static_cast<size_t>(copySize));
    return static_cast<int>(sizeof(BITMAP));
}

BOOL WINAPI DeleteObject(HGDIOBJ ho)
{
    auto* bitmap = AsCompatBitmap(ho);
    if (!bitmap) {
        return FALSE;
    }

    AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                        g_diagCompatBitmapPixelCapacityHighWaterBytes,
                        -static_cast<int64_t>(bitmap->pixels.capacity()));
    delete bitmap;
    g_diagCompatBitmaps.fetch_sub(1, std::memory_order_relaxed);
    g_diagCompatBitmapsDestroyed.fetch_add(1, std::memory_order_relaxed);
    return TRUE;
}

// TODO: CreateBitmap creates a GDI-compatible bitmap from raw pixel bits.
// Planet Blupi uses this for the minimap: it writes 8-bit or 16-bit pixels into
// a raw buffer, calls CreateBitmap, then passes the HBITMAP to DDConnectBitmap
// which reads dimensions via GetObject and blits via StretchBlt.
// We convert the source pixels to RGBA32 so the existing StretchBlt path works.
HBITMAP WINAPI CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitCount, const void* lpBits)
{
    (void)nPlanes; // always 1 for device-independent bitmaps
    if (nWidth <= 0 || nHeight <= 0) {
        return NULL;
    }

    auto* bitmap = new CompatBitmap{};
    g_diagCompatBitmaps.fetch_add(1, std::memory_order_relaxed);
    g_diagCompatBitmapsEver.fetch_add(1, std::memory_order_relaxed);
    bitmap->width = nWidth;
    bitmap->height = nHeight;
    bitmap->bitsPerPixel = 32; // store as RGBA32 internally
    bitmap->pitch = nWidth * 4;
    bitmap->pixels.resize(static_cast<size_t>(nWidth) * static_cast<size_t>(nHeight) * 4u, 0);
    AdjustDiagLiveBytes(g_diagCompatBitmapPixelCapacityBytes,
                        g_diagCompatBitmapPixelCapacityHighWaterBytes,
                        static_cast<int64_t>(bitmap->pixels.capacity()));

    if (lpBits) {
        if (nBitCount == 8) {
            // 8-bit indexed: store index as grey (palette expansion not yet supported)
            // TODO: apply palette if one is set
            const uint8_t* src = static_cast<const uint8_t*>(lpBits);
            uint8_t* dst = bitmap->pixels.data();
            for (int y = 0; y < nHeight; ++y) {
                for (int x = 0; x < nWidth; ++x) {
                    uint8_t idx = src[y * nWidth + x];
                    // Expand to RGBA: treat index as greyscale placeholder
                    dst[(y * nWidth + x) * 4 + 0] = idx;
                    dst[(y * nWidth + x) * 4 + 1] = idx;
                    dst[(y * nWidth + x) * 4 + 2] = idx;
                    dst[(y * nWidth + x) * 4 + 3] = 0xFF;
                }
            }
        } else if (nBitCount == 16) {
            // 16-bit RGB565: convert to RGBA32
            const uint16_t* src = static_cast<const uint16_t*>(lpBits);
            uint8_t* dst = bitmap->pixels.data();
            for (int y = 0; y < nHeight; ++y) {
                for (int x = 0; x < nWidth; ++x) {
                    uint16_t px = src[y * nWidth + x];
                    uint8_t r = static_cast<uint8_t>(((px >> 11) & 0x1F) * 255 / 31);
                    uint8_t g = static_cast<uint8_t>(((px >> 5)  & 0x3F) * 255 / 63);
                    uint8_t b = static_cast<uint8_t>(((px >> 0)  & 0x1F) * 255 / 31);
                    dst[(y * nWidth + x) * 4 + 0] = r;
                    dst[(y * nWidth + x) * 4 + 1] = g;
                    dst[(y * nWidth + x) * 4 + 2] = b;
                    dst[(y * nWidth + x) * 4 + 3] = 0xFF;
                }
            }
        } else if (nBitCount == 32) {
            const size_t byteCount = static_cast<size_t>(nWidth) * static_cast<size_t>(nHeight) * 4u;
            memcpy(bitmap->pixels.data(), lpBits, byteCount);
        } else {
            SDL_Log("free-api CreateBitmap: unsupported bpp=%u, pixels zeroed", nBitCount);
        }
    }

    return reinterpret_cast<HBITMAP>(bitmap);
}

HDC WINAPI CreateCompatibleDC(HDC hdc)
{
    (void)hdc;
    auto* dc = new CompatDC{};
    g_diagCompatDcs.fetch_add(1, std::memory_order_relaxed);
    g_diagCompatDcsEver.fetch_add(1, std::memory_order_relaxed);
    dc->kind = CompatDcKind::Memory;
    return reinterpret_cast<HDC>(dc);
}

HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ h)
{
    CompatDC* dc = AsCompatDC(hdc);
    if (!dc) {
        return NULL;
    }

    if (CompatBitmap* bitmap = AsCompatBitmap(h)) {
        HGDIOBJ previous = reinterpret_cast<HGDIOBJ>(dc->selectedBitmap);
        dc->selectedBitmap = bitmap;
        return previous;
    }

    return NULL;
}

BOOL WINAPI DeleteDC(HDC hdc)
{
    CompatDC* dc = AsCompatDC(hdc);
    if (!dc) {
        return FALSE;
    }

    delete dc;
    g_diagCompatDcs.fetch_sub(1, std::memory_order_relaxed);
    g_diagCompatDcsDestroyed.fetch_add(1, std::memory_order_relaxed);
    return TRUE;
}

BOOL WINAPI StretchBlt(HDC hdcDest,
                       int xDest,
                       int yDest,
                       int wDest,
                       int hDest,
                       HDC hdcSrc,
                       int xSrc,
                       int ySrc,
                       int wSrc,
                       int hSrc,
                       DWORD rop)
{
    if (rop != SRCCOPY) {
        SDL_Log("free-api StretchBlt: unsupported ROP=0x%08lx", static_cast<unsigned long>(rop));
        return FALSE;
    }

    CompatDC* dst = AsCompatDC(hdcDest);
    CompatDC* src = AsCompatDC(hdcSrc);
    if (!dst || !src || dst->kind != CompatDcKind::Surface || dst->surfaceBitsPerPixel != 32 || !dst->surfacePixels || !src->selectedBitmap) {
        SDL_Log("free-api StretchBlt: unsupported DC pair dst=%p src=%p", reinterpret_cast<void*>(hdcDest), reinterpret_cast<void*>(hdcSrc));
        return FALSE;
    }

    if (wDest <= 0 || hDest <= 0 || wSrc <= 0 || hSrc <= 0) {
        return FALSE;
    }

    CompatBitmap* srcBitmap = src->selectedBitmap;
    const uint8_t* srcPixels = srcBitmap->pixels.data();

    for (int y = 0; y < hDest; ++y) {
        const int dstY = yDest + y;
        if (dstY < 0 || dstY >= dst->surfaceHeight) {
            continue;
        }

        const int srcY = ySrc + static_cast<int>((static_cast<int64_t>(y) * static_cast<int64_t>(hSrc)) / static_cast<int64_t>(hDest));
        if (srcY < 0 || srcY >= srcBitmap->height) {
            continue;
        }

        auto* dstRow = dst->surfacePixels + static_cast<size_t>(dstY) * static_cast<size_t>(dst->surfacePitch);
        const auto* srcRow = srcPixels + static_cast<size_t>(srcY) * static_cast<size_t>(srcBitmap->pitch);

        for (int x = 0; x < wDest; ++x) {
            const int dstX = xDest + x;
            if (dstX < 0 || dstX >= dst->surfaceWidth) {
                continue;
            }

            const int srcX = xSrc + static_cast<int>((static_cast<int64_t>(x) * static_cast<int64_t>(wSrc)) / static_cast<int64_t>(wDest));
            if (srcX < 0 || srcX >= srcBitmap->width) {
                continue;
            }

            const auto* srcPixel = srcRow + static_cast<size_t>(srcX) * 4u;
            auto* dstPixel = dstRow + static_cast<size_t>(dstX) * 4u;
            dstPixel[0] = srcPixel[0];
            dstPixel[1] = srcPixel[1];
            dstPixel[2] = srcPixel[2];
            dstPixel[3] = 255;
        }
    }

    int dstSampleX = xDest;
    int dstSampleY = yDest;
    if (dstSampleX < 0) dstSampleX = 0;
    if (dstSampleY < 0) dstSampleY = 0;
    if (dstSampleX >= dst->surfaceWidth) dstSampleX = dst->surfaceWidth - 1;
    if (dstSampleY >= dst->surfaceHeight) dstSampleY = dst->surfaceHeight - 1;

    uint8_t dstR = 0;
    uint8_t dstG = 0;
    uint8_t dstB = 0;
    if (dst->surfaceWidth > 0 && dst->surfaceHeight > 0) {
        const auto* dstSample = dst->surfacePixels + static_cast<size_t>(dstSampleY) * static_cast<size_t>(dst->surfacePitch) + static_cast<size_t>(dstSampleX) * 4u;
        dstR = dstSample[0];
        dstG = dstSample[1];
        dstB = dstSample[2];
    }

    int srcSampleX = xSrc;
    int srcSampleY = ySrc;
    if (srcSampleX < 0) srcSampleX = 0;
    if (srcSampleY < 0) srcSampleY = 0;
    if (srcSampleX >= srcBitmap->width) srcSampleX = srcBitmap->width - 1;
    if (srcSampleY >= srcBitmap->height) srcSampleY = srcBitmap->height - 1;

    uint8_t srcR = 0;
    uint8_t srcG = 0;
    uint8_t srcB = 0;
    if (srcBitmap->width > 0 && srcBitmap->height > 0) {
        const auto* srcSample = srcPixels + static_cast<size_t>(srcSampleY) * static_cast<size_t>(srcBitmap->pitch) + static_cast<size_t>(srcSampleX) * 4u;
        srcR = srcSample[0];
        srcG = srcSample[1];
        srcB = srcSample[2];
    }

    SDL_Log("free-api StretchBlt: copied src=%dx%d[%d,%d] rgb=(%u,%u,%u) to dst=%dx%d[%d,%d] rgb=(%u,%u,%u)",
            wSrc,
            hSrc,
            xSrc,
            ySrc,
            static_cast<unsigned>(srcR),
            static_cast<unsigned>(srcG),
            static_cast<unsigned>(srcB),
            wDest,
            hDest,
            xDest,
            yDest,
            static_cast<unsigned>(dstR),
            static_cast<unsigned>(dstG),
            static_cast<unsigned>(dstB));
    return TRUE;
}

COLORREF WINAPI GetPixel(HDC hdc, int x, int y)
{
    CompatDC* dc = AsCompatDC(hdc);
    if (!dc) {
        return 0;
    }

    const uint8_t* pixel = nullptr;
    if (dc->kind == CompatDcKind::Surface) {
        if (x < 0 || y < 0 || x >= dc->surfaceWidth || y >= dc->surfaceHeight || !dc->surfacePixels) {
            return 0;
        }
        pixel = dc->surfacePixels + static_cast<size_t>(y) * static_cast<size_t>(dc->surfacePitch) + static_cast<size_t>(x) * 4u;
    } else if (dc->selectedBitmap) {
        if (x < 0 || y < 0 || x >= dc->selectedBitmap->width || y >= dc->selectedBitmap->height) {
            return 0;
        }
        pixel = dc->selectedBitmap->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(dc->selectedBitmap->pitch) + static_cast<size_t>(x) * 4u;
    }

    if (!pixel) {
        return 0;
    }

    return RGB(pixel[0], pixel[1], pixel[2]);
}

COLORREF WINAPI SetPixel(HDC hdc, int x, int y, COLORREF color)
{
    CompatDC* dc = AsCompatDC(hdc);
    if (!dc) {
        return color;
    }

    uint8_t* pixel = nullptr;
    if (dc->kind == CompatDcKind::Surface) {
        if (x < 0 || y < 0 || x >= dc->surfaceWidth || y >= dc->surfaceHeight || !dc->surfacePixels) {
            return color;
        }
        pixel = dc->surfacePixels + static_cast<size_t>(y) * static_cast<size_t>(dc->surfacePitch) + static_cast<size_t>(x) * 4u;
    } else if (dc->selectedBitmap) {
        if (x < 0 || y < 0 || x >= dc->selectedBitmap->width || y >= dc->selectedBitmap->height) {
            return color;
        }
        pixel = dc->selectedBitmap->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(dc->selectedBitmap->pitch) + static_cast<size_t>(x) * 4u;
    }

    if (!pixel) {
        return color;
    }

    // COLORREF layout is 0x00BBGGRR; surface pixel layout is RGBA.
    pixel[0] = static_cast<uint8_t>(color & 0xFFu);          // R
    pixel[1] = static_cast<uint8_t>((color >> 8) & 0xFFu);   // G
    pixel[2] = static_cast<uint8_t>((color >> 16) & 0xFFu);  // B
    pixel[3] = 255;
    return color;
}

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
        UINT value = (iStartIndex + i) & 0xFFu;
        entries[i].peRed = static_cast<BYTE>(value);
        entries[i].peGreen = static_cast<BYTE>(value);
        entries[i].peBlue = static_cast<BYTE>(value);
        entries[i].peFlags = 0;
    }
    return nEntries;
}

BOOL WINAPI GetClientRect(HWND hWnd, LPRECT lpRect)
{
    if (!hWnd || !lpRect) {
        return FALSE;
    }

    int w = 0;
    int h = 0;
    if (!SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(hWnd), &w, &h)) {
        return FALSE;
    }

    lpRect->left = 0;
    lpRect->top = 0;
    lpRect->right = w;
    lpRect->bottom = h;
    return TRUE;
}

HRSRC WINAPI FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType)
{
    (void)hModule;
    (void)lpName;
    (void)lpType;
    return NULL;
}

HGLOBAL WINAPI LoadResource(HMODULE hModule, HRSRC hResInfo)
{
    (void)hModule;
    (void)hResInfo;
    return NULL;
}

DWORD WINAPI SizeofResource(HMODULE hModule, HRSRC hResInfo)
{
    (void)hModule;
    (void)hResInfo;
    return 0;
}

LPVOID WINAPI LockResource(HGLOBAL hResData)
{
    return hResData;
}

int WINAPI _lopen(LPCSTR lpPathName, int iReadWrite)
{
    int flags = O_RDONLY;
    (void)iReadWrite;
    return open(lpPathName, flags);
}

UINT WINAPI _lread(int hFile, LPVOID lpBuffer, UINT uBytes)
{
    if (!lpBuffer) {
        return 0;
    }

    ssize_t bytesRead = read(hFile, lpBuffer, uBytes);
    return bytesRead > 0 ? static_cast<UINT>(bytesRead) : 0;
}

int WINAPI _lclose(int hFile)
{
    return close(hFile);
}

intptr_t _findfirst(const char* filespec, struct _finddata_t* fileinfo)
{
    (void)filespec;
    if (fileinfo) {
        memset(fileinfo, 0, sizeof(*fileinfo));
    }
    return -1;
}

int _findnext(intptr_t handle, struct _finddata_t* fileinfo)
{
    (void)handle;
    (void)fileinfo;
    return -1;
}

int _findclose(intptr_t handle)
{
    (void)handle;
    return 0;
}

BOOL WINAPI DeleteFileA(LPCSTR lpFileName)
{
    if (!lpFileName) {
        return FALSE;
    }
    return remove(lpFileName) == 0 ? TRUE : FALSE;
}

// TODO: CreateDirectoryA - creates directory hierarchy, ignores SECURITY_ATTRIBUTES
BOOL WINAPI CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    (void)lpSecurityAttributes; // security descriptors not supported on Linux
    if (!lpPathName) {
        return FALSE;
    }
    // Convert backslashes to forward slashes and remove drive letter
    std::string path(lpPathName);
    if (path.size() >= 2 && isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        path.erase(0, 2);
    }
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    while (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
        path.erase(0, 1);
    }

    if (path.empty()) return TRUE; // Already exists (root)

    // mkdir returns 0 on success, -1 on error (EEXIST is treated as success)
    int rc = mkdir(path.c_str(), 0755);
    if (rc == 0 || errno == EEXIST) {
        return TRUE;
    }
    SDL_Log("free-api CreateDirectoryA: failed to create '%s' (orig: '%s'): %s", path.c_str(), lpPathName, strerror(errno));
    return FALSE;
}

BOOL WINAPI UnlockResource(HGLOBAL hResData)
{
    (void)hResData;
    return FALSE;
}

BOOL WINAPI FreeResource(HGLOBAL hResData)
{
    (void)hResData;
    return FALSE;
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

int WINAPI GetSystemMetrics(int nIndex)
{
    if (nIndex == SM_CXSCREEN) {
        return 1024;
    }

    if (nIndex == SM_CYSCREEN) {
        return 768;
    }

    if (nIndex == SM_CYCAPTION) {
        return 24;
    }

    return 0;
}

BOOL WINAPI AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    (void)dwStyle;
    (void)bMenu;
    return lpRect ? TRUE : FALSE;
}

HWND WINAPI SetFocus(HWND hWnd)
{
    HWND oldFocus = g_focusWindow;
    g_focusWindow = hWnd;
    if (hWnd) {
        auto* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
        SDL_RaiseWindow(sdlWindow);
        SDL_Log("free-api SetFocus: old=%p new=%p window=%p", oldFocus, hWnd, static_cast<void*>(sdlWindow));
    } else {
        SDL_Log("free-api SetFocus: old=%p new=<null>", oldFocus);
    }
    return oldFocus;
}

UINT_PTR WINAPI SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc)
{
    (void)lpTimerFunc;

    if (nIDEvent == 0) {
        nIDEvent = g_nextTimerId.fetch_add(1);
    }

    WinTimer wt;
    wt.hwnd = hWnd;
    wt.id = nIDEvent;
    wt.intervalMs = (uElapse > 0) ? uElapse : 1;
    wt.lastFireTick = SDL_GetTicks();

    {
        std::lock_guard<std::mutex> lock(g_winTimerMutex);
        g_winTimers[nIDEvent] = wt;
    }
    SDL_Log("free-api SetTimer: hwnd=%p id=%lu elapse=%u ms",
            static_cast<void*>(hWnd),
            static_cast<unsigned long>(nIDEvent),
            static_cast<unsigned>(uElapse));
    FreeApiDiagSnapshot("timer-set");
    return nIDEvent;
}

BOOL WINAPI KillTimer(HWND hWnd, UINT_PTR uIDEvent)
{
    (void)hWnd;
    {
        std::lock_guard<std::mutex> lock(g_winTimerMutex);
        g_winTimers.erase(uIDEvent);
    }
    SDL_Log("free-api KillTimer: hwnd=%p id=%lu",
            static_cast<void*>(hWnd),
            static_cast<unsigned long>(uIDEvent));
    FreeApiDiagSnapshot("timer-kill");
    return TRUE;
}

BOOL WINAPI PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    (void)hWnd;
    (void)wMsgFilterMin;
    (void)wMsgFilterMax;
    FreeApiDiagTick();

    if (!lpMsg) {
        return FALSE;
    }

    // Always pump SDL events so real OS mouse/keyboard events cannot be starved
    // by other queued messages (e.g. timer-style messages) staying ahead in the queue.
    PumpSdlEvents();

    // Generate WM_TIMER messages for elapsed WinAPI timers.
    // Collect into a local vector first to avoid nested locking.
    {
        std::vector<MSG> pendingTimers;
        {
            std::lock_guard<std::mutex> timerLock(g_winTimerMutex);
            const uint64_t now = SDL_GetTicks();
            for (auto& [id, wt] : g_winTimers) {
                if (now - wt.lastFireTick >= wt.intervalMs) {
                    wt.lastFireTick = now;
                    MSG timerMsg{};
                    timerMsg.hwnd = wt.hwnd;
                    timerMsg.message = WM_TIMER;
                    timerMsg.wParam = static_cast<WPARAM>(wt.id);
                    timerMsg.lParam = 0;
                    pendingTimers.push_back(timerMsg);
                }
            }
        }
        if (!pendingTimers.empty()) {
            std::lock_guard<std::mutex> qLock(g_messageQueueMutex);
            for (auto& m : pendingTimers) {
                g_diagMessagesPosted.fetch_add(1, std::memory_order_relaxed);
                g_messageQueue.push(m);
            }
        }
    }

    std::lock_guard<std::mutex> lock(g_messageQueueMutex);
    if (g_messageQueue.empty()) {
        // Yield CPU briefly to avoid busy-spinning in the main game loop.
        SDL_Delay(1);
        return FALSE;
    }

    *lpMsg = g_messageQueue.front();
    if ((wRemoveMsg & PM_REMOVE) != 0) {
        g_messageQueue.pop();
        if (lpMsg->message == kDiagWmUpdate) {
            g_diagWmUpdatePending.fetch_sub(1, std::memory_order_relaxed);
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
                return FALSE;
            }
            return TRUE;
        }

        SDL_Delay(1);
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

    g_diagMessagesDispatched.fetch_add(1, std::memory_order_relaxed);
    if (lpMsg->message == kDiagWmUpdate) {
        g_diagWmUpdateDispatched.fetch_add(1, std::memory_order_relaxed);
    }
    FreeApiDiagTick();

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
        DestroyWindow(hWnd);
        PostQuitMessage(0);
        return 0;
    }

    if (Msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return 0;
}

void WINAPI PostQuitMessage(int nExitCode)
{
    PushMessage(NULL, WM_QUIT, static_cast<WPARAM>(nExitCode), 0);
}

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

    SDL_Delay(1);
    PumpSdlEvents();
    return TRUE;
}

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
        cb = it->second.callback;
        mmId = it->second.mmId;
        user = it->second.user;
    }
    if (cb) {
        cb(mmId, 0, static_cast<DWORD>(user), 0, 0);
    }
    return interval;
}

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
        auto& entry = g_mmTimers[timerId];
        entry.mmId = timerId;
        entry.callback = lpTimeProc;
        entry.user = dwUser;
        entry.sdlId = 0;
        entryPtr = &entry;
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
    // TODO: MCI_OPEN with MCI_OPEN_TYPE only (no MCI_OPEN_ELEMENT) is an
    // "open device class" call, used by CMovie::initAVI() for "avivideo".
    // AVI video playback is not implemented; return an error so the game sets
    // m_bEnable=FALSE and skips movie playback gracefully.
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

int WINAPI FreeApiRunWinMain(FREE_API_WINMAIN_PROC entryPoint, int argc, char** argv)
{
    if (!entryPoint) {
        return -1;
    }

    if (argc > 0 && argv && argv[0]) {
        _pgmptr = argv[0];
    }

    std::string commandLine = BuildCommandLine(argc, argv);
    return entryPoint(NULL, NULL, commandLine.empty() ? NULL : commandLine.data(), SW_SHOW);
}

}

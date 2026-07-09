#include "internal/FreeApiSdlVideo.hpp"
#include "internal/FreeApiDiagnostics.hpp"
#include "internal/FreeApiMessageQueue.hpp"
#include "internal/FreeApiWindowRegistry.hpp"

#include <SDL3/SDL.h>

namespace FreeApi::Internal {

namespace {
    bool g_videoInitialized = false;
}

bool EnsureVideoSubsystem()
{
    if (g_videoInitialized) {
        return true;
    }

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        SDL_Log("free-api EnsureVideoSubsystem: SDL_INIT_VIDEO failed: %s", SDL_GetError());  // sdl-log-gating: intentional (failure)
        return false;
    }

    g_videoInitialized = true;
    if (FreeApiDiagnosticsEnabled()) {
        SDL_Log("free-api EnsureVideoSubsystem: SDL video initialized");
    }
    // Initialize debug input flag from environment
    const char* dbgInput = SDL_getenv("FREE_API_DEBUG_INPUT");
    const char* dbgMouse = SDL_getenv("FREE_API_DEBUG_MOUSE");
    const char* dbgReal  = SDL_getenv("FREE_API_DEBUG_REAL_INPUT");
    // TASK-24H-1243: relaxed, not the implicit-conversion default
    // (seq_cst) -- g_debugInput is a debug on/off flag that synchronizes no
    // other data, so relaxed is sufficient and free on ARM/Android (which
    // this codebase explicitly supports), unlike a seq_cst store/load pair.
    g_debugInput.store((dbgInput && dbgInput[0] == '1')
        || (dbgMouse && dbgMouse[0] == '1')
        || (dbgReal  && dbgReal[0]  == '1'), std::memory_order_relaxed);
    return true;
}

void ShutdownVideoSubsystemIfLastWindow()
{
    if (g_windowProcedures.empty() && g_videoInitialized) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        g_videoInitialized = false;
    }
}

} // namespace FreeApi::Internal

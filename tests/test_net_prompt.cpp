/**
 * @file test_net_prompt.cpp
 * @brief The native connection dialog of Free Direct's "FreeDirect server" provider
 *        (FreeApiPromptServerConnection, FreeApiShowRoomCreated in free_api_bridge.h).
 *
 * Drives the dialog with queued SDL events, as a player would with the keyboard, in a window
 * created through Free API with a 640x480 logical presentation like Free Direct's. Runs with
 * SDL's dummy video driver. With FREE_API_DIALOG_SNAPSHOT=<file.bmp> each dialog's first frame
 * is saved for a visual check.
 */
#include <windows.h>
#include <free_api_bridge.h>
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* expr, int line) {
    if (!condition) {
        std::printf("[net-prompt-test] FAIL: %s (line %d)\n", expr, line);
        ++g_failures;
    }
}
#define CHECK(expr) Check((expr), #expr, __LINE__)

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

SDL_Window* g_window = nullptr;

void Text(const char* text) {
    SDL_Event ev{};
    ev.type = SDL_EVENT_TEXT_INPUT;
    ev.text.windowID = SDL_GetWindowID(g_window);
    ev.text.text = text;  // a literal SDL never tries to free (see test_input_pipeline.cpp)
    SDL_PushEvent(&ev);
}

void Key(SDL_Keycode key, SDL_Keymod mod = SDL_KMOD_NONE) {
    SDL_Event ev{};
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.windowID = SDL_GetWindowID(g_window);
    ev.key.key = key;
    ev.key.mod = mod;
    ev.key.down = true;
    SDL_PushEvent(&ev);
}

void Snapshot(const char* name) {
    const char* dir = SDL_getenv("NET_PROMPT_SNAPSHOT_DIR");
    if (!dir) return;
    const std::string path = std::string(dir) + "/" + name + ".bmp";
    SDL_setenv_unsafe("FREE_API_DIALOG_SNAPSHOT", path.c_str(), 1);
}

void NoSnapshot() { SDL_setenv_unsafe("FREE_API_DIALOG_SNAPSHOT", "", 1); }

} // namespace

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("[net-prompt-test] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = "NetPromptTest";
    wc.hInstance = (HINSTANCE)1;
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, "NetPromptTest", "NetPromptTest", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480,
                                nullptr, nullptr, (HINSTANCE)1, nullptr);
    CHECK(hwnd != nullptr);
    g_window = reinterpret_cast<SDL_Window*>(hwnd);
    SDL_Renderer* renderer = SDL_CreateRenderer(g_window, nullptr);
    CHECK(renderer != nullptr);
    SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // 1. Join a room: type the server, skip the prefilled port, type a password, choose
    //    "Join" with Enter, type the room ID, connect with Enter.
    {
        FreeApiServerConnection c{};
        std::snprintf(c.port, sizeof(c.port), "51325");
        c.createRoom = 1;
        std::snprintf(c.message, sizeof(c.message), "cannot connect to ws://old.example:51325/: connection refused");
        Text("games.example.org");
        Key(SDLK_TAB);
        Key(SDLK_TAB);
        Text("secret");
        Key(SDLK_TAB);
        Key(SDLK_TAB);
        Key(SDLK_RETURN);  // on "Join": selects it and moves to the room ID
        Text("abcde-fghjk-");
        Text("mnpqr-stvwx");
        Key(SDLK_RETURN);
        Snapshot("net-prompt-join");
        CHECK(FreeApiPromptServerConnection(&c) == TRUE);
        NoSnapshot();
        CHECK(std::strcmp(c.server, "games.example.org") == 0);
        CHECK(std::strcmp(c.port, "51325") == 0);
        CHECK(std::strcmp(c.password, "secret") == 0);
        CHECK(std::strcmp(c.roomId, "abcde-fghjk-mnpqr-stvwx") == 0);
        CHECK(c.createRoom == 0);
    }

    // 2. Editing: Backspace, digits only in the port, Ctrl+V pastes into the focused field.
    {
        FreeApiServerConnection c{};
        std::snprintf(c.server, sizeof(c.server), "host.example");
        std::snprintf(c.port, sizeof(c.port), "1");
        c.createRoom = 1;
        SDL_SetClipboardText("WXYZ");
        // The dialog starts on "Connect" when the server is known and a room is to be created.
        Key(SDLK_TAB, SDL_KMOD_LSHIFT);  // -> Room ID
        Key(SDLK_TAB, SDL_KMOD_LSHIFT);  // -> Join (moving over it does not select it)
        Key(SDLK_TAB, SDL_KMOD_LSHIFT);  // -> Create
        Key(SDLK_TAB, SDL_KMOD_LSHIFT);  // -> Password
        Key(SDLK_TAB, SDL_KMOD_LSHIFT);  // -> Port
        Key(SDLK_BACKSPACE);
        Text("6a0");  // the letter is dropped
        Key(SDLK_TAB, SDL_KMOD_LSHIFT);  // -> Server
        Key(SDLK_V, SDL_KMOD_LCTRL);
        Key(SDLK_RETURN);
        CHECK(FreeApiPromptServerConnection(&c) == TRUE);
        CHECK(std::strcmp(c.server, "host.exampleWXYZ") == 0);
        CHECK(std::strcmp(c.port, "60") == 0);
        CHECK(c.createRoom == 1);
    }

    // 3. Escape cancels and leaves the fields unchanged.
    {
        FreeApiServerConnection c{};
        std::snprintf(c.server, sizeof(c.server), "unchanged");
        Text("typed");
        Key(SDLK_ESCAPE);
        CHECK(FreeApiPromptServerConnection(&c) == FALSE);
        CHECK(std::strcmp(c.server, "unchanged") == 0);
    }

    // 4. The new room's ID: copied to the clipboard, dismissed with Enter.
    {
        SDL_SetClipboardText("");
        Key(SDLK_RETURN);
        Snapshot("net-prompt-room");
        FreeApiShowRoomCreated("ABCDE-FGHJK-MNPQR-STVWX", "ws://games.example.org:51325/");
        NoSnapshot();
        char* clip = SDL_GetClipboardText();
        CHECK(clip && std::strcmp(clip, "ABCDE-FGHJK-MNPQR-STVWX") == 0);
        SDL_free(clip);
    }

    SDL_DestroyRenderer(renderer);
    DestroyWindow(hwnd);
    SDL_Quit();
    if (g_failures == 0) {
        std::printf("[net-prompt-test] ALL TESTS PASSED\n");
        return 0;
    }
    return 1;
}

/**
 * @file winuser_netprompt.cpp
 * @brief The connection dialog of Free Direct's "FreeDirect server" provider
 *        (FreeApiPromptServerConnection, FreeApiShowRoomCreated in free_api_bridge.h).
 *
 * Not WinAPI: like the rest of free_api_bridge.h this exists for ../free-direct. The original
 * DirectPlay TCP/IP service provider likewise showed its own dialog asking for the host's
 * address; Free Direct's Internet provider asks for its server and room the same way, so the
 * games' unchanged code never needs to know.
 *
 * Native: a modal screen drawn into the game's window with SDL's built-in 8x8 font, in the
 * renderer's logical coordinates, so it shows in a window and in full screen alike. The game is
 * blocked in its DirectPlay call meanwhile and repaints its screen afterwards.
 *
 * Web: an HTML form over the page (Asyncify waits for it). Keyboard events inside it do not reach
 * SDL, so the fields can be typed in. The JavaScript avoids backslashes: the preprocessor that
 * turns an EM_JS body into a string drops those outside string literals.
 *
 * Kept in its own translation unit: a static-library consumer that never calls these functions
 * (every game without the server provider) links none of it.
 * @note Status: IMPLEMENTED
 */
#include "windows.h"
#include "free_api_bridge.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdlib>
#else
#include <SDL3/SDL.h>
#include "internal/FreeApiWindowRegistry.hpp"
#endif

namespace {

void CopyOut(char* dest, std::size_t size, const std::string& value) {
    std::snprintf(dest, size, "%s", value.c_str());
}

#ifdef __EMSCRIPTEN__

// Returns a malloc'ed "server\x1fport\x1fpassword\x1froomId\x1fcreate" string, or 0 on cancel.
EM_ASYNC_JS(char*, FreeApiWebPromptServer,
            (const char* serverPtr, const char* portPtr, const char* passwordPtr, const char* roomPtr,
             int createRoom, const char* messagePtr), {
    const initial = {
        server: UTF8ToString(serverPtr), port: UTF8ToString(portPtr), password: UTF8ToString(passwordPtr),
        roomId: UTF8ToString(roomPtr), createRoom: createRoom !== 0, message: UTF8ToString(messagePtr)
    };
    const params = new URLSearchParams(typeof location !== 'undefined' ? location.search : '');
    let stored = {};
    try { stored = JSON.parse(localStorage.getItem('freeDirectServer') || '{}'); } catch (e) {}
    if (!initial.server) initial.server = params.get('server') || stored.server || '';
    if (!initial.port || (params.get('port') && !UTF8ToString(serverPtr))) initial.port = params.get('port') || stored.port || initial.port;
    if (!initial.roomId && params.get('room')) { initial.roomId = params.get('room'); initial.createRoom = false; }

    let result;
    if (typeof Module !== 'undefined' && typeof Module.freeApiServerPrompt === 'function') {
        result = await Module.freeApiServerPrompt(initial);
    } else {
        result = await new Promise((resolve) => {
            const esc = (t) => Array.from(String(t), (c) => ('&<>' + String.fromCharCode(34)).includes(c) ? '&#' + c.charCodeAt(0) + ';' : c).join('');
            const root = document.createElement('div');
            root.className = 'free-direct-dialog';
            root.setAttribute('role', 'dialog');
            root.setAttribute('aria-modal', 'true');
            root.style.cssText = 'position:fixed;inset:0;z-index:2147483000;display:flex;align-items:center;' +
                'justify-content:center;background:rgba(0,0,0,.6);font:15px/1.4 system-ui,sans-serif;color:#eef2ff;padding:16px;';
            root.innerHTML =
                '<form style="background:#1c2540;border:1px solid #5b6b99;border-radius:10px;padding:20px 22px;' +
                'width:min(460px,100%);max-height:100%;overflow:auto;box-shadow:0 10px 40px rgba(0,0,0,.5)">' +
                '<h2 style="margin:0 0 4px;font-size:20px">Free Direct server</h2>' +
                '<p style="margin:0 0 14px;color:#b8c2e0">Internet multiplayer through a Free Direct server.</p>' +
                '<label style="display:block;margin:8px 0 2px">Server (address or URL)</label>' +
                '<input name="server" autocomplete="url" style="width:100%;box-sizing:border-box;padding:7px" value="' + esc(initial.server) + '">' +
                '<label style="display:block;margin:8px 0 2px">Port</label>' +
                '<input name="port" inputmode="numeric" style="width:8em;padding:7px" value="' + esc(initial.port) + '">' +
                '<label style="display:block;margin:8px 0 2px">Server password (if it has one)</label>' +
                '<input name="password" type="password" autocomplete="current-password" style="width:100%;box-sizing:border-box;padding:7px" value="' + esc(initial.password) + '">' +
                '<fieldset style="border:1px solid #3c4a75;border-radius:6px;margin:12px 0 0;padding:8px 10px">' +
                '<legend>Room</legend>' +
                '<label style="display:block"><input type="radio" name="mode" value="create"> Create a new room</label>' +
                '<label style="display:block;margin-top:4px"><input type="radio" name="mode" value="join"> Join a room with its ID</label>' +
                '<input name="roomId" placeholder="XXXXX-XXXXX-XXXXX-XXXXX" style="width:100%;box-sizing:border-box;padding:7px;margin-top:6px;font-family:ui-monospace,monospace" value="' + esc(initial.roomId) + '">' +
                '</fieldset>' +
                '<p class="free-direct-message" style="color:#ffb4a8;margin:10px 0 0;min-height:1em">' + esc(initial.message) + '</p>' +
                '<div style="display:flex;gap:10px;justify-content:flex-end;margin-top:14px">' +
                '<button type="button" name="cancel" style="padding:8px 16px">Cancel</button>' +
                '<button type="submit" style="padding:8px 16px;font-weight:600">Connect</button></div></form>';
            const form = root.querySelector('form');
            const mode = form.querySelectorAll('input[name=mode]');
            mode[initial.createRoom ? 0 : 1].checked = true;
            form.roomId.addEventListener('input', () => { mode[1].checked = true; });
            // Keep keys away from SDL's window-level listeners, so the fields receive them.
            for (const type of ['keydown', 'keyup', 'keypress']) root.addEventListener(type, (e) => e.stopPropagation());
            const finish = (value) => { root.remove(); resolve(value); };
            form.addEventListener('submit', (e) => {
                e.preventDefault();
                finish({ server: form.server.value, port: form.port.value, password: form.password.value,
                         roomId: form.roomId.value, createRoom: mode[0].checked });
            });
            form.cancel.addEventListener('click', () => finish(null));
            root.addEventListener('keydown', (e) => { if (e.key === 'Escape') finish(null); });
            document.body.appendChild(root);
            (initial.server ? (initial.createRoom || initial.roomId ? form.querySelector('button[type=submit]') : form.roomId) : form.server).focus();
        });
    }
    if (!result) return 0;
    try { localStorage.setItem('freeDirectServer', JSON.stringify({ server: result.server, port: result.port })); } catch (e) {}
    const sep = String.fromCharCode(31);
    return stringToNewUTF8([result.server || '', result.port || '', result.password || '', result.roomId || '',
                            result.createRoom ? '1' : '0'].join(sep));
});

EM_ASYNC_JS(void, FreeApiWebShowRoom, (const char* roomPtr, const char* urlPtr), {
    const roomId = UTF8ToString(roomPtr);
    const serverUrl = UTF8ToString(urlPtr);
    if (typeof Module !== 'undefined' && typeof Module.freeApiShowRoomCreated === 'function') {
        await Module.freeApiShowRoomCreated(roomId, serverUrl);
        return;
    }
    let link = '';
    try {
        const u = new URL(location.href);
        u.searchParams.set('server', serverUrl.endsWith('/') ? serverUrl.slice(0, -1) : serverUrl);
        u.searchParams.delete('port');
        u.searchParams.set('room', roomId);
        link = u.toString();
    } catch (e) {}
    try { await navigator.clipboard.writeText(roomId); } catch (e) {}
    await new Promise((resolve) => {
        const esc = (t) => Array.from(String(t), (c) => ('&<>' + String.fromCharCode(34)).includes(c) ? '&#' + c.charCodeAt(0) + ';' : c).join('');
        const root = document.createElement('div');
        root.className = 'free-direct-dialog';
        root.setAttribute('role', 'dialog');
        root.style.cssText = 'position:fixed;inset:0;z-index:2147483000;display:flex;align-items:center;' +
            'justify-content:center;background:rgba(0,0,0,.6);font:15px/1.4 system-ui,sans-serif;color:#eef2ff;padding:16px;';
        root.innerHTML =
            '<div style="background:#1c2540;border:1px solid #5b6b99;border-radius:10px;padding:20px 22px;width:min(520px,100%)">' +
            '<h2 style="margin:0 0 8px;font-size:20px">Room created</h2>' +
            '<p style="margin:0 0 8px">Give the room ID to the other players. They choose <b>FreeDirect server</b>, ' +
            'enter this server and the room ID.</p>' +
            '<p style="font:600 22px ui-monospace,monospace;letter-spacing:1px;margin:8px 0;user-select:all">' + esc(roomId) + '</p>' +
            '<p style="margin:0 0 6px;color:#b8c2e0">Server: ' + esc(serverUrl) + '</p>' +
            (link ? '<p style="margin:8px 0 2px">Or send them this link:</p><input readonly style="width:100%;box-sizing:border-box;padding:6px" value="' + esc(link) + '">' : '') +
            '<div style="display:flex;gap:10px;justify-content:flex-end;margin-top:14px">' +
            '<button name="copy" style="padding:8px 16px">Copy ID</button>' +
            (link ? '<button name="copylink" style="padding:8px 16px">Copy link</button>' : '') +
            '<button name="ok" style="padding:8px 16px;font-weight:600">OK</button></div></div>';
        for (const type of ['keydown', 'keyup', 'keypress']) root.addEventListener(type, (e) => e.stopPropagation());
        const q = (n) => root.querySelector('button[name=' + n + ']');
        q('copy').addEventListener('click', () => navigator.clipboard && navigator.clipboard.writeText(roomId).catch(() => {}));
        if (link) q('copylink').addEventListener('click', () => navigator.clipboard && navigator.clipboard.writeText(link).catch(() => {}));
        const done = () => { root.remove(); resolve(); };
        q('ok').addEventListener('click', done);
        root.addEventListener('keydown', (e) => { if (e.key === 'Escape' || e.key === 'Enter') done(); });
        document.body.appendChild(root);
        q('ok').focus();
    });
});

std::vector<std::string> SplitFields(const char* text) {
    std::vector<std::string> fields(1);
    for (const char* p = text; *p; ++p) {
        if (*p == '\x1f') fields.emplace_back();
        else fields.back().push_back(*p);
    }
    return fields;
}

#else  // native

// ===== A tiny immediate-mode form in SDL's debug font =====

constexpr float kChar = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;

struct Rect {
    float x, y, w, h;
    bool Contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct Ui {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    float width = 640, height = 480;  // logical canvas
    float scale = 1;                  // text scale: 1 at 640x480, 2 from 1280x960 logical
    bool ownRenderer = false;
    bool ownWindow = false;
    bool quitRequested = false;

    void Color(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { SDL_SetRenderDrawColor(renderer, r, g, b, a); }
    void Fill(const Rect& rc) {
        SDL_FRect f{rc.x, rc.y, rc.w, rc.h};
        SDL_RenderFillRect(renderer, &f);
    }
    void Frame(const Rect& rc) {
        SDL_FRect f{rc.x, rc.y, rc.w, rc.h};
        SDL_RenderRect(renderer, &f);
    }
    void Text(float x, float y, const std::string& s, float textScale = 0) {
        const float k = textScale > 0 ? textScale : scale;
        float sx = 1, sy = 1;
        SDL_GetRenderScale(renderer, &sx, &sy);
        SDL_SetRenderScale(renderer, sx * k, sy * k);
        SDL_RenderDebugText(renderer, x / k, y / k, s.c_str());
        SDL_SetRenderScale(renderer, sx, sy);
    }
    float TextWidth(const std::string& s, float textScale = 0) const {
        return static_cast<float>(s.size()) * kChar * (textScale > 0 ? textScale : scale);
    }
};

bool OpenUi(Ui& ui) {
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO)) return false;
    ui.window = reinterpret_cast<SDL_Window*>(FreeApi::Internal::GetActiveWindow());
    if (!ui.window && !FreeApi::Internal::g_windowsById.empty()) {
        ui.window = reinterpret_cast<SDL_Window*>(FreeApi::Internal::g_windowsById.begin()->second);
    }
    if (!ui.window) {
        ui.window = SDL_CreateWindow("Free Direct server", 640, 480, 0);
        if (!ui.window) return false;
        ui.ownWindow = true;
    }
    ui.renderer = SDL_GetRenderer(ui.window);
    if (!ui.renderer) {
        ui.renderer = SDL_CreateRenderer(ui.window, nullptr);
        if (!ui.renderer) return false;
        ui.ownRenderer = ui.ownWindow;
    }
    int lw = 0, lh = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_GetRenderLogicalPresentation(ui.renderer, &lw, &lh, &mode);
    if (mode == SDL_LOGICAL_PRESENTATION_DISABLED || lw <= 0 || lh <= 0) {
        SDL_GetCurrentRenderOutputSize(ui.renderer, &lw, &lh);
    }
    ui.width = static_cast<float>(lw > 0 ? lw : 640);
    ui.height = static_cast<float>(lh > 0 ? lh : 480);
    ui.scale = ui.width >= 1280 ? 2.0f : 1.0f;
    return true;
}

void CloseUi(Ui& ui) {
    if (ui.ownRenderer) SDL_DestroyRenderer(ui.renderer);
    if (ui.ownWindow) SDL_DestroyWindow(ui.window);
    if (ui.quitRequested) {
        // The player closed the game while the dialog was open: let the game see it.
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quit);
    }
}

/// Saves and restores the renderer state the game relies on, and shows the system cursor.
struct UiSession {
    Ui& ui;
    SDL_Texture* target;
    bool cursorWasVisible;
    bool textInputWasActive;
    SDL_BlendMode blend = SDL_BLENDMODE_NONE;
    Uint8 r = 0, g = 0, b = 0, a = 0;

    explicit UiSession(Ui& u)
        : ui(u),
          target(SDL_GetRenderTarget(u.renderer)),
          cursorWasVisible(SDL_CursorVisible()),
          textInputWasActive(SDL_TextInputActive(u.window)) {
        SDL_GetRenderDrawColor(ui.renderer, &r, &g, &b, &a);
        SDL_GetRenderDrawBlendMode(ui.renderer, &blend);
        SDL_SetRenderTarget(ui.renderer, nullptr);
        SDL_SetRenderDrawBlendMode(ui.renderer, SDL_BLENDMODE_BLEND);
        SDL_ShowCursor();
        SDL_StartTextInput(ui.window);
    }
    ~UiSession() {
        if (!textInputWasActive) SDL_StopTextInput(ui.window);
        if (!cursorWasVisible) SDL_HideCursor();
        SDL_SetRenderDrawBlendMode(ui.renderer, blend);
        SDL_SetRenderDrawColor(ui.renderer, r, g, b, a);
        SDL_SetRenderTarget(ui.renderer, target);
        // Leave a plain screen; the game repaints its own on its next frame.
        SDL_SetRenderDrawColor(ui.renderer, 0, 0, 0, 255);
        SDL_RenderClear(ui.renderer);
        SDL_RenderPresent(ui.renderer);
        SDL_SetRenderDrawColor(ui.renderer, r, g, b, a);
        // Keys typed into the dialog must not reach the game afterwards.
        SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_TEXT_EDITING_CANDIDATES);
    }
};

/// FREE_API_DIALOG_SNAPSHOT=<file.bmp>: saves the first frame of each dialog (tests, docs).
void SnapshotOnce(Ui& ui, bool& taken) {
    if (taken) return;
    taken = true;
    const char* path = SDL_getenv("FREE_API_DIALOG_SNAPSHOT");
    if (!path || !*path) return;
    if (SDL_Surface* shot = SDL_RenderReadPixels(ui.renderer, nullptr)) {
        SDL_SaveBMP(shot, path);
        SDL_DestroySurface(shot);
    }
}

std::vector<std::string> Wrap(const std::string& text, std::size_t columns) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t i = 0;
    while (i < text.size()) {
        std::size_t j = text.find(' ', i);
        if (j == std::string::npos) j = text.size();
        std::string word = text.substr(i, j - i);
        while (word.size() > columns) {
            if (!line.empty()) {
                lines.push_back(line);
                line.clear();
            }
            lines.push_back(word.substr(0, columns));
            word = word.substr(columns);
        }
        if (!line.empty() && line.size() + 1 + word.size() > columns) {
            lines.push_back(line);
            line.clear();
        }
        if (!line.empty()) line.push_back(' ');
        line += word;
        i = j + 1;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

/// Keeps printable ASCII (the debug font has nothing else) up to `maxBytes`.
void AppendText(std::string& field, const char* text, std::size_t maxBytes, bool digitsOnly) {
    for (const char* p = text; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x20 || c > 0x7E) continue;
        if (digitsOnly && (c < '0' || c > '9')) continue;
        if (field.size() + 1 >= maxBytes) break;
        field.push_back(static_cast<char>(c));
    }
}

enum Focus { kServer, kPort, kPassword, kCreate, kJoin, kRoom, kConnect, kCancel, kFocusCount };

void DrawField(Ui& ui, const Rect& box, const std::string& value, bool focused, bool masked, Uint32 ticks) {
    ui.Color(12, 16, 30);
    ui.Fill(box);
    if (focused) ui.Color(255, 214, 102); else ui.Color(110, 125, 170);
    ui.Frame(box);
    std::string shown = masked ? std::string(value.size(), '*') : value;
    const std::size_t visible = static_cast<std::size_t>((box.w - 8) / (kChar * ui.scale)) - 1;
    if (shown.size() > visible) shown = shown.substr(shown.size() - visible);
    if (focused && (ticks / 500) % 2 == 0) shown.push_back('_');
    ui.Color(235, 240, 255);
    ui.Text(box.x + 4, box.y + (box.h - kChar * ui.scale) / 2, shown);
}

void DrawButton(Ui& ui, const Rect& box, const std::string& label, bool focused, bool primary) {
    if (primary) ui.Color(58, 92, 170); else ui.Color(44, 54, 86);
    ui.Fill(box);
    if (focused) ui.Color(255, 214, 102); else ui.Color(130, 145, 190);
    ui.Frame(box);
    ui.Color(255, 255, 255);
    ui.Text(box.x + (box.w - ui.TextWidth(label)) / 2, box.y + (box.h - kChar * ui.scale) / 2, label);
}

BOOL RunServerDialog(FreeApiServerConnection* c) {
    Ui ui;
    if (!OpenUi(ui)) {
        std::fprintf(stderr, "free-api: cannot show the Free Direct server dialog: %s\n", SDL_GetError());
        return FALSE;
    }
    std::string server = c->server, port = c->port, password = c->password, room = c->roomId;
    bool create = c->createRoom != 0;
    const std::string message = c->message;
    int focus = server.empty() ? kServer : (create ? kConnect : (room.empty() ? kRoom : kConnect));
    int result = -1;
    bool snapshotTaken = false;
    {
        UiSession session(ui);
        while (result < 0) {
            const float s = ui.scale;
            const float pw = 560 * s > ui.width - 16 ? ui.width - 16 : 560 * s;
            const float ph = 400 * s > ui.height - 16 ? ui.height - 16 : 400 * s;
            const Rect panel{(ui.width - pw) / 2, (ui.height - ph) / 2, pw, ph};
            const float left = panel.x + 20 * s;
            const float fieldX = panel.x + 190 * s;
            const float fieldW = panel.x + panel.w - 20 * s - fieldX;
            const float rowH = 20 * s;
            const Rect serverBox{fieldX, panel.y + 70 * s, fieldW, rowH};
            const Rect portBox{fieldX, panel.y + 100 * s, 70 * s, rowH};
            const Rect passBox{fieldX, panel.y + 130 * s, fieldW, rowH};
            const Rect createBox{left, panel.y + 172 * s, panel.w - 40 * s, rowH};
            const Rect joinBox{left, panel.y + 196 * s, panel.w - 40 * s, rowH};
            const Rect roomBox{fieldX, panel.y + 222 * s, fieldW, rowH};
            const Rect connectBox{panel.x + panel.w - 250 * s, panel.y + panel.h - 56 * s, 110 * s, 26 * s};
            const Rect cancelBox{panel.x + panel.w - 130 * s, panel.y + panel.h - 56 * s, 110 * s, 26 * s};

            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_MOTION) {
                    SDL_ConvertEventToRenderCoordinates(ui.renderer, &e);
                }
                if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    ui.quitRequested = true;
                    result = 0;
                } else if (e.type == SDL_EVENT_TEXT_INPUT) {
                    if (focus == kServer) AppendText(server, e.text.text, sizeof(c->server), false);
                    if (focus == kPort) AppendText(port, e.text.text, 6, true);
                    if (focus == kPassword) AppendText(password, e.text.text, sizeof(c->password), false);
                    if (focus == kRoom) {
                        AppendText(room, e.text.text, sizeof(c->roomId), false);
                        create = false;
                    }
                } else if (e.type == SDL_EVENT_KEY_DOWN) {
                    const SDL_Keycode key = e.key.key;
                    const bool ctrl = (e.key.mod & SDL_KMOD_CTRL) != 0;
                    std::string* field = focus == kServer ? &server : focus == kPort ? &port
                                       : focus == kPassword ? &password : focus == kRoom ? &room : nullptr;
                    if (key == SDLK_ESCAPE) {
                        result = 0;
                    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                        if (focus == kCancel) result = 0;
                        else if (focus == kCreate) create = true, focus = kConnect;
                        else if (focus == kJoin) create = false, focus = kRoom;
                        else result = 1;
                    } else if (key == SDLK_TAB) {
                        focus = (focus + ((e.key.mod & SDL_KMOD_SHIFT) ? kFocusCount - 1 : 1)) % kFocusCount;
                    } else if (key == SDLK_DOWN && focus < kConnect) {
                        focus = focus + 1;
                    } else if (key == SDLK_UP && focus > kServer && focus <= kRoom) {
                        focus = focus - 1;
                    } else if (key == SDLK_SPACE && (focus == kCreate || focus == kJoin)) {
                        create = focus == kCreate;
                    } else if (key == SDLK_BACKSPACE && field && !field->empty()) {
                        if (ctrl) field->clear(); else field->pop_back();
                    } else if (ctrl && key == SDLK_V && field) {
                        if (char* clip = SDL_GetClipboardText()) {
                            AppendText(*field, clip, field == &port ? 6 : sizeof(c->server), field == &port);
                            if (field == &room) create = false;
                            SDL_free(clip);
                        }
                    } else if ((key == SDLK_LEFT || key == SDLK_RIGHT) && (focus == kConnect || focus == kCancel)) {
                        focus = focus == kConnect ? kCancel : kConnect;
                    }
                } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                    const float x = e.button.x, y = e.button.y;
                    if (serverBox.Contains(x, y)) focus = kServer;
                    else if (portBox.Contains(x, y)) focus = kPort;
                    else if (passBox.Contains(x, y)) focus = kPassword;
                    else if (createBox.Contains(x, y)) focus = kCreate, create = true;
                    else if (joinBox.Contains(x, y)) focus = kJoin, create = false;
                    else if (roomBox.Contains(x, y)) focus = kRoom, create = false;
                    else if (connectBox.Contains(x, y)) result = 1;
                    else if (cancelBox.Contains(x, y)) result = 0;
                }
            }

            const Uint32 ticks = static_cast<Uint32>(SDL_GetTicks());
            ui.Color(10, 14, 28);
            SDL_RenderClear(ui.renderer);
            ui.Color(30, 40, 68);
            ui.Fill(panel);
            ui.Color(110, 125, 170);
            ui.Frame(panel);
            ui.Color(255, 255, 255);
            ui.Text(left, panel.y + 18 * s, "Free Direct server", 2 * s);
            ui.Color(184, 194, 224);
            ui.Text(left, panel.y + 44 * s, "Internet multiplayer through a Free Direct server.");
            ui.Color(235, 240, 255);
            ui.Text(left, serverBox.y + 6 * s, "Server (address/URL)");
            ui.Text(left, portBox.y + 6 * s, "Port");
            ui.Text(left, passBox.y + 6 * s, "Password (optional)");
            DrawField(ui, serverBox, server, focus == kServer, false, ticks);
            DrawField(ui, portBox, port, focus == kPort, false, ticks);
            DrawField(ui, passBox, password, focus == kPassword, true, ticks);
            if (focus == kCreate) ui.Color(255, 214, 102); else ui.Color(235, 240, 255);
            ui.Text(createBox.x, createBox.y + 6 * s, std::string(create ? "(*)" : "( )") + " Create a new room");
            if (focus == kJoin) ui.Color(255, 214, 102); else ui.Color(235, 240, 255);
            ui.Text(joinBox.x, joinBox.y + 6 * s, std::string(create ? "( )" : "(*)") + " Join a room with its ID:");
            ui.Color(235, 240, 255);
            ui.Text(left + 16 * s, roomBox.y + 6 * s, "Room ID");
            DrawField(ui, roomBox, room, focus == kRoom, false, ticks);
            if (!message.empty()) {
                ui.Color(255, 160, 140);
                const std::size_t columns = static_cast<std::size_t>((panel.w - 40 * s) / (kChar * s));
                float y = roomBox.y + 34 * s;
                for (const std::string& line : Wrap(message, columns)) {
                    if (y > connectBox.y - 24 * s) break;
                    ui.Text(left, y, line);
                    y += 11 * s;
                }
            }
            DrawButton(ui, connectBox, "Connect", focus == kConnect, true);
            DrawButton(ui, cancelBox, "Cancel", focus == kCancel, false);
            ui.Color(140, 152, 190);
            ui.Text(left, panel.y + panel.h - 18 * s, "Tab: next  Enter: connect  Esc: cancel  Ctrl+V: paste");
            SnapshotOnce(ui, snapshotTaken);
            SDL_RenderPresent(ui.renderer);
            SDL_Delay(16);
        }
    }
    CloseUi(ui);
    if (result != 1) return FALSE;
    CopyOut(c->server, sizeof(c->server), server);
    CopyOut(c->port, sizeof(c->port), port);
    CopyOut(c->password, sizeof(c->password), password);
    CopyOut(c->roomId, sizeof(c->roomId), room);
    c->createRoom = create ? 1 : 0;
    return TRUE;
}

void RunRoomCreated(const char* roomId, const char* serverUrl) {
    SDL_SetClipboardText(roomId);
    Ui ui;
    if (!OpenUi(ui)) return;
    {
        UiSession session(ui);
        bool done = false;
        bool snapshotTaken = false;
        std::string status = "The room ID is copied to the clipboard.";
        while (!done) {
            const float s = ui.scale;
            const float pw = 560 * s > ui.width - 16 ? ui.width - 16 : 560 * s;
            const float ph = 260 * s > ui.height - 16 ? ui.height - 16 : 260 * s;
            const Rect panel{(ui.width - pw) / 2, (ui.height - ph) / 2, pw, ph};
            const float left = panel.x + 20 * s;
            const Rect copyBox{panel.x + panel.w - 250 * s, panel.y + panel.h - 46 * s, 110 * s, 26 * s};
            const Rect okBox{panel.x + panel.w - 130 * s, panel.y + panel.h - 46 * s, 110 * s, 26 * s};
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) SDL_ConvertEventToRenderCoordinates(ui.renderer, &e);
                if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    ui.quitRequested = true;
                    done = true;
                } else if (e.type == SDL_EVENT_KEY_DOWN &&
                           (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER || e.key.key == SDLK_ESCAPE)) {
                    done = true;
                } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_C && (e.key.mod & SDL_KMOD_CTRL)) {
                    SDL_SetClipboardText(roomId);
                } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                    if (okBox.Contains(e.button.x, e.button.y)) done = true;
                    if (copyBox.Contains(e.button.x, e.button.y)) {
                        SDL_SetClipboardText(roomId);
                        status = "Copied again.";
                    }
                }
            }
            ui.Color(10, 14, 28);
            SDL_RenderClear(ui.renderer);
            ui.Color(30, 40, 68);
            ui.Fill(panel);
            ui.Color(110, 125, 170);
            ui.Frame(panel);
            ui.Color(255, 255, 255);
            ui.Text(left, panel.y + 18 * s, "Room created", 2 * s);
            ui.Color(235, 240, 255);
            ui.Text(left, panel.y + 48 * s, "Give the room ID to the other players. They choose");
            ui.Text(left, panel.y + 60 * s, "FreeDirect server, enter this server and the room ID.");
            ui.Color(255, 214, 102);
            ui.Text(left, panel.y + 86 * s, roomId, 2 * s);
            ui.Color(184, 194, 224);
            ui.Text(left, panel.y + 120 * s, std::string("Server: ") + serverUrl);
            ui.Text(left, panel.y + 136 * s, status);
            DrawButton(ui, copyBox, "Copy ID", false, false);
            DrawButton(ui, okBox, "OK", true, true);
            SnapshotOnce(ui, snapshotTaken);
            SDL_RenderPresent(ui.renderer);
            SDL_Delay(16);
        }
    }
    CloseUi(ui);
}

#endif

} // namespace

extern "C" BOOL FreeApiPromptServerConnection(FreeApiServerConnection* connection) {
    if (!connection) return FALSE;
#ifdef __EMSCRIPTEN__
    char* reply = FreeApiWebPromptServer(connection->server, connection->port, connection->password,
                                         connection->roomId, connection->createRoom, connection->message);
    if (!reply) return FALSE;
    const std::vector<std::string> fields = SplitFields(reply);
    std::free(reply);
    if (fields.size() < 5) return FALSE;
    CopyOut(connection->server, sizeof(connection->server), fields[0]);
    CopyOut(connection->port, sizeof(connection->port), fields[1]);
    CopyOut(connection->password, sizeof(connection->password), fields[2]);
    CopyOut(connection->roomId, sizeof(connection->roomId), fields[3]);
    connection->createRoom = fields[4] == "1" ? 1 : 0;
    return TRUE;
#else
    return RunServerDialog(connection);
#endif
}

extern "C" void FreeApiShowRoomCreated(const char* roomId, const char* serverUrl) {
    if (!roomId) return;
#ifdef __EMSCRIPTEN__
    FreeApiWebShowRoom(roomId, serverUrl ? serverUrl : "");
#else
    RunRoomCreated(roomId, serverUrl ? serverUrl : "");
#endif
}

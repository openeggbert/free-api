# Free Direct's server dialog, and `WM_CHAR`

Two pieces of Free API serve Internet multiplayer through Free Direct's "FreeDirect server"
provider (`../free-direct/docs/free-direct-server.md`).

## The connection dialog (`free_api_bridge.h`)

`FreeApiPromptServerConnection(FreeApiServerConnection*)` asks the player for the server (an
address, `host:port` or `ws://`/`wss://` URL), its port, its password if it has one, and whether
to create a room or join one with its secret ID. Free Direct prefills the fields and passes the
previous attempt's error in `message`. TRUE: the player chose Connect and the fields hold the
entries; FALSE: Cancel or Escape, the fields are unchanged.
`FreeApiShowRoomCreated(roomId, serverUrl)` shows a new room's ID, copies it to the clipboard and
returns when the player dismisses it. The original DirectPlay TCP/IP provider likewise showed its
own address dialog, so the games' code never needs to know.

Implementation: `src/winuser_netprompt.cpp`, its own translation unit, so a game that never
calls these functions links none of it.

**Native.** A modal screen drawn into the game's window, through the renderer Free Direct created
for it and in its logical (640x480) coordinates, with SDL's built-in 8x8 font (scaled 2x from a
1280-pixel-wide logical size). It therefore shows in a window and in full screen. The game is
blocked in its DirectPlay call meanwhile; afterwards the screen is cleared and the game repaints
its own. Keys: Tab / Shift+Tab move between fields, Up/Down too, Space or Enter chooses a room
option, Enter connects, Escape cancels, Backspace deletes (Ctrl+Backspace clears a field), Ctrl+V
pastes. The mouse works on every field and button. Closing the window cancels the dialog and is
passed on to the game. `FREE_API_DIALOG_SNAPSHOT=<file.bmp>` saves each dialog's first frame
(tests, documentation).

**Web.** An HTML form over the page (`EM_ASYNC_JS`, so the build needs Asyncify, which the
blupi_recon web build has). Keyboard events inside it are stopped before they reach SDL's
window-level listeners, so the fields can be typed in. Empty fields are prefilled from the page's
`server`, `port` and `room` URL parameters, then from the last server and port in
`localStorage` (`freeDirectServer`; never the password). The room dialog offers to copy the ID and
a link to the page with `server` and `room` set. A page can replace both dialogs:
`Module.freeApiServerPrompt(initial)` returns a promise of
`{server, port, password, roomId, createRoom}` or `null`, and
`Module.freeApiShowRoomCreated(roomId, serverUrl)` a promise. The JavaScript avoids backslashes:
the preprocessor that turns an `EM_JS` body into a string drops those outside string literals.

Test: `tests/test_net_prompt.cpp` drives the native dialog with SDL events (dummy video driver).
blupi_recon's `freeapi/tools/net_web.mjs` types into the web dialog in Chrome.

## `TranslateMessage` and `WM_CHAR`

blupi_recon reads `WM_CHAR` for player names and the name of a new multiplayer game
(`src/event.cpp` `TreatTextInput`), and its message loop calls `TranslateMessage`, as every target
game's does. SDL's text input is never started (on the web it would stop SDL from preventing
Space's default action, which scrolls the page), so `SDL_EVENT_TEXT_INPUT` does not arrive.
`TranslateMessage` therefore does what Win32's does: for a `WM_KEYDOWN` of a key that types, it
posts that character's `WM_CHAR`. The character is computed when the key goes down
(`FreeApiMessageQueue.cpp` `KeyDownChar`): the key in the current layout with Shift and Caps Lock,
Ctrl+letter as a control character, Backspace 8, Tab 9, Enter 13, Escape 27, nothing for keys that
type nothing or with Alt. ASCII only. While SDL's text input is on, `TranslateMessage` posts
nothing, since `SDL_EVENT_TEXT_INPUT` already becomes `WM_CHAR`.

Test: `tests/test_input_pipeline.cpp` (Test 13).

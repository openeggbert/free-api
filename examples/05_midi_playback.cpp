/**
 * @file 05_midi_playback.cpp
 * @brief Demonstrates both target games' MIDI music sequence: MCI_OPEN
 * ("sequencer") -> MCI_PLAY (MCI_NOTIFY) -> MM_MCINOTIFY -> MCI_CLOSE, with
 * automatic looping on MCI_NOTIFY_SUCCESSFUL (mirroring free-eggbert's
 * MM_MCINOTIFY handler: SuspendMusic()/MCI_CLOSE then RestartMusic()/
 * MCI_OPEN+MCI_PLAY).
 *
 * You should HEAR music if a SoundFont (.sf2) is available -- set
 * FREE_API_SOUNDFONT=/path/to/file.sf2, or place default.sf2 in
 * assets/soundfont/ or soundfont/ (see README.md's "SoundFont requirement"
 * section). Without one, playback is silent (not an error) -- watch the
 * console for the loop/notify messages either way.
 *
 * Usage: 05_midi_playback [path/to/file.mid]
 * With no argument, a short built-in melody (a five-note ascending scale)
 * is generated and played.
 *
 * Close the window to exit.
 */
#include <windows.h>
#include <windowsx.h>
#include <digitalv.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static bool g_running = true;

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) { g_running = false; return 0; }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) { PostMessageA(hwnd, WM_CLOSE, 0, 0); return 0; }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// Appends a MIDI variable-length quantity (delta-time encoding).
static void AppendVlq(std::vector<uint8_t>& out, uint32_t value)
{
    uint8_t buf[4];
    int n = 0;
    buf[n++] = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    while (value > 0) {
        buf[n++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
        value >>= 7;
    }
    for (int i = n - 1; i >= 0; --i) out.push_back(buf[i]);
}

// Builds a short, valid Type-0 MIDI file: a five-note ascending scale
// (C4-D4-E4-F4-G4), quarter notes at a moderate tempo.
static bool WriteBuiltinMelody(const std::string& path)
{
    std::vector<uint8_t> track;
    const uint8_t notes[] = {60, 62, 64, 65, 67}; // C D E F G
    const uint32_t noteDurationTicks = 240;       // quarter note at 480 ticks/beat

    AppendVlq(track, 0);
    track.push_back(0xC0); track.push_back(0x00); // program change: channel 0, instrument 0 (piano)

    for (unsigned char note : notes) {
        AppendVlq(track, 0);
        track.push_back(0x90); track.push_back(note); track.push_back(0x60); // note on
        AppendVlq(track, noteDurationTicks);
        track.push_back(0x80); track.push_back(note); track.push_back(0x00); // note off
    }
    AppendVlq(track, 0);
    track.push_back(0xFF); track.push_back(0x2F); track.push_back(0x00); // end of track

    std::vector<uint8_t> file;
    file.insert(file.end(), {'M', 'T', 'h', 'd', 0, 0, 0, 6});
    file.insert(file.end(), {0, 0});    // format 0
    file.insert(file.end(), {0, 1});    // one track
    file.insert(file.end(), {0x01, 0xE0}); // 480 ticks per quarter note
    file.insert(file.end(), {'M', 'T', 'r', 'k'});
    const uint32_t trackLen = static_cast<uint32_t>(track.size());
    file.push_back(static_cast<uint8_t>((trackLen >> 24) & 0xFF));
    file.push_back(static_cast<uint8_t>((trackLen >> 16) & 0xFF));
    file.push_back(static_cast<uint8_t>((trackLen >> 8) & 0xFF));
    file.push_back(static_cast<uint8_t>(trackLen & 0xFF));
    file.insert(file.end(), track.begin(), track.end());

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(file.data(), 1, file.size(), f);
    fclose(f);
    return true;
}

static MCIDEVICEID g_deviceId = 0;
static std::string g_path;
static int g_loopCount = 0;

static bool OpenAndPlay(HWND hwnd)
{
    MCI_OPEN_PARMSA openParms{};
    openParms.lpstrDeviceType = const_cast<LPSTR>("sequencer");
    openParms.lpstrElementName = const_cast<LPSTR>(g_path.c_str());
    if (mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT,
                        reinterpret_cast<DWORD_PTR>(&openParms)) != 0) {
        printf("MCI_OPEN failed.\n");
        return false;
    }
    g_deviceId = openParms.wDeviceID;

    MCI_PLAY_PARMS playParms{};
    playParms.dwCallback = reinterpret_cast<DWORD_PTR>(hwnd);
    if (mciSendCommandA(g_deviceId, MCI_PLAY, MCI_NOTIFY,
                        reinterpret_cast<DWORD_PTR>(&playParms)) != 0) {
        printf("MCI_PLAY failed.\n");
        return false;
    }
    ++g_loopCount;
    printf("Playing (loop #%d)...\n", g_loopCount);
    return true;
}

int main(int argc, char** argv)
{
    printf("=== 05_midi_playback ===\n");

    bool generated = false;
    if (argc > 1) {
        g_path = argv[1];
        printf("Playing file: %s\n", g_path.c_str());
    } else {
        g_path = "free_api_example_melody.mid";
        if (!WriteBuiltinMelody(g_path)) {
            printf("Failed to write the built-in melody file.\n");
            return 1;
        }
        generated = true;
        printf("Playing a built-in 5-note melody (pass a .mid path as argv[1] to play your own).\n");
    }
    printf("(If you don't hear anything, set FREE_API_SOUNDFONT=/path/to/file.sf2\n");
    printf(" -- see README.md's SoundFont requirement section.)\n\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = (HINSTANCE)1;
    wc.hCursor       = LoadCursorA(nullptr, "IDC_ARROW");
    wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
    wc.lpszClassName = "FreeApiExample_MidiPlayback";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "FreeApiExample_MidiPlayback",
                                 "Free API Example: MIDI Playback (see console)",
                                 WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
                                 100, 100, 500, 120,
                                 nullptr, nullptr, (HINSTANCE)1, nullptr);
    if (!hwnd) {
        printf("CreateWindowExA failed.\n");
        return 1;
    }

    if (!OpenAndPlay(hwnd)) {
        DestroyWindow(hwnd);
        return 1;
    }

    // The mixer thread renders MIDI-to-PCM as fast as the CPU allows (it is
    // not paced to real wall-clock playback time -- see src/MidiMusic.cpp
    // MixerThread), so a short track can be flagged "finished" and re-armed
    // for another loop far faster than it's actually audible. Bounding the
    // loop count and pacing each reopen keeps this demo well-behaved
    // regardless of audio backend, instead of spinning as fast as possible.
    const int kMaxLoops = 3;

    MSG msg{};
    while (g_running) {
        if (PeekMessageA(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
            if (!GetMessageA(&msg, nullptr, 0, 0)) break;
            if (msg.message == MM_MCINOTIFY) {
                // Mirrors free-eggbert's MM_MCINOTIFY handler: close, then
                // (if the track finished successfully) reopen and replay --
                // both games' own game-side looping mechanism.
                mciSendCommandA(g_deviceId, MCI_CLOSE, 0, 0);
                if (msg.wParam == MCI_NOTIFY_SUCCESSFUL) {
                    if (g_loopCount >= kMaxLoops) {
                        printf("Played %d times -- done. Close the window (or press Escape) to exit.\n", g_loopCount);
                    } else {
                        printf("Track finished (MM_MCINOTIFY/MCI_NOTIFY_SUCCESSFUL) -- looping.\n");
                        SDL_Delay(300); // let this loop's tail actually finish playing before reopening
                        OpenAndPlay(hwnd);
                    }
                } else {
                    printf("Track ended abnormally (wParam=%u).\n", (unsigned)msg.wParam);
                }
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            WaitMessage();
        }
    }

    mciSendCommandA(g_deviceId, MCI_CLOSE, 0, 0);
    if (generated) remove(g_path.c_str());

    printf("Exiting cleanly.\n");
    return 0;
}

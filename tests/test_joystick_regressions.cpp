/**
 * @file test_joystick_regressions.cpp
 * @brief Regression tests for the real, SDL-backed joyGetPosEx/joyGetNumDevs
 * implementation (plan.md TASK-0103), using SDL's virtual joystick API so
 * this is testable without real hardware.
 *
 * Only free-eggbert reads joystick state (event.cpp:2069-2127), and only
 * dwXpos/dwYpos (2 axes) and dwButtons bits 0-3 (JOY_BUTTON1-4, the first 4
 * buttons) -- this test covers exactly that surface.
 */
#include <windows.h>
#include <SDL3/SDL.h>
#include <cstdio>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[joystick-regressions] PASS: %s\n", what);
    } else {
        printf("[joystick-regressions] FAIL: %s\n", what);
        ++g_failures;
    }
}

static void TestVirtualJoystickAxisAndButtonRoundTrip()
{
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = 2;
    desc.nbuttons = 4;
    desc.name = "Free API Test Virtual Joystick";

    SDL_JoystickID instanceId = SDL_AttachVirtualJoystick(&desc);
    Check(instanceId != 0, "SDL_AttachVirtualJoystick succeeds");
    if (instanceId == 0) return;

    SDL_Joystick* virtualHandle = SDL_OpenJoystick(instanceId);
    Check(virtualHandle != nullptr, "the attached virtual joystick can be opened directly (to drive its virtual inputs)");

    UINT numDevs = joyGetNumDevs();
    Check(numDevs >= 1, "joyGetNumDevs reports at least the attached virtual joystick");

    // Find which Win32-style index the virtual joystick landed at (it's the
    // only joystick this test attaches, but other tests/processes could in
    // principle already have one; scan to be robust).
    int foundIndex = -1;
    for (UINT i = 0; i < numDevs && foundIndex < 0; ++i) {
        JOYINFOEX probe{};
        probe.dwSize = sizeof(JOYINFOEX);
        probe.dwFlags = 0xFF;
        // We can't directly ask "is this index our virtual device", so just
        // use index 0 in the common case (single test joystick) and fall
        // back to scanning if joyGetPosEx succeeds for a later index too.
        if (joyGetPosEx(i, &probe) == JOYERR_NOERROR) {
            foundIndex = static_cast<int>(i);
        }
    }
    Check(foundIndex >= 0, "joyGetPosEx succeeds for at least one device index");

    if (foundIndex >= 0) {
        // Center / no buttons pressed.
        SDL_SetJoystickVirtualAxis(virtualHandle, 0, 0);
        SDL_SetJoystickVirtualAxis(virtualHandle, 1, 0);
        for (int b = 0; b < 4; ++b) SDL_SetJoystickVirtualButton(virtualHandle, b, false);
        SDL_Delay(20);

        JOYINFOEX joy{};
        joy.dwSize = sizeof(JOYINFOEX);
        joy.dwFlags = 0xFF;
        MMRESULT rc = joyGetPosEx(static_cast<UINT>(foundIndex), &joy);
        Check(rc == JOYERR_NOERROR, "joyGetPosEx succeeds for the virtual joystick's index");
        Check(joy.dwXpos == 32768 && joy.dwYpos == 32768,
              "centered virtual axes (SDL value 0) map to Win32's centered 32768");
        Check(joy.dwButtons == 0, "no buttons pressed maps to dwButtons == 0");

        // Full left/up (SDL axis minimum) and all 4 buttons pressed.
        SDL_SetJoystickVirtualAxis(virtualHandle, 0, -32768);
        SDL_SetJoystickVirtualAxis(virtualHandle, 1, 32767);
        for (int b = 0; b < 4; ++b) SDL_SetJoystickVirtualButton(virtualHandle, b, true);
        SDL_Delay(20);

        JOYINFOEX joy2{};
        joy2.dwSize = sizeof(JOYINFOEX);
        joy2.dwFlags = 0xFF;
        joyGetPosEx(static_cast<UINT>(foundIndex), &joy2);
        Check(joy2.dwXpos == 0, "SDL axis minimum (-32768) maps to Win32's dwXpos == 0 (full left, matching free-eggbert's <16384 threshold)");
        Check(joy2.dwYpos == 65535, "SDL axis maximum (32767) maps to Win32's dwYpos == 65535 (full down, matching free-eggbert's >49152 threshold)");
        Check(joy2.dwButtons == (JOY_BUTTON1 | JOY_BUTTON2 | JOY_BUTTON3 | JOY_BUTTON4),
              "all 4 virtual buttons pressed map to JOY_BUTTON1|2|3|4 in dwButtons");
    }

    SDL_CloseJoystick(virtualHandle);
    SDL_DetachVirtualJoystick(instanceId);
}

static void TestJoyGetPosExRejectsNullAndTooSmallStruct()
{
    Check(joyGetPosEx(0, nullptr) != JOYERR_NOERROR, "joyGetPosEx rejects a null pointer");

    JOYINFOEX tooSmall{};
    tooSmall.dwSize = 4; // smaller than sizeof(JOYINFOEX)
    Check(joyGetPosEx(0, &tooSmall) != JOYERR_NOERROR, "joyGetPosEx rejects a dwSize smaller than sizeof(JOYINFOEX)");
}

static void TestJoyGetPosExReportsUnpluggedForOutOfRangeIndex()
{
    JOYINFOEX joy{};
    joy.dwSize = sizeof(JOYINFOEX);
    MMRESULT rc = joyGetPosEx(999, &joy);
    Check(rc == JOYERR_UNPLUGGED, "joyGetPosEx reports JOYERR_UNPLUGGED for an out-of-range device index");
}

int main()
{
    printf("[joystick-regressions] Starting\n");

    TestVirtualJoystickAxisAndButtonRoundTrip();
    TestJoyGetPosExRejectsNullAndTooSmallStruct();
    TestJoyGetPosExReportsUnpluggedForOutOfRangeIndex();

    if (g_failures > 0) {
        printf("[joystick-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[joystick-regressions] ALL TESTS PASSED\n");
    return 0;
}

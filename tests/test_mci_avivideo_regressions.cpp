/**
 * @file test_mci_avivideo_regressions.cpp
 * @brief Regression test locking in MCI digital-video's graceful decline.
 *
 * Both target games' CMovie::initAVI() call
 * mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE, ...) (device-class-only open,
 * no MCI_OPEN_ELEMENT) to probe for an "avivideo" MCI driver. Free API does
 * not implement AVI video playback, so mciSendCommandA must return a
 * non-zero MCIERROR for this exact call shape -- src/winmm.cpp returns
 * MCIERR_UNSUPPORTED_FUNCTION early, specifically so initAVI() returns
 * FALSE.
 *
 * This is load-bearing, not incidental: both games' CMovie::Create() sets
 * m_bEnable=FALSE when initAVI() fails, and CEvent::StartMovie()/
 * CEvent::MovieToStart() (identical in both games) then immediately call
 * ChangePhase(m_phaseAfterMovie) -- the same phase transition a real movie
 * would trigger on completion. The net effect, confirmed by tracing both
 * games' source: cutscenes are silently and safely skipped end-to-end, with
 * no crash, no hang, and no visible error. See plan.md section 10 and
 * NEXT.md for the full investigation (formerly the single highest-risk
 * unresolved item in this project).
 *
 * A prior "TODO: segfault is happening here for the game Planet Blupi"
 * comment in winmm.cpp predates the early-return guard this test locks in;
 * as long as this test passes, that call shape can never reach the
 * MidiMusicSendCommand() fallthrough that comment was warning about.
 */
#include <windows.h>
#include <mmsystem.h>
#include <digitalv.h>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    if (condition) {
        printf("[mci-avivideo-regressions] PASS: %s\n", what);
    } else {
        printf("[mci-avivideo-regressions] FAIL: %s\n", what);
        ++g_failures;
    }
}

static void TestAviVideoDeviceClassOpenIsDeclinedGracefully()
{
    // Mirrors CMovie::initAVI() in both games exactly: MCI_OPEN_TYPE only,
    // no MCI_OPEN_ELEMENT, lpstrDeviceType="avivideo".
    MCI_DGV_OPEN_PARMS mciOpen{};
    mciOpen.dwCallback       = 0;
    mciOpen.wDeviceID        = 0;
    mciOpen.lpstrDeviceType  = "avivideo";
    mciOpen.lpstrElementName = nullptr;
    mciOpen.lpstrAlias       = nullptr;
    mciOpen.dwStyle          = 0;
    mciOpen.hWndParent       = nullptr;

    MCIERROR result = mciSendCommandA(0, MCI_OPEN, static_cast<DWORD_PTR>(MCI_OPEN_TYPE),
                                       reinterpret_cast<DWORD_PTR>(&mciOpen));

    Check(result != 0,
          "mciSendCommandA(MCI_OPEN, MCI_OPEN_TYPE only, \"avivideo\") returns a non-zero MCIERROR "
          "(both games' CMovie::initAVI() checks == 0 for success)");

    // Both games' CMovie::Create() sets m_bEnable = FALSE precisely because
    // initAVI() (== this exact call) fails; the games' own StartMovie()/
    // MovieToStart() guards then skip movie playback cleanly. As long as
    // this call keeps returning non-zero, that graceful-skip path stays
    // correct -- no crash, no hang, nothing further for Free API to do here.
    char errBuf[256] = {};
    BOOL gotErrString = mciGetErrorStringA(result, errBuf, sizeof(errBuf));
    Check(gotErrString == TRUE && errBuf[0] != '\0',
          "mciGetErrorStringA returns a non-empty string for the decline error code");
}

int main()
{
    printf("[mci-avivideo-regressions] Starting\n");

    TestAviVideoDeviceClassOpenIsDeclinedGracefully();

    if (g_failures > 0) {
        printf("[mci-avivideo-regressions] %d FAILURE(S)\n", g_failures);
        return 1;
    }

    printf("[mci-avivideo-regressions] ALL TESTS PASSED\n");
    return 0;
}

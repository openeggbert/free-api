#include <windows.h>
#include <mmsystem.h>
#include <SDL3/SDL.h>

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static bool WriteMinimalMidi(const std::string& path)
{
    static const unsigned char kMidi[] = {
        'M', 'T', 'h', 'd',
        0x00, 0x00, 0x00, 0x06,
        0x00, 0x00,
        0x00, 0x01,
        0x00, 0x60,
        'M', 'T', 'r', 'k',
        0x00, 0x00, 0x00, 0x0F,
        0x00, 0xC0, 0x00,
        0x00, 0x90, 0x3C, 0x40,
        0x60, 0x80, 0x3C, 0x00,
        0x00, 0xFF, 0x2F, 0x00,
    };

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        std::cerr << "failed to create MIDI file: " << path << " errno=" << errno << "\n";
        return false;
    }

    const size_t written = fwrite(kMidi, 1, sizeof(kMidi), f);
    fclose(f);
    return written == sizeof(kMidi);
}

static int RunSleepSmokeTest()
{
    DWORD start = GetTickCount();
    std::cout << "[basic] Starting Sleep(100)..." << std::endl;
    Sleep(100);
    DWORD end = GetTickCount();

    std::cout << "[basic] Elapsed time: " << (end - start) << " ms" << std::endl;
    if ((end - start) < 100) {
        std::cerr << "[basic] Sleep test failed" << std::endl;
        return 1;
    }
    return 0;
}

static int RunMidiCaseFallbackRegression()
{
    char dirTemplate[] = "free-api-midi-case-test";
    const char* tempDir = dirTemplate;

#ifndef _WIN32
    static char posixTemplate[] = "free-api-midi-case-XXXXXX";
    tempDir = mkdtemp(posixTemplate);
    if (!tempDir) {
        std::cerr << "mkdtemp failed errno=" << errno << "\n";
        return 1;
    }
#else
    // On Windows, just try to create the directory directly.
    // If it exists, we'll just use it.
    (void)CreateDirectoryA(tempDir, NULL);
#endif

    const std::string root(tempDir);
    const std::string upperDir = root + "/SOUND";
    const std::string upperFile = upperDir + "/MUSIC000.BLP";
    const std::string lowerRequested = root + "/sound/music000.blp";

#ifdef _WIN32
    if (!CreateDirectoryA(upperDir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        std::cerr << "CreateDirectoryA failed: " << upperDir << " err=" << GetLastError() << "\n";
        RemoveDirectoryA(root.c_str());
        return 1;
    }
#else
    if (mkdir(upperDir.c_str(), 0700) != 0) {
        std::cerr << "mkdir failed: " << upperDir << " errno=" << errno << "\n";
        rmdir(root.c_str());
        return 1;
    }
#endif

    if (!WriteMinimalMidi(upperFile)) {
#ifdef _WIN32
        RemoveDirectoryA(upperDir.c_str());
        RemoveDirectoryA(root.c_str());
#else
        rmdir(upperDir.c_str());
        rmdir(root.c_str());
#endif
        return 1;
    }

#ifndef _WIN32
    if (::access(lowerRequested.c_str(), F_OK) == 0) {
        std::cerr << "unexpected: lowercase path exists, regression scenario invalid" << std::endl;
        remove(upperFile.c_str());
        rmdir(upperDir.c_str());
        rmdir(root.c_str());
        return 1;
    }
#endif

#ifdef _WIN32
    SetEnvironmentVariableA("SDL_AUDIODRIVER", "dummy");
#else
    setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif

    MCI_OPEN_PARMSA openParms{};
    openParms.wDeviceID = 0;
    openParms.lpstrDeviceType = const_cast<LPSTR>("sequencer");
    openParms.lpstrElementName = const_cast<LPSTR>(lowerRequested.c_str());

    const MCIERROR openResult = mciSendCommandA(
        0,
        MCI_OPEN,
        MCI_OPEN_TYPE | MCI_OPEN_ELEMENT,
        reinterpret_cast<DWORD_PTR>(&openParms));

    int rc = 0;
    if (openResult != 0) {
        char err[256] = {};
        mciGetErrorStringA(openResult, err, sizeof(err));
        std::cerr << "MCI_OPEN failed (" << openResult << "): " << err << "\n";
        rc = 1;
    } else {
        mciSendCommandA(openParms.wDeviceID, MCI_CLOSE, 0, 0);
    }

    remove(upperFile.c_str());
#ifdef _WIN32
    RemoveDirectoryA(upperDir.c_str());
    RemoveDirectoryA(root.c_str());
#else
    rmdir(upperDir.c_str());
    rmdir(root.c_str());
#endif
    return rc;
}

int main()
{
    if (RunSleepSmokeTest() != 0) {
        std::cout << "FAILURE" << std::endl;
        return 1;
    }

    if (RunMidiCaseFallbackRegression() != 0) {
        std::cout << "FAILURE" << std::endl;
        return 1;
    }

    std::cout << "SUCCESS" << std::endl;
    return 0;
}

/**
 * @file test_header_compile.cpp
 * @brief Compile-only smoke test: exactly the public headers both target
 * games (../free-eggbert, ../planetblupi) include, in one translation unit.
 *
 * This test calls nothing — it only proves the header set compiles cleanly
 * together, matching plan.md section 3.1's evidence. It must not gain any
 * header not actually included by one of the two target games.
 */
#include <windows.h>
#include <windowsx.h>
#include <wtypes.h>
#include <mmsystem.h>
#include <digitalv.h>
#include <commdlg.h>
#include <io.h>
#include <direct.h>

#if !defined(_WIN32)
#include <sys/timeb.h>
#endif

int main()
{
    return 0;
}

/**
 * @file test_header_compile.cpp
 * @brief Compile-only smoke test: exactly the public headers both target
 * games (../free-eggbert, ../planetblupi) include, in one translation unit.
 *
 * This test calls nothing — it only proves the header set compiles cleanly
 * together, matching plan.md section 3.1's evidence. It must not gain any
 * header not actually included by one of the two target games.
 *
 * TASK-0026: this target is linked against ONLY the `freeapi_compat_headers`
 * interface target (CMakeLists.txt), whose include path is `include/` (and
 * `include_non_windows/` off-Windows) -- no SDL3, no `external/` (tsf.h/
 * tml.h). A successful compile of this file is therefore itself the proof
 * that no public header transitively requires an SDL3 or `external/`
 * include path, i.e. that public headers never leak SDL types (this is
 * checked on every `ctest` run, not just once).
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

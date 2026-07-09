// Windows.h
//
// Case-insensitive-filesystem compatibility shim: real Win32 headers are
// spelled "Windows.h"/"WinUser.h" and only resolve correctly on a
// case-insensitive filesystem. This directory provides matching-case
// forwarders to the real (lowercase-named) headers so #include <Windows.h>
// keeps working on a case-sensitive filesystem too. No independent status
// of its own -- see the forwarded-to header for real status.
//
// @note Status: HEADER_ONLY
#pragma once
#include <windows.h>
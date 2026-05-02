#pragma once

#include <minwindef.h>
#include <windef.h>

/**
 * @brief Win32-style message entry used by the Free API dispatch loop.
 *
 * `MSG` is the public container passed through the emulated message queue.
 * It is filled by functions such as `GetMessage()` and `PeekMessage()`,
 * optionally processed by `TranslateMessage()`, and delivered to a window
 * procedure by `DispatchMessage()`.
 *
 * In Free API, messages may come from translated SDL events, timer callbacks,
 * internal window events, or explicit calls such as `PostMessage()` and
 * `PostQuitMessage()`.
 *
 * @note Status: PARTIAL
 *       The layout matches the supported Win32 subset, but Free API does not
 *       fully emulate all native queue, thread, timing, and coordinate
 *       semantics.
 * @note REVIEWED
 */
typedef struct tagMSG
{
    /** @brief Target window handle, or `NULL` for thread/global messages. */
    HWND hwnd;

    /** @brief Message identifier, for example, `WM_PAINT`, `WM_KEYDOWN`, or `WM_QUIT`. */
    UINT message;

    /** @brief First message-specific parameter; meaning depends on @ref message. */
    WPARAM wParam;

    /** @brief Second message-specific parameter; meaning depends on @ref message. */
    LPARAM lParam;

    /**
     * @brief Message timestamp in milliseconds.
     *
     * Present for Win32 `MSG` compatibility. Free API may currently provide this
     * only for selected message sources; otherwise it may be zero.
     */
    DWORD time;

    /** @brief Cursor position associated with the message, where available. */
    POINT pt;
} MSG, *PMSG, *LPMSG;

#pragma once

/**
 * @brief Callback type used by multimedia periodic timers.
 * @note Status: PARTIAL
 * @note REVIEWED
 */
typedef void(CALLBACK* LPTIMECALLBACK)(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);

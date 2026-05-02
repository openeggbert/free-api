//
// Created by robertvokac on 5/2/26.
//

#ifndef FREE_API_WINDOWS_SYNCHAPI_H
#define FREE_API_WINDOWS_SYNCHAPI_H

/**
 * @name WinBase subset
 * @brief Process, timing and debug helpers used by the game.
 * @note Status: PARTIAL
 */
/** @{ */
/** @brief Suspends the current thread for at least `dwMilliseconds`. @note Status: IMPLEMENTED */
void WINAPI Sleep(DWORD dwMilliseconds);

#endif //FREE_API_WINDOWS_SYNCHAPI_H

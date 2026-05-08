#pragma once

namespace FreeApi::Platform {

/**
 * @brief Returns the resident set size (RSS) of the current process in KB.
 *
 * On Linux reads /proc/self/status; on other platforms returns 0.
 * The __linux__-specific code is isolated inside the .cpp file.
 */
long ReadRssKB();

} // namespace FreeApi::Platform

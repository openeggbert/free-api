#pragma once

#include <string>

namespace FreeApi::Internal {

std::string NormalizePath(const char* path);
std::string BuildCommandLine(int argc, char** argv);

} // namespace FreeApi::Internal

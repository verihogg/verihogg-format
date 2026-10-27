#pragma once

#include <optional>
#include <string>

namespace format {

// Computed once per process from the executable and loaded libraries on Linux.
// Unsupported platforms and unreadable objects return nullopt: caching must
// then be disabled rather than sharing an "unknown" build identity.
[[nodiscard]] auto formatterBuildId() -> const std::optional<std::string>&;

}  // namespace format

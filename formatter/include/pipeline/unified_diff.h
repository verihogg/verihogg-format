#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace format {

// Generates a Git-style unified patch in memory, without a repository or
// subprocess. Returns an empty string when bytes match. Preserves CRLF and
// missing final newlines and quotes unusual file labels.
// path is relative to the patch root. Invalid labels throw
// std::invalid_argument; libgit2 failures throw std::runtime_error.
[[nodiscard]] auto makeUnifiedDiff(std::string_view original,
                                   std::string_view formatted,
                                   const std::filesystem::path& path,
                                   size_t context_lines = 3) -> std::string;

}  // namespace format

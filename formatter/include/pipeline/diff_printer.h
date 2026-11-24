#pragma once

#include <filesystem>
#include <iosfwd>
#include <string_view>

namespace format {

struct DiffPrintOptions {
  bool color = false;
  bool hyperlinks = false;
  std::filesystem::path source_path{};
};

// Prints a unified patch, optionally highlighting removed lines in red, added
// lines in green, hunk headers in cyan, and file headers in bold. One-to-one
// replacements emphasize changed spans with inverse video, including spaces.
// Hyperlinks wrap hunk headers in OSC 8 links to the first change in the
// original source_path. Pure insertions link to the preceding line (at least
// line 1). An empty source_path disables links. The patch must describe that
// single file. Stripping added terminal sequences recovers the patch bytes.
auto printDiff(std::ostream& out, std::string_view patch,
               const DiffPrintOptions& options = {}) -> void;

}  // namespace format

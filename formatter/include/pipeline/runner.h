#pragma once
#include <filesystem>
#include <gsl/span>
#include <ostream>

#include "data/format_style.h"

namespace format {

struct Streams {
  std::ostream* out = nullptr;
  std::ostream* err = nullptr;
  bool diff_color = false;
  bool diff_hyperlinks = false;
};

// Uses styles[i] for files[i]. Check and diff modes never write source files;
// diff emits patches, and check reports dirty files as failures. Both print
// the grouped summary only when warnings occur, without per-file status lines.
// Regular mode prints per-file status and an end-of-run summary to streams.err.
// Returns failure count in check mode, read error count in diff-only mode,
// and warning count otherwise. Patch output errors in diff mode throw.
auto runFormatter(gsl::span<const std::filesystem::path> files,
                  gsl::span<const FormatStyle> styles, const RunConfig& run,
                  Streams streams) -> int;
}  // namespace format

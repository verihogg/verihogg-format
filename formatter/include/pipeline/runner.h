#pragma once
#include <filesystem>
#include <gsl/span>
#include <ostream>

#include "data/format_style.h"

namespace format {

struct Streams {
  std::ostream* out;
  std::ostream* err;
};

// Formats files. Returns the number of warnings, or with run.check the number
// of files that need formatting.
auto runFormatter(gsl::span<const std::filesystem::path> files,
                  const format::FormatStyle& style,
                  const format::RunConfig& run, Streams streams) -> int;
}  // namespace format

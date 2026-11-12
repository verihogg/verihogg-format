#pragma once
#include <filesystem>
#include <gsl/span>
#include <ostream>
#include <string_view>

#include "data/format_style.h"
#include "data/format_warning.h"

namespace format {

struct Streams {
  std::ostream* out;
  std::ostream* err;
};

// Prints a formatter warning to the given stream.
auto printWarning(std::ostream& os, std::string_view path,
                  const FormatWarning& warning) -> void;

// Uses styles[i] for files[i]. Check and diff modes never write source files;
// diff emits patches, and check reports dirty files as failures.
// Returns failure count in check mode, read error count in diff-only mode,
// and warning count otherwise. Patch output errors in diff mode throw.
auto runFormatter(gsl::span<const std::filesystem::path> files,
                  gsl::span<const FormatStyle> styles, const RunConfig& run,
                  Streams streams) -> int;
}  // namespace format

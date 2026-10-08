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

// Formats each file with the style already resolved for it: styles[i] is the
// style for files[i].
auto runFormatter(gsl::span<const std::filesystem::path> files,
                  gsl::span<const FormatStyle> styles, const RunConfig& run,
                  Streams streams) -> int;
}  // namespace format

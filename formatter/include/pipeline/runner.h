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

// Formats each file with the style already resolved for it: styles[i] is the
// style for files[i]. With run.check, reports differences without writing
// files, printing per-file status and — when there are diagnostics — the same
// end-of-run summary (minus per-file status lines). In regular mode, prints
// per-file status and an end-of-run summary to streams.err. Returns failed
// check count in check mode, otherwise warning count.
auto runFormatter(gsl::span<const std::filesystem::path> files,
                  gsl::span<const FormatStyle> styles, const RunConfig& run,
                  Streams streams) -> int;
}  // namespace format

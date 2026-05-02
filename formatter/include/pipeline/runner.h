#pragma once
#include <filesystem>
#include <ostream>
#include <span>

#include "data/format_style.h"

namespace format {

struct Streams {
  std::ostream* out;
  std::ostream* err;
};

auto runFormatter(std::span<const std::filesystem::path> files,
                  const format::FormatStyle& style,
                  const format::RunConfig& run, Streams streams) -> int;
}  // namespace format

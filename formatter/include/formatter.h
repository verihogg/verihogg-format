#pragma once

#include <string>
#include <string_view>

#include "data/format_style.h"

namespace format {
struct FormatResult {
  std::string formatted_text;
};

auto format(std::string_view source_text, FormatStyle style) -> FormatResult;
}  // namespace format

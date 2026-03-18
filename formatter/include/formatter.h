#pragma once

#include <string.h>

#include <string_view>

#include "format_style.h"

namespace format {
struct FormatResult {
  std::string formatted_text;
  bool changed{false};
};

FormatResult format(std::string_view source_text, const FormatStyle& style);
}  // namespace format

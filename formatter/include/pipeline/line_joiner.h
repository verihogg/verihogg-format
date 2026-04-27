#pragma once

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {
void joinLines(std::vector<UnwrappedLine<FormatToken>>& lines,
               const FormatStyle& style);
}  // namespace format

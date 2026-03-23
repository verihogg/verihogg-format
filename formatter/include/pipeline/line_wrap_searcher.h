#pragma once

#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {
[[nodiscard]] auto searchLineWraps(const UnwrappedLine<FormatToken>& line,
                                   const FormatStyle& style,
                                   ColumnNumber initial_column = 0)
    -> std::vector<InterTokenDecision>;
}  // namespace format

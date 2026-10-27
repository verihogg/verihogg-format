#pragma once

#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {
// With try_wrap_long_lines disabled, only mandatory breaks are introduced;
// overflowing lines preserve their original inter-token whitespace.
[[nodiscard]] auto searchLineWraps(const UnwrappedLine<FormatToken>& line,
                                   const FormatStyle& style,
                                   ColumnNumber initial_column = 0)
    -> std::vector<InterTokenDecision>;

auto applyLineWraps(std::vector<UnwrappedLine<FormatToken>>& lines,
                    const FormatStyle& style) -> void;
}  // namespace format

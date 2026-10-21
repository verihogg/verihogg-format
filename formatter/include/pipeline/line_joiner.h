#pragma once

#include <functional>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {

class LineJoiner {
 public:
  explicit LineJoiner(const FormatStyle& style) : style(style) {}

  // Join a control header and one simple body, optionally followed by fitting
  // else branches. Keep blocks, fallback lines, directives and mandatory breaks
  // separate; include indentation in the column budget. Bracket links must be
  // local to each input line (as produced by TokenAnnotator) and are rebased
  // after merging. Structural nesting levels must come from TreeUnwrapper;
  // they are independent of the printed indentation, including zero spaces.
  // Retain the unwrapper's EOF partition: its trivia can prevent joining the
  // last statement when a trailing comment has unmeasured width.
  auto join(std::vector<UnwrappedLine<FormatToken>>& lines) const -> void;

 private:
  std::reference_wrapper<const FormatStyle> style;
};

}  // namespace format

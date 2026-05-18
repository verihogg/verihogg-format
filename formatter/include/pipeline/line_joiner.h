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

  auto join(std::vector<UnwrappedLine<FormatToken>>& lines) const -> void;

 private:
  std::reference_wrapper<const FormatStyle> style;
};

}  // namespace format

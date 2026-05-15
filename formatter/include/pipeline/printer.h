#pragma once

#include <ostream>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {

class Printer {
 public:
  explicit Printer(const FormatStyle& style);

  auto print(const std::vector<UnwrappedLine<FormatToken>>& lines,
             std::ostream& os) const -> void;

 private:
  std::string_view line_ending_;
};

}  // namespace format

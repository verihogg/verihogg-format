#pragma once

#include <cstddef>
#include <ostream>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {

// Maps an original-source byte offset to the first formatted-output line
// that originates at or after it. Anchors are sorted by source_offset.
struct SourceOutputAnchor {
  std::size_t source_offset = 0;
  std::size_t output_line = 0;  // 1-based line in the formatted output
};

// 1-based formatted-output line for source_offset, or 0 when unknown.
[[nodiscard]] auto mapToOutputLine(
    const std::vector<SourceOutputAnchor>& anchors, std::size_t source_offset)
    -> std::size_t;

class Printer {
 public:
  explicit Printer(const FormatStyle& style);

  // Writes the formatted text to os and returns the source-to-output anchor
  // list for mapping diagnostics onto post-format line numbers.
  auto print(const std::vector<UnwrappedLine<FormatToken>>& lines,
             std::ostream& os) const -> std::vector<SourceOutputAnchor>;

 private:
  std::string_view line_ending_;
};

}  // namespace format

#pragma once

#include <cstddef>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {
struct AlignmentCell {
  size_t start_idx = 0;
  size_t end_idx = 0;
  size_t width = 0;
};

using AlignmentRow = std::vector<AlignmentCell>;

struct AlignmentGroup {
  std::vector<size_t> line_indices;
  size_t num_columns = 0;
  std::vector<AlignmentRow> table;
  std::vector<size_t> col_max_width;
};

void align(std::vector<UnwrappedLine<FormatToken>>& lines,
           const FormatStyle& style);
}  // namespace format

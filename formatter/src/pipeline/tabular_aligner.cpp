#include "pipeline/tabular_aligner.h"

#include <algorithm>
#include <string_view>

namespace format {

static constexpr size_t kMinColumnGap = 4;
static constexpr size_t kMinGroupSize = 2;

[[nodiscard]] auto static range_width(
    const std::vector<UnwrappedLineNode<FormatToken>>& tokens, size_t start,
    size_t end) -> size_t {
  size_t width = 0;
  for (size_t i = start; i < end; ++i) {
    if (i != start) {
      width += tokens[i].token.before.spaces_required;
    }
    width += tokens[i].token.token.rawText().size();
  }
  return width;
}

[[nodiscard]] static auto is_suffix_token(const FormatToken& ft) -> bool {
  if (ft.type != TokenType::kUnknown) {
    return ft.type == TokenType::kComma || ft.type == TokenType::kSemicolon;
  }
  using TK = slang::parsing::TokenKind;
  return ft.token.kind == TK::Comma || ft.token.kind == TK::Semicolon;
}

[[nodiscard]] auto static build_row(const UnwrappedLine<FormatToken>& line)
    -> AlignmentRow {
  AlignmentRow row;
  const auto& tokens = line.tokens;
  if (tokens.empty()) {
    return row;
  }

  size_t cell_start = 0;

  auto close_cell = [&](size_t end) {
    if (end <= cell_start) {
      return;
    }
    row.push_back(AlignmentCell{
        .start_idx = cell_start,
        .end_idx = end,
        .width = range_width(tokens, cell_start, end),
    });
    cell_start = end;
  };

  size_t i = 1;
  while (i < tokens.size()) {
    const FormatToken& ft = tokens[i].token;

    if (ft.balancing == GroupBalancing::kOpen) {
      if (ft.matching_bracket != nullptr) {
        size_t j = i + 1;
        while (j < tokens.size() && &tokens[j].token != ft.matching_bracket) {
          ++j;
        }
        i = j + 1;
      } else {
        int depth = 1;
        ++i;
        while (i < tokens.size() && depth > 0) {
          if (tokens[i].token.balancing == GroupBalancing::kOpen) {
            ++depth;
          }
          if (tokens[i].token.balancing == GroupBalancing::kClose) {
            --depth;
          }
          ++i;
        }
      }
      continue;
    }
    if (is_suffix_token(ft)) {
      ++i;
      continue;
    }
    close_cell(i);
    ++i;
  }
  close_cell(tokens.size());
  return row;
}

static void apply_group(AlignmentGroup& group,
                        std::vector<UnwrappedLine<FormatToken>>& children) {
  if (group.line_indices.size() < kMinGroupSize) {
    return;
  }

  std::vector<size_t> column_offset(group.num_columns, 0);
  for (size_t c = 1; c < group.num_columns; ++c) {
    column_offset[c] =
        column_offset[c - 1] + group.col_max_width[c - 1] + kMinColumnGap;
  }

  for (size_t row_i = 0; row_i < group.line_indices.size(); ++row_i) {
    const size_t line_i = group.line_indices[row_i];
    auto& line = children[line_i];
    auto& tokens = line.tokens;
    const AlignmentRow& row = group.table[row_i];

    size_t cursor = 0;

    for (size_t c = 0; c < row.size(); ++c) {
      const AlignmentCell& cell = row[c];

      if (c == 0) {
        cursor = cell.width;
        continue;
      }

      const size_t target = column_offset[c];
      const size_t spaces = (target > cursor) ? (target - cursor) : 1;

      FormatToken& first = tokens[cell.start_idx].token;
      first.decision.spaces_before = spaces;

      cursor = target + cell.width;
    }
    line.partition_policy = PartitionPolicy::kAlreadyFormatted;
  }
}

static void process_children(
    std::vector<UnwrappedLine<FormatToken>>& children) {
  AlignmentGroup current_group;

  auto flush = [&]() {
    apply_group(current_group, children);
    current_group = AlignmentGroup{};
  };

  for (size_t i = 0; i < children.size(); ++i) {
    auto& child = children[i];

    for (auto& node : child.tokens) {
      if (!node.children.empty()) {
        process_children(node.children);
      }
    }

    if (child.partition_policy != PartitionPolicy::kTabularAlignment) {
      continue;
    }

    if (child.tokens.empty()) {
      continue;
    }

    AlignmentRow row = build_row(child);
    if (row.empty()) {
      continue;
    }

    const size_t ncols = row.size();

    if (!current_group.line_indices.empty() &&
        current_group.num_columns != ncols) {
      flush();
    }

    if (current_group.line_indices.empty()) {
      current_group.num_columns = ncols;
      current_group.col_max_width.assign(ncols, 0);
    }

    for (size_t c = 0; c < ncols; ++c) {
      current_group.col_max_width[c] =
          std::max(current_group.col_max_width[c], row[c].width);
    }

    current_group.line_indices.push_back(i);
    current_group.table.push_back(std::move(row));
  }
  flush();
}

void align(std::vector<UnwrappedLine<FormatToken>>& lines,
           const FormatStyle& /*style*/) {
  process_children(lines);
}

}  // namespace format

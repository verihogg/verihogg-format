#include "pipeline/tabular_aligner.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

#include "pipeline/alignment_layout.h"

namespace format {

static constexpr size_t kMinColumnGap = 4;
static constexpr size_t kMinGroupSize = 2;
static constexpr size_t kCommentColumnGap = 8;
static constexpr size_t kMinSpacesBeforeAssignment = 2;
static constexpr size_t kMinSpacesAfterAssignment = 1;
static constexpr size_t kMinSpacesCaseItem = 1;
static constexpr size_t kMinSpacesPortDirectionType = 2;
static constexpr size_t kMinSpacesPortTypeName = 8;

struct PendingGroup {
  PartitionPolicy policy = PartitionPolicy::kAlwaysExpand;
  std::vector<size_t> line_indices;
  alignment::Group layout;
};

[[nodiscard]] static auto is_assignment_operator(const FormatToken& ft)
    -> bool {
  using TK = slang::parsing::TokenKind;
  return ft.type == TokenType::kAssignmentOperator ||
         ft.token.kind == TK::LessThanEquals;
}

[[nodiscard]] static auto is_directive_line(
    const UnwrappedLine<FormatToken>& line) -> bool {
  if (line.tokens.empty()) {
    return false;
  }
  return line.tokens.front().token.kind == slang::parsing::TokenKind::Directive;
}

[[nodiscard]] static auto is_suffix_token(const FormatToken& ft) -> bool {
  if (ft.type != TokenType::kUnknown) {
    return ft.type == TokenType::kComma || ft.type == TokenType::kSemicolon;
  }
  using TK = slang::parsing::TokenKind;
  return ft.token.kind == TK::Comma || ft.token.kind == TK::Semicolon;
}

[[nodiscard]] auto static build_row(const UnwrappedLine<FormatToken>& line)
    -> std::vector<alignment::Cell> {
  std::vector<alignment::Cell> row;
  const auto& tokens = line.tokens;
  if (tokens.empty()) {
    return row;
  }

  size_t cell_start = 0;

  auto close_cell = [&](size_t end) {
    if (end <= cell_start) {
      return;
    }
    row.push_back({.begin = cell_start, .end = end});
    cell_start = end;
  };

  size_t i = 0;
  while (i < tokens.size()) {
    const FormatToken& ft = tokens.at(i);
    if (ft.balancing == GroupBalancing::kOpen) {
      if (ft.matching_bracket != nullptr) {
        size_t j = i + 1;
        while (j < tokens.size() && &tokens.at(j) != ft.matching_bracket) {
          ++j;
        }
        i = j + 1;
      } else {
        size_t depth = 1;
        ++i;
        while (i < tokens.size() && depth > 0) {
          if (tokens.at(i).balancing == GroupBalancing::kOpen) {
            ++depth;
          }
          if (tokens.at(i).balancing == GroupBalancing::kClose) {
            --depth;
          }
          ++i;
        }
      }
      continue;
    }
    if (is_suffix_token(ft) || (i > 0 && ft.before.spaces_required == 0)) {
      ++i;
      continue;
    }
    close_cell(i);
    ++i;
  }
  close_cell(tokens.size());
  return row;
}

[[nodiscard]] static auto build_assignment_row(
    const UnwrappedLine<FormatToken>& line) -> std::vector<alignment::Cell> {
  std::vector<alignment::Cell> row;
  auto& tokens = line.tokens;
  if (tokens.empty()) {
    return row;
  }

  size_t op_idx = tokens.size();
  int depth = 0;

  for (size_t i = 0; i < tokens.size(); ++i) {
    const auto& ft = tokens.at(i);

    if (ft.balancing == GroupBalancing::kOpen) {
      ++depth;
      continue;
    }
    if (ft.balancing == GroupBalancing::kClose) {
      --depth;
      continue;
    }
    if (depth > 0) {
      continue;
    }

    if (is_assignment_operator(ft)) {
      op_idx = i;
      break;
    }
  }

  if (op_idx == 0 || op_idx == tokens.size()) {
    return row;
  }

  row.push_back({.begin = 0, .end = op_idx});
  row.push_back({.begin = op_idx, .end = tokens.size()});
  return row;
}

[[nodiscard]] static auto has_comment(
    const std::vector<UnwrappedLine<FormatToken>>& lines, size_t line_idx)
    -> bool {
  const size_t next_idx = line_idx + 1;
  if (next_idx >= lines.size()) {
    return false;
  }

  const auto& next_tokens = lines.at(next_idx).tokens;
  if (next_tokens.empty()) {
    return false;
  }

  for (const auto& trivia : next_tokens.at(0).token.trivia()) {
    using TK = slang::parsing::TriviaKind;
    if (trivia.kind == TK::LineComment || trivia.kind == TK::BlockComment) {
      return true;
    }
    if (trivia.kind == TK::EndOfLine) {
      break;
    }
  }
  return false;
}

[[nodiscard]] static auto is_case_item_group(
    const PendingGroup& group,
    const std::vector<UnwrappedLine<FormatToken>>& lines) -> bool {
  if (group.policy != PartitionPolicy::kTabularAlignment ||
      group.layout.rows.empty() || group.layout.rows.front().cells.size() < 2) {
    return false;
  }
  const auto& tokens = lines.at(group.line_indices.front()).tokens;
  const auto& second = group.layout.rows.front().cells.at(1);
  return (second.end > second.begin) &&
         tokens.at(second.begin).token.kind == slang::parsing::TokenKind::Colon;
}

[[nodiscard]] static auto is_port_list_group(
    const PendingGroup& group,
    const std::vector<UnwrappedLine<FormatToken>>& lines) -> bool {
  if (group.policy != PartitionPolicy::kTabularAlignment ||
      group.line_indices.empty()) {
    return false;
  }
  const auto& tokens = lines.at(group.line_indices.front()).tokens;
  return !tokens.empty() && tokens.front().type == TokenType::kPortDirection;
}

[[nodiscard]] static auto gap_before_column(
    const PendingGroup& group,
    const std::vector<UnwrappedLine<FormatToken>>& lines, size_t column_index)
    -> size_t {
  if (group.policy == PartitionPolicy::kAssignmentAlignment) {
    return kMinSpacesBeforeAssignment;
  }
  if (is_case_item_group(group, lines)) {
    return kMinSpacesCaseItem;
  }
  if (is_port_list_group(group, lines)) {
    if (column_index == 1) {
      return kMinSpacesPortDirectionType;
    }
    if (column_index == 2) {
      return kMinSpacesPortTypeName;
    }
  }
  return kMinColumnGap;
}

static void apply_group(PendingGroup& group,
                        std::vector<UnwrappedLine<FormatToken>>& lines,
                        const FormatStyle& style) {
  if (group.line_indices.size() < kMinGroupSize) {
    return;
  }
  const size_t num_columns = group.layout.rows.front().cells.size();
  std::vector<size_t> gaps(num_columns, 0);
  for (size_t c = 1; c < num_columns; ++c) {
    gaps.at(c) = gap_before_column(group, lines, c);
  }
  group.layout.alignment_gaps = std::move(gaps);
  const auto action = alignment::applyPolicy(
      group.layout, AlignmentPolicy::kAlign, style.column_limit);
  const auto* formatted =
      action ? std::get_if<alignment::FormatSpacing>(&*action) : nullptr;
  if (formatted == nullptr || !formatted->alignment_width) {
    return;
  }

  size_t max_total_width = 0;
  bool any_comment = false;
  for (size_t line_i : group.line_indices) {
    max_total_width =
        std::max(max_total_width, lines.at(line_i).indentation_spaces +
                                      *formatted->alignment_width);
    any_comment = any_comment || has_comment(lines, line_i);
  }
  const size_t comment_column = max_total_width + kCommentColumnGap;

  for (size_t row_i = 0; row_i < group.line_indices.size(); ++row_i) {
    const size_t line_i = group.line_indices.at(row_i);
    auto& line = lines.at(line_i);
    auto& tokens = line.tokens;
    const auto& row = group.layout.rows.at(row_i);
    const auto& spaces = formatted->spaces.at(row_i);
    size_t end_column = line.indentation_spaces;
    for (size_t i = 0; i < row.tokens.size(); ++i) {
      if (i > 0) {
        end_column += spaces.at(i);
      }
      end_column += row.tokens.at(i).text.size();
    }
    for (size_t c = 1; c < row.cells.size(); ++c) {
      const size_t index = row.cells.at(c).begin;
      tokens.at(index).before.spaces_required = spaces.at(index);
      if (group.policy == PartitionPolicy::kAssignmentAlignment && c == 1 &&
          index + 1 < tokens.size()) {
        tokens.at(index + 1).before.spaces_required = kMinSpacesAfterAssignment;
      }
    }
    if (any_comment) {
      const size_t next_i = line_i + 1;
      if (next_i < lines.size() && !lines.at(next_i).tokens.empty()) {
        const size_t comment_spaces = (comment_column > end_column)
                                          ? (comment_column - end_column)
                                          : kCommentColumnGap;
        lines.at(next_i).tokens.at(0).before.comment_spaces = comment_spaces;
      }
    }
    line.partition_policy = PartitionPolicy::kAlreadyFormatted;
  }
}

static void process_lines(std::vector<UnwrappedLine<FormatToken>>& lines,
                          const FormatStyle& style) {
  PendingGroup current_group;

  auto flush = [&]() {
    apply_group(current_group, lines, style);
    current_group = PendingGroup{};
  };

  for (size_t i = 0; i < lines.size(); ++i) {
    auto& line = lines.at(i);

    const bool is_tabular =
        (line.partition_policy == PartitionPolicy::kTabularAlignment);
    const bool is_assignment =
        (line.partition_policy == PartitionPolicy::kAssignmentAlignment);

    if (!is_tabular && !is_assignment) {
      if (is_directive_line(line)) {
        continue;
      }
      if (!current_group.line_indices.empty()) {
        flush();
      }
      continue;
    }

    if (line.tokens.empty()) {
      if (!current_group.line_indices.empty()) {
        flush();
      }
      continue;
    }

    const bool unsupported_token =
        std::ranges::any_of(line.tokens, [](const FormatToken& token) {
          const auto text = token.token.rawText();
          return text.empty() ||
                 text.find_first_of("\r\n") != std::string_view::npos;
        });
    if (unsupported_token) {
      if (!current_group.line_indices.empty()) {
        flush();
      }
      continue;
    }

    std::vector<alignment::Cell> cells;
    if (is_tabular) {
      cells = build_row(line);
    } else {
      cells = build_assignment_row(line);
    }

    if (cells.empty()) {
      if (!current_group.line_indices.empty()) {
        flush();
      }
      continue;
    }

    if (!current_group.line_indices.empty() &&
        (current_group.layout.rows.front().cells.size() != cells.size() ||
         current_group.policy != line.partition_policy)) {
      flush();
    }

    if (current_group.line_indices.empty()) {
      current_group.policy = line.partition_policy;
    }

    auto& row = current_group.layout.rows.emplace_back();
    row.indentation = line.indentation_spaces;
    row.cells = std::move(cells);
    for (const auto& token : line.tokens) {
      row.tokens.push_back({.text = token.token.rawText(),
                            .min_spaces = token.before.spaces_required,
                            .source_offset = std::nullopt});
    }
    current_group.line_indices.push_back(i);
  }
  flush();
}

void align(std::vector<UnwrappedLine<FormatToken>>& lines,
           const FormatStyle& style) {
  process_lines(lines, style);
}

}  // namespace format

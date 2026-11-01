#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "data/format_style.h"

namespace format::alignment {

// A parser-independent table calculation. Gaps are indexed by column, with
// the first gap unused. Widths do not include the gap before a cell.
// Returns no layout for mismatched input sizes.
struct Layout {
  std::vector<size_t> offsets;
  size_t width = 0;
};

[[nodiscard]] auto calculateLayout(const std::vector<size_t>& widths,
                                   const std::vector<size_t>& gaps)
    -> std::optional<Layout>;

struct Token {
  std::string_view text;
  size_t min_spaces = 0;
  // Offset within Group::original_text; required for preserve and infer.
  std::optional<size_t> source_offset;
};

struct Cell {
  size_t begin = 0;
  size_t end = 0;  // exclusive
};

struct Row {
  size_t indentation = 0;
  std::vector<Token> tokens;
  // Ordered, non-empty ranges covering all tokens in this row.
  std::vector<Cell> cells;
};

struct Group {
  std::vector<Row> rows;
  // Exact source range containing the tokens, comments included as tokens.
  std::optional<std::string_view> original_text;
  // Exact alignment gaps by column; the first entry is unused. These override
  // cell minimums for alignment only. An empty vector uses cell minimums.
  std::vector<size_t> alignment_gaps{};
};

// Minimum or aligned spaces before each token, indexed by row and token.
struct FormatSpacing {
  std::vector<std::vector<size_t>> spaces;
  // Width of the shared layout when alignment was applied to multiple rows.
  std::optional<size_t> alignment_width;
};

// The caller must output this exact source range.
struct PreserveSource {
  std::string text;
};

using AlignmentAction = std::variant<FormatSpacing, PreserveSource>;

// Returns no result for invalid inputs, unavailable source, unsupported source
// whitespace in infer, or alignment exceeding the limit. No input is modified.
[[nodiscard]] auto applyPolicy(const Group& group, AlignmentPolicy policy,
                               size_t column_limit)
    -> std::optional<AlignmentAction>;

}  // namespace format::alignment

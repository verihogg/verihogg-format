#include "pipeline/alignment_layout.h"

#include <algorithm>
#include <utility>

namespace format::alignment {

namespace {

[[nodiscard]] auto validGroup(const Group& group) -> bool {
  if (group.rows.empty()) {
    return group.alignment_gaps.empty();
  }
  const size_t count = group.rows.front().cells.size();
  if (count == 0 ||
      (!group.alignment_gaps.empty() && group.alignment_gaps.size() != count)) {
    return false;
  }
  for (const auto& row : group.rows) {
    if (row.cells.size() != count) {
      return false;
    }
    size_t end = 0;
    for (const auto& cell : row.cells) {
      if (cell.begin != end || cell.end <= cell.begin ||
          cell.end > row.tokens.size()) {
        return false;
      }
      end = cell.end;
    }
    if (end != row.tokens.size()) {
      return false;
    }
    for (const auto& token : row.tokens) {
      if (token.text.empty()) {
        return false;
      }
    }
  }
  return true;
}

[[nodiscard]] auto whitespace(std::string_view text) -> bool {
  return text.find_first_not_of(" \t\r\n\f\v") == std::string_view::npos;
}

// Validate the complete source slice and every offset before producing output.
[[nodiscard]] auto validSource(const Group& group) -> bool {
  if (!group.original_text) {
    return false;
  }
  const auto source = group.original_text.value_or(std::string_view{});
  size_t end = 0;
  for (const auto& row : group.rows) {
    for (const auto& token : row.tokens) {
      // The offset must be present (zero is valid), ordered, and in bounds.
      // Gaps must contain only whitespace, and token text must match source.
      if (!token.source_offset || token.source_offset.value_or(0) < end ||
          token.source_offset.value_or(0) > source.size() ||
          token.text.size() > source.size() - token.source_offset.value_or(0) ||
          !whitespace(
              source.substr(end, token.source_offset.value_or(0) - end)) ||
          source.substr(token.source_offset.value_or(0), token.text.size()) !=
              token.text) {
        return false;
      }
      end = token.source_offset.value_or(0) + token.text.size();
    }
  }
  return whitespace(source.substr(end));
}

}  // namespace

auto calculateLayout(const std::vector<size_t>& widths,
                     const std::vector<size_t>& gaps) -> std::optional<Layout> {
  if (widths.size() != gaps.size()) {
    return std::nullopt;
  }
  Layout result{.offsets = std::vector<size_t>(widths.size(), 0), .width = 0};
  for (size_t i = 1; i < widths.size(); ++i) {
    result.offsets.at(i) =
        result.offsets.at(i - 1) + widths.at(i - 1) + gaps.at(i);
  }
  if (!widths.empty()) {
    result.width = result.offsets.back() + widths.back();
  }
  return result;
}

auto applyPolicy(const Group& group, AlignmentPolicy policy,
                 size_t column_limit) -> std::optional<AlignmentAction> {
  if (!validGroup(group)) {
    return std::nullopt;
  }
  if ((policy == AlignmentPolicy::kPreserve ||
       policy == AlignmentPolicy::kInfer) &&
      !validSource(group)) {
    return std::nullopt;
  }
  if (policy == AlignmentPolicy::kPreserve) {
    return PreserveSource{
        std::string(group.original_text.value_or(std::string_view{}))};
  }

  FormatSpacing result;
  if (group.rows.empty()) {
    return result;
  }
  for (const auto& row : group.rows) {
    for (const auto& token : row.tokens) {
      if (token.text.find_first_of("\r\n") != std::string_view::npos) {
        return std::nullopt;
      }
    }
  }

  const size_t count = group.rows.front().cells.size();
  std::vector<size_t> widths(count, 0), gaps(count, 0);
  std::vector<std::vector<size_t>> row_widths;

  for (const auto& row : group.rows) {
    auto& spaces = result.spaces.emplace_back();
    for (const auto& token : row.tokens) {
      spaces.push_back(token.min_spaces);
    }

    auto& current_widths = row_widths.emplace_back();
    for (size_t c = 0; c < count; ++c) {
      const auto cell = row.cells.at(c);
      size_t width = 0;
      for (size_t i = cell.begin; i < cell.end; ++i) {
        if (i != cell.begin) {
          width += row.tokens.at(i).min_spaces;
        }
        width += row.tokens.at(i).text.size();
      }
      current_widths.push_back(width);

      widths.at(c) = std::max(widths.at(c), width);
      if (c > 0) {
        gaps.at(c) = std::max(gaps.at(c), row.tokens.at(cell.begin).min_spaces);
      }
    }
  }

  if (!group.alignment_gaps.empty()) {
    gaps = group.alignment_gaps;
  }
  if (policy == AlignmentPolicy::kFlushLeft) {
    return result;
  }
  const auto layout = calculateLayout(widths, gaps);
  if (!layout) {
    return std::nullopt;
  }

  size_t max_delta = 0;
  bool fits = true;
  auto aligned = result.spaces;
  for (size_t r = 0; r < group.rows.size(); ++r) {
    const auto& row = group.rows.at(r);
    const size_t total_width = row.indentation + layout->width;
    if (group.rows.size() > 1 && total_width > column_limit) {
      fits = false;
    }

    for (size_t c = 1; c < count; ++c) {
      const size_t prev_end =
          layout->offsets.at(c - 1) + row_widths.at(r).at(c - 1);
      const size_t spaces = layout->offsets.at(c) - prev_end;
      const size_t index = row.cells.at(c).begin;
      aligned.at(r).at(index) = spaces;
      const size_t minimum = row.tokens.at(index).min_spaces;
      max_delta = std::max(
          max_delta, spaces > minimum ? spaces - minimum : minimum - spaces);
    }
  }
  if (policy == AlignmentPolicy::kInfer) {
    // Match Verible's thresholds: <= 2 alignment difference forces alignment;
    // otherwise <= 2 excess spaces flush left, >= 4 align, and 3 preserve.
    size_t excess = 0;
    const auto source = group.original_text.value_or(std::string_view{});
    for (const auto& row : group.rows) {
      for (size_t i = 1; i < row.tokens.size(); ++i) {
        const auto& prev = row.tokens.at(i - 1);
        const auto& token = row.tokens.at(i);
        const size_t start = prev.source_offset.value_or(0) + prev.text.size();
        const auto gap =
            source.substr(start, token.source_offset.value_or(0) - start);
        if (gap.find_first_not_of(' ') != std::string_view::npos) {
          return std::nullopt;
        }
        if (gap.size() > token.min_spaces) {
          excess = std::max(excess, gap.size() - token.min_spaces);
        }
      }
    }
    if (max_delta <= 2 || excess >= 4) {
      policy = AlignmentPolicy::kAlign;
    } else if (excess <= 2) {
      policy = AlignmentPolicy::kFlushLeft;
    } else {
      policy = AlignmentPolicy::kPreserve;
    }
    if (policy == AlignmentPolicy::kPreserve) {
      return PreserveSource{std::string(source)};
    }
  }
  if (policy == AlignmentPolicy::kAlign && group.rows.size() > 1) {
    if (!fits) {
      return std::nullopt;
    }
    result.spaces = std::move(aligned);
    result.alignment_width = layout->width;
  }
  return result;
}

}  // namespace format::alignment

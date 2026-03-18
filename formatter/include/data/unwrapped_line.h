#pragma once

#include <cstddef>
#include <cstdint>

#include "format_style.h"  // IndentLevel, ColumnNumber
#include "format_token.h"  // FormatToken, FormatTokenRange

namespace format {
enum class PartitionPolicy : uint8_t {
  kAlwaysExpand,
  kFitOnLineElseExpand,
  kTabularAlignment,
  kAlreadyFormatted,
  kPreserveOriginal,
};

struct UnwrappedLine {
  FormatTokenRange tokens{nullptr, nullptr};  // Диапазон [begin, end) ???
  IndentLevel indentation_spaces{0};
  PartitionPolicy partition_policy{PartitionPolicy::kFitOnLineElseExpand};
  const void* origin{nullptr};

  FormatToken* begin() const noexcept { return tokens.first; }
  FormatToken* end() const noexcept { return tokens.second; }

  bool empty() const noexcept { return tokens.first == tokens.second; }
  std::ptrdiff_t size() const noexcept { return tokens.second - tokens.first; }

  ColumnNumber computeWidth() const noexcept;

  bool fitsOnLine(ColumnNumber current_column,
                  ColumnNumber column_limit) const noexcept;
};
}  // namespace format

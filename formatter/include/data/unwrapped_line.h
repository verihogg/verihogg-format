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
};

struct UnwrappedLine {
  std::span<FormatToken> tokens;
  IndentLevel indentation_spaces;
  PartitionPolicy partition_policy;

  ColumnNumber computeWidth() const noexcept;

  bool fitsOnLine(ColumnNumber current_column,
                  ColumnNumber column_limit) const noexcept;
};
}  // namespace format

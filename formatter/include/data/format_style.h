#pragma once

#include <cstddef>

namespace format {

using ColumnNumber = size_t;
using IndentLevel = size_t;

struct FormatStyle {
  ColumnNumber column_limit;
  IndentLevel indentation_spaces;

  static auto defaults() noexcept -> FormatStyle;
};

}  // namespace format

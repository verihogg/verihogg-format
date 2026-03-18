#pragma once

#include <cstdint>

namespace format {

using ColumnNumber = uint32_t;
using IndentLevel = uint8_t;

struct FormatStyle {
  ColumnNumber column_limit{100};
  IndentLevel indentation_spaces{2};

  static FormatStyle defaults() noexcept;
};

}  // namespace format

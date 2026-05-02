#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace format {

using ColumnNumber = size_t;
using IndentLevel = size_t;

enum class LineTerminator : uint8_t {
  kAuto,  // определить по исходнику
  kLf,    // \n
  kCrLf,  // \r\n
};

[[nodiscard]] inline auto lineTerminatorFromString(std::string_view s)
    -> LineTerminator {
  if (s == "lf") {
    return LineTerminator::kLf;
  }
  if (s == "crlf") {
    return LineTerminator::kCrLf;
  }
  if (s == "auto") {
    return LineTerminator::kAuto;
  }
  throw std::invalid_argument(
      std::string("Unknown --line_terminator: ").append(s));
}

struct RunConfig {
  bool inplace = false;
  std::string stdin_name = "<stdin>";
  std::vector<std::string> input_files;
};

namespace defaults {
inline constexpr ColumnNumber kColumnLimit = 100;
inline constexpr IndentLevel kIndentationSpaces = 2;
inline constexpr IndentLevel kWrapSpaces = 4;
inline constexpr size_t kLineBreakPenalty = 2;
inline constexpr size_t kOverColumnLimitPenalty = 100;
}  // namespace defaults

struct FormatStyle {
  ColumnNumber column_limit = defaults::kColumnLimit;
  IndentLevel indentation_spaces = defaults::kIndentationSpaces;
  IndentLevel wrap_spaces = defaults::kWrapSpaces;
  size_t line_break_penalty = defaults::kLineBreakPenalty;
  size_t over_column_limit_penalty = defaults::kOverColumnLimitPenalty;
  LineTerminator line_terminator = LineTerminator::kAuto;

  [[nodiscard]] static auto defaults() noexcept -> FormatStyle { return {}; }
};

}  // namespace format

#pragma once

#include <slang/driver/Driver.h>

#include <utility>

#include "data/format_style.h"

namespace format {

class FormatArgsBinder {
 public:
  explicit FormatArgsBinder(slang::driver::Driver& driver);
  [[nodiscard]] auto buildStyle() -> std::pair<FormatStyle, RunConfig>;

 private:
  std::optional<uint32_t> column_limit_;
  std::optional<uint32_t> indentation_spaces_;
  std::optional<uint32_t> wrap_spaces_;
  std::optional<uint32_t> line_break_penalty_;
  std::optional<uint32_t> over_column_limit_penalty_;
  std::optional<std::string> line_terminator_;
  std::optional<bool> inplace_;
};

}  // namespace format

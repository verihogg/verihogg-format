#pragma once

#include <slang/parsing/Token.h>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {

class TokenAnnotator {
 public:
  explicit TokenAnnotator(const FormatStyle& style) : style(style) {};

  [[nodiscard]] auto annotate(
      const std::vector<UnwrappedLine<const slang::parsing::Token>>& lines)
      -> std::vector<UnwrappedLine<FormatToken>>;

 private:
  std::reference_wrapper<const FormatStyle> style;
};
}  // namespace format

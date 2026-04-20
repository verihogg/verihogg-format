#pragma once

#include <slang/ast/Compilation.h>
#include <slang/parsing/Token.h>

#include "data/format_style.h"
#include "data/unwrapped_line.h"

namespace format {

class TreeUnwrapper {
 public:
  TreeUnwrapper(std::span<const slang::parsing::Token> tokens,
                const FormatStyle& style)
      : tokens(tokens), style(style) {}

  [[nodiscard]] auto unwrap() const
      -> std::vector<UnwrappedLine<const slang::parsing::Token>>;

 private:
  std::span<const slang::parsing::Token> tokens;
  std::reference_wrapper<const FormatStyle> style;
};
}  // namespace format

#pragma once

#include <slang/ast/Compilation.h>
#include <slang/parsing/Token.h>

#include "data/format_style.h"
#include "data/token_partition_tree.h"

namespace format {

class TreeUnwrapper {
 public:
  TreeUnwrapper(const slang::syntax::SyntaxTree& syntax_tree,
                const FormatStyle& style)
      : syntax_tree(syntax_tree), style(style) {};

  [[nodiscard]] auto unwrap() const
      -> TokenPartitionTree<slang::parsing::Token>;

 private:
  std::reference_wrapper<const slang::syntax::SyntaxTree> syntax_tree;
  std::reference_wrapper<const FormatStyle> style;
};
}  // namespace format

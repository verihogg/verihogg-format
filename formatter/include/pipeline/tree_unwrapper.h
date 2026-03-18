#pragma once

#include <vector>

#include "format_style.h"
#include "format_token.h"
#include "token_partition_tree.h"

namespace format {

class TreeUnwrapper {
 public:
  TreeUnwrapper(const slang::syntax::SyntaxTree& syntax_tree,
                std::vector<FormatToken>& tokens, const FormatStyle& style);

  TokenPartitionTree unwrap();

 private:
  const slang::syntax::SyntaxTree& syntax_tree_;
  std::vector<FormatToken>& tokens_;
  const FormatStyle& style_;
};
}  // namespace format

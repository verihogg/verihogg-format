#pragma once

#include <vector>

#include "format_style.h"
#include "format_token.h"
#include "token_partition_tree.h"

namespace format {

class TokenAnnotator {
 public:
  explicit TokenAnnotator(const FormatStyle& style);

  std::vector<FormatToken> annotate(std::span<slang::parsing::Token>& tokens,
                const TokenPartitionTree& tree);

 private:
  const FormatStyle& style;
};
}  // namespace format

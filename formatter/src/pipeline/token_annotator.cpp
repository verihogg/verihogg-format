#include "pipeline/token_annotator.h"

#include <slang/parsing/Token.h>

#include <stdexcept>

#include "data/format_token.h"
#include "data/token_partition_tree.h"

namespace format {

[[nodiscard]] auto TokenAnnotator::annotate(
    const TokenPartitionTree<slang::parsing::Token>& /*tree*/) const
    -> TokenPartitionTree<FormatToken> {
  throw std::runtime_error("TODO");
}
}  // namespace format

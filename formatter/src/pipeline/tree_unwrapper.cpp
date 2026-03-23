#include "pipeline/tree_unwrapper.h"

#include <slang/parsing/Token.h>

#include <stdexcept>

#include "data/token_partition_tree.h"

namespace format {
[[nodiscard]] auto TreeUnwrapper::unwrap() const
    -> TokenPartitionTree<slang::parsing::Token> {
  throw std::runtime_error("TODO");
}
}  // namespace format

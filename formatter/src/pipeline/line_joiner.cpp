#include "pipeline/line_joiner.h"

#include <stdexcept>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/token_partition_tree.h"

namespace format {
void joinLines(TokenPartitionTree<FormatToken>& /*tree*/,
               const FormatStyle& /*style*/) {
  throw std::runtime_error("TODO");
}
}  // namespace format

#pragma once

#include "format_style.h"
#include "token_partition_tree.h"

namespace format {
void joinLines(TokenPartitionTree& tree, const FormatStyle& style);
}  // namespace format

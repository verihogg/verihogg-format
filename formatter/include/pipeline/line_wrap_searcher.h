#pragma once

#include <vector>

#include "format_style.h"
#include "format_token.h"
#include "token_partition_tree.h"

namespace format {
std::vector<InterTokenDecision> searchLineWraps(
    const UnwrappedLine& line, const FormmatStyle& style,
    ColumnNumber initial_column = 0);
}  // namespace format

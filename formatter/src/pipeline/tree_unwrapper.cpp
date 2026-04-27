#include "pipeline/tree_unwrapper.h"

#include <slang/parsing/Token.h>

#include <stdexcept>
#include <vector>

#include "data/unwrapped_line.h"

namespace format {
[[nodiscard]] auto TreeUnwrapper::unwrap() const
    -> std::vector<UnwrappedLine<slang::parsing::Token>> {
  throw std::runtime_error("TODO");
}
}  // namespace format

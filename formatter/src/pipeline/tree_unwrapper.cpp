#include "pipeline/tree_unwrapper.h"

#include <stdexcept>

namespace format {
[[nodiscard]] auto TreeUnwrapper::unwrap() const
    -> std::vector<UnwrappedLine<const slang::parsing::Token>> {
  throw std::runtime_error("TODO");
}
}  // namespace format

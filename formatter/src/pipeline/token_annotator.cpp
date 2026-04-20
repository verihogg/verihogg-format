#include "pipeline/token_annotator.h"

#include <stdexcept>

namespace format {

[[nodiscard]] auto TokenAnnotator::annotate(
    const std::vector<UnwrappedLine<const slang::parsing::Token>>& /*lines*/)
    -> std::vector<UnwrappedLine<FormatToken>> {
  throw std::runtime_error("TODO");
}
}  // namespace format

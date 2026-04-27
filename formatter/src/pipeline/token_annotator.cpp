#include "pipeline/token_annotator.h"

#include <slang/parsing/Token.h>

#include <stdexcept>
#include <vector>

#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {

[[nodiscard]] auto TokenAnnotator::annotate(
    const std::vector<UnwrappedLine<slang::parsing::Token>>& /*lines*/)
    -> std::vector<UnwrappedLine<FormatToken>> {
  throw std::runtime_error("TODO");
}
}  // namespace format

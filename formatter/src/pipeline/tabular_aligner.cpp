#include "pipeline/tabular_aligner.h"

#include <stdexcept>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {
void align(std::vector<UnwrappedLine<FormatToken>>& /*lines*/,
           const FormatStyle& /*style*/) {
  throw std::runtime_error("TODO");
}
}  // namespace format

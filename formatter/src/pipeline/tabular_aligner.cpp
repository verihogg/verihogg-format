#include "pipeline/tabular_aligner.h"

#include <stdexcept>

namespace format {
void align(std::vector<UnwrappedLine<FormatToken>>& /*lines*/,
           const FormatStyle& /*style*/) {
  throw std::runtime_error("TODO");
}
}  // namespace format

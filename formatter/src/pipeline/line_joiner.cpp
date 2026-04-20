#include "pipeline/line_joiner.h"

#include <stdexcept>

namespace format {
void joinLines(std::vector<UnwrappedLine<FormatToken>>& /*lines*/,
               const FormatStyle& /*style*/) {
  throw std::runtime_error("TODO");
}
}  // namespace format

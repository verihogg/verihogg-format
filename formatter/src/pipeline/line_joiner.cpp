#include "pipeline/line_joiner.h"

#include <stdexcept>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {
void joinLines(std::vector<UnwrappedLine<FormatToken>>& /*lines*/,
               const FormatStyle& /*style*/) {
  throw std::runtime_error("TODO");
}
}  // namespace format

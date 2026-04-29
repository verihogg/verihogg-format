#include <cassert>
#include <cstddef>
#include <gsl/span>
#include <gsl/span_ext>

#include "data/lex_context.h"

auto main(int argc, const char** argv) -> int {
  assert(argc >= 0);
  const auto args = gsl::span{argv, static_cast<size_t>(argc)};
  if (argc < 2) {
    return 1;
  }

  LexContext ctx;
  auto tokens = ctx.lex_file(args[1]);
  return 0;
}

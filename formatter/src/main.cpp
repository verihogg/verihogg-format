#include <cassert>
#include <cstddef>
#include <gsl/span>
#include <gsl/span_ext>
#include <iostream>

#include "data/format_style.h"
#include "data/lex_context.h"
#include "data/unwrapped_line.h"
#include "pipeline/tree_unwrapper.h"

auto main(int argc, const char** argv) -> int {
  assert(argc >= 0);
  const auto args = gsl::span{argv, static_cast<size_t>(argc)};
  if (argc < 2) {
    return 1;
  }

  LexContext ctx;
  auto tokens = ctx.lex_file(gsl::at(args, 1));
  format::FormatStyle style = {
      .column_limit = 2,
      .indentation_spaces = 2,
  };
  format::TreeUnwrapper unwrapper(tokens, style);
  auto unwrapped_lines = unwrapper.unwrap();
  format::printUnwrappedLines(unwrapped_lines);
  return 0;
}

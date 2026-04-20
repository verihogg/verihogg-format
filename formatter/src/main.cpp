
#include "data/lex_context.h"

auto main(int argc, char** argv) -> int {
  if (argc < 2) {
    return 1;
  }

  LexContext ctx;
  auto tokens = ctx.lex_file(argv[1]);
  return 0;
}

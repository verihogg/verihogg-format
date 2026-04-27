#include "data/lex_context.h"

#include <slang/parsing/Lexer.h>

auto LexContext::lex_file(std::string_view path)
    -> std::vector<slang::parsing::Token> {
  std::vector<slang::parsing::Token> tokens;

  auto buffer = source_manager_.readSource(path, /*library=*/nullptr);
  if (!buffer) {
    return tokens;
  }

  slang::parsing::Lexer lexer(*buffer, alloc_, diagnostics_, source_manager_);

  while (true) {
    auto tok = lexer.lex();
    if (tok.kind == slang::parsing::TokenKind::EndOfFile) {
      break;
    }
    tokens.push_back(tok);
  }
  return tokens;
}

#include "data/lex_context.h"

#include <slang/parsing/Lexer.h>
#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>

#include <string_view>
#include <vector>

auto LexContext::lex_file(const std::filesystem::path& path)
    -> std::vector<slang::parsing::Token> {
  std::vector<slang::parsing::Token> tokens;

  auto buffer = source_manager_.readSource(path.string(), /*library=*/nullptr);
  if (!buffer) {
    return tokens;
  }

  slang::parsing::Lexer lexer(*buffer, alloc_, diagnostics_, source_manager_);

  while (true) {
    auto tok = lexer.lex();
    tokens.push_back(tok);
    if (tok.kind == slang::parsing::TokenKind::EndOfFile) {
      break;
    }
  }
  return tokens;
}

auto LexContext::lex_string(std::string_view src)
    -> std::vector<slang::parsing::Token> {
  std::vector<slang::parsing::Token> tokens;

  auto buffer = source_manager_.assignText(src);
  slang::parsing::Lexer lexer(buffer, alloc_, diagnostics_, source_manager_);

  while (true) {
    auto tok = lexer.lex();
    tokens.push_back(tok);
    if (tok.kind == slang::parsing::TokenKind::EndOfFile) {
      break;
    }
  }
  return tokens;
}

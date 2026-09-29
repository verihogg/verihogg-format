#include "data/lex_context.h"

#include <slang/parsing/Lexer.h>
#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>

#include <string_view>
#include <vector>

namespace {

auto withoutSourceTerminator(std::string_view text) -> std::string_view {
  // slang keeps a NUL terminator in SourceBuffer::data for its lexer. The
  // formatter must not treat that byte as part of the source text.
  if (!text.empty() && text.back() == '\0') {
    text.remove_suffix(1);
  }
  return text;
}

}  // namespace

auto LexContext::lex_file(const std::filesystem::path& path)
    -> std::vector<slang::parsing::Token> {
  std::vector<slang::parsing::Token> tokens;
  source_text_ = {};

  auto buffer = source_manager_.readSource(path.string(), /*library=*/nullptr);
  if (!buffer) {
    return tokens;
  }

  source_text_ = withoutSourceTerminator(buffer->data);
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
  source_text_ = withoutSourceTerminator(buffer.data);
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

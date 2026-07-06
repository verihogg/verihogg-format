#include <gtest/gtest.h>
#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>

#include <string>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/lex_context.h"
#include "pipeline/token_annotator.h"
#include "pipeline/tree_unwrapper.h"

namespace {

using FT = format::FormatToken;

class TokenAnnotatorTest : public ::testing::Test {
 protected:
  auto annotateTokens(std::string_view source)
      -> std::vector<format::UnwrappedLine<FT>> {
    auto tokens = ctx_.lex_string(source);
    auto unwrapped = format::TreeUnwrapper(tokens, style).unwrap();
    return format::TokenAnnotator(style).annotate(unwrapped);
  }

 private:
  format::FormatStyle style = format::FormatStyle::defaults();
  LexContext ctx_;
};

TEST_F(TokenAnnotatorTest, DirectiveArgumentMustNotBreak) {
  auto lines = annotateTokens("`include \"file.svh\"");
  ASSERT_FALSE(lines.empty());
  const auto& line = lines.at(0);
  ASSERT_GE(line.tokens.size(), 2);

  const FT* argument = nullptr;
  for (size_t i = 0; i + 1 < line.tokens.size(); ++i) {
    if (line.tokens.at(i).type == format::TokenType::kDirective) {
      argument = &line.tokens.at(i + 1);
      break;
    }
  }
  ASSERT_NE(argument, nullptr) << "No token after directive";

  EXPECT_EQ(argument->before.break_decision,
            format::BreakDecision::kMustNotBreak)
      << "Argument after directive must stay on same line";
}

TEST_F(TokenAnnotatorTest, DirectiveArgumentMustStayWithMatchingParen) {
  auto lines = annotateTokens("`if (EXPR)");
  ASSERT_FALSE(lines.empty());
  const auto& line = lines.at(0);
  ASSERT_GE(line.tokens.size(), 2);

  const FT* argument = nullptr;
  for (size_t i = 0; i + 1 < line.tokens.size(); ++i) {
    if (line.tokens.at(i).type == format::TokenType::kDirective) {
      argument = &line.tokens.at(i + 1);
      break;
    }
  }
  ASSERT_NE(argument, nullptr) << "No token after directive";
  EXPECT_EQ(argument->before.break_decision,
            format::BreakDecision::kMustNotBreak)
      << "Token after directive keyword must not break";
}

}  // namespace

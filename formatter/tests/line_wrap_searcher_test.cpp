#include "pipeline/line_wrap_searcher.h"

#include <gtest/gtest.h>
#include <slang/parsing/TokenKind.h>

#include <cstddef>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/lex_context.h"
#include "data/unwrapped_line.h"

namespace {

class LineWrapSearcherTest : public ::testing::Test {
 protected:
  auto makeLine(std::string_view source, format::IndentLevel indent = 0)
      -> format::UnwrappedLine<format::FormatToken> {
    tokens_ = ctx_.lex_string(source);

    format::UnwrappedLine<format::FormatToken> line;
    line.indentation_spaces = indent;
    for (const auto& token : tokens_) {
      if (token.kind == slang::parsing::TokenKind::EndOfFile) {
        continue;
      }
      line.tokens.push_back(format::FormatToken{.token = token});
    }

    for (size_t i = 0; i < line.tokens.size(); ++i) {
      line.tokens.at(i).before = format::InterTokenInfo{
          .spaces_required = i == 0 ? 99U : 1U,
          .break_penalty = 1,
          .comment_spaces = 0,
          .break_decision = format::BreakDecision::kUndecided,
      };
    }
    return line;
  }

  static auto testStyle() -> format::FormatStyle {
    format::FormatStyle style = format::FormatStyle::defaults();
    style.column_limit = 20;
    style.indentation_spaces = 3;
    style.wrap_spaces = 6;
    style.line_break_penalty = 2;
    style.over_column_limit_penalty = 80;
    return style;
  }

 private:
  LexContext ctx_;
  std::vector<slang::parsing::Token> tokens_;
};

}  // namespace

TEST_F(LineWrapSearcherTest, WrapsBeforeOverflowingToken) {
  const format::FormatStyle style = testStyle();
  const auto line = makeLine("zz yyy xxxx wwwwww", 3);

  const auto decisions = format::searchLineWraps(line, style);

  ASSERT_EQ(decisions.size(), 4U);
  EXPECT_EQ(decisions.at(0).action, format::TokenAction::kAppend);
  EXPECT_EQ(decisions.at(1).action, format::TokenAction::kAppend);
  EXPECT_EQ(decisions.at(2).action, format::TokenAction::kAppend);
  EXPECT_EQ(decisions.at(3).action, format::TokenAction::kWrap);
  EXPECT_EQ(decisions.at(3).spaces_before, 9U);
}

TEST_F(LineWrapSearcherTest, WrapsEarlierToHonorMustNotBreak) {
  const format::FormatStyle style = testStyle();
  auto line = makeLine("aaaaaa bbbbb ccccc", 3);
  line.tokens.at(2).before.break_decision =
      format::BreakDecision::kMustNotBreak;

  const auto decisions = format::searchLineWraps(line, style);

  ASSERT_EQ(decisions.size(), 3U);
  EXPECT_EQ(decisions.at(0).action, format::TokenAction::kAppend);
  EXPECT_EQ(decisions.at(1).action, format::TokenAction::kWrap);
  EXPECT_EQ(decisions.at(2).action, format::TokenAction::kAppend);
  EXPECT_EQ(decisions.at(1).spaces_before, 9U);
}

TEST_F(LineWrapSearcherTest, UsesNestedShapeAfterWrappedOpenGroup) {
  format::FormatStyle style = testStyle();
  style.column_limit = 30;
  style.wrap_spaces = 4;

  auto line = makeLine("function_caller ( 11 )", 2);
  line.tokens.at(1).balancing = format::GroupBalancing::kOpen;
  line.tokens.at(1).before.break_decision = format::BreakDecision::kMustBreak;
  line.tokens.at(2).before.break_decision = format::BreakDecision::kMustBreak;
  line.tokens.at(3).balancing = format::GroupBalancing::kClose;

  const auto decisions = format::searchLineWraps(line, style);

  ASSERT_EQ(decisions.size(), 4U);
  EXPECT_EQ(decisions.at(1).action, format::TokenAction::kWrap);
  EXPECT_EQ(decisions.at(1).spaces_before, 6U);
  EXPECT_EQ(decisions.at(2).action, format::TokenAction::kWrap);
  EXPECT_EQ(decisions.at(2).spaces_before, 10U);
  EXPECT_EQ(decisions.at(3).action, format::TokenAction::kAppend);
}

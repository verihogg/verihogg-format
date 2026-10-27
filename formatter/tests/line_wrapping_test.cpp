#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "data/format_style.h"
#include "data/lex_context.h"
#include "formatter.h"

namespace {

constexpr format::ColumnNumber kColumnLimit = 40;
constexpr std::string_view kLongExpression =
    "module m;\n"
    "  assign result=alpha_long+beta_long+gamma_long;\n"
    "endmodule\n";
constexpr std::string_view kWrappedExpression =
    "module m;\n"
    "  assign result = alpha_long +\n"
    "      beta_long + gamma_long;\n"
    "endmodule\n";
constexpr std::string_view kHandWrappedExpression =
    "module m;\n"
    "  assign result  = alpha_long +  \n"
    "\t beta_long+gamma_long;\n"
    "endmodule\n";
constexpr std::string_view kCommentedExpression =
    "module m;\n"
    "  assign result = alpha_long + // keep this comment\n"
    "      beta_long /* inside */ + gamma_long; // tail\n"
    "  logic   short_name ;\n"
    "endmodule\n";
constexpr std::string_view kNestedExpression =
    "module m;\n"
    "  assign result = foo(alpha_long, bar(beta_long, gamma_long));\n"
    "endmodule\n";
constexpr std::string_view kMultilineCommentExpression =
    "module m;\n"
    "  assign result=alpha_long+/* first\n"
    " second */beta_long+gamma_long;\n"
    "endmodule\n";
constexpr std::string_view kDirectiveExpression =
    "module m;\n"
    "`ifdef FLAG\n"
    "  assign result=alpha_long+beta_long+gamma_long;\n"
    "`else\n"
    "  assign result=0;\n"
    "`endif\n"
    "endmodule\n";

class LineWrappingTest : public ::testing::Test {
 protected:
  auto formatText(std::string_view source, const format::FormatStyle& style)
      -> std::string {
    LexContext ctx;
    const auto tokens = ctx.lex_string(source);
    const auto result = format::format(tokens, style, ctx.source_text());
    EXPECT_TRUE(result.warnings.empty());
    return result.formatted_text;
  }

  static auto testStyle() -> format::FormatStyle {
    auto style = format::FormatStyle::defaults();
    style.column_limit = kColumnLimit;
    return style;
  }
};

TEST_F(LineWrappingTest, DisabledByDefaultPreservesLongSingleLine) {
  const auto style = testStyle();
  ASSERT_FALSE(style.try_wrap_long_lines);
  EXPECT_EQ(formatText(kLongExpression, style), kLongExpression);
}

TEST_F(LineWrappingTest, EnabledOptimizationWrapsLongExpression) {
  auto style = testStyle();
  style.try_wrap_long_lines = true;
  EXPECT_EQ(formatText(kLongExpression, style), kWrappedExpression);
}

TEST_F(LineWrappingTest, DisabledOptimizationPreservesExistingWhitespace) {
  EXPECT_EQ(formatText(kHandWrappedExpression, testStyle()),
            kHandWrappedExpression);
}

TEST_F(LineWrappingTest, EitherModeFormatsFittingExpression) {
  for (const bool enabled : {false, true}) {
    auto style = testStyle();
    style.try_wrap_long_lines = enabled;
    EXPECT_EQ(formatText("module m; assign y=\n a+b; endmodule\n", style),
              "module m;\n"
              "  assign y = a + b;\n"
              "endmodule\n");
  }
}

TEST_F(LineWrappingTest, PreservesCommentsAndFormatsFollowingLine) {
  EXPECT_EQ(formatText(kCommentedExpression, testStyle()),
            "module m;\n"
            "  assign result = alpha_long + // keep this comment\n"
            "      beta_long /* inside */ + gamma_long; // tail\n"
            "  logic short_name;\n"
            "endmodule\n");
}

TEST_F(LineWrappingTest, EitherModeKeepsStructuralLinesAndAlignment) {
  constexpr std::string_view kSource =
      "module m(input logic a,input logic [7:0] data);\n"
      "  logic x;\n"
      "  logic longer;\n"
      "endmodule\n";
  auto style = testStyle();
  const auto without_optimization = formatText(kSource, style);
  style.try_wrap_long_lines = true;
  EXPECT_EQ(formatText(kSource, style), without_optimization);
  EXPECT_NE(without_optimization.find("\n);\n"), std::string::npos);
  EXPECT_NE(without_optimization.find("\n  logic    x;\n  logic    longer;\n"),
            std::string::npos);
}

TEST_F(LineWrappingTest, ConfiguredLineEndingAppliesToPreservedBreaks) {
  auto style = testStyle();
  style.line_terminator = format::LineTerminator::kCrLf;
  std::string expected;
  for (const char ch : kHandWrappedExpression) {
    if (ch == '\n') {
      expected += '\r';
    }
    expected += ch;
  }
  EXPECT_EQ(formatText(kHandWrappedExpression, style), expected);
  EXPECT_EQ(formatText(expected, style), expected);
}

TEST_F(LineWrappingTest, WrappedMultilineCommentStaysAttached) {
  auto style = testStyle();
  style.try_wrap_long_lines = true;
  const auto formatted = formatText(kMultilineCommentExpression, style);
  EXPECT_EQ(formatted,
            "module m;\n"
            "  assign result = alpha_long +/* first\n"
            " second */\n"
            "      beta_long + gamma_long;\n"
            "endmodule\n");
  EXPECT_EQ(formatText(formatted, style), formatted);
}

TEST_F(LineWrappingTest, EitherModePreservesOpaqueRecovery) {
  constexpr std::string_view kSource =
      "module m;\n"
      "  assign result=alpha_long+beta_long+gamma_long;\n"
      "  covergroup cg;\n"
      "  endgroup\n"
      "  logic   good ;\n"
      "endmodule\n";
  for (const bool enabled : {false, true}) {
    auto style = testStyle();
    style.try_wrap_long_lines = enabled;
    LexContext ctx;
    const auto tokens = ctx.lex_string(kSource);
    const auto result = format::format(tokens, style, ctx.source_text());
    ASSERT_EQ(result.warnings.size(), 1U);
    EXPECT_NE(result.formatted_text.find("\n  covergroup cg;\n  endgroup\n"),
              std::string::npos);
    EXPECT_NE(result.formatted_text.find("\n  logic good;\n"),
              std::string::npos);
  }
}

TEST_F(LineWrappingTest, BothModesPreserveTokensAndConverge) {
  for (const auto source :
       {kLongExpression, kWrappedExpression, kHandWrappedExpression,
        kCommentedExpression, kNestedExpression, kMultilineCommentExpression,
        kDirectiveExpression}) {
    SCOPED_TRACE(source);
    for (const bool enabled : {false, true}) {
      SCOPED_TRACE(enabled);
      auto style = testStyle();
      style.try_wrap_long_lines = enabled;
      const auto formatted = formatText(source, style);
      EXPECT_EQ(formatText(formatted, style), formatted);

      LexContext original_ctx;
      LexContext formatted_ctx;
      const auto original_tokens = original_ctx.lex_string(source);
      const auto formatted_tokens = formatted_ctx.lex_string(formatted);
      ASSERT_EQ(original_tokens.size(), formatted_tokens.size());
      for (size_t i = 0; i < original_tokens.size(); ++i) {
        EXPECT_EQ(original_tokens.at(i).kind, formatted_tokens.at(i).kind);
        EXPECT_EQ(original_tokens.at(i).rawText(),
                  formatted_tokens.at(i).rawText());
      }
    }
  }
}

}  // namespace

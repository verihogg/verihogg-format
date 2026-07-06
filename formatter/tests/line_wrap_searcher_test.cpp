#include <gtest/gtest.h>

#include <slang/parsing/Token.h>

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/lex_context.h"
#include "data/unwrapped_line.h"
#include "formatter.h"
#include "pipeline/line_wrap_searcher.h"

namespace {

using namespace format;

// =============================================================================
// Unit tests for searchLineWraps — direct DP verification
// =============================================================================

class SearchLineWrapsTest : public ::testing::Test {
 protected:
  FormatStyle style = FormatStyle::defaults();

  // Build a FormatToken with a given raw text and before info.
  // The token kind is set to Identifier for simplicity.
  auto makeToken(std::string_view text, size_t spaces_required = 1,
                 size_t break_penalty = 50,
                 BreakDecision break_decision = BreakDecision::kUndecided,
                 size_t comment_spaces = 0) const -> FormatToken {
    // We need a real Token with raw text for the line width calculation.
    // Build minimal trivia and token data.
    slang::parsing::Token tok;
    // We can't easily construct a slang Token, so we store the text in a way
    // the printer can use.  For unit tests we only need rawText() to work.
    // We'll store it via a hack — construct a real token from lexing.
    return FormatToken{
        .token = makeSlangToken(text),
        .before =
            {
                .spaces_required = spaces_required,
                .break_penalty = break_penalty,
                .comment_spaces = comment_spaces,
                .break_decision = break_decision,
            },
    };
  }

  slang::parsing::Token makeSlangToken(std::string_view text) const {
    // Use LexContext to create a real token with rawText.
    // This is fragile but gives us a real Token object.
    if (text.empty()) {
      return {};
    }
    // We need at least one real token to copy the structure from.
    // Cache lexed tokens by text to avoid re-lexing.
    auto it = token_cache_.find(std::string(text));
    if (it != token_cache_.end()) {
      return it->second;
    }
    // Lex a trivial snippet and grab a token.
    auto tokens = ctx_.lex_string(std::string(text) + " ");
    for (auto& tok : tokens) {
      if (tok.rawText() == text) {
        token_cache_.emplace(std::string(text), tok);
        return tok;
      }
    }
    return {};
  }

  auto lineFromTokens(std::vector<FormatToken> tokens,
                      IndentLevel indent = 0,
                      PartitionPolicy policy = PartitionPolicy::kAlwaysExpand)
      -> UnwrappedLine<FormatToken> {
    return {.tokens = std::move(tokens),
            .indentation_spaces = indent,
            .partition_policy = policy};
  }

  void setBreakDecision(std::vector<FormatToken>& tokens, size_t idx,
                        BreakDecision d) {
    if (idx > 0 && idx < tokens.size()) {
      tokens[idx].before.break_decision = d;
    }
  }

  mutable LexContext ctx_;

 private:
  mutable std::map<std::string, slang::parsing::Token> token_cache_;
};

// -- Fits within limit -------------------------------------------------------

TEST_F(SearchLineWrapsTest, EmptyLineReturnsEmpty) {
  auto line = lineFromTokens({});
  auto decisions = searchLineWraps(line, style, 0);
  EXPECT_TRUE(decisions.empty());
}

TEST_F(SearchLineWrapsTest, SingleTokenNeverWraps) {
  auto line = lineFromTokens({makeToken("abcdef")}, 0);
  auto decisions = searchLineWraps(line, style, 0);
  ASSERT_EQ(decisions.size(), 1);
  EXPECT_EQ(decisions[0].action, TokenAction::kAppend);
}

TEST_F(SearchLineWrapsTest, ShortLineFitsNoWrap) {
  auto line = lineFromTokens({makeToken("a"), makeToken("="),
                              makeToken("b"), makeToken("+"), makeToken("c")},
                             0);
  auto decisions = searchLineWraps(line, style, 0);
  ASSERT_EQ(decisions.size(), 5);
  for (const auto& d : decisions) {
    EXPECT_EQ(d.action, TokenAction::kAppend);
  }
}

TEST_F(SearchLineWrapsTest, AlreadyFormattedPreserved) {
  auto line = lineFromTokens(
      {makeToken("a", 0), makeToken("b", 5), makeToken("c", 3)}, 0,
      PartitionPolicy::kAlreadyFormatted);
  auto decisions = searchLineWraps(line, style, 0);
  ASSERT_EQ(decisions.size(), 3);
  EXPECT_EQ(decisions[0].spaces_before, 0);
  EXPECT_EQ(decisions[1].spaces_before, 5);
  EXPECT_EQ(decisions[2].spaces_before, 3);
  for (const auto& d : decisions) {
    EXPECT_EQ(d.action, TokenAction::kAppend);
  }
}

// -- Exceeds column limit ----------------------------------------------------

TEST_F(SearchLineWrapsTest, WrapsLongExpression) {
  // Build a line with many short tokens that exceed column_limit.
  // Column limit 20, indent 2, so content width = 18.
  // Tokens:  aaa + bbb + ccc + ddd + eee
  // Widths:  3+1+3+1+3+1+3+1+3 = 19 > 18 → need wrap.
  std::vector<FormatToken> tokens;
  tokens.push_back(makeToken("aaa"));
  for (auto name : {"+", "bbb", "+", "ccc", "+", "ddd", "+", "eee"}) {
    tokens.push_back(makeToken(name));
  }
  auto line = lineFromTokens(std::move(tokens), 2);
  style.column_limit = 20;

  auto decisions = searchLineWraps(line, style, 0);
  ASSERT_EQ(decisions.size(), 9);

  // First token always append.
  EXPECT_EQ(decisions[0].action, TokenAction::kAppend);
  // There should be at least one wrap among the rest.
  int wrap_count = 0;
  for (size_t i = 1; i < decisions.size(); ++i) {
    if (decisions[i].action == TokenAction::kWrap)
      ++wrap_count;
  }
  EXPECT_GE(wrap_count, 1);
}

TEST_F(SearchLineWrapsTest, WrapUsesContinuationIndent) {
  // Column limit very tight to force wrapping after every token.
  style.column_limit = 10;
  style.wrap_spaces = 4;

  // Indent = 2, tokens: "aaaa" (4) + "bbbb" (4) + "cccc" (4)
  // After indent 2, adding "aaaa" (4) = 6. Then +1+4 = 11 > 10 → wrap.
  // Then continuation indent = 2+4 = 6, plus "bbbb" (4) = 10. Fits!
  // Then +1+4 = 15 > 10 → wrap again.
  auto line = lineFromTokens(
      {makeToken("aaaa"), makeToken("bbbb"), makeToken("cccc")}, 2);
  auto decisions = searchLineWraps(line, style, 0);

  ASSERT_EQ(decisions.size(), 3);
  EXPECT_EQ(decisions[0].action, TokenAction::kAppend);
  EXPECT_EQ(decisions[1].action, TokenAction::kWrap);
  EXPECT_EQ(decisions[2].action, TokenAction::kWrap);
}

// -- kMustBreak / kMustNotBreak -----------------------------------------------

TEST_F(SearchLineWrapsTest, MustBreakForcesLineBreak) {
  // Force a break before token "ccc".
  auto line = lineFromTokens({makeToken("aaa"), makeToken("bbb"),
                              makeToken("ccc"), makeToken("ddd")},
                             0);
  setBreakDecision(line.tokens, 2, BreakDecision::kMustBreak);

  style.column_limit = 200;  // generous limit, but forced break still happens
  auto decisions = searchLineWraps(line, style, 0);

  ASSERT_EQ(decisions.size(), 4);
  EXPECT_EQ(decisions[2].action, TokenAction::kWrap)
      << "kMustBreak at token 2 must force a wrap before it";
}

TEST_F(SearchLineWrapsTest, MustNotBreakPreventsLineBreak) {
  // Tight column limit to normally force a break, but must-not-break should
  // prevent breaking at that gap.
  style.column_limit = 10;

  // Tokens: aaaa (4), bbbb (4), cccc (4)
  // Total: 4+1+4+1+4 = 14 > 10 → would normally wrap.
  auto line = lineFromTokens(
      {makeToken("aaaa"), makeToken("bbbb"), makeToken("cccc")}, 0);
  setBreakDecision(line.tokens, 2, BreakDecision::kMustNotBreak);

  auto decisions = searchLineWraps(line, style, 0);

  ASSERT_EQ(decisions.size(), 3);
  // Must not break before token 2 (cccc), so the only valid break is before
  // token 1 (bbbb):
  //   line 0: aaaa
  //   line 1: bbbb cccc  (cannot break between bbbb and cccc)
  EXPECT_EQ(decisions[1].action, TokenAction::kWrap)
      << "Must wrap before bbbb since we can't wrap after it";
  EXPECT_EQ(decisions[2].action, TokenAction::kAppend)
      << "MustNotBreak before cccc must prevent wrap";
}

// -- Penalty-driven break selection -----------------------------------------

TEST_F(SearchLineWrapsTest, PrefersLowPenaltyBreakPoint) {
  // Build a line where we have break points with different penalties.
  // Break before "ccc" has penalty 0 (comma-like), break before "bbb" has
  // penalty 200 (hard). When wrapping must happen, the DP should prefer the
  // low-penalty break.
  style.column_limit = 15;

  // aaaa(4) +1+ bbbb(4) +1+ cccc(4) = 14 → fits in 15, so no wrap.
  // Make it longer:
  // aaaa(4) +1+ bbbb(4) +1+ cccc(4) +1+ dddd(4) = 19 > 15.
  auto line = lineFromTokens(
      {makeToken("aaaa"), makeToken("bbbbb", 1, 200),
       makeToken("ccccc", 1, 0), makeToken("dddd")},
      0);
  style.column_limit = 15;

  auto decisions = searchLineWraps(line, style, 0);

  ASSERT_EQ(decisions.size(), 4);
  // Low penalty at token 2 (before "ccccc") → wrap should happen there.
  //   line 0: aaaa bbbbb
  //   line 1: ccccc dddd
  EXPECT_EQ(decisions[2].action, TokenAction::kWrap)
      << "Should break before token 2 (lowest penalty)";
  EXPECT_EQ(decisions[1].action, TokenAction::kAppend);
}

// -- optimizeLineWraps integration -------------------------------------------

class OptimizeLineWrapsTest : public ::testing::Test {
 protected:
  auto formatText(std::string_view source,
                  FormatStyle fmt_style = FormatStyle::defaults())
      -> std::string {
    tokens_ = ctx_.lex_string(source);
    return format::format(tokens_, fmt_style).formatted_text;
  }

  LexContext ctx_;
  std::vector<slang::parsing::Token> tokens_;
};

TEST_F(OptimizeLineWrapsTest, ShortInputNotWrapped) {
  auto result = formatText("module m (); endmodule");
  EXPECT_EQ(result, "module m (\n"
                    ");\n"
                    "endmodule\n");
}

TEST_F(OptimizeLineWrapsTest, LongStatementGetsWrapped) {
  FormatStyle style = FormatStyle::defaults();
  style.column_limit = 30;

  auto result = formatText(
      "module m ();\n"
      "always_comb begin\n"
      "if (very_long_condition || another_long_condition) begin\n"
      "out = in_a + in_b + in_c + in_d;\n"
      "end\n"
      "end\n"
      "endmodule",
      style);

  // The long line should have been wrapped into multiple lines.
  // Count lines in the body (between begin and endmodule).
  std::istringstream stream(result);
  std::string line;
  int line_count = 0;
  int over_limit_count = 0;
  while (std::getline(stream, line)) {
    ++line_count;
    if (line.size() > style.column_limit)
      ++over_limit_count;
  }
  // At least 3 body lines (not just module/begin/end).
  EXPECT_GE(line_count, 6) << "Expected wrapping to create more lines:\n"
                           << result;
  // Allow tokens that are individually wider than column_limit.
  EXPECT_LE(over_limit_count, 3)
      << "At most a few lines should exceed the limit:\n"
      << result;
}

TEST_F(OptimizeLineWrapsTest, LongPortListWraps) {
  FormatStyle style = FormatStyle::defaults();
  style.column_limit = 40;

  auto result = formatText(
      "module m ();\n"
      "assign x = a + b + c + d + e + f + g + h + i + j;\n"
      "endmodule",
      style);

  std::istringstream stream(result);
  std::string line;
  while (std::getline(stream, line)) {
    EXPECT_LE(line.size(), style.column_limit)
        << "Line exceeds column limit:\n"
        << line;
  }
}

TEST_F(OptimizeLineWrapsTest, SingleTokenExceedingLimit) {
  // A single very long token (module name) should remain on one line.
  FormatStyle style = FormatStyle::defaults();
  style.column_limit = 20;

  auto result = formatText(
      "module very_long_module_name_here (); endmodule", style);

  // The module line itself is one token and can't be split.
  EXPECT_NE(result.find("very_long_module_name_here"), std::string::npos);
}

TEST_F(OptimizeLineWrapsTest, ContinuationIndentApplied) {
  FormatStyle style = FormatStyle::defaults();
  style.column_limit = 25;
  style.wrap_spaces = 4;

  auto result = formatText(
      "module m ();\n"
      "assign out = a + b + c + d + e;\n"
      "endmodule",
      style);

  // Check that continuation lines use increased indent.
  auto base_indent = result.find("assign");
  ASSERT_NE(base_indent, std::string::npos);
  auto base_col = base_indent - result.rfind('\n', base_indent) - 1;

  // Find a continuation line (something indented more than base).
  std::istringstream stream(result);
  std::string line;
  bool found_continuation = false;
  while (std::getline(stream, line)) {
    if (line.size() > base_col + style.wrap_spaces &&
        line.find_first_not_of(' ') > base_col) {
      found_continuation = true;
      auto cont_indent = line.find_first_not_of(' ');
      EXPECT_EQ(cont_indent, base_col + style.wrap_spaces)
          << "Continuation indent should be base + wrap_spaces\n"
          << line;
    }
  }
  EXPECT_TRUE(found_continuation)
      << "Expected at least one continuation line in output:\n"
      << result;
}

TEST_F(OptimizeLineWrapsTest, IdempotentSecondPass) {
  FormatStyle style = FormatStyle::defaults();
  style.column_limit = 30;

  auto result1 = formatText(
      "module m ();\n"
      "assign very_long_sum = a + b + c + d + e + f + g;\n"
      "endmodule",
      style);

  auto result2 = formatText(result1, style);

  EXPECT_EQ(result1, result2)
      << "Formatting must be stable under a second pass";
}

TEST_F(OptimizeLineWrapsTest, TightColumnLimitWrapsMultipleLines) {
  FormatStyle style = FormatStyle::defaults();
  style.column_limit = 15;

  auto result = formatText(
      "module m ();\n"
      "logic a, b, c, d;\n"
      "endmodule",
      style);

  // With column limit 15, "logic a, b, c, d;" should be broken.
  std::istringstream stream(result);
  std::string line;
  while (std::getline(stream, line)) {
    EXPECT_LE(line.size(), style.column_limit)
        << "Line exceeds column limit:\n"
        << line;
  }
}

TEST_F(OptimizeLineWrapsTest, AlignedLinesNotWrapped) {
  // Lines that underwent tabular alignment must not be re-wrapped.
  // The aligner may adjust spacing, but the port list should remain as three
  // separate lines (not broken further).
  FormatStyle style = FormatStyle::defaults();
  style.column_limit = 30;

  auto result = formatText(
      "module m (\n"
      "input  logic       clk,\n"
      "input  logic       rst_n,\n"
      "output logic [3:0] count\n"
      "); endmodule",
      style);

  // Port list should have exactly 3 port lines (clk, rst_n, count).
  // No extra wrapping should occur within aligned lines.
  auto count_occurrences = [&](const std::string& s, const std::string& sub) {
    size_t n = 0, pos = 0;
    while ((pos = result.find(sub, pos)) != std::string::npos) {
      ++n;
      pos += sub.size();
    }
    return n;
  };
  EXPECT_EQ(count_occurrences(result, "input "), 2)
      << "Expected exactly 2 'input' port lines\n"
      << result;
  EXPECT_EQ(count_occurrences(result, "output"), 1)
      << "Expected exactly 1 'output' port line\n"
      << result;
}

}  // namespace

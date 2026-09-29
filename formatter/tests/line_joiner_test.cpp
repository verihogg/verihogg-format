#include "pipeline/line_joiner.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "data/format_style.h"
#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/policy_assigner.h"
#include "pipeline/token_annotator.h"
#include "pipeline/tree_unwrapper.h"

namespace {

class LineJoinerTest : public ::testing::Test {
 protected:
  using Line = format::UnwrappedLine<format::FormatToken>;

  auto annotated(std::string_view source, const format::FormatStyle& style)
      -> std::vector<Line> {
    tokens_ = ctx_.lex_string(source);
    auto lines = format::TokenAnnotator(style).annotate(
        format::TreeUnwrapper(tokens_, style).unwrap());
    format::PolicyAssigner(style).assign(lines);
    return lines;
  }

  auto formatText(std::string_view source,
                  format::FormatStyle style = format::FormatStyle::defaults())
      -> std::string {
    tokens_ = ctx_.lex_string(source);
    return format::format(tokens_, style).formatted_text;
  }

 private:
  LexContext ctx_;
  std::vector<slang::parsing::Token> tokens_;
};

class SingleBodyHeaderTest : public LineJoinerTest,
                             public ::testing::WithParamInterface<const char*> {
};

TEST_P(SingleBodyHeaderTest, JoinsSupportedHeaderWithItsBody) {
  const auto style = format::FormatStyle::defaults();
  const std::string source = std::string(GetParam()) + " x = 1;";
  SCOPED_TRACE(source);
  auto lines = annotated(source, style);
  ASSERT_EQ(lines.size(), 3);
  const size_t header_size = lines.front().tokens.size();
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 2);
  ASSERT_GT(lines.front().tokens.size(), header_size);
  const auto& body_start = lines.front().tokens.at(header_size);
  EXPECT_EQ(body_start.token.rawText(), "x");
  EXPECT_EQ(body_start.before.spaces_required, 1);
  EXPECT_EQ(body_start.before.break_decision,
            format::BreakDecision::kMustNotBreak);
}

INSTANTIATE_TEST_SUITE_P(ProceduralAndLoopHeaders, SingleBodyHeaderTest,
                         ::testing::Values("always @(posedge clk)",
                                           "always_latch", "initial", "final",
                                           "forever", "foreach (a[i])"));

// Exercise joining across independent syntax, trivia and style choices.
auto forSupportedLayouts(
    const std::function<void(std::string_view, const format::FormatStyle&)>&
        check) -> void {
  for (size_t indent : {0, 2, 4}) {
    for (size_t width : {20, 40, 100}) {
      for (const std::string header :
           {"if (a)", "while (a)", "repeat (3)", "forever",
            "for (int i = 0; i < 2; i++)", "foreach (a[i])", "label: if (a)"}) {
        for (const std::string body :
             {"x = 1;", "x = f(a[b], g(c));", ";", "if (b) f(c); else g(d);",
              "begin f(a); end", "fork f(a); g(b); join",
              "x = /*comment*/ f(a);", "x = f('{a: 1, b: '{2, 3}});"}) {
          // Foreach requires a statement rather than a null statement.
          if (header == "foreach (a[i])" && body == ";") {
            continue;
          }
          for (const std::string gap :
               {" ", "\n", "\n\n", " //header\n", " /*header*/ "}) {
            std::string source = "module m; initial begin ";
            source.append(header).append(gap).append(body).append(
                "\nend endmodule");
            auto style = format::FormatStyle::defaults();
            style.indentation_spaces = indent;
            style.column_limit = width;
            SCOPED_TRACE("indent=" + std::to_string(indent) +
                         " width=" + std::to_string(width) + "\n" + source);
            check(source, style);
          }
        }
      }
    }
  }
}

}  // namespace

TEST_F(LineJoinerTest, JoinsSimpleIfBody) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "if (a) b = 1;\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      if (a) b = 1;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, JoinsIfElseSimpleBodies) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "if (a) b = 1;\n"
                       "else b = 0;\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      if (a) b = 1; else b = 0;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, JoinsIfElseIfElseChain) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "if (a) b = 1;\n"
                       "else if (c) b = 2;\n"
                       "else b = 0;\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      if (a) b = 1; else if (c) b = 2; else b = 0;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, JoinsForLoopSimpleBody) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "for (int i = 0; i < 4; i = i + 1) b = 0;\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      for (int i = 0; i < 4; i = i + 1) b = 0;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, DoesNotJoinBeginEndBlock) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb\n"
                       "if (a) begin\n"
                       "b = 1;\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    if (a)\n"
            "      begin\n"
            "        b = 1;\n"
            "      end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, DoesNotJoinWhenTooLong) {
  constexpr size_t kTinyColumnLimit = 20;
  format::FormatStyle style = format::FormatStyle::defaults();
  style.column_limit = kTinyColumnLimit;

  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "if (a) bbb = 1;\n"
                       "end\n"
                       "endmodule",
                       style),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      if (a)\n"
            "        bbb = 1;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, JoinsAtExactColumnLimit) {
  constexpr size_t kTinyColumnLimit = 20;
  format::FormatStyle style = format::FormatStyle::defaults();
  style.column_limit = kTinyColumnLimit;

  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "if (a) bb = 1;\n"
                       "end\n"
                       "endmodule",
                       style),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      if (a) bb = 1;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, DoesNotJoinWhenBodyHasLeadingComment) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "if (a)\n"
                       "// comment\n"
                       "b = 1;\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      if (a)\n"
            "        // comment\n"
            "        b = 1;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, DoesNotJoinWhenElseHasLeadingComment) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "if (a) b = 1;\n"
                       "// comment\n"
                       "else b = 0;\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      if (a) b = 1;\n"
            "      // comment\n"
            "      else b = 0;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, DoesNotJoinElseBeginBody) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "if (a) b = 1;\n"
                       "else begin\n"
                       "b = 0;\n"
                       "end\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      if (a) b = 1;\n"
            "      else\n"
            "        begin\n"
            "          b = 0;\n"
            "        end\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, JoinsAlwaysCombBody) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb\n"
                       "q = d;\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb q = d;\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, DoesNotJoinAlwaysCombBodyWhenTooLong) {
  constexpr size_t kTinyColumnLimit = 20;
  format::FormatStyle style = format::FormatStyle::defaults();
  style.column_limit = kTinyColumnLimit;

  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb\n"
                       "bbb = 1;\n"
                       "endmodule",
                       style),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    bbb = 1;\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, JoinsAlwaysFFBody) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_ff @(posedge clk)\n"
                       "q <= d;\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_ff @(posedge clk) q <= d;\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, JoinsWhileBody) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "while (cond) b = 0;\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      while (cond) b = 0;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, JoinsRepeatBody) {
  EXPECT_EQ(formatText("module m ();\n"
                       "always_comb begin\n"
                       "repeat (4) b = 0;\n"
                       "end\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_comb\n"
            "    begin\n"
            "      repeat (4) b = 0;\n"
            "    end\n"
            "endmodule\n");
}

TEST_F(LineJoinerTest, KeepsModuleBodySeparate) {
  EXPECT_EQ(formatText("module m (); assign x = 1; assign y = 2; endmodule"),
            "module m (\n);\n  assign x  = 1;\n  assign y  = 2;\nendmodule\n");
}

TEST_F(LineJoinerTest, KeepsFunctionBodySeparate) {
  EXPECT_EQ(formatText("function int f(); f = 1; endfunction"),
            "function int f ();\n  f = 1;\nendfunction\n");
}

TEST_F(LineJoinerTest, KeepsLabeledBlockExpanded) {
  EXPECT_EQ(formatText("initial begin : work x = 1; end"),
            "initial\n  begin: work\n    x = 1;\n  end\n");
}

TEST_F(LineJoinerTest, KeepsLabeledStatementBlockExpanded) {
  EXPECT_EQ(formatText("work: begin x = 1; end"),
            "work : begin\n  x = 1;\nend\n");
}

TEST_F(LineJoinerTest, JoinsLabeledIfBody) {
  EXPECT_EQ(formatText("work: if (a) x = 1;"), "work : if (a) x = 1;\n");
}

TEST_F(LineJoinerTest, JoinsNullBody) {
  EXPECT_EQ(formatText("while (busy) ;"), "while (busy) ;\n");
}

TEST_F(LineJoinerTest, KeepsNestedIfAndDanglingElseAtTheirOwnLevels) {
  EXPECT_EQ(formatText("if (a) if (b) x = 1; else x = 2;"),
            "if (a)\n  if (b) x = 1; else x = 2;\n");
}

TEST_F(LineJoinerTest, KeepsBlankLineBeforeBody) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a)\n\nx = 1;", style);
  ASSERT_EQ(lines.size(), 3);
  format::LineJoiner(style).join(lines);
  EXPECT_EQ(lines.size(), 3);
}

TEST_F(LineJoinerTest, BlankLineBeforeBodySurvivesRepeatedFormatting) {
  const std::string source = "if (a)\n\nx = 1;";
  const auto once = formatText(source);
  EXPECT_EQ(once, "if (a)\n\n  x = 1;\n");
  EXPECT_EQ(formatText(once), once) << "Input: " << source;
}

TEST_F(LineJoinerTest, KeepsHeaderTrailingCommentAndBodyIndent) {
  EXPECT_EQ(formatText("if (a) // explanation\nx = 1;"),
            "if (a) // explanation\n  x = 1;\n");
}

TEST_F(LineJoinerTest, DoesNotFlattenAnInternalLineComment) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) x = f( // argument\nb);", style);
  ASSERT_EQ(lines.size(), 3);
  format::LineJoiner(style).join(lines);
  EXPECT_EQ(lines.size(), 3);
}

TEST_F(LineJoinerTest, DoesNotJoinBodyContainingBlockComment) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) x = /* explanation */ 1;", style);
  ASSERT_EQ(lines.size(), 3);
  format::LineJoiner(style).join(lines);
  EXPECT_EQ(lines.size(), 3);
}

TEST_F(LineJoinerTest, DoesNotCrossCompilerDirective) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a)\n`ifdef FEATURE\nx = 1;\n`endif\n", style);
  ASSERT_EQ(lines.size(), 5);
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 5);
  EXPECT_EQ(lines.at(1).tokens.front().token.rawText(), "`ifdef");
  EXPECT_EQ(lines.at(lines.size() - 2).tokens.front().token.rawText(),
            "`endif");
}

TEST_F(LineJoinerTest, KeepsMacroBodySeparate) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) x = `VALUE;", style);
  ASSERT_EQ(lines.size(), 3);
  format::LineJoiner(style).join(lines);
  EXPECT_EQ(lines.size(), 3);
}

TEST_F(LineJoinerTest, PreservesBracketLinksAfterMerging) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) q = f(b);", style);
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 2);
  auto& tokens = lines.front().tokens;
  ASSERT_EQ(tokens.size(), 11);
  EXPECT_EQ(tokens.at(1).matching_bracket, &tokens.at(3));
  EXPECT_EQ(tokens.at(3).matching_bracket, &tokens.at(1));
  EXPECT_EQ(tokens.at(7).matching_bracket, &tokens.at(9));
  EXPECT_EQ(tokens.at(9).matching_bracket, &tokens.at(7));
}

TEST_F(LineJoinerTest, PreservesInternalMandatoryBreak) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) q = f(b);", style);
  ASSERT_EQ(lines.size(), 3);
  lines.at(lines.size() - 2).tokens.at(2).before.break_decision =
      format::BreakDecision::kMustBreak;
  format::LineJoiner(style).join(lines);
  EXPECT_EQ(lines.size(), 3);
}

TEST_F(LineJoinerTest, PreservesAlreadyFormattedBody) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) q = 1;", style);
  ASSERT_EQ(lines.size(), 3);
  lines.at(lines.size() - 2).partition_policy =
      format::PartitionPolicy::kAlreadyFormatted;
  format::LineJoiner(style).join(lines);
  EXPECT_EQ(lines.size(), 3);
}

TEST_F(LineJoinerTest, DoesNotAbsorbASecondIndentedStatement) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) x = 1; y = 2;", style);
  ASSERT_EQ(lines.size(), 4);
  // Even if an upstream stage gives siblings the same indentation, only the
  // single body belongs to the header.
  lines.at(lines.size() - 2).indentation_spaces =
      lines.at(1).indentation_spaces;
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 3);
  EXPECT_EQ(lines.at(lines.size() - 2).tokens.front().token.rawText(), "y");
}

TEST_F(LineJoinerTest, KeepsNonImmediateDescendantSeparate) {
  auto style = format::FormatStyle::defaults();
  style.indentation_spaces = 0;
  auto lines = annotated("if (a) x = 1;", style);
  ASSERT_EQ(lines.size(), 3);
  // Exercise the joiner's metadata guard directly: a deeper descendant is
  // not the header's single body, even when printed indentation is identical.
  ++lines.at(1).nesting_level;
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 3);
  EXPECT_EQ(lines.at(1).tokens.front().token.rawText(), "x");
}

TEST_F(LineJoinerTest, JoinsOnlyTheBranchesThatFit) {
  auto style = format::FormatStyle::defaults();
  const std::string prefix = "if (a) x = 1; else if (b) x = 2;";
  style.column_limit = prefix.size();
  EXPECT_EQ(formatText(prefix + " else x = 3;", style),
            prefix + "\nelse x = 3;\n");
}

TEST_F(LineJoinerTest, DoesNotOverflowWidthBudget) {
  auto style = format::FormatStyle::defaults();
  style.column_limit = std::numeric_limits<size_t>::max();
  auto lines = annotated("if (a) x = 1;", style);
  ASSERT_EQ(lines.size(), 3);
  constexpr size_t kRemainingColumns = 10;
  lines.front().indentation_spaces = style.column_limit - kRemainingColumns;
  lines.at(lines.size() - 2).indentation_spaces =
      lines.front().indentation_spaces + style.indentation_spaces;
  format::LineJoiner(style).join(lines);
  EXPECT_EQ(lines.size(), 3);
}

TEST_F(LineJoinerTest, EmptyInputIsUnchanged) {
  const auto style = format::FormatStyle::defaults();
  std::vector<Line> lines;
  format::LineJoiner(style).join(lines);
  EXPECT_TRUE(lines.empty());
}

TEST_F(LineJoinerTest, EmptyPartitionIsABarrier) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) x = 1;", style);
  lines.insert(lines.begin() + 1, Line{});
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 4);
  EXPECT_TRUE(lines.at(1).tokens.empty());
}

TEST_F(LineJoinerTest, EmptyPartitionDoesNotHideTrailingComment) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) x = 1; // body", style);
  ASSERT_EQ(lines.size(), 3);
  // Empty partitions own no trivia; EOF still owns the body's comment.
  lines.insert(lines.begin() + 2, Line{});
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 4);
  EXPECT_EQ(lines.at(1).tokens.front().token.rawText(), "x");
  EXPECT_TRUE(lines.at(2).tokens.empty());
}

TEST_F(LineJoinerTest, JoinsElseChainWithZeroIndentation) {
  auto style = format::FormatStyle::defaults();
  style.indentation_spaces = 0;
  EXPECT_EQ(formatText("if (a) x = 1; else x = 2;", style),
            "if (a) x = 1; else x = 2;\n");
}

TEST_F(LineJoinerTest, ZeroIndentationPreservesNestedElseOwnership) {
  auto style = format::FormatStyle::defaults();
  style.indentation_spaces = 0;
  const std::string source = "if (a) if (b) x = 1; else x = 2; else x = 3;";
  const auto once = formatText(source, style);
  EXPECT_EQ(once, "if (a)\nif (b) x = 1; else x = 2;\nelse x = 3;\n");
  EXPECT_EQ(formatText(once, style), once) << "Input: " << source;
}

TEST_F(LineJoinerTest, JoinsBodyContainingNestedTypeCast) {
  EXPECT_EQ(formatText("if (a) f(int'(b));"), "if (a) f (int'(b));\n");
}

TEST_F(LineJoinerTest, JoinsVoidCastCallBody) {
  EXPECT_EQ(formatText("if (a) void'(f());"), "if (a) void'(f ());\n");
}

TEST_F(LineJoinerTest, JoinsReturnWithTypeCast) {
  EXPECT_EQ(formatText("if (a) return int'(x);"), "if (a) return int'(x);\n");
}

TEST_F(LineJoinerTest, JoinsCaseLabelContainingAssignmentExpression) {
  EXPECT_EQ(formatText("case (s) (a = b): x = 1; endcase"),
            "case (s)\n  (a = b) : x = 1;\nendcase\n");
}

TEST_F(LineJoinerTest, JoinsReturnWithQualifiedTypeCast) {
  EXPECT_EQ(formatText("if (a) return int signed'(x);"),
            "if (a) return int signed'(x);\n");
}

TEST_F(LineJoinerTest, JoinsReturnWithPackedTypeCast) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) return logic [3:0]'(x);", style);
  ASSERT_EQ(lines.size(), 3);
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 2);
  EXPECT_EQ(lines.front().tokens.back().token.kind,
            slang::parsing::TokenKind::Semicolon);
}

TEST_F(LineJoinerTest, JoinsCaseLabelContainingLessThanOrEqual) {
  EXPECT_EQ(formatText("case (s) a <= b: x = 1; endcase"),
            "case (s)\n  a <= b : x = 1;\nendcase\n");
}

TEST_F(LineJoinerTest, LeadingBlockCommentDoesNotOverflowJoinedLine) {
  auto style = format::FormatStyle::defaults();
  constexpr size_t kColumnLimit = 30;
  style.column_limit = kColumnLimit;
  EXPECT_EQ(formatText("/* a long explanation */ if (a) x = 1;", style),
            "/* a long explanation */\nif (a) x = 1;\n");
}

TEST_F(LineJoinerTest, RepeatedJoiningDoesNotFlattenNestedControlFlow) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) if (b) x = 1;", style);
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 3);
  format::LineJoiner(style).join(lines);
  EXPECT_EQ(lines.size(), 3);
}

TEST_F(LineJoinerTest, JoinsElseChainAtExactColumnLimit) {
  auto style = format::FormatStyle::defaults();
  const std::string expected = "if (a) x = 1; else x = 2;";
  style.column_limit = expected.size();
  EXPECT_EQ(formatText(expected, style), expected + "\n");
}

TEST_F(LineJoinerTest, SeparatesElseChainOneColumnOverLimit) {
  auto style = format::FormatStyle::defaults();
  const std::string source = "if (a) x = 1; else x = 2;";
  style.column_limit = source.size() - 1;
  EXPECT_EQ(formatText(source, style), "if (a) x = 1;\nelse x = 2;\n");
}

TEST_F(LineJoinerTest, PreservesBracketLinksAcrossElseChain) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) f(b); else if (c) g(d); else h(e);", style);
  std::vector<std::string> original;
  for (const auto& line : lines) {
    for (const auto& token : line.tokens) {
      if (token.token.kind != slang::parsing::TokenKind::EndOfFile) {
        original.emplace_back(token.token.rawText());
      }
    }
  }
  format::LineJoiner(style).join(lines);
  ASSERT_EQ(lines.size(), 2);
  const auto& tokens = lines.front().tokens;
  ASSERT_EQ(tokens.size(), original.size());
  for (size_t i = 0; i < tokens.size(); ++i) {
    SCOPED_TRACE("token " + std::to_string(i) + ": " + original.at(i));
    const auto& token = tokens.at(i);
    EXPECT_EQ(token.token.rawText(), original.at(i));
    if (token.matching_bracket != nullptr) {
      const auto match = std::ranges::find_if(tokens, [&](const auto& other) {
        return &other == token.matching_bracket;
      });
      ASSERT_NE(match, tokens.end());
      EXPECT_EQ(match->matching_bracket, &token);
    }
  }
}

TEST_F(LineJoinerTest, KeepsForkBlockExpanded) {
  EXPECT_EQ(formatText("initial fork x = 1; y = 2; join"),
            "initial\n  fork\n    x  = 1;\n    y  = 2;\n  join\n");
}

TEST_F(LineJoinerTest, DoesNotMoveBodyWithTrailingComment) {
  EXPECT_EQ(formatText("initial begin if (a) x = 1; // body\nend"),
            "initial\n  begin\n    if (a)\n      x = 1; // body\n  end\n");
}

TEST_F(LineJoinerTest, TrailingCommentAtEofIsABarrierWithoutFormatterWrapper) {
  const auto style = format::FormatStyle::defaults();
  auto lines = annotated("if (a) x = 1; // explanation", style);
  format::LineJoiner(style).join(lines);
  ASSERT_GE(lines.size(), 2);
  EXPECT_EQ(lines.at(1).tokens.front().token.rawText(), "x");
}

TEST_F(LineJoinerTest, TrailingBlockCommentAtEofRemainsABarrier) {
  const std::string source = "if (a) x = 1; /* body */";
  const auto once = formatText(source);
  EXPECT_EQ(once, "if (a)\n  x = 1; /* body */\n");
  EXPECT_EQ(formatText(once), once) << "Input: " << source;
}

TEST_F(LineJoinerTest, TrailingBlockCommentBeforeElseRemainsABarrier) {
  const std::string source = "if (a) x = 1; /* body */else x = 2;";
  const auto once = formatText(source);
  EXPECT_EQ(once, "if (a)\n  x = 1; /* body */\nelse x = 2;\n");
  EXPECT_EQ(formatText(once), once) << "Input: " << source;
}

TEST_F(LineJoinerTest, KeepsUnsupportedBodySeparate) {
  EXPECT_EQ(formatText("if (a) wait (b) f();"), "if (a)\n  wait (b) f ();\n");
}

TEST_F(LineJoinerTest, SupportedJoiningPreservesTokensAndTrivia) {
  const auto snapshot = [](const auto& lines) {
    std::vector<std::tuple<bool, int, std::string>> result;
    for (const auto& line : lines) {
      for (const auto& ft : line.tokens) {
        for (const auto& trivia : ft.token.trivia()) {
          result.emplace_back(false, static_cast<int>(trivia.kind),
                              trivia.getRawText());
        }
        result.emplace_back(true, static_cast<int>(ft.token.kind),
                            ft.token.rawText());
      }
    }
    return result;
  };
  forSupportedLayouts([&](std::string_view source, const auto& style) {
    auto lines = annotated(source, style);
    const auto before = snapshot(lines);
    format::LineJoiner(style).join(lines);
    EXPECT_EQ(snapshot(lines), before);
  });
}

TEST_F(LineJoinerTest, SupportedJoiningPreservesBracketPartners) {
  const auto partners = [](const auto& lines) {
    std::vector<const format::FormatToken*> tokens;
    for (const auto& line : lines) {
      for (const auto& token : line.tokens) {
        tokens.push_back(&token);
      }
    }
    std::vector<size_t> result;
    for (const auto* token : tokens) {
      if (token->matching_bracket == nullptr) {
        result.push_back(tokens.size());
        continue;
      }
      const auto found = std::ranges::find(tokens, token->matching_bracket);
      EXPECT_NE(found, tokens.end());
      result.push_back(static_cast<size_t>(found - tokens.begin()));
    }
    return result;
  };
  forSupportedLayouts([&](std::string_view source, const auto& style) {
    auto lines = annotated(source, style);
    const auto before = partners(lines);
    format::LineJoiner(style).join(lines);
    EXPECT_EQ(partners(lines), before);
  });
}

TEST_F(LineJoinerTest, SupportedJoiningIsIdempotent) {
  const auto partitions = [](const auto& lines) {
    std::vector<size_t> result;
    result.reserve(lines.size());
    for (const auto& line : lines) {
      result.push_back(line.tokens.size());
    }
    return result;
  };
  forSupportedLayouts([&](std::string_view source, const auto& style) {
    auto lines = annotated(source, style);
    format::LineJoiner(style).join(lines);
    const auto once = partitions(lines);
    format::LineJoiner(style).join(lines);
    EXPECT_EQ(partitions(lines), once);
  });
}

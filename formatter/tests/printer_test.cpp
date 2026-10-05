#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/lex_context.h"
#include "formatter.h"

namespace {

class PrinterTest : public ::testing::Test {
 protected:
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

}  // namespace

TEST_F(PrinterTest, PrintsFormattedCode) {
  EXPECT_EQ(formatText("module m (); logic a; endmodule"),
            "module m (\n"
            ");\n"
            "  logic a;\n"
            "endmodule\n");
}

TEST_F(PrinterTest, RespectsCrLfLineTerminator) {
  format::FormatStyle style = format::FormatStyle::defaults();
  style.line_terminator = format::LineTerminator::kCrLf;

  EXPECT_EQ(formatText("module m (); endmodule", style),
            "module m (\r\n"
            ");\r\n"
            "endmodule\r\n");
}

TEST_F(PrinterTest, PreservesLeadingAndTrailingLineComments) {
  EXPECT_EQ(formatText("module m (); // header\n"
                       "// body\n"
                       "logic a; // data\n"
                       "endmodule"),
            "module m (\n"
            "); // header\n"
            "  // body\n"
            "  logic a; // data\n"
            "endmodule\n");
}

TEST_F(PrinterTest, PreservesWhitespaceAroundInlineBlockComments) {
  EXPECT_EQ(formatText("module m (); assign y = a/*tight*/b; "
                       "assign z = a /*left*/b; assign w = a/*right*/ b; "
                       "assign v = a /*both*/ b; endmodule"),
            "module m (\n"
            ");\n"
            "  assign y  = a/*tight*/b;\n"
            "  assign z  = a /*left*/b;\n"
            "  assign w  = a/*right*/ b;\n"
            "  assign v  = a /*both*/ b;\n"
            "endmodule\n");
}

TEST_F(PrinterTest, PreservesWhitespaceBeforeTrailingLineComments) {
  EXPECT_EQ(formatText("module m (); logic a;   // spaced\nendmodule"),
            "module m (\n"
            ");\n"
            "  logic a;   // spaced\n"
            "endmodule\n");

  EXPECT_EQ(formatText("module m (); logic a;// tight\nendmodule"),
            "module m (\n"
            ");\n"
            "  logic a;// tight\n"
            "endmodule\n");
}

TEST_F(PrinterTest, PrintsCompilerDirectivesAtLeftEdge) {
  EXPECT_EQ(formatText("module m ();\n"
                       "`ifdef FOO\n"
                       "assign y = `WIDTH'd0;\n"
                       "`include \"defs.svh\"\n"
                       "`endif\n"
                       "endmodule"),
            "module m (\n"
            ");\n"
            "`ifdef FOO\n"
            "  assign y = `WIDTH'd0;\n"
            "`include \"defs.svh\"\n"
            "`endif\n"
            "endmodule\n");
}

TEST_F(PrinterTest, NormalizesNumericLiteralTextBeforeFormatting) {
  EXPECT_EQ(formatText("module m (); assign y = 8'HFF; assign z = 'X; "
                       "assign r = 1E-3; endmodule"),
            "module m (\n"
            ");\n"
            "  assign y = 8'hff;\n"
            "  assign z = 'x;\n"
            "  assign r = 1e-3;\n"
            "endmodule\n");
}

TEST_F(PrinterTest, DoesNotInsertBeginEndDuringNormalization) {
  EXPECT_EQ(formatText("module m (); always_ff @(posedge clk) if (en) q <= d; "
                       "endmodule"),
            "module m (\n"
            ");\n"
            "  always_ff @(posedge clk)\n"
            "    if (en)\n"
            "      q <= d;\n"
            "endmodule\n");
}

TEST_F(PrinterTest, MultilineLeadingBlockCommentIsStable) {
  const std::string source =
      "module m;\n/* explanation\n * continued\n */\ninitial x = 1; endmodule";
  const auto once = formatText(source);
  EXPECT_EQ(once,
            "module m;\n  /* explanation\n * continued\n */\n"
            "  initial x = 1;\nendmodule\n");
  EXPECT_EQ(formatText(once), once) << "Input: " << source;
}

TEST_F(PrinterTest, RemovesLeadingBlankLinesConsistently) {
  const std::string source = "\n\n/* explanation */\nmodule m; endmodule";
  const auto once = formatText(source);
  EXPECT_EQ(once, "/* explanation */\nmodule m;\nendmodule\n");
  EXPECT_EQ(formatText(once), once) << "Input: " << source;
}

TEST_F(PrinterTest, RemovesTrailingBlankLines) {
  EXPECT_EQ(formatText("module m;\n  logic a;\nendmodule\n\n\n"),
            "module m;\n  logic a;\nendmodule\n");
}

TEST_F(PrinterTest, PreservesBlankLineBeforeFinalComment) {
  EXPECT_EQ(formatText("module m; endmodule\n\n// footer\n\n\n"),
            "module m;\nendmodule\n\n// footer\n");
}

TEST_F(PrinterTest, RemovesBlankLinesAfterFinalBlockComment) {
  EXPECT_EQ(formatText("module m; endmodule /* footer */\n\n\n"),
            "module m;\nendmodule /* footer */\n");
}

TEST_F(PrinterTest, PreservesCommentAfterLastToken) {
  EXPECT_EQ(formatText("module m; endmodule // last comment"),
            "module m;\nendmodule // last comment\n");
}

TEST_F(PrinterTest, PreservesTrailingBlockAndLineCommentOrder) {
  const std::string source = "x = 1; /* block */ // line\n";
  const auto once = formatText(source);
  EXPECT_EQ(once, source);
  EXPECT_EQ(formatText(once), once);
}

TEST_F(PrinterTest, TrailingMultilineBlockCommentUsesConfiguredLineEndings) {
  auto style = format::FormatStyle::defaults();
  style.line_terminator = format::LineTerminator::kCrLf;
  const auto once = formatText("x = 1; /* first\n second */", style);
  EXPECT_EQ(once, "x = 1; /* first\r\n second */\r\n");
  EXPECT_EQ(formatText(once, style), once);
}

TEST_F(PrinterTest, PreservesCommentOnlyFile) {
  EXPECT_EQ(formatText("/* explanation */"), "/* explanation */\n");
}

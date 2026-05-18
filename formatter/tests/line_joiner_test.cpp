#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/lex_context.h"
#include "formatter.h"

namespace {

class LineJoinerTest : public ::testing::Test {
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

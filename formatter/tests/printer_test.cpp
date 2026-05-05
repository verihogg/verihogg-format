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

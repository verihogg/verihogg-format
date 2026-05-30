#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/lex_context.h"
#include "formatter.h"

namespace {

class TabularAlignerTest : public ::testing::Test {
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

// ---------------------------------------------------------------------------
// Port list — direction / type / name columns
// ---------------------------------------------------------------------------

TEST_F(TabularAlignerTest, TwoPortsSameStructureAlignColumns) {
  // input logic clk: col0="input", col1="logic", col2="clk"
  // input logic rst: col0="input", col1="logic", col2="rst"
  // After alignment col1 and col2 must start at the same offset on both lines.
  const std::string result =
      formatText("module m (input logic clk, input logic rst); endmodule");
  auto pos_first = result.find("logic clk");
  auto pos_second = result.find("logic rst");
  ASSERT_NE(pos_first, std::string::npos);
  ASSERT_NE(pos_second, std::string::npos);

  auto col_of = [&](std::string::size_type pos) -> size_t {
    auto nl = result.rfind('\n', pos);
    return pos - (nl == std::string::npos ? 0 : nl + 1);
  };

  EXPECT_EQ(col_of(pos_first), col_of(pos_second))
      << "\"logic\" keyword must start at the same column on both port lines\n"
      << result;
}

// Three ports — all three name tokens must start at the same column.
TEST_F(TabularAlignerTest, ThreePortsNameColumnAligned) {
  const std::string result = formatText(
      "module counter ("
      "input  logic       clk,"
      "input  logic       rst_n,"
      "output logic [3:0] count"
      "); endmodule");

  auto col_of = [&](std::string::size_type pos) -> size_t {
    auto nl = result.rfind('\n', pos);
    return pos - (nl == std::string::npos ? 0 : nl + 1);
  };

  auto p_clk = result.find("clk");
  auto p_rst = result.find("rst_n");
  auto p_count = result.find("count");

  ASSERT_NE(p_clk, std::string::npos);
  ASSERT_NE(p_rst, std::string::npos);
  ASSERT_NE(p_count, std::string::npos);

  EXPECT_EQ(col_of(p_clk), col_of(p_rst))
      << "clk and rst_n must be in the same column\n"
      << result;
  EXPECT_EQ(col_of(p_clk), col_of(p_count))
      << "clk and count must be in the same column\n"
      << result;
}

// ---------------------------------------------------------------------------
// Module-body variable declarations
// ---------------------------------------------------------------------------

// Two declarations with the same column count.
TEST_F(TabularAlignerTest, BodyDeclarationsAlignTypeAndName) {
  const std::string result = formatText(
      "module m ();"
      "logic      a;"
      "logic      bb;"
      "endmodule");

  auto col_of = [&](std::string::size_type pos) -> size_t {
    auto nl = result.rfind('\n', pos);
    return pos - (nl == std::string::npos ? 0 : nl + 1);
  };

  auto p_a = result.find(" a;");
  auto p_bb = result.find(" bb;");

  ASSERT_NE(p_a, std::string::npos);
  ASSERT_NE(p_bb, std::string::npos);

  EXPECT_EQ(col_of(p_a + 1), col_of(p_bb + 1))
      << "Identifiers 'a' and 'bb' must start at the same column\n"
      << result;
}

// Declaration with and without direction keyword → different column counts →
// must NOT be merged into one group.
TEST_F(TabularAlignerTest, DifferentColumnCountsFormSeparateGroups) {
  const std::string result = formatText(
      "module m ();"
      "logic a;"
      "output logic b;"
      "endmodule");

  EXPECT_NE(result.find("logic a"), std::string::npos) << result;
  EXPECT_NE(result.find("logic b"), std::string::npos) << result;

  auto col_of = [&](std::string::size_type pos) -> size_t {
    auto nl = result.rfind('\n', pos);
    return pos - (nl == std::string::npos ? 0 : nl + 1);
  };

  auto p_a = result.find(" a;");
  auto p_b = result.find(" b;");
  ASSERT_NE(p_a, std::string::npos);
  ASSERT_NE(p_b, std::string::npos);
  EXPECT_NE(col_of(p_a + 1), col_of(p_b + 1))
      << "Identifiers from separate groups must NOT be in the same column\n"
      << result;
}

// ---------------------------------------------------------------------------
// Minimum group size — a single kTabularAlignment line is NOT aligned
// ---------------------------------------------------------------------------

TEST_F(TabularAlignerTest, SinglePortNotAligned) {
  const std::string result =
      formatText("module m (input logic clk); endmodule");
  EXPECT_NE(result.find("input logic clk"), std::string::npos) << result;
}

TEST_F(TabularAlignerTest, SingleBodyDeclarationNotAligned) {
  const std::string result = formatText("module m (); logic a; endmodule");

  EXPECT_NE(result.find("logic a"), std::string::npos) << result;
  EXPECT_NE(result.find("logic a;"), std::string::npos) << result;
}

// ---------------------------------------------------------------------------
// Bracket content forms a single cell (packed dimensions)
// ---------------------------------------------------------------------------

// Ports with packed dimensions — the content inside [...] must be treated as
// one cell so it doesn't split the column structure.
TEST_F(TabularAlignerTest, PackedDimensionsTreatedAsSingleCell) {
  const std::string result = formatText(
      "module m ("
      "input  logic [7:0]  data,"
      "input  logic [15:0] wide,"
      "output logic        out"
      "); endmodule");

  // "data", "wide", and "out" must align to the same column.
  auto col_of = [&](std::string::size_type pos) -> size_t {
    auto nl = result.rfind('\n', pos);
    return pos - (nl == std::string::npos ? 0 : nl + 1);
  };

  auto p_data = result.find("data");
  auto p_wide = result.find("wide");
  auto p_out = result.find("out\n");

  ASSERT_NE(p_data, std::string::npos) << result;
  ASSERT_NE(p_wide, std::string::npos) << result;
  ASSERT_NE(p_out, std::string::npos) << result;

  EXPECT_EQ(col_of(p_data), col_of(p_wide))
      << "'data' and 'wide' must be in the same column\n"
      << result;
  EXPECT_EQ(col_of(p_data), col_of(p_out))
      << "'data' and 'out' must be in the same column\n"
      << result;
}

// ---------------------------------------------------------------------------
// Suffix tokens (comma / semicolon) excluded from cells
// ---------------------------------------------------------------------------

// Commas are suffix tokens and must NOT form their own column; the aligner
// should treat them as part of the preceding cell so adding them doesn't
// create an extra alignment column.
TEST_F(TabularAlignerTest, TrailingCommaDoesNotCreateExtraColumn) {
  const std::string result = formatText(
      "module m ("
      "input logic a,"
      "input logic bb"
      "); endmodule");

  auto col_of = [&](std::string::size_type pos) -> size_t {
    auto nl = result.rfind('\n', pos);
    return pos - (nl == std::string::npos ? 0 : nl + 1);
  };

  auto p_a = result.find(" a,");
  auto p_bb = result.find(" bb\n");
  ASSERT_NE(p_a, std::string::npos) << result;
  ASSERT_NE(p_bb, std::string::npos) << result;

  EXPECT_EQ(col_of(p_a + 1), col_of(p_bb + 1))
      << "'a' and 'bb' must be in the same column\n"
      << result;
}

// ---------------------------------------------------------------------------
// Comment column — trailing line comments align to the same column
// ---------------------------------------------------------------------------

TEST_F(TabularAlignerTest, TrailingLineCommentsAlignedAcrossGroup) {
  const std::string result = formatText(
      "module m ();"
      "logic      a;  // short\n"
      "logic      bb; // longer\n"
      "endmodule");

  auto first_comment = result.find("// short");
  auto second_comment = result.find("// longer");

  ASSERT_NE(first_comment, std::string::npos) << result;
  ASSERT_NE(second_comment, std::string::npos) << result;

  auto col_of = [&](std::string::size_type pos) -> size_t {
    auto nl = result.rfind('\n', pos);
    return pos - (nl == std::string::npos ? 0 : nl + 1);
  };

  EXPECT_EQ(col_of(first_comment), col_of(second_comment))
      << "Trailing comments must start at the same column\n"
      << result;
}

// ---------------------------------------------------------------------------
// Lines NOT tagged kTabularAlignment pass through unchanged
// ---------------------------------------------------------------------------

TEST_F(TabularAlignerTest, NonTabularLinesPassThroughUnchanged) {
  const std::string result = formatText(
      "module m ();"
      "always_comb begin"
      "  a = 1;"
      "end"
      "endmodule");

  EXPECT_NE(result.find("always_comb"), std::string::npos) << result;
  EXPECT_NE(result.find("begin"), std::string::npos) << result;
  EXPECT_NE(result.find("end"), std::string::npos) << result;
}

// ---------------------------------------------------------------------------
// Idempotence — formatting already-formatted output must be stable
// ---------------------------------------------------------------------------

TEST_F(TabularAlignerTest, FormattingIsIdempotent) {
  const std::string_view src =
      "module counter ("
      "input  logic       clk,"
      "input  logic       rst_n,"
      "output logic [3:0] count"
      "); endmodule";

  const std::string first_pass = formatText(src);
  const std::string second_pass = formatText(first_pass);

  EXPECT_EQ(first_pass, second_pass)
      << "Formatter output must be stable under a second pass";
}

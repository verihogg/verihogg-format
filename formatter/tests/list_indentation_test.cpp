#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/lex_context.h"
#include "formatter.h"

namespace {

using format::FormatStyle;
using format::IndentationPolicy;

constexpr size_t kIndentationSpaces = 3;
constexpr size_t kWrapSpaces = 7;
constexpr size_t kNamedItemIndent = 6;
constexpr size_t kWrappedNamedItemIndent = 10;

struct ListCase {
  std::string_view name;
  std::string_view source;
  IndentationPolicy FormatStyle::* option;
  std::string_view item;
  std::string_view closing;
  size_t blockDepth;
};

constexpr std::array kListCases = {
    ListCase{"ModulePorts", "module m(input logic clk); endmodule",
             &FormatStyle::port_declarations_indentation, "input logic clk",
             ");", 0},
    ListCase{"InterfacePorts", "interface i(input logic clk); endinterface",
             &FormatStyle::port_declarations_indentation, "input logic clk",
             ");", 0},
    ListCase{"AutomaticModulePorts",
             "module automatic m(input logic clk); endmodule",
             &FormatStyle::port_declarations_indentation, "input logic clk",
             ");", 0},
    ListCase{"ModuleParameters",
             "module m #(parameter int WIDTH = 8)(); endmodule",
             &FormatStyle::formal_parameters_indentation, "parameter int WIDTH",
             ") (", 0},
    ListCase{"InterfaceParameters",
             "interface i #(parameter int WIDTH = 8)(); endinterface",
             &FormatStyle::formal_parameters_indentation, "parameter int WIDTH",
             ") (", 0},
    ListCase{"ClassParameters", "class c #(parameter int WIDTH = 8); endclass",
             &FormatStyle::formal_parameters_indentation, "parameter int WIDTH",
             ");", 0},
    ListCase{"StaticInterfaceParameters",
             "interface static i #(parameter int WIDTH = 8)(); endinterface",
             &FormatStyle::formal_parameters_indentation, "parameter int WIDTH",
             ") (", 0},
    ListCase{"StaticClassParameters",
             "class static c #(parameter int WIDTH = 8); endclass",
             &FormatStyle::formal_parameters_indentation, "parameter int WIDTH",
             ");", 0},
    ListCase{"TypeParameters", "class c #(parameter type T = logic); endclass",
             &FormatStyle::formal_parameters_indentation, "parameter type T",
             ");", 0},
    ListCase{"NamedParameters",
             "module m; child #(.WIDTH(8)) inst(); endmodule",
             &FormatStyle::named_parameter_indentation, ".WIDTH", ") inst", 1},
    ListCase{"NamedPorts", "module m; child inst(.clk(clk)); endmodule",
             &FormatStyle::named_port_indentation, ".clk", ");", 1},
    ListCase{"WildcardPorts", "module m; child inst(.*); endmodule",
             &FormatStyle::named_port_indentation, ".*", ");", 1},
};

class ListIndentationTest : public ::testing::Test {
 protected:
  auto formatText(std::string_view source, const FormatStyle& style = {})
      -> std::string {
    LexContext ctx;
    const auto tokens = ctx.lex_string(source);
    const auto result = format::format(tokens, style, ctx.source_text());
    EXPECT_TRUE(result.warnings.empty());
    LexContext formattedCtx;
    const auto formattedTokens = formattedCtx.lex_string(result.formatted_text);
    EXPECT_EQ(tokens.size(), formattedTokens.size());
    for (size_t i = 0; i < tokens.size() && i < formattedTokens.size(); ++i) {
      EXPECT_EQ(tokens.at(i).kind, formattedTokens.at(i).kind);
      EXPECT_EQ(tokens.at(i).rawText(), formattedTokens.at(i).rawText());
    }
    return result.formatted_text;
  }

  static void expectIndentation(std::string_view text, std::string_view item,
                                size_t spaces) {
    const auto pos = text.find(item);
    ASSERT_NE(pos, std::string_view::npos) << text;
    const auto newline = text.rfind('\n', pos);
    const auto start = newline == std::string_view::npos ? 0 : newline + 1;
    EXPECT_EQ(text.substr(start, pos - start), std::string(spaces, ' '))
        << text;
  }
};

class ListIndentationPolicyTest
    : public ListIndentationTest,
      public ::testing::WithParamInterface<ListCase> {};

TEST_P(ListIndentationPolicyTest, UsesConfiguredWidthRelativeToBlock) {
  const auto& testCase = GetParam();
  for (const auto policy :
       {IndentationPolicy::kIndent, IndentationPolicy::kWrap}) {
    SCOPED_TRACE(policy == IndentationPolicy::kIndent ? "indent" : "wrap");
    FormatStyle style;
    style.indentation_spaces = kIndentationSpaces;
    style.wrap_spaces = kWrapSpaces;
    style.*(testCase.option) = policy;
    const auto formatted = formatText(testCase.source, style);
    const size_t base = testCase.blockDepth * style.indentation_spaces;
    const size_t continuation =
        policy == IndentationPolicy::kIndent ? kIndentationSpaces : kWrapSpaces;
    expectIndentation(formatted, testCase.item, base + continuation);
    expectIndentation(formatted, testCase.closing, base);
    EXPECT_EQ(formatText(formatted, style), formatted);
  }
}

TEST_P(ListIndentationPolicyTest, AcceptsZeroContinuationWidth) {
  const auto& testCase = GetParam();
  for (const auto policy :
       {IndentationPolicy::kIndent, IndentationPolicy::kWrap}) {
    SCOPED_TRACE(policy == IndentationPolicy::kIndent ? "indent" : "wrap");
    FormatStyle style;
    style.indentation_spaces =
        policy == IndentationPolicy::kIndent ? 0 : kIndentationSpaces;
    style.wrap_spaces = policy == IndentationPolicy::kWrap ? 0 : kWrapSpaces;
    style.*(testCase.option) = policy;
    const auto formatted = formatText(testCase.source, style);
    const size_t base = testCase.blockDepth * style.indentation_spaces;
    expectIndentation(formatted, testCase.item, base);
    expectIndentation(formatted, testCase.closing, base);
    EXPECT_EQ(formatText(formatted, style), formatted);
  }
}

TEST_P(ListIndentationPolicyTest, DefaultsToWrap) {
  const auto& testCase = GetParam();
  const auto formatted = formatText(testCase.source);
  expectIndentation(formatted, testCase.item, testCase.blockDepth * 2 + 4);
  expectIndentation(formatted, testCase.closing, testCase.blockDepth * 2);
}

INSTANTIATE_TEST_SUITE_P(Lists, ListIndentationPolicyTest,
                         ::testing::ValuesIn(kListCases),
                         [](const ::testing::TestParamInfo<ListCase>& info) {
                           return std::string(info.param.name);
                         });

TEST_F(ListIndentationTest, PoliciesAreIndependentAndDoNotLeakIntoBody) {
  FormatStyle style;
  style.indentation_spaces = kIndentationSpaces;
  style.wrap_spaces = kWrapSpaces;
  style.port_declarations_indentation = IndentationPolicy::kIndent;
  style.formal_parameters_indentation = IndentationPolicy::kWrap;
  style.named_parameter_indentation = IndentationPolicy::kIndent;
  style.named_port_indentation = IndentationPolicy::kWrap;
  const auto formatted = formatText(
      "module m #(parameter int WIDTH = 8)(input logic clk);"
      "child #(.WIDTH(WIDTH)) inst(.clk(clk)); logic after_list; endmodule",
      style);

  expectIndentation(formatted, "parameter int WIDTH", kWrapSpaces);
  expectIndentation(formatted, "input logic clk", kIndentationSpaces);
  expectIndentation(formatted, ".WIDTH", kNamedItemIndent);
  expectIndentation(formatted, ".clk", kWrappedNamedItemIndent);
  expectIndentation(formatted, "logic after_list", kIndentationSpaces);
  expectIndentation(formatted, "endmodule", 0);
  EXPECT_EQ(formatText(formatted, style), formatted);
}

TEST_F(ListIndentationTest, NamedPoliciesDoNotAffectPositionalArguments) {
  const std::string source =
      "module m; child #(8, 16) inst(clk, rst); endmodule";
  FormatStyle style;
  style.indentation_spaces = kIndentationSpaces;
  style.wrap_spaces = kWrapSpaces;
  const auto wrapped = formatText(source, style);
  style.named_parameter_indentation = IndentationPolicy::kIndent;
  style.named_port_indentation = IndentationPolicy::kIndent;
  EXPECT_EQ(formatText(source, style), wrapped);
}

TEST_F(ListIndentationTest, NestedDelimitersDoNotSplitListItems) {
  const auto formatted = formatText(
      "module m #(parameter A = f(1, 2), parameter B = '{3, 4})"
      "(input logic [f(1, 2)-1:0] data);"
      "child #(.A(f(1, 2)), .B('{3, 4})) inst(.data({data[1:0], data[3:2]}));"
      "endmodule");
  expectIndentation(formatted, "parameter A", 4);
  expectIndentation(formatted, "parameter B", 4);
  expectIndentation(formatted, "input logic", 4);
  expectIndentation(formatted, ".A", kNamedItemIndent);
  expectIndentation(formatted, ".B", kNamedItemIndent);
  expectIndentation(formatted, ".data", kNamedItemIndent);
  EXPECT_EQ(formatText(formatted), formatted);
}

TEST_F(ListIndentationTest, DirectivesAndCommentsKeepListIndentation) {
  const std::string source =
      "module m #(\n"
      "`ifdef WIDE\n"
      "// wide parameter\n"
      "parameter WIDTH = 16,\n"
      "`else\n"
      "parameter WIDTH = 8,\n"
      "`endif\n"
      "parameter DEPTH = 4 // depth\n"
      ")(\n"
      "`ifdef INPUT\n"
      "input logic clk,\n"
      "`endif\n"
      "output logic out\n"
      "); endmodule";
  const auto formatted = formatText(source);
  expectIndentation(formatted, "`ifdef WIDE", 0);
  expectIndentation(formatted, "`else", 0);
  expectIndentation(formatted, "`endif", 0);
  expectIndentation(formatted, "// wide parameter", 4);
  expectIndentation(formatted, "parameter WIDTH", 4);
  expectIndentation(formatted, "parameter DEPTH", 4);
  expectIndentation(formatted, "input", 4);
  expectIndentation(formatted, "output", 4);
  EXPECT_NE(formatted.find("// depth\n"), std::string::npos);
  EXPECT_EQ(formatText(formatted), formatted);
}

TEST_F(ListIndentationTest, ClassBaseParametersAreNotFormalDeclarations) {
  FormatStyle style;
  style.formal_parameters_indentation = IndentationPolicy::kIndent;
  const auto formatted = formatText(
      "class c #(parameter WIDTH = 8) extends base #(WIDTH); endclass", style);
  expectIndentation(formatted, "parameter WIDTH", 2);
  EXPECT_NE(formatted.find(") extends base #(WIDTH);\n"), std::string::npos);
  EXPECT_EQ(formatText(formatted, style), formatted);
}

TEST_F(ListIndentationTest, NamedListsKeepIndentationAcrossDirectives) {
  const auto formatted = formatText(
      "module m; child #(\n"
      "`ifdef WIDE\n"
      ".WIDTH(16),\n"
      "`else\n"
      ".WIDTH(8),\n"
      "`endif\n"
      ".DEPTH(4)) inst(\n"
      "`ifdef INPUT\n"
      "// clock input\n"
      ".clk(clk),\n"
      "`endif\n"
      ".out(out)); endmodule");
  expectIndentation(formatted, "`ifdef WIDE", 0);
  expectIndentation(formatted, "`else", 0);
  expectIndentation(formatted, "`endif", 0);
  expectIndentation(formatted, ".WIDTH", kNamedItemIndent);
  expectIndentation(formatted, ".DEPTH", kNamedItemIndent);
  expectIndentation(formatted, "// clock input", kNamedItemIndent);
  expectIndentation(formatted, ".clk", kNamedItemIndent);
  expectIndentation(formatted, ".out", kNamedItemIndent);
  EXPECT_EQ(formatText(formatted), formatted);
}

TEST_F(ListIndentationTest, EmptyListsDoNotAffectFollowingIndentation) {
  EXPECT_EQ(formatText("module m #()(); child #() inst(); logic a; endmodule"),
            "module m #(\n"
            ") (\n"
            ");\n"
            "  child #(\n"
            "  ) inst (\n"
            "  );\n"
            "  logic a;\n"
            "endmodule\n");
}

}  // namespace

#include <gtest/gtest.h>
#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/format_style.h"
#include "data/lex_context.h"
#include "data/unwrapped_line.h"
#include "pipeline/tree_unwrapper.h"

namespace {

using TK = slang::parsing::TokenKind;
using Line = format::UnwrappedLine<slang::parsing::Token>;
using PP = format::PartitionPolicy;

constexpr size_t kColumnLimit = 100;
constexpr size_t kIndent = 2;

// ---------------------------------------------------------------------------
// Snapshot types — a lightweight copy of the tree that supports operator==
// and can be built by hand for expected values.
// ---------------------------------------------------------------------------

struct TokSnap {
  TK kind;
  std::string text;
  auto operator==(const TokSnap&) const -> bool = default;
};

struct LineSnap {
  std::vector<TokSnap> tokens;
  size_t indent = 0;
  auto operator==(const LineSnap& other) const -> bool = default;
};

// Converters from real lines to snapshots.

auto snap(const slang::parsing::Token& t) -> TokSnap {
  return {.kind = t.kind, .text = std::string(t.rawText())};
}

auto snap(const Line& l) -> LineSnap {
  LineSnap s{.indent = l.indentation_spaces};
  for (const auto& token : l.tokens) {
    s.tokens.push_back(snap(token));
  }
  return s;
}

auto snap(const std::vector<Line>& lines) -> std::vector<LineSnap> {
  std::vector<LineSnap> result;
  result.reserve(lines.size());
  for (const auto& l : lines) {
    result.push_back(snap(l));
  }
  return result;
}

// Builders for constructing expected snapshots by hand.

// N — build a token snapshot from its kind and raw text.
auto N(TK kind, std::string_view text) -> TokSnap {
  return {.kind = kind, .text = std::string(text)};
}

// L — build a line snapshot from indent level, partition policy, and tokens.
auto L(size_t indent, PP policy, std::vector<TokSnap> tokens) -> LineSnap {
  return {.tokens = std::move(tokens), .indent = indent};
}

auto policyName(PP policy) -> std::string_view {
  switch (policy) {
    case PP::kAlwaysExpand:
      return "AlwaysExpand";
    case PP::kFitOnLineElseExpand:
      return "FitOnLineElseExpand";
    case PP::kAssignmentAlignment:
      return "AssignmentAlignment";
    case PP::kTabularAlignment:
      return "TabularAlignment";
    case PP::kAlreadyFormatted:
      return "AlreadyFormatted";
  }
  return "Unknown";
}

auto operator<<(std::ostream& os, const TokSnap& tok) -> std::ostream& {
  return os << slang::parsing::toString(tok.kind) << "(" << tok.text << ")";
}

auto operator<<(std::ostream& os, const LineSnap& line) -> std::ostream& {
  os << "{indent=" << line.indent << ", tokens=";
  ::testing::internal::UniversalPrint(line.tokens, &os);
  return os << "}";
}

// ---------------------------------------------------------------------------
// Fixture — keeps LexContext alive so token memory outlives UnwrappedLine.
// GTest creates a fresh instance per TEST_F, so no shared state between tests.
// ---------------------------------------------------------------------------

class TreeUnwrapperTest : public ::testing::Test {
 protected:
  auto parse(std::string_view src) -> std::vector<Line> {
    tokens_ = ctx_.lex_string(src);
    const format::FormatStyle style = {.column_limit = kColumnLimit,
                                       .indentation_spaces = kIndent};
    return format::TreeUnwrapper(tokens_, style, ctx_.source_text()).unwrap();
  }

  auto parseWithDiagnostics(std::string_view src) -> format::UnwrapResult {
    tokens_ = ctx_.lex_string(src);
    const format::FormatStyle style = {.column_limit = kColumnLimit,
                                       .indentation_spaces = kIndent};
    return format::TreeUnwrapper(tokens_, style, ctx_.source_text())
        .unwrapWithDiagnostics();
  }

 private:
  LexContext ctx_;
  std::vector<slang::parsing::Token> tokens_;
};

}  // namespace

// NOLINTBEGIN(misc-use-internal-linkage,bugprone-throwing-static-initialization,cert-err58-cpp,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

// ---- module header ----------------------------------------------------------

TEST_F(TreeUnwrapperTest, ModuleHeaderWithoutPorts) {
  auto lines = parse("module foo (); endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "foo"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, ThreePortsBecomeThreeLines) {
  auto lines = parse(
      "module counter (input logic clk, input logic rst_n, output logic [3:0] "
      "count);"
      " endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "counter"),
            N(TK::OpenParenthesis, "("),
        }),
      // port 0: input logic clk ,
      L(0, PP::kTabularAlignment,
        {
            N(TK::InputKeyword, "input"),
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "clk"),
            N(TK::Comma, ","),
        }),
      // port 1: input logic rst_n ,
      L(0, PP::kTabularAlignment,
        {
            N(TK::InputKeyword, "input"),
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "rst_n"),
            N(TK::Comma, ","),
        }),
      // port 2: output logic [3:0] count  — no trailing comma
      L(0, PP::kTabularAlignment,
        {
            N(TK::OutputKeyword, "output"),
            N(TK::LogicKeyword, "logic"),
            N(TK::OpenBracket, "["),
            N(TK::IntegerLiteral, "3"),
            N(TK::Colon, ":"),
            N(TK::IntegerLiteral, "0"),
            N(TK::CloseBracket, "]"),
            N(TK::Identifier, "count"),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, BodyDeclarationsWithKeywordsUseTabularAlignment) {
  auto lines = parse("module m (); logic a; output logic y; endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(kIndent, PP::kTabularAlignment,
        {
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "a"),
            N(TK::Semicolon, ";"),
        }),
      L(kIndent, PP::kTabularAlignment,
        {
            N(TK::OutputKeyword, "output"),
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "y"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, ParameterizedInstantiationIsParsedBeforeFallback) {
  auto lines = parse("module m (); child #(8) u_child (clk, rst_n); endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(kIndent, PP::kFitOnLineElseExpand,
        {
            N(TK::Identifier, "child"),
            N(TK::Hash, "#"),
            N(TK::OpenParenthesis, "("),
            N(TK::IntegerLiteral, "8"),
            N(TK::CloseParenthesis, ")"),
            N(TK::Identifier, "u_child"),
            N(TK::OpenParenthesis, "("),
        }),
      L(kIndent, PP::kTabularAlignment,
        {
            N(TK::Identifier, "clk"),
            N(TK::Comma, ","),
        }),
      L(kIndent, PP::kTabularAlignment,
        {
            N(TK::Identifier, "rst_n"),
        }),
      L(kIndent, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, MprfConditionalPortsKeepDirectivesAsLines) {
  auto lines = parse(
      "`include \"scr1_arch_description.svh\"\n"
      "module scr1_pipe_mprf (\n"
      "`ifdef SCR1_MPRF_RST_EN\n"
      "input logic rst_n,\n"
      "`endif\n"
      "input logic clk\n"
      ");"
      "endmodule : scr1_pipe_mprf");

  const std::vector<LineSnap> expected = {
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`include"),
            N(TK::StringLiteral, "\"scr1_arch_description.svh\""),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "scr1_pipe_mprf"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`ifdef"),
            N(TK::Identifier, "SCR1_MPRF_RST_EN"),
        }),
      L(0, PP::kTabularAlignment,
        {
            N(TK::InputKeyword, "input"),
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "rst_n"),
            N(TK::Comma, ","),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`endif"),
        }),
      L(0, PP::kTabularAlignment,
        {
            N(TK::InputKeyword, "input"),
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "clk"),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
            N(TK::Colon, ":"),
            N(TK::Identifier, "scr1_pipe_mprf"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, PortListKeepsUnknownBacktickAsMacroUsage) {
  auto lines = parse(
      "module m (`SCR1_PORT(input logic a, b), input logic c); endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kTabularAlignment,
        {
            N(TK::Directive, "`SCR1_PORT"),
            N(TK::OpenParenthesis, "("),
            N(TK::InputKeyword, "input"),
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "a"),
            N(TK::Comma, ","),
            N(TK::Identifier, "b"),
            N(TK::CloseParenthesis, ")"),
            N(TK::Comma, ","),
        }),
      L(0, PP::kTabularAlignment,
        {
            N(TK::InputKeyword, "input"),
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "c"),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, MprfRamAttributeDeclarationsStayFlat) {
  auto lines = parse(
      "module m ();\n"
      "`ifdef SCR1_TRGT_FPGA_INTEL_MAX10\n"
      "(* ramstyle = \"M9K\" *) logic [`SCR1_XLEN-1:0] mprf_int "
      "[1:`SCR1_MPRF_SIZE-1];\n"
      "`else\n"
      "type_scr1_mprf_v [1:`SCR1_MPRF_SIZE-1] mprf_int;\n"
      "`endif\n"
      "endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`ifdef"),
            N(TK::Identifier, "SCR1_TRGT_FPGA_INTEL_MAX10"),
        }),
      L(kIndent, PP::kTabularAlignment,
        {
            N(TK::OpenParenthesis, "("),
            N(TK::Star, "*"),
            N(TK::Identifier, "ramstyle"),
            N(TK::Equals, "="),
            N(TK::StringLiteral, "\"M9K\""),
            N(TK::Star, "*"),
            N(TK::CloseParenthesis, ")"),
            N(TK::LogicKeyword, "logic"),
            N(TK::OpenBracket, "["),
            N(TK::Directive, "`SCR1_XLEN"),
            N(TK::Minus, "-"),
            N(TK::IntegerLiteral, "1"),
            N(TK::Colon, ":"),
            N(TK::IntegerLiteral, "0"),
            N(TK::CloseBracket, "]"),
            N(TK::Identifier, "mprf_int"),
            N(TK::OpenBracket, "["),
            N(TK::IntegerLiteral, "1"),
            N(TK::Colon, ":"),
            N(TK::Directive, "`SCR1_MPRF_SIZE"),
            N(TK::Minus, "-"),
            N(TK::IntegerLiteral, "1"),
            N(TK::CloseBracket, "]"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`else"),
        }),
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::Identifier, "type_scr1_mprf_v"),
            N(TK::OpenBracket, "["),
            N(TK::IntegerLiteral, "1"),
            N(TK::Colon, ":"),
            N(TK::Directive, "`SCR1_MPRF_SIZE"),
            N(TK::Minus, "-"),
            N(TK::IntegerLiteral, "1"),
            N(TK::CloseBracket, "]"),
            N(TK::Identifier, "mprf_int"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`endif"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, ConditionalModuleBranchesShareEndmodule) {
  auto result = parseWithDiagnostics(
      "`ifdef USE_A\n"
      "module m_a ();\n"
      "`else\n"
      "module m_b ();\n"
      "`endif\n"
      "logic x;\n"
      "endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`ifdef"),
            N(TK::Identifier, "USE_A"),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m_a"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`else"),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m_b"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`endif"),
        }),
      L(kIndent, PP::kTabularAlignment,
        {
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "x"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_TRUE(result.warnings.empty());
  EXPECT_EQ(snap(result.lines), expected);
}

TEST_F(TreeUnwrapperTest,
       ConditionalModuleBranchesCanHavePreludeBeforeSharedEndmodule) {
  auto result = parseWithDiagnostics(
      "`ifdef USE_A\n"
      "localparam int SELECTED = 0;\n"
      "module m_a ();\n"
      "`else\n"
      "localparam int SELECTED = 1;\n"
      "module m_b ();\n"
      "`endif\n"
      "logic x;\n"
      "endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`ifdef"),
            N(TK::Identifier, "USE_A"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::LocalParamKeyword, "localparam"),
            N(TK::IntKeyword, "int"),
            N(TK::Identifier, "SELECTED"),
            N(TK::Equals, "="),
            N(TK::IntegerLiteral, "0"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m_a"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`else"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::LocalParamKeyword, "localparam"),
            N(TK::IntKeyword, "int"),
            N(TK::Identifier, "SELECTED"),
            N(TK::Equals, "="),
            N(TK::IntegerLiteral, "1"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m_b"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`endif"),
        }),
      L(kIndent, PP::kTabularAlignment,
        {
            N(TK::LogicKeyword, "logic"),
            N(TK::Identifier, "x"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_TRUE(result.warnings.empty());
  EXPECT_EQ(snap(result.lines), expected);
}

// ---- always_ff --------------------------------------------------------------

TEST_F(TreeUnwrapperTest, AlwaysFFIsOwnLine) {
  auto lines =
      parse("module m (); always_ff @(posedge clk) begin end endmodule");

  const std::vector<LineSnap> expected = {
      // module m ();
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      // always_ff @(posedge clk)  — indent 1*kIndent
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::AlwaysFFKeyword, "always_ff"),
            N(TK::At, "@"),
            N(TK::OpenParenthesis, "("),
            N(TK::PosEdgeKeyword, "posedge"),
            N(TK::Identifier, "clk"),
            N(TK::CloseParenthesis, ")"),
        }),
      // begin  — indent 2*kIndent
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::BeginKeyword, "begin"),
        }),
      // end  — indent 2*kIndent
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::EndKeyword, "end"),
        }),
      // endmodule
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, ConditionalAlwaysFFBranchesShareBeginEndBody) {
  auto result = parseWithDiagnostics(
      "module m ();\n"
      "`ifndef USE_ALT_CLK\n"
      "always_ff @(posedge clk) begin\n"
      "`else\n"
      "always_ff @(posedge alt_clk) begin\n"
      "`endif\n"
      "if (rst) begin\n"
      "q <= d;\n"
      "end\n"
      "end\n"
      "endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`ifndef"),
            N(TK::Identifier, "USE_ALT_CLK"),
        }),
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::AlwaysFFKeyword, "always_ff"),
            N(TK::At, "@"),
            N(TK::OpenParenthesis, "("),
            N(TK::PosEdgeKeyword, "posedge"),
            N(TK::Identifier, "clk"),
            N(TK::CloseParenthesis, ")"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::BeginKeyword, "begin"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`else"),
        }),
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::AlwaysFFKeyword, "always_ff"),
            N(TK::At, "@"),
            N(TK::OpenParenthesis, "("),
            N(TK::PosEdgeKeyword, "posedge"),
            N(TK::Identifier, "alt_clk"),
            N(TK::CloseParenthesis, ")"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::BeginKeyword, "begin"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`endif"),
        }),
      L(kIndent * 3, PP::kAlwaysExpand,
        {
            N(TK::IfKeyword, "if"),
            N(TK::OpenParenthesis, "("),
            N(TK::Identifier, "rst"),
            N(TK::CloseParenthesis, ")"),
        }),
      L(kIndent * 4, PP::kAlwaysExpand,
        {
            N(TK::BeginKeyword, "begin"),
        }),
      L(kIndent * 5, PP::kAlwaysExpand,
        {
            N(TK::Identifier, "q"),
            N(TK::LessThanEquals, "<="),
            N(TK::Identifier, "d"),
            N(TK::Semicolon, ";"),
        }),
      L(kIndent * 4, PP::kAlwaysExpand,
        {
            N(TK::EndKeyword, "end"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::EndKeyword, "end"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_TRUE(result.warnings.empty());
  EXPECT_EQ(snap(result.lines), expected);
}

TEST_F(TreeUnwrapperTest, MprfResetAlwaysFFWithAggregateLiteral) {
  auto lines = parse(
      "module m ();\n"
      "`ifdef SCR1_MPRF_RST_EN\n"
      "always_ff @(posedge clk, negedge rst_n) begin "
      "if (~rst_n) begin "
      "mprf_int <= '{default: '0}; "
      "end else if (wr_req_vd) begin "
      "mprf_int[exu2mprf_rd_addr_i] <= exu2mprf_rd_data_i; "
      "end "
      "end\n"
      "`endif\n"
      "endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`ifdef"),
            N(TK::Identifier, "SCR1_MPRF_RST_EN"),
        }),
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::AlwaysFFKeyword, "always_ff"),
            N(TK::At, "@"),
            N(TK::OpenParenthesis, "("),
            N(TK::PosEdgeKeyword, "posedge"),
            N(TK::Identifier, "clk"),
            N(TK::Comma, ","),
            N(TK::NegEdgeKeyword, "negedge"),
            N(TK::Identifier, "rst_n"),
            N(TK::CloseParenthesis, ")"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::BeginKeyword, "begin"),
        }),
      L(kIndent * 3, PP::kAlwaysExpand,
        {
            N(TK::IfKeyword, "if"),
            N(TK::OpenParenthesis, "("),
            N(TK::Tilde, "~"),
            N(TK::Identifier, "rst_n"),
            N(TK::CloseParenthesis, ")"),
        }),
      L(kIndent * 4, PP::kAlwaysExpand,
        {
            N(TK::BeginKeyword, "begin"),
        }),
      L(kIndent * 5, PP::kAlwaysExpand,
        {
            N(TK::Identifier, "mprf_int"),
            N(TK::LessThanEquals, "<="),
            N(TK::ApostropheOpenBrace, "'{"),
            N(TK::DefaultKeyword, "default"),
            N(TK::Colon, ":"),
            N(TK::UnbasedUnsizedLiteral, "'0"),
            N(TK::CloseBrace, "}"),
            N(TK::Semicolon, ";"),
        }),
      L(kIndent * 4, PP::kAlwaysExpand,
        {
            N(TK::EndKeyword, "end"),
        }),
      L(kIndent * 3, PP::kAlwaysExpand,
        {
            N(TK::ElseKeyword, "else"),
            N(TK::IfKeyword, "if"),
            N(TK::OpenParenthesis, "("),
            N(TK::Identifier, "wr_req_vd"),
            N(TK::CloseParenthesis, ")"),
        }),
      L(kIndent * 4, PP::kAlwaysExpand,
        {
            N(TK::BeginKeyword, "begin"),
        }),
      L(kIndent * 5, PP::kAlwaysExpand,
        {
            N(TK::Identifier, "mprf_int"),
            N(TK::OpenBracket, "["),
            N(TK::Identifier, "exu2mprf_rd_addr_i"),
            N(TK::CloseBracket, "]"),
            N(TK::LessThanEquals, "<="),
            N(TK::Identifier, "exu2mprf_rd_data_i"),
            N(TK::Semicolon, ";"),
        }),
      L(kIndent * 4, PP::kAlwaysExpand,
        {
            N(TK::EndKeyword, "end"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::EndKeyword, "end"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`endif"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, MprfSimulationAssertionStaysSingleStatementLine) {
  auto lines = parse(
      "module m ();\n"
      "`ifdef SCR1_TRGT_SIMULATION\n"
      "SCR1_SVA_MPRF_WRITEX : assert property ("
      "@(negedge clk) disable iff (~rst_n) "
      "exu2mprf_w_req_i |-> !$isunknown({exu2mprf_rd_addr_i, "
      "(|exu2mprf_rd_addr_i ? exu2mprf_rd_data_i : `SCR1_XLEN'd0)})"
      ") else $error(\"MPRF error: unknown values\");\n"
      "`endif\n"
      "endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`ifdef"),
            N(TK::Identifier, "SCR1_TRGT_SIMULATION"),
        }),
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::Identifier, "SCR1_SVA_MPRF_WRITEX"),
            N(TK::Colon, ":"),
            N(TK::AssertKeyword, "assert"),
            N(TK::PropertyKeyword, "property"),
            N(TK::OpenParenthesis, "("),
            N(TK::At, "@"),
            N(TK::OpenParenthesis, "("),
            N(TK::NegEdgeKeyword, "negedge"),
            N(TK::Identifier, "clk"),
            N(TK::CloseParenthesis, ")"),
            N(TK::DisableKeyword, "disable"),
            N(TK::IffKeyword, "iff"),
            N(TK::OpenParenthesis, "("),
            N(TK::Tilde, "~"),
            N(TK::Identifier, "rst_n"),
            N(TK::CloseParenthesis, ")"),
            N(TK::Identifier, "exu2mprf_w_req_i"),
            N(TK::OrMinusArrow, "|->"),
            N(TK::Exclamation, "!"),
            N(TK::SystemIdentifier, "$isunknown"),
            N(TK::OpenParenthesis, "("),
            N(TK::OpenBrace, "{"),
            N(TK::Identifier, "exu2mprf_rd_addr_i"),
            N(TK::Comma, ","),
            N(TK::OpenParenthesis, "("),
            N(TK::Or, "|"),
            N(TK::Identifier, "exu2mprf_rd_addr_i"),
            N(TK::Question, "?"),
            N(TK::Identifier, "exu2mprf_rd_data_i"),
            N(TK::Colon, ":"),
            N(TK::Directive, "`SCR1_XLEN"),
            N(TK::IntegerBase, "'d"),
            N(TK::IntegerLiteral, "0"),
            N(TK::CloseParenthesis, ")"),
            N(TK::CloseBrace, "}"),
            N(TK::CloseParenthesis, ")"),
            N(TK::CloseParenthesis, ")"),
            N(TK::ElseKeyword, "else"),
            N(TK::SystemIdentifier, "$error"),
            N(TK::OpenParenthesis, "("),
            N(TK::StringLiteral, "\"MPRF error: unknown values\""),
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`endif"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, LabeledAssertionElseBeginParsesActionBlock) {
  auto result = parseWithDiagnostics(
      "module m ();\n"
      "`ifdef SIM\n"
      "A_CHECK : assert property (@(posedge clk) ok) else begin\n"
      "$error(\"bad\");\n"
      "end\n"
      "`endif\n"
      "endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`ifdef"),
            N(TK::Identifier, "SIM"),
        }),
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::Identifier, "A_CHECK"),
            N(TK::Colon, ":"),
            N(TK::AssertKeyword, "assert"),
            N(TK::PropertyKeyword, "property"),
            N(TK::OpenParenthesis, "("),
            N(TK::At, "@"),
            N(TK::OpenParenthesis, "("),
            N(TK::PosEdgeKeyword, "posedge"),
            N(TK::Identifier, "clk"),
            N(TK::CloseParenthesis, ")"),
            N(TK::Identifier, "ok"),
            N(TK::CloseParenthesis, ")"),
            N(TK::ElseKeyword, "else"),
            N(TK::BeginKeyword, "begin"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::SystemIdentifier, "$error"),
            N(TK::OpenParenthesis, "("),
            N(TK::StringLiteral, "\"bad\""),
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::EndKeyword, "end"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::Directive, "`endif"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_TRUE(result.warnings.empty());
  EXPECT_EQ(snap(result.lines), expected);
}

// ---- begin/end --------------------------------------------------------------

TEST_F(TreeUnwrapperTest, BeginEndAreOwnLines) {
  auto lines = parse("module m (); always_comb begin end endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      // always_comb has no sensitivity list, so no @ token
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::AlwaysCombKeyword, "always_comb"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::BeginKeyword, "begin"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::EndKeyword, "end"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

// ---- if ---------------------------------------------------------------------

TEST_F(TreeUnwrapperTest, IfIsOwnLine) {
  auto lines =
      parse("module m (); always_comb begin if (a) b = 1; end endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
        }),
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::CloseParenthesis, ")"),
            N(TK::Semicolon, ";"),
        }),
      L(kIndent, PP::kAlwaysExpand,
        {
            N(TK::AlwaysCombKeyword, "always_comb"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::BeginKeyword, "begin"),
        }),
      // if (a)  — indent 3*kIndent (inside begin, inside always_comb, inside
      // module)
      L(kIndent * 3, PP::kAlwaysExpand,
        {
            N(TK::IfKeyword, "if"),
            N(TK::OpenParenthesis, "("),
            N(TK::Identifier, "a"),
            N(TK::CloseParenthesis, ")"),
        }),
      // b = 1;  — indent 4*kIndent
      L(kIndent * 4, PP::kAlwaysExpand,
        {
            N(TK::Identifier, "b"),
            N(TK::Equals, "="),
            N(TK::IntegerLiteral, "1"),
            N(TK::Semicolon, ";"),
        }),
      L(kIndent * 2, PP::kAlwaysExpand,
        {
            N(TK::EndKeyword, "end"),
        }),
      L(0, PP::kAlwaysExpand,
        {
            N(TK::EndModuleKeyword, "endmodule"),
        }),
  };

  EXPECT_EQ(snap(lines), expected);
}

TEST_F(TreeUnwrapperTest, MacroCastDoesNotTriggerIdentifierRecovery) {
  auto result = parseWithDiagnostics(
      "module m;\n"
      "  always_comb begin\n"
      "    data = `WIDTH'(other);\n"
      "  end\n"
      "endmodule\n");
  EXPECT_TRUE(result.warnings.empty());
  for (const auto& line : result.lines) {
    EXPECT_FALSE(line.is_opaque);
  }
}

TEST_F(TreeUnwrapperTest, IdentifierStatementsContinueAcrossSourceLines) {
  auto result = parseWithDiagnostics(
      "module m;\n"
      "  initial begin\n"
      "    value = a &\n"
      "            b +\n"
      "            int'(c);\n"
      "    next = value +\n"
      "           d;\n"
      "    casted =\n"
      "             int'(next);\n"
      "    incremented = ++\n"
      "                  next;\n"
      "  end\n"
      "endmodule\n");

  EXPECT_TRUE(result.warnings.empty());
  size_t assignment_count = 0;
  for (const auto& line : result.lines) {
    EXPECT_FALSE(line.is_opaque);
    if (line.tokens.empty()) {
      continue;
    }
    const auto first = line.tokens.front().rawText();
    if (first == "value" || first == "next" || first == "casted" ||
        first == "incremented") {
      ++assignment_count;
      EXPECT_EQ(line.tokens.back().kind, TK::Semicolon);
      bool saw_continuation = false;
      for (const auto& token : line.tokens) {
        if (token.rawText() == (first == "value"  ? "c"
                                : first == "next" ? "d"
                                                  : "next")) {
          saw_continuation = true;
        }
      }
      EXPECT_TRUE(saw_continuation);
    }
  }
  EXPECT_EQ(assignment_count, 4U);
}

TEST_F(TreeUnwrapperTest, IdentifierRecoveryKeepsUnknownStatement) {
  auto result = parseWithDiagnostics(
      "module m;\n"
      "  mystery words\n"
      "  next = 1;\n"
      "endmodule\n");

  ASSERT_EQ(result.warnings.size(), 1U);
  bool preserved_following_statement = false;
  for (const auto& line : result.lines) {
    if (line.is_opaque &&
        line.raw_text.find("  next = 1;") != std::string::npos) {
      preserved_following_statement = true;
    }
  }
  EXPECT_TRUE(preserved_following_statement);
}

TEST_F(TreeUnwrapperTest, UnsupportedCovergroupPreservesBodyAndResumes) {
  auto result = parseWithDiagnostics(
      "module m;\n"
      "  covergroup cg;\n"
      "    logic should_stay ;\n"
      "  endgroup\n"
      "  logic b;\n"
      "endmodule\n");

  ASSERT_EQ(result.warnings.size(), 1U);
  EXPECT_EQ(result.warnings.front().code, "unsupported-construct");
  EXPECT_NE(result.warnings.front().message.find("covergroup"),
            std::string::npos);

  size_t opaque_count = 0;
  bool resumed = false;
  for (const auto& line : result.lines) {
    if (line.is_opaque) {
      ++opaque_count;
      EXPECT_EQ(line.raw_text,
                "\n  covergroup cg;\n"
                "    logic should_stay ;\n"
                "  endgroup\n");
    } else if (!line.tokens.empty() &&
               line.tokens.front().rawText() == "logic") {
      resumed = true;
    }
  }
  EXPECT_EQ(opaque_count, 1U);
  EXPECT_TRUE(resumed);
}

TEST_F(TreeUnwrapperTest, NestedOpaqueBlocksResumeAfterOuterClose) {
  auto result = parseWithDiagnostics(
      "module m;\n"
      "  covergroup outer;\n"
      "    covergroup inner;\n"
      "    endgroup\n"
      "  endgroup\n"
      "  logic b;\n"
      "endmodule\n");

  ASSERT_EQ(result.warnings.size(), 1U);
  size_t opaque_count = 0;
  bool resumed = false;
  for (const auto& line : result.lines) {
    if (line.is_opaque) {
      ++opaque_count;
      EXPECT_NE(line.raw_text.find("    endgroup\n  endgroup\n"),
                std::string::npos);
    } else if (!line.tokens.empty() &&
               line.tokens.front().rawText() == "logic") {
      resumed = true;
    }
  }
  EXPECT_EQ(opaque_count, 1U);
  EXPECT_TRUE(resumed);
}

TEST_F(TreeUnwrapperTest, UnknownBoundaryPreservesToEnclosingClose) {
  auto result = parseWithDiagnostics(
      "class c;\n"
      "  constraint rule { x inside {[0:3]}; }\n"
      "  logic should_stay ;\n"
      "endclass\n"
      "module m;\n"
      "  logic b;\n"
      "endmodule\n");

  ASSERT_EQ(result.warnings.size(), 1U);
  EXPECT_EQ(result.warnings.front().code, "unsupported-construct");
  bool preserved_sibling = false;
  bool resumed_module = false;
  for (const auto& line : result.lines) {
    if (line.is_opaque) {
      preserved_sibling =
          line.raw_text.find("logic should_stay ;") != std::string::npos;
    } else if (!line.tokens.empty() &&
               line.tokens.front().rawText() == "module") {
      resumed_module = true;
    }
  }
  EXPECT_TRUE(preserved_sibling);
  EXPECT_TRUE(resumed_module);
}

TEST_F(TreeUnwrapperTest, UnknownTextSkipsNestedBeginBeforeOuterEnd) {
  auto result = parseWithDiagnostics(
      "module m;\n"
      "  initial begin\n"
      "    ??? unknown\n"
      "    begin\n"
      "      logic inner;\n"
      "    end\n"
      "    logic still_inside;\n"
      "  end\n"
      "  logic after;\n"
      "endmodule\n");

  ASSERT_EQ(result.warnings.size(), 1U);
  bool preserved_inner_end = false;
  bool resumed_after_outer_end = false;
  for (const auto& line : result.lines) {
    if (line.is_opaque) {
      preserved_inner_end =
          line.raw_text.find("    end\n    logic still_inside;") !=
          std::string::npos;
    } else if (!line.tokens.empty() &&
               line.tokens.front().rawText() == "logic" &&
               line.tokens.size() > 1 &&
               line.tokens.at(1).rawText() == "after") {
      resumed_after_outer_end = true;
    }
  }
  EXPECT_TRUE(preserved_inner_end);
  EXPECT_TRUE(resumed_after_outer_end);
}

TEST_F(TreeUnwrapperTest, ForkJoinAnyUsesSharedBlockBoundary) {
  auto result = parseWithDiagnostics(
      "module m;\n"
      "  initial fork\n"
      "    a();\n"
      "  join_any\n"
      "  logic b;\n"
      "endmodule\n");

  EXPECT_TRUE(result.warnings.empty());
  bool resumed = false;
  for (const auto& line : result.lines) {
    if (!line.tokens.empty() && line.tokens.front().rawText() == "logic") {
      resumed = true;
    }
  }
  EXPECT_TRUE(resumed);
}

// NOLINTEND(misc-use-internal-linkage,bugprone-throwing-static-initialization,cert-err58-cpp,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

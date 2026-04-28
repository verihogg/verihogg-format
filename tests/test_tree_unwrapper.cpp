#include <gtest/gtest.h>
#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>

#include <cstddef>
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

// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct TokSnap {
  TK kind;
  std::string text;
  auto operator==(const TokSnap&) const -> bool = default;
};

struct LineSnap;

// NOLINTBEGIN(misc-no-recursion)
struct NodeSnap {
  TokSnap tok;
  std::vector<LineSnap> children;
  auto operator==(const NodeSnap&) const -> bool = default;
};

struct LineSnap {
  std::vector<NodeSnap> nodes;
  size_t indent = 0;
  PP policy = PP::kAlwaysExpand;
  auto operator==(const LineSnap&) const -> bool = default;
};
// NOLINTEND(misc-no-recursion)
// NOLINTEND(misc-non-private-member-variables-in-classes)

// Converters from real tree to snapshot.

auto snap(const slang::parsing::Token& t) -> TokSnap {
  return {.kind = t.kind, .text = std::string(t.rawText())};
}

auto snap(const format::UnwrappedLineNode<slang::parsing::Token>& n)
    -> NodeSnap;
auto snap(const Line& l) -> LineSnap;

// NOLINTBEGIN(misc-no-recursion)
auto snap(const format::UnwrappedLineNode<slang::parsing::Token>& n)
    -> NodeSnap {
  NodeSnap s{.tok = snap(n.token)};
  for (const auto& child : n.children) {
    s.children.push_back(snap(child));
  }
  return s;
}

auto snap(const Line& l) -> LineSnap {
  LineSnap s{.indent = l.indentation_spaces, .policy = l.partition_policy};
  for (const auto& n : l.tokens) {
    s.nodes.push_back(snap(n));
  }
  return s;
}
// NOLINTEND(misc-no-recursion)

auto snap(const std::vector<Line>& lines) -> std::vector<LineSnap> {
  std::vector<LineSnap> result;
  result.reserve(lines.size());
  for (const auto& l : lines) {
    result.push_back(snap(l));
  }
  return result;
}

// Builders for constructing expected snapshots by hand.

auto N(TK kind, std::string_view text, std::vector<LineSnap> children = {})
    -> NodeSnap {
  return {.tok = {.kind = kind, .text = std::string(text)},
          .children = std::move(children)};
}

auto L(size_t indent, PP policy, std::vector<NodeSnap> nodes) -> LineSnap {
  return {.nodes = std::move(nodes), .indent = indent, .policy = policy};
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
    return format::TreeUnwrapper(tokens_, style).unwrap();
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

TEST_F(TreeUnwrapperTest, ThreePortsBecomeThreeChildren) {
  auto lines = parse(
      "module counter (input logic clk, input logic rst_n, output logic [3:0] "
      "count);"
      " endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "counter"),
            N(TK::OpenParenthesis, "(",
              {
                  // port 0: input logic clk ,
                  L(kIndent, PP::kTabularAlignment,
                    {
                        N(TK::InputKeyword, "input"),
                        N(TK::LogicKeyword, "logic"),
                        N(TK::Identifier, "clk"),
                        N(TK::Comma, ","),
                    }),
                  // port 1: input logic rst_n ,
                  L(kIndent, PP::kTabularAlignment,
                    {
                        N(TK::InputKeyword, "input"),
                        N(TK::LogicKeyword, "logic"),
                        N(TK::Identifier, "rst_n"),
                        N(TK::Comma, ","),
                    }),
                  // port 2: output logic [3:0] count  — no trailing comma
                  L(kIndent, PP::kTabularAlignment,
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
              }),
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

// ---- begin/end --------------------------------------------------------------

TEST_F(TreeUnwrapperTest, BeginEndAreOwnLines) {
  auto lines = parse("module m (); always_comb begin end endmodule");

  const std::vector<LineSnap> expected = {
      L(0, PP::kFitOnLineElseExpand,
        {
            N(TK::ModuleKeyword, "module"),
            N(TK::Identifier, "m"),
            N(TK::OpenParenthesis, "("),
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

// NOLINTEND(misc-use-internal-linkage,bugprone-throwing-static-initialization,cert-err58-cpp,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

#include "pipeline/cache_key.h"

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string_view>

namespace {

using format::AlignmentGroupBoundary;
using format::AlignmentPolicy;
using format::FormatStyle;
using format::IndentationPolicy;
using format::LineTerminator;
using format::makeCacheKey;
using format::sha256;

TEST(CacheKeyTest, Sha256MatchesKnownVectors) {
  EXPECT_EQ(sha256(""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(sha256("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(sha256(std::string_view{"a\0b", 3}),
            "59b271ae1bbcb1d31d41929817f4b16fb439eb4f31520b5ad1d5ce98920a7138");
}

TEST(CacheKeyTest, ContentAndBuildChangesInvalidateKey) {
  const FormatStyle style;
  const auto key = makeCacheKey("a = b;", style, "build-a");
  EXPECT_EQ(makeCacheKey("a = b;", style, "build-a"), key);
  EXPECT_NE(makeCacheKey("a =b ;", style, "build-a"), key);
  EXPECT_NE(makeCacheKey("a = b;", style, "build-b"), key);
  EXPECT_NE(makeCacheKey("a = b;\n", style, "build-a"), key);
  EXPECT_NE(makeCacheKey("ab", style, "c"), makeCacheKey("a", style, "bc"));
}

TEST(CacheKeyTest, MissingBuildIdentityIsRejected) {
  EXPECT_THROW((void)makeCacheKey("source", FormatStyle{}, ""),
               std::invalid_argument);
}

TEST(CacheKeyTest, EveryCurrentStyleFieldInvalidatesKey) {
  const FormatStyle base;
  const auto key = makeCacheKey("source", base, "build");
  {
    const std::array members{
        &FormatStyle::column_limit,
        &FormatStyle::indentation_spaces,
        &FormatStyle::wrap_spaces,
        &FormatStyle::line_break_penalty,
        &FormatStyle::over_column_limit_penalty,
    };
    for (const auto member : members) {
      auto changed = base;
      changed.*member += 1;
      EXPECT_NE(makeCacheKey("source", changed, "build"), key);
    }
  }
  {
    const std::array members{
        &FormatStyle::port_declarations_alignment,
        &FormatStyle::module_net_variable_alignment,
        &FormatStyle::assignment_statement_alignment,
        &FormatStyle::formal_parameters_alignment,
        &FormatStyle::named_parameter_alignment,
        &FormatStyle::named_port_alignment,
        &FormatStyle::parameter_declaration_alignment,
        &FormatStyle::case_items_alignment,
        &FormatStyle::enum_assignment_statement_alignment,
        &FormatStyle::struct_union_members_alignment,
        &FormatStyle::class_member_variable_alignment,
        &FormatStyle::distribution_items_alignment,
    };
    for (const auto member : members) {
      auto changed = base;
      changed.*member = AlignmentPolicy::kAlign;
      EXPECT_NE(makeCacheKey("source", changed, "build"), key);
    }
  }
  {
    const std::array members{
        &FormatStyle::port_declarations_indentation,
        &FormatStyle::formal_parameters_indentation,
        &FormatStyle::named_parameter_indentation,
        &FormatStyle::named_port_indentation,
    };
    for (const auto member : members) {
      auto changed = base;
      changed.*member = IndentationPolicy::kIndent;
      EXPECT_NE(makeCacheKey("source", changed, "build"), key);
    }
  }
  {
    const std::array members{
        &FormatStyle::port_declarations_right_align_packed_dimensions,
        &FormatStyle::port_declarations_right_align_unpacked_dimensions,
        &FormatStyle::compact_indexing_and_selections,
        &FormatStyle::class_parameter_space,
        &FormatStyle::expand_coverpoints,
        &FormatStyle::try_wrap_long_lines,
        &FormatStyle::wrap_end_else_clauses,
    };
    for (const auto member : members) {
      auto changed = base;
      changed.*member = !(changed.*member);
      EXPECT_NE(makeCacheKey("source", changed, "build"), key);
    }
  }
  {
    auto changed = base;
    changed.line_terminator = LineTerminator::kCrLf;
    EXPECT_NE(makeCacheKey("source", changed, "build"), key);
  }
  {
    auto changed = base;
    changed.alignment_group_boundary = AlignmentGroupBoundary::kBlankLines;
    EXPECT_NE(makeCacheKey("source", changed, "build"), key);
  }
}

}  // namespace

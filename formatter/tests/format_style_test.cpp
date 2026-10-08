#include "data/format_style.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

using format::AlignmentGroupBoundary;
using format::AlignmentPolicy;
using format::FormatStyle;
using format::IndentationPolicy;
using format::LineTerminator;

TEST(FormatStyleTest, DefaultsMatchSchema) {
  const FormatStyle style = FormatStyle::defaults();

  EXPECT_EQ(style.column_limit, 100U);
  EXPECT_EQ(style.indentation_spaces, 2U);
  EXPECT_EQ(style.wrap_spaces, 4U);
  EXPECT_EQ(style.line_break_penalty, 2U);
  EXPECT_EQ(style.over_column_limit_penalty, 100U);
  EXPECT_EQ(style.line_terminator, LineTerminator::kAuto);

  EXPECT_EQ(style.port_declarations_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.module_net_variable_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.assignment_statement_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.formal_parameters_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.named_parameter_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.named_port_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.parameter_declaration_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.case_items_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.enum_assignment_statement_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.struct_union_members_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.class_member_variable_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.distribution_items_alignment, AlignmentPolicy::kInfer);

  EXPECT_EQ(style.port_declarations_indentation, IndentationPolicy::kWrap);
  EXPECT_EQ(style.formal_parameters_indentation, IndentationPolicy::kWrap);
  EXPECT_EQ(style.named_parameter_indentation, IndentationPolicy::kWrap);
  EXPECT_EQ(style.named_port_indentation, IndentationPolicy::kWrap);

  EXPECT_EQ(style.alignment_group_boundary, AlignmentGroupBoundary::kNone);

  EXPECT_FALSE(style.port_declarations_right_align_packed_dimensions);
  EXPECT_FALSE(style.port_declarations_right_align_unpacked_dimensions);
  EXPECT_TRUE(style.compact_indexing_and_selections);
  EXPECT_FALSE(style.class_parameter_space);
  EXPECT_FALSE(style.expand_coverpoints);
  EXPECT_FALSE(style.try_wrap_long_lines);
  EXPECT_FALSE(style.wrap_end_else_clauses);
}

TEST(FormatStyleTest, LineTerminatorFromString) {
  EXPECT_EQ(format::lineTerminatorFromString("auto"), LineTerminator::kAuto);
  EXPECT_EQ(format::lineTerminatorFromString("lf"), LineTerminator::kLf);
  EXPECT_EQ(format::lineTerminatorFromString("crlf"), LineTerminator::kCrLf);
  EXPECT_THROW((void)format::lineTerminatorFromString("bogus"),
               std::invalid_argument);
}

TEST(FormatStyleTest, AlignmentPolicyFromString) {
  EXPECT_EQ(format::alignmentPolicyFromString("align"),
            AlignmentPolicy::kAlign);
  EXPECT_EQ(format::alignmentPolicyFromString("flush-left"),
            AlignmentPolicy::kFlushLeft);
  EXPECT_EQ(format::alignmentPolicyFromString("preserve"),
            AlignmentPolicy::kPreserve);
  EXPECT_EQ(format::alignmentPolicyFromString("infer"),
            AlignmentPolicy::kInfer);
  EXPECT_THROW((void)format::alignmentPolicyFromString("bogus"),
               std::invalid_argument);
}

TEST(FormatStyleTest, IndentationPolicyFromString) {
  EXPECT_EQ(format::indentationPolicyFromString("indent"),
            IndentationPolicy::kIndent);
  EXPECT_EQ(format::indentationPolicyFromString("wrap"),
            IndentationPolicy::kWrap);
  EXPECT_THROW((void)format::indentationPolicyFromString("bogus"),
               std::invalid_argument);
}

TEST(FormatStyleTest, AlignmentGroupBoundaryFromString) {
  EXPECT_EQ(format::alignmentGroupBoundaryFromString("none"),
            AlignmentGroupBoundary::kNone);
  EXPECT_EQ(format::alignmentGroupBoundaryFromString("blank-lines"),
            AlignmentGroupBoundary::kBlankLines);
  EXPECT_EQ(format::alignmentGroupBoundaryFromString("separator-comments"),
            AlignmentGroupBoundary::kSeparatorComments);
  EXPECT_EQ(format::alignmentGroupBoundaryFromString(
                "blank-lines-and-separator-comments"),
            AlignmentGroupBoundary::kBlankLinesAndSeparatorComments);
  EXPECT_THROW((void)format::alignmentGroupBoundaryFromString("bogus"),
               std::invalid_argument);
}

}  // namespace

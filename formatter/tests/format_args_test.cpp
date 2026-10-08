#include "cli/format_args.h"

#include <gtest/gtest.h>

#include "data/format_style.h"

namespace {
class FormatArgsTest : public ::testing::Test {
 protected:
  void SetUp() override { binder.emplace(); }

  [[nodiscard]] auto parse(const std::vector<const char*>& args) -> bool {
    if (!binder.has_value()) {
      return false;
    }

    std::vector<std::string> storage;
    storage.reserve(args.size() + 1);
    storage.emplace_back("formatter");
    for (const auto* arg : args) {
      storage.emplace_back(arg);
    }

    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (auto& s : storage) {
      argv.push_back(s.data());
    }

    try {
      binder->parse(static_cast<int>(argv.size()), argv.data());
      return true;
    } catch (const CLI::ParseError&) {
      return false;
    }
  }

  [[nodiscard]] auto buildStyle(
      const std::vector<const char*>& args = {},
      const format::FormatStyle& base = format::FormatStyle::defaults())
      -> std::pair<format::FormatStyle, format::RunConfig> {
    EXPECT_TRUE(parse(args));
    if (!binder.has_value()) {
      throw std::runtime_error("binder is not initialized");
    }
    format::FormatStyle style = base;
    binder->applyStyleOverrides(style);
    return {style, binder->buildRunConfig()};
  }

  [[nodiscard]] auto getBinder() -> format::FormatArgsBinder& {
    if (!binder.has_value()) {
      throw std::runtime_error("binder is not initialized");
    }
    return *binder;
  }

 private:
  std::optional<format::FormatArgsBinder> binder;
};

// ---------------------------------------------------------------------------
// Default values
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, DefaultsAreApplied) {
  auto [style, run] = buildStyle();

  EXPECT_EQ(style.column_limit, format::defaults::kColumnLimit);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
  EXPECT_EQ(style.wrap_spaces, format::defaults::kWrapSpaces);
  EXPECT_EQ(style.line_break_penalty, format::defaults::kLineBreakPenalty);
  EXPECT_EQ(style.over_column_limit_penalty,
            format::defaults::kOverColumnLimitPenalty);
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kAuto);
  EXPECT_FALSE(run.inplace);
}

TEST_F(FormatArgsTest, CustomColumnLimit) {
  auto [style, run] = buildStyle({"--column_limit", "120"});

  EXPECT_EQ(style.column_limit, 120U);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
  EXPECT_EQ(style.wrap_spaces, format::defaults::kWrapSpaces);
  EXPECT_EQ(style.line_break_penalty, format::defaults::kLineBreakPenalty);
  EXPECT_EQ(style.over_column_limit_penalty,
            format::defaults::kOverColumnLimitPenalty);
  EXPECT_FALSE(run.inplace);
}

TEST_F(FormatArgsTest, CustomIndentationSpaces) {
  auto [style, run] = buildStyle({"--indentation_spaces", "4"});

  EXPECT_EQ(style.indentation_spaces, 4U);
  EXPECT_EQ(style.column_limit, format::defaults::kColumnLimit);
  EXPECT_EQ(style.wrap_spaces, format::defaults::kWrapSpaces);
}

TEST_F(FormatArgsTest, CustomWrapSpaces) {
  auto [style, run] = buildStyle({"--wrap_spaces", "8"});

  EXPECT_EQ(style.wrap_spaces, 8U);
  EXPECT_EQ(style.column_limit, format::defaults::kColumnLimit);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
}

TEST_F(FormatArgsTest, CustomLineBreakPenalty) {
  auto [style, run] = buildStyle({"--line_break_penalty", "10"});

  EXPECT_EQ(style.line_break_penalty, 10U);
  EXPECT_EQ(style.over_column_limit_penalty,
            format::defaults::kOverColumnLimitPenalty);
}

TEST_F(FormatArgsTest, CustomOverColumnLimitPenalty) {
  auto [style, run] = buildStyle({"--over_column_limit_penalty", "50"});

  EXPECT_EQ(style.over_column_limit_penalty, 50U);
  EXPECT_EQ(style.line_break_penalty, format::defaults::kLineBreakPenalty);
}

// ---------------------------------------------------------------------------
// --inplace flag
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, InplaceFlagSetsRunConfig) {
  auto [style, run] = buildStyle({"--inplace"});

  EXPECT_TRUE(run.inplace);
  EXPECT_EQ(style.column_limit, format::defaults::kColumnLimit);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
}

// ---------------------------------------------------------------------------
// --line_terminator: all three valid values
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, LineTerminatorAuto) {
  auto [style, run] = buildStyle({"--line_terminator", "auto"});
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kAuto);
}

TEST_F(FormatArgsTest, LineTerminatorLf) {
  auto [style, run] = buildStyle({"--line_terminator", "lf"});
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kLf);
}

TEST_F(FormatArgsTest, LineTerminatorCrlf) {
  auto [style, run] = buildStyle({"--line_terminator", "crlf"});
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kCrLf);
}

TEST_F(FormatArgsTest, InvalidLineTerminatorRejectedByParser) {
  EXPECT_FALSE(parse({"--line_terminator", "windows"}));
}

TEST_F(FormatArgsTest, EmptyLineTerminatorRejectedByParser) {
  EXPECT_FALSE(parse({"--line_terminator", ""}));
}

// ---------------------------------------------------------------------------
// Short Aliases (Тесты для короче флагов: -c, -i, -w и т.д.)
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, ShortFlagsWork) {
  auto [style, run] = buildStyle({"-c", "80", "-i", "4", "-w", "8", "-b", "5",
                                  "-p", "200", "-t", "lf", "-n"});

  EXPECT_EQ(style.column_limit, 80U);
  EXPECT_EQ(style.indentation_spaces, 4U);
  EXPECT_EQ(style.wrap_spaces, 8U);
  EXPECT_EQ(style.line_break_penalty, 5U);
  EXPECT_EQ(style.over_column_limit_penalty, 200U);
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kLf);
  EXPECT_TRUE(run.inplace);
}

// ---------------------------------------------------------------------------
// Boundary values for numeric flags
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, ColumnLimitOfOne) {
  auto [style, run] = buildStyle({"--column_limit", "1"});
  EXPECT_EQ(style.column_limit, 1U);
}

TEST_F(FormatArgsTest, LargeColumnLimit) {
  constexpr auto kMax = std::numeric_limits<uint32_t>::max();
  auto [style, run] =
      buildStyle({"--column_limit", std::to_string(kMax).c_str()});
  EXPECT_EQ(style.column_limit, kMax);
}

TEST_F(FormatArgsTest, ZeroIndentationSpaces) {
  auto [style, run] = buildStyle({"--indentation_spaces", "0"});
  EXPECT_EQ(style.indentation_spaces, 0U);
}

TEST_F(FormatArgsTest, ColumnLimitOfZeroAcceptedByParser) {
  auto [style, run] = buildStyle({"--column_limit", "0"});
  EXPECT_EQ(style.column_limit, 0U);
}

// ---------------------------------------------------------------------------
// Invalid input and unknown flags
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, NonNumericColumnLimitRejectedByParser) {
  EXPECT_FALSE(parse({"--column_limit", "abc"}));
}

TEST_F(FormatArgsTest, NegativeColumnLimitRejectedByParser) {
  EXPECT_FALSE(parse({"--column_limit", "-1"}));
}

// ---------------------------------------------------------------------------
// --config and merging CLI options over a base style
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, ConfigPathIsCollected) {
  (void)buildStyle({"--config", "my-config.yaml"});

  EXPECT_EQ(getBinder().configPath().value_or("<unset>"), "my-config.yaml");
}

TEST_F(FormatArgsTest, NoConfigPathByDefault) {
  (void)buildStyle();

  EXPECT_FALSE(getBinder().configPath().has_value());
}

TEST_F(FormatArgsTest, CliOverridesBaseStyle) {
  constexpr format::ColumnNumber kBaseColumnLimit = 120;
  constexpr format::IndentLevel kBaseWrapSpaces = 8;

  format::FormatStyle base = format::FormatStyle::defaults();
  base.column_limit = kBaseColumnLimit;
  base.wrap_spaces = kBaseWrapSpaces;
  base.port_declarations_alignment = format::AlignmentPolicy::kFlushLeft;

  auto [style, run] = buildStyle({"--column_limit", "140"}, base);

  EXPECT_EQ(style.column_limit, 140U);
  EXPECT_EQ(style.wrap_spaces, kBaseWrapSpaces);
  EXPECT_EQ(style.port_declarations_alignment,
            format::AlignmentPolicy::kFlushLeft);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
  EXPECT_FALSE(run.inplace);
}

TEST_F(FormatArgsTest, CliLineTerminatorOverridesBaseStyle) {
  format::FormatStyle base = format::FormatStyle::defaults();
  base.line_terminator = format::LineTerminator::kCrLf;

  auto [style, run] = buildStyle({"--line_terminator", "lf"}, base);

  EXPECT_EQ(style.line_terminator, format::LineTerminator::kLf);
}

// ---------------------------------------------------------------------------
// Alignment policy options (mirror the YAML configuration keys)
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, AlignmentPolicyOptionsAreApplied) {
  auto [style, run] = buildStyle({
      "--port_declarations_alignment",
      "flush-left",
      "--module_net_variable_alignment",
      "align",
      "--assignment_statement_alignment",
      "preserve",
      "--formal_parameters_alignment",
      "align",
      "--named_parameter_alignment",
      "flush-left",
      "--named_port_alignment",
      "preserve",
      "--parameter_declaration_alignment",
      "align",
      "--case_items_alignment",
      "flush-left",
      "--enum_assignment_statement_alignment",
      "align",
      "--struct_union_members_alignment",
      "preserve",
      "--class_member_variable_alignment",
      "align",
      "--distribution_items_alignment",
      "flush-left",
  });

  EXPECT_EQ(style.port_declarations_alignment,
            format::AlignmentPolicy::kFlushLeft);
  EXPECT_EQ(style.module_net_variable_alignment,
            format::AlignmentPolicy::kAlign);
  EXPECT_EQ(style.assignment_statement_alignment,
            format::AlignmentPolicy::kPreserve);
  EXPECT_EQ(style.formal_parameters_alignment, format::AlignmentPolicy::kAlign);
  EXPECT_EQ(style.named_parameter_alignment,
            format::AlignmentPolicy::kFlushLeft);
  EXPECT_EQ(style.named_port_alignment, format::AlignmentPolicy::kPreserve);
  EXPECT_EQ(style.parameter_declaration_alignment,
            format::AlignmentPolicy::kAlign);
  EXPECT_EQ(style.case_items_alignment, format::AlignmentPolicy::kFlushLeft);
  EXPECT_EQ(style.enum_assignment_statement_alignment,
            format::AlignmentPolicy::kAlign);
  EXPECT_EQ(style.struct_union_members_alignment,
            format::AlignmentPolicy::kPreserve);
  EXPECT_EQ(style.class_member_variable_alignment,
            format::AlignmentPolicy::kAlign);
  EXPECT_EQ(style.distribution_items_alignment,
            format::AlignmentPolicy::kFlushLeft);
}

TEST_F(FormatArgsTest, AlignmentPolicyAcceptsInfer) {
  auto [style, run] = buildStyle({"--port_declarations_alignment", "infer"});
  EXPECT_EQ(style.port_declarations_alignment, format::AlignmentPolicy::kInfer);
}

TEST_F(FormatArgsTest, InvalidAlignmentPolicyRejectedByParser) {
  EXPECT_FALSE(parse({"--port_declarations_alignment", "left"}));
}

// ---------------------------------------------------------------------------
// Indentation policy options
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, IndentationPolicyOptionsAreApplied) {
  auto [style, run] = buildStyle({
      "--port_declarations_indentation",
      "indent",
      "--formal_parameters_indentation",
      "indent",
      "--named_parameter_indentation",
      "indent",
      "--named_port_indentation",
      "indent",
  });

  EXPECT_EQ(style.port_declarations_indentation,
            format::IndentationPolicy::kIndent);
  EXPECT_EQ(style.formal_parameters_indentation,
            format::IndentationPolicy::kIndent);
  EXPECT_EQ(style.named_parameter_indentation,
            format::IndentationPolicy::kIndent);
  EXPECT_EQ(style.named_port_indentation, format::IndentationPolicy::kIndent);
}

TEST_F(FormatArgsTest, IndentationPolicyWrapIsApplied) {
  auto [style, run] = buildStyle({"--named_port_indentation", "wrap"});
  EXPECT_EQ(style.named_port_indentation, format::IndentationPolicy::kWrap);
}

TEST_F(FormatArgsTest, InvalidIndentationPolicyRejectedByParser) {
  EXPECT_FALSE(parse({"--named_port_indentation", "align"}));
}

// ---------------------------------------------------------------------------
// alignment_group_boundary
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, AlignmentGroupBoundaryIsApplied) {
  auto [style, run] = buildStyle(
      {"--alignment_group_boundary", "blank-lines-and-separator-comments"});
  EXPECT_EQ(style.alignment_group_boundary,
            format::AlignmentGroupBoundary::kBlankLinesAndSeparatorComments);
}

TEST_F(FormatArgsTest, InvalidAlignmentGroupBoundaryRejectedByParser) {
  EXPECT_FALSE(parse({"--alignment_group_boundary", "blank-lines-and"}));
}

// ---------------------------------------------------------------------------
// Boolean options: --name enables, --no_name disables
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, BooleanOptionsEnable) {
  auto [style, run] = buildStyle({
      "--port_declarations_right_align_packed_dimensions",
      "--port_declarations_right_align_unpacked_dimensions",
      "--compact_indexing_and_selections",
      "--class_parameter_space",
      "--expand_coverpoints",
      "--try_wrap_long_lines",
      "--wrap_end_else_clauses",
  });

  EXPECT_TRUE(style.port_declarations_right_align_packed_dimensions);
  EXPECT_TRUE(style.port_declarations_right_align_unpacked_dimensions);
  EXPECT_TRUE(style.compact_indexing_and_selections);
  EXPECT_TRUE(style.class_parameter_space);
  EXPECT_TRUE(style.expand_coverpoints);
  EXPECT_TRUE(style.try_wrap_long_lines);
  EXPECT_TRUE(style.wrap_end_else_clauses);
}

TEST_F(FormatArgsTest, BooleanOptionsNegated) {
  format::FormatStyle base = format::FormatStyle::defaults();
  base.compact_indexing_and_selections = true;
  base.expand_coverpoints = true;

  auto [style, run] = buildStyle(
      {
          "--no_port_declarations_right_align_packed_dimensions",
          "--no_port_declarations_right_align_unpacked_dimensions",
          "--no_compact_indexing_and_selections",
          "--no_class_parameter_space",
          "--no_expand_coverpoints",
          "--no_try_wrap_long_lines",
          "--no_wrap_end_else_clauses",
      },
      base);

  EXPECT_FALSE(style.port_declarations_right_align_packed_dimensions);
  EXPECT_FALSE(style.port_declarations_right_align_unpacked_dimensions);
  EXPECT_FALSE(style.compact_indexing_and_selections);
  EXPECT_FALSE(style.class_parameter_space);
  EXPECT_FALSE(style.expand_coverpoints);
  EXPECT_FALSE(style.try_wrap_long_lines);
  EXPECT_FALSE(style.wrap_end_else_clauses);
}

// ---------------------------------------------------------------------------
// Unspecified options keep the value from the base style
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, UnspecifiedOptionsKeepBaseStyle) {
  format::FormatStyle base = format::FormatStyle::defaults();
  base.named_port_alignment = format::AlignmentPolicy::kPreserve;
  base.named_port_indentation = format::IndentationPolicy::kIndent;
  base.expand_coverpoints = true;

  auto [style, run] = buildStyle({"--column_limit", "80"}, base);

  EXPECT_EQ(style.named_port_alignment, format::AlignmentPolicy::kPreserve);
  EXPECT_EQ(style.named_port_indentation, format::IndentationPolicy::kIndent);
  EXPECT_TRUE(style.expand_coverpoints);
}

}  // namespace

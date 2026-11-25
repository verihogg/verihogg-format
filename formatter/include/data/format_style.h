#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace format {

using ColumnNumber = size_t;
using IndentLevel = size_t;

enum class LineTerminator : uint8_t {
  kAuto,  // detect from source
  kLf,    // \n
  kCrLf,  // \r\n
};

[[nodiscard]] inline auto lineTerminatorFromString(std::string_view s)
    -> LineTerminator {
  if (s == "lf") {
    return LineTerminator::kLf;
  }
  if (s == "crlf") {
    return LineTerminator::kCrLf;
  }
  if (s == "auto") {
    return LineTerminator::kAuto;
  }
  throw std::invalid_argument(
      std::string("Unknown --line_terminator: ").append(s));
}

enum class AlignmentPolicy : uint8_t {
  kAlign,
  kFlushLeft,
  kPreserve,
  kInfer,
};

[[nodiscard]] inline auto alignmentPolicyFromString(std::string_view s)
    -> AlignmentPolicy {
  if (s == "align") {
    return AlignmentPolicy::kAlign;
  }
  if (s == "flush-left") {
    return AlignmentPolicy::kFlushLeft;
  }
  if (s == "preserve") {
    return AlignmentPolicy::kPreserve;
  }
  if (s == "infer") {
    return AlignmentPolicy::kInfer;
  }
  throw std::invalid_argument(
      std::string("Unknown alignment policy: ").append(s));
}

enum class IndentationPolicy : uint8_t {
  kIndent,
  kWrap,
};

[[nodiscard]] inline auto indentationPolicyFromString(std::string_view s)
    -> IndentationPolicy {
  if (s == "indent") {
    return IndentationPolicy::kIndent;
  }
  if (s == "wrap") {
    return IndentationPolicy::kWrap;
  }
  throw std::invalid_argument(
      std::string("Unknown indentation policy: ").append(s));
}

enum class AlignmentGroupBoundary : uint8_t {
  kNone,
  kBlankLines,
  kSeparatorComments,
  kBlankLinesAndSeparatorComments,
};

[[nodiscard]] inline auto alignmentGroupBoundaryFromString(std::string_view s)
    -> AlignmentGroupBoundary {
  if (s == "none") {
    return AlignmentGroupBoundary::kNone;
  }
  if (s == "blank-lines") {
    return AlignmentGroupBoundary::kBlankLines;
  }
  if (s == "separator-comments") {
    return AlignmentGroupBoundary::kSeparatorComments;
  }
  if (s == "blank-lines-and-separator-comments") {
    return AlignmentGroupBoundary::kBlankLinesAndSeparatorComments;
  }
  throw std::invalid_argument(
      std::string("Unknown alignment group boundary: ").append(s));
}

enum class ModuleBodyIndentation : uint8_t {
  kIndent,
  kNone,
};

[[nodiscard]] inline auto moduleBodyIndentationFromString(
    std::string_view value) -> ModuleBodyIndentation {
  if (value == "indent") {
    return ModuleBodyIndentation::kIndent;
  }
  if (value == "none") {
    return ModuleBodyIndentation::kNone;
  }
  throw std::invalid_argument("Unknown module body indentation: " +
                              std::string(value));
}

enum class ListOpening : uint8_t {
  kSameLine,
  kNextLine,
  kPreserve,
};

[[nodiscard]] inline auto listOpeningFromString(std::string_view value)
    -> ListOpening {
  if (value == "same-line") {
    return ListOpening::kSameLine;
  }
  if (value == "next-line") {
    return ListOpening::kNextLine;
  }
  if (value == "preserve") {
    return ListOpening::kPreserve;
  }
  throw std::invalid_argument("Unknown list opening: " + std::string(value));
}

enum class DimensionSpacing : uint8_t {
  kCompact,
  kSpace,
  kPreserve,
};

[[nodiscard]] inline auto dimensionSpacingFromString(std::string_view value)
    -> DimensionSpacing {
  if (value == "compact") {
    return DimensionSpacing::kCompact;
  }
  if (value == "space") {
    return DimensionSpacing::kSpace;
  }
  if (value == "preserve") {
    return DimensionSpacing::kPreserve;
  }
  throw std::invalid_argument("Unknown dimension spacing: " +
                              std::string(value));
}

enum class ParenAlignment : uint8_t {
  kOpening,
  kBoth,
};

[[nodiscard]] inline auto parenAlignmentFromString(std::string_view value)
    -> ParenAlignment {
  if (value == "opening") {
    return ParenAlignment::kOpening;
  }
  if (value == "both") {
    return ParenAlignment::kBoth;
  }
  throw std::invalid_argument("Unknown parenthesis alignment: " +
                              std::string(value));
}

enum class ParenPadding : uint8_t {
  kCompact,
  kSpaced,
};

[[nodiscard]] inline auto parenPaddingFromString(std::string_view value)
    -> ParenPadding {
  if (value == "compact") {
    return ParenPadding::kCompact;
  }
  if (value == "spaced") {
    return ParenPadding::kSpaced;
  }
  throw std::invalid_argument("Unknown parenthesis padding: " +
                              std::string(value));
}

enum class ListCommaPlacement : uint8_t {
  kLeading,
  kTrailing,
  kPreserve,
};

[[nodiscard]] inline auto listCommaPlacementFromString(std::string_view value)
    -> ListCommaPlacement {
  if (value == "leading") {
    return ListCommaPlacement::kLeading;
  }
  if (value == "trailing") {
    return ListCommaPlacement::kTrailing;
  }
  if (value == "preserve") {
    return ListCommaPlacement::kPreserve;
  }
  throw std::invalid_argument("Unknown list comma placement: " +
                              std::string(value));
}

struct RunConfig {
  bool inplace = false;
  bool check = false;
  bool no_links = false;
  std::string stdin_name = "<stdin>";
  std::vector<std::string> input_files{};
};

namespace defaults {
inline constexpr ColumnNumber kColumnLimit = 100;
inline constexpr IndentLevel kIndentationSpaces = 2;
inline constexpr IndentLevel kWrapSpaces = 4;
inline constexpr size_t kLineBreakPenalty = 2;
inline constexpr size_t kOverColumnLimitPenalty = 100;
inline constexpr AlignmentPolicy kAlignmentPolicy = AlignmentPolicy::kInfer;
inline constexpr IndentationPolicy kIndentationPolicy =
    IndentationPolicy::kWrap;
inline constexpr AlignmentGroupBoundary kAlignmentGroupBoundary =
    AlignmentGroupBoundary::kNone;
inline constexpr bool kPortDeclarationsRightAlignPackedDimensions = false;
inline constexpr bool kPortDeclarationsRightAlignUnpackedDimensions = false;
inline constexpr bool kCompactIndexingAndSelections = true;
inline constexpr bool kClassParameterSpace = false;
inline constexpr bool kExpandCoverpoints = false;
inline constexpr bool kTryWrapLongLines = false;
inline constexpr bool kWrapEndElseClauses = false;
inline constexpr bool kBreakAfterParameterOverride = false;
}  // namespace defaults

struct FormatStyle {
  ColumnNumber column_limit = defaults::kColumnLimit;
  IndentLevel indentation_spaces = defaults::kIndentationSpaces;
  IndentLevel wrap_spaces = defaults::kWrapSpaces;
  size_t line_break_penalty = defaults::kLineBreakPenalty;
  size_t over_column_limit_penalty = defaults::kOverColumnLimitPenalty;
  LineTerminator line_terminator = LineTerminator::kAuto;

  AlignmentPolicy port_declarations_alignment = defaults::kAlignmentPolicy;
  AlignmentPolicy module_net_variable_alignment = defaults::kAlignmentPolicy;
  AlignmentPolicy assignment_statement_alignment = defaults::kAlignmentPolicy;
  AlignmentPolicy formal_parameters_alignment = defaults::kAlignmentPolicy;
  AlignmentPolicy named_parameter_alignment = defaults::kAlignmentPolicy;
  AlignmentPolicy named_port_alignment = defaults::kAlignmentPolicy;
  ParenAlignment named_port_paren_alignment = ParenAlignment::kOpening;
  ParenAlignment named_parameter_paren_alignment = ParenAlignment::kOpening;
  AlignmentPolicy parameter_declaration_alignment = defaults::kAlignmentPolicy;
  AlignmentPolicy case_items_alignment = defaults::kAlignmentPolicy;
  AlignmentPolicy enum_assignment_statement_alignment =
      defaults::kAlignmentPolicy;
  AlignmentPolicy struct_union_members_alignment = defaults::kAlignmentPolicy;
  AlignmentPolicy class_member_variable_alignment = defaults::kAlignmentPolicy;
  AlignmentPolicy distribution_items_alignment = defaults::kAlignmentPolicy;

  ModuleBodyIndentation module_body_indentation =
      ModuleBodyIndentation::kIndent;
  IndentationPolicy port_declarations_indentation =
      defaults::kIndentationPolicy;
  IndentationPolicy formal_parameters_indentation =
      defaults::kIndentationPolicy;
  IndentationPolicy named_parameter_indentation = defaults::kIndentationPolicy;
  IndentationPolicy named_port_indentation = defaults::kIndentationPolicy;

  ListOpening module_parameter_list_opening = ListOpening::kSameLine;
  ListOpening module_port_list_opening = ListOpening::kSameLine;
  ListCommaPlacement port_list_comma_placement = ListCommaPlacement::kTrailing;
  ListCommaPlacement parameter_list_comma_placement =
      ListCommaPlacement::kTrailing;
  bool break_after_parameter_override = defaults::kBreakAfterParameterOverride;
  AlignmentGroupBoundary alignment_group_boundary =
      defaults::kAlignmentGroupBoundary;

  bool port_declarations_right_align_packed_dimensions =
      defaults::kPortDeclarationsRightAlignPackedDimensions;
  bool port_declarations_right_align_unpacked_dimensions =
      defaults::kPortDeclarationsRightAlignUnpackedDimensions;
  DimensionSpacing packed_dimension_separator = DimensionSpacing::kCompact;
  DimensionSpacing spacing_before_unpacked_dimensions =
      DimensionSpacing::kCompact;
  ParenPadding named_port_paren_padding = ParenPadding::kCompact;
  ParenPadding named_parameter_paren_padding = ParenPadding::kCompact;
  bool compact_indexing_and_selections =
      defaults::kCompactIndexingAndSelections;
  bool class_parameter_space = defaults::kClassParameterSpace;
  bool expand_coverpoints = defaults::kExpandCoverpoints;
  bool try_wrap_long_lines = defaults::kTryWrapLongLines;
  bool wrap_end_else_clauses = defaults::kWrapEndElseClauses;

  [[nodiscard]] static auto defaults() noexcept -> FormatStyle { return {}; }
};

}  // namespace format

#include "cli/format_args.h"

#include <fmt/format.h>
#include <slang/driver/Driver.h>

#include <array>
#include <span>
#include <string>

#include "data/format_style.h"

namespace format {

template <typename T>
void add_aliased(CLI::App& app, std::string_view lng, std::string_view sht,
                 std::optional<T>& val, std::string_view desc) {
  app.add_option(fmt::format("{},{}", sht, lng), val, std::string(desc));
}

namespace {

// Allowed values shared by the enum-valued options. These mirror the enums in
// schemas/verihogg-format.schema.json.
constexpr std::array<std::string_view, 4> kAlignmentPolicyValues = {
    "align", "flush-left", "preserve", "infer"};
constexpr std::array<std::string_view, 2> kIndentationPolicyValues = {"indent",
                                                                      "wrap"};
constexpr std::array<std::string_view, 4> kAlignmentGroupBoundaryValues = {
    "none", "blank-lines", "separator-comments",
    "blank-lines-and-separator-comments"};
constexpr std::array<std::string_view, 2> kModuleBodyIndentationValues = {
    "indent", "none"};
constexpr std::array<std::string_view, 3> kListOpeningValues = {
    "same-line", "next-line", "preserve"};
constexpr std::array<std::string_view, 3> kDimensionSpacingValues = {
    "compact", "space", "preserve"};
constexpr std::array<std::string_view, 2> kParenAlignmentValues = {"opening",
                                                                   "both"};
constexpr std::array<std::string_view, 2> kParenPaddingValues = {"compact",
                                                                 "spaced"};
constexpr std::array<std::string_view, 3> kListCommaPlacementValues = {
    "leading", "trailing", "preserve"};

// Describes a long-only option constrained to a fixed set of values.
struct ChoiceOption {
  std::string_view name{};
  std::string_view description{};
  std::span<const std::string_view> values{};
};

// Registers a long-only option constrained to a fixed set of values.
void add_choice(CLI::App& app, std::optional<std::string>& val,
                const ChoiceOption& option) {
  app.add_option(std::string(option.name), val, std::string(option.description))
      ->check(CLI::IsMember(option.values));
}

// Registers a boolean option together with its "--no_<name>" counterpart so
// that a value coming from a configuration file can be turned off again.
void add_negatable_flag(CLI::App& app, std::string_view lng,
                        std::optional<bool>& val, std::string_view desc) {
  app.add_flag(fmt::format("{},!--no_{}", lng, lng.substr(2)), val,
               std::string(desc));
}

}  // namespace

void FormatArgsBinder::printFormatterHelp() const {
  fmt::print(R"(
Usage: formatter [options] <files>

Formatting options:
  -c, --column_limit <N>               Maximum line length (default: 100)
  -i, --indentation_spaces <N>         Spaces per indentation level (default: 2)
  -w, --wrap_spaces <N>                Additional indentation when wrapping (default: 4)
  -b, --line_break_penalty <N>         Penalty for each line break (default: 2)
  -p, --over_column_limit_penalty <N>  Penalty per character over limit (default: 100)
  -t, --line_terminator <mode>         auto | lf | crlf (default: auto; auto currently emits LF)
      --config <path>                  Load configuration from a YAML file
  -n, --inplace                        Overwrite source files instead of stdout
      --check                          Check formatting without rewriting files
      --no-links                       Omit GitHub issue links in the summary

The style options below are reserved and do not affect formatting yet.
Line-wrap search currently always runs, regardless of --try_wrap_long_lines.

Alignment options:
  Policies (align | flush-left | preserve | infer, default: infer):
      --port_declarations_alignment
      --module_net_variable_alignment
      --assignment_statement_alignment
      --formal_parameters_alignment
      --named_parameter_alignment
      --named_port_alignment
      --parameter_declaration_alignment
      --case_items_alignment
      --enum_assignment_statement_alignment
      --struct_union_members_alignment
      --class_member_variable_alignment
      --distribution_items_alignment

  Named connection parentheses (opening | both, default: opening):
      --named_port_paren_alignment
      --named_parameter_paren_alignment

      --alignment_group_boundary <rule>  none | blank-lines | separator-comments |
                                         blank-lines-and-separator-comments (default: none)

Indentation options:
      --module_body_indentation <mode>  indent | none (default: indent)

  List indentation (indent | wrap, default: wrap):
      --port_declarations_indentation
      --formal_parameters_indentation
      --named_parameter_indentation
      --named_port_indentation

List layout options:
  Opening placement (same-line | next-line | preserve, default: same-line):
      --module_parameter_list_opening
      --module_port_list_opening

  Comma placement (leading | trailing | preserve, default: trailing):
      --port_list_comma_placement
      --parameter_list_comma_placement

  Break after the closing ) of an instance parameter override list (default: off):
      --break_after_parameter_override
      --no_break_after_parameter_override

Spacing options:
  Dimension spacing (compact | space | preserve, default: compact):
      --packed_dimension_separator
      --spacing_before_unpacked_dimensions

  Named connection parentheses (compact | spaced, default: compact):
      --named_port_paren_padding
      --named_parameter_paren_padding

  Boolean spacing options (use --no_<name> to disable):
      --compact_indexing_and_selections   (default: true)
      --class_parameter_space

Boolean options (use --no_<name> to disable):
      --port_declarations_right_align_packed_dimensions
      --port_declarations_right_align_unpacked_dimensions
      --expand_coverpoints
      --try_wrap_long_lines
      --wrap_end_else_clauses

Every option above mirrors a key of the YAML configuration file.
)");
}

struct FlagsName {
  std::string_view lng{};
  std::string_view sht{};
};

FormatArgsBinder::FormatArgsBinder() {
  // Custom help — disable automatic (-h/--help are not registered
  // as a CLI11 option at all; main intercepts them before parsing).
  app_.set_help_flag();

  add_aliased(app_, "--column_limit", "-c", column_limit_,
              "Maximum line length (default: 100)");
  add_aliased(app_, "--indentation_spaces", "-i", indentation_spaces_,
              "Spaces per indentation level (default: 2)");
  add_aliased(app_, "--wrap_spaces", "-w", wrap_spaces_,
              "Additional indentation when wrapping (default: 4)");
  add_aliased(app_, "--line_break_penalty", "-b", line_break_penalty_,
              "Penalty for each line break (default: 2)");
  add_aliased(app_, "--over_column_limit_penalty", "-p",
              over_column_limit_penalty_,
              "Penalty per character over limit (default: 100)");

  app_.add_option("-t,--line_terminator", line_terminator_,
                  "End of line character: auto | lf | crlf (default: auto)")
      ->check(CLI::IsMember({"auto", "lf", "crlf"}));

  auto* inplace = app_.add_flag(
      "-n,--inplace", inplace_,
      "Overwrite the source files instead of outputting to stdout");
  app_.add_flag("--check", check_, "Check formatting without rewriting files")
      ->excludes(inplace);

  app_.add_option("--config", config_path_,
                  "Load configuration from a YAML file");

  app_.add_flag("--no-links", no_links_,
                "Do not print GitHub issue links in the end-of-run summary");

  // The options below mirror the keys of the YAML configuration file, so that
  // every configurable style property is also reachable from the command line.
  add_choice(
      app_, port_declarations_alignment_,
      {.name = "--port_declarations_alignment",
       .description = "Alignment of port direction, type, dimensions and name",
       .values = kAlignmentPolicyValues});
  add_choice(app_, module_net_variable_alignment_,
             {.name = "--module_net_variable_alignment",
              .description =
                  "Alignment of net and variable declarations inside blocks",
              .values = kAlignmentPolicyValues});
  add_choice(app_, assignment_statement_alignment_,
             {.name = "--assignment_statement_alignment",
              .description = "Alignment of assignment statements",
              .values = kAlignmentPolicyValues});
  add_choice(
      app_, formal_parameters_alignment_,
      {.name = "--formal_parameters_alignment",
       .description =
           "Alignment of formal parameters in module, interface and class "
           "headers",
       .values = kAlignmentPolicyValues});
  add_choice(app_, named_parameter_alignment_,
             {.name = "--named_parameter_alignment",
              .description = "Alignment of named parameters in instantiations",
              .values = kAlignmentPolicyValues});
  add_choice(app_, named_port_alignment_,
             {.name = "--named_port_alignment",
              .description = "Alignment of named port connections",
              .values = kAlignmentPolicyValues});
  add_choice(app_, named_port_paren_alignment_,
             {.name = "--named_port_paren_alignment",
              .description = "Align only opening parentheses or both "
                             "parentheses when named port alignment is enabled",
              .values = kParenAlignmentValues});
  add_choice(
      app_, named_parameter_paren_alignment_,
      {.name = "--named_parameter_paren_alignment",
       .description = "Align only opening parentheses or both parentheses when "
                      "named parameter alignment is enabled",
       .values = kParenAlignmentValues});
  add_choice(
      app_, parameter_declaration_alignment_,
      {.name = "--parameter_declaration_alignment",
       .description =
           "Alignment of parameter and localparam declarations in block bodies",
       .values = kAlignmentPolicyValues});
  add_choice(app_, case_items_alignment_,
             {.name = "--case_items_alignment",
              .description = "Alignment of case item labels",
              .values = kAlignmentPolicyValues});
  add_choice(app_, enum_assignment_statement_alignment_,
             {.name = "--enum_assignment_statement_alignment",
              .description = "Alignment of enum elements with assignments",
              .values = kAlignmentPolicyValues});
  add_choice(app_, struct_union_members_alignment_,
             {.name = "--struct_union_members_alignment",
              .description = "Alignment of struct and union members",
              .values = kAlignmentPolicyValues});
  add_choice(app_, class_member_variable_alignment_,
             {.name = "--class_member_variable_alignment",
              .description = "Alignment of class member variables",
              .values = kAlignmentPolicyValues});
  add_choice(app_, distribution_items_alignment_,
             {.name = "--distribution_items_alignment",
              .description = "Alignment of distribution items",
              .values = kAlignmentPolicyValues});

  add_choice(
      app_, module_body_indentation_,
      {.name = "--module_body_indentation",
       .description = "Additional indentation level for the body of a module",
       .values = kModuleBodyIndentationValues});
  add_choice(
      app_, port_declarations_indentation_,
      {.name = "--port_declarations_indentation",
       .description = "Indentation of ports in a module or interface header",
       .values = kIndentationPolicyValues});
  add_choice(app_, formal_parameters_indentation_,
             {.name = "--formal_parameters_indentation",
              .description = "Indentation of formal parameters in a header",
              .values = kIndentationPolicyValues});
  add_choice(
      app_, named_parameter_indentation_,
      {.name = "--named_parameter_indentation",
       .description = "Indentation of named parameters in an instantiation",
       .values = kIndentationPolicyValues});
  add_choice(app_, named_port_indentation_,
             {.name = "--named_port_indentation",
              .description = "Indentation of named ports in an instantiation",
              .values = kIndentationPolicyValues});

  add_choice(app_, module_parameter_list_opening_,
             {.name = "--module_parameter_list_opening",
              .description = "Placement of the opening #( of a module "
                             "parameter list relative to the module name",
              .values = kListOpeningValues});
  add_choice(
      app_, module_port_list_opening_,
      {.name = "--module_port_list_opening",
       .description = "Placement of the opening ( of a module port list "
                      "relative to the preceding part of the module header",
       .values = kListOpeningValues});
  add_choice(app_, port_list_comma_placement_,
             {.name = "--port_list_comma_placement",
              .description = "Placement of an existing comma relative to a "
                             "line break in a port list",
              .values = kListCommaPlacementValues});
  add_choice(app_, parameter_list_comma_placement_,
             {.name = "--parameter_list_comma_placement",
              .description = "Placement of an existing comma relative to a "
                             "line break in a parameter list",
              .values = kListCommaPlacementValues});
  add_negatable_flag(app_, "--break_after_parameter_override",
                     break_after_parameter_override_,
                     "Break after the closing ) of an instance parameter "
                     "override list");
  add_choice(app_, alignment_group_boundary_,
             {.name = "--alignment_group_boundary",
              .description = "Rule determining where an alignment group ends",
              .values = kAlignmentGroupBoundaryValues});

  add_negatable_flag(app_, "--port_declarations_right_align_packed_dimensions",
                     port_declarations_right_align_packed_dimensions_,
                     "Right-align packed dimensions in port declarations");
  add_negatable_flag(app_,
                     "--port_declarations_right_align_unpacked_dimensions",
                     port_declarations_right_align_unpacked_dimensions_,
                     "Right-align unpacked dimensions in port declarations");
  add_choice(
      app_, packed_dimension_separator_,
      {.name = "--packed_dimension_separator",
       .description =
           "Whitespace between adjacent packed dimensions in a declaration",
       .values = kDimensionSpacingValues});
  add_choice(app_, spacing_before_unpacked_dimensions_,
             {.name = "--spacing_before_unpacked_dimensions",
              .description = "Whitespace between a declared name and its first "
                             "unpacked dimension",
              .values = kDimensionSpacingValues});
  add_choice(app_, named_port_paren_padding_,
             {.name = "--named_port_paren_padding",
              .description = "Spacing at the edges of an expression inside a "
                             "named port connection",
              .values = kParenPaddingValues});
  add_choice(app_, named_parameter_paren_padding_,
             {.name = "--named_parameter_paren_padding",
              .description = "Spacing at the edges of an expression inside a "
                             "named parameter override",
              .values = kParenPaddingValues});
  add_negatable_flag(app_, "--compact_indexing_and_selections",
                     compact_indexing_and_selections_,
                     "Use compact expressions inside indexes and selections");
  add_negatable_flag(app_, "--class_parameter_space", class_parameter_space_,
                     "Insert a space before # in parameterized class typedefs");
  add_negatable_flag(app_, "--expand_coverpoints", expand_coverpoints_,
                     "Always expand coverpoints");
  add_negatable_flag(app_, "--try_wrap_long_lines", try_wrap_long_lines_,
                     "Allow optimization-based wrapping of long lines");
  add_negatable_flag(app_, "--wrap_end_else_clauses", wrap_end_else_clauses_,
                     "Place end and else clauses on separate lines");

  // Positional file arguments are collected by CLI11; -- allows filenames
  // starting with a dash.
  app_.add_option("files", files_, "Source files to format")->type_name("FILE");
}

void FormatArgsBinder::applyStyleOverrides(FormatStyle& style) const {
  if (column_limit_.has_value()) {
    style.column_limit = *column_limit_;
  }
  if (indentation_spaces_.has_value()) {
    style.indentation_spaces = *indentation_spaces_;
  }
  if (wrap_spaces_.has_value()) {
    style.wrap_spaces = *wrap_spaces_;
  }
  if (line_break_penalty_.has_value()) {
    style.line_break_penalty = *line_break_penalty_;
  }
  if (over_column_limit_penalty_.has_value()) {
    style.over_column_limit_penalty = *over_column_limit_penalty_;
  }
  if (line_terminator_.has_value()) {
    style.line_terminator = lineTerminatorFromString(*line_terminator_);
  }

  if (port_declarations_alignment_.has_value()) {
    style.port_declarations_alignment =
        alignmentPolicyFromString(*port_declarations_alignment_);
  }
  if (module_net_variable_alignment_.has_value()) {
    style.module_net_variable_alignment =
        alignmentPolicyFromString(*module_net_variable_alignment_);
  }
  if (assignment_statement_alignment_.has_value()) {
    style.assignment_statement_alignment =
        alignmentPolicyFromString(*assignment_statement_alignment_);
  }
  if (formal_parameters_alignment_.has_value()) {
    style.formal_parameters_alignment =
        alignmentPolicyFromString(*formal_parameters_alignment_);
  }
  if (named_parameter_alignment_.has_value()) {
    style.named_parameter_alignment =
        alignmentPolicyFromString(*named_parameter_alignment_);
  }
  if (named_port_alignment_.has_value()) {
    style.named_port_alignment =
        alignmentPolicyFromString(*named_port_alignment_);
  }
  if (named_port_paren_alignment_.has_value()) {
    style.named_port_paren_alignment =
        parenAlignmentFromString(*named_port_paren_alignment_);
  }
  if (named_parameter_paren_alignment_.has_value()) {
    style.named_parameter_paren_alignment =
        parenAlignmentFromString(*named_parameter_paren_alignment_);
  }
  if (parameter_declaration_alignment_.has_value()) {
    style.parameter_declaration_alignment =
        alignmentPolicyFromString(*parameter_declaration_alignment_);
  }
  if (case_items_alignment_.has_value()) {
    style.case_items_alignment =
        alignmentPolicyFromString(*case_items_alignment_);
  }
  if (enum_assignment_statement_alignment_.has_value()) {
    style.enum_assignment_statement_alignment =
        alignmentPolicyFromString(*enum_assignment_statement_alignment_);
  }
  if (struct_union_members_alignment_.has_value()) {
    style.struct_union_members_alignment =
        alignmentPolicyFromString(*struct_union_members_alignment_);
  }
  if (class_member_variable_alignment_.has_value()) {
    style.class_member_variable_alignment =
        alignmentPolicyFromString(*class_member_variable_alignment_);
  }
  if (distribution_items_alignment_.has_value()) {
    style.distribution_items_alignment =
        alignmentPolicyFromString(*distribution_items_alignment_);
  }

  if (module_body_indentation_.has_value()) {
    style.module_body_indentation =
        moduleBodyIndentationFromString(*module_body_indentation_);
  }
  if (port_declarations_indentation_.has_value()) {
    style.port_declarations_indentation =
        indentationPolicyFromString(*port_declarations_indentation_);
  }
  if (formal_parameters_indentation_.has_value()) {
    style.formal_parameters_indentation =
        indentationPolicyFromString(*formal_parameters_indentation_);
  }
  if (named_parameter_indentation_.has_value()) {
    style.named_parameter_indentation =
        indentationPolicyFromString(*named_parameter_indentation_);
  }
  if (named_port_indentation_.has_value()) {
    style.named_port_indentation =
        indentationPolicyFromString(*named_port_indentation_);
  }

  if (module_parameter_list_opening_.has_value()) {
    style.module_parameter_list_opening =
        listOpeningFromString(*module_parameter_list_opening_);
  }
  if (module_port_list_opening_.has_value()) {
    style.module_port_list_opening =
        listOpeningFromString(*module_port_list_opening_);
  }
  if (port_list_comma_placement_.has_value()) {
    style.port_list_comma_placement =
        listCommaPlacementFromString(*port_list_comma_placement_);
  }
  if (parameter_list_comma_placement_.has_value()) {
    style.parameter_list_comma_placement =
        listCommaPlacementFromString(*parameter_list_comma_placement_);
  }
  if (break_after_parameter_override_.has_value()) {
    style.break_after_parameter_override = *break_after_parameter_override_;
  }
  if (alignment_group_boundary_.has_value()) {
    style.alignment_group_boundary =
        alignmentGroupBoundaryFromString(*alignment_group_boundary_);
  }

  if (port_declarations_right_align_packed_dimensions_.has_value()) {
    style.port_declarations_right_align_packed_dimensions =
        *port_declarations_right_align_packed_dimensions_;
  }
  if (port_declarations_right_align_unpacked_dimensions_.has_value()) {
    style.port_declarations_right_align_unpacked_dimensions =
        *port_declarations_right_align_unpacked_dimensions_;
  }
  if (packed_dimension_separator_.has_value()) {
    style.packed_dimension_separator =
        dimensionSpacingFromString(*packed_dimension_separator_);
  }
  if (spacing_before_unpacked_dimensions_.has_value()) {
    style.spacing_before_unpacked_dimensions =
        dimensionSpacingFromString(*spacing_before_unpacked_dimensions_);
  }
  if (named_port_paren_padding_.has_value()) {
    style.named_port_paren_padding =
        parenPaddingFromString(*named_port_paren_padding_);
  }
  if (named_parameter_paren_padding_.has_value()) {
    style.named_parameter_paren_padding =
        parenPaddingFromString(*named_parameter_paren_padding_);
  }
  if (compact_indexing_and_selections_.has_value()) {
    style.compact_indexing_and_selections = *compact_indexing_and_selections_;
  }
  if (class_parameter_space_.has_value()) {
    style.class_parameter_space = *class_parameter_space_;
  }
  if (expand_coverpoints_.has_value()) {
    style.expand_coverpoints = *expand_coverpoints_;
  }
  if (try_wrap_long_lines_.has_value()) {
    style.try_wrap_long_lines = *try_wrap_long_lines_;
  }
  if (wrap_end_else_clauses_.has_value()) {
    style.wrap_end_else_clauses = *wrap_end_else_clauses_;
  }
}

auto FormatArgsBinder::buildRunConfig() const -> RunConfig {
  RunConfig run;
  if (inplace_.has_value()) {
    run.inplace = *inplace_;
  }
  if (check_.has_value()) {
    run.check = *check_;
  }
  run.no_links = no_links_;
  return run;
}

void FormatArgsBinder::parse(int argc, char** argv) { app_.parse(argc, argv); }

}  // namespace format

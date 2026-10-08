#include "cli/format_args.h"

#include <slang/driver/Driver.h>

#include <array>
#include <span>
#include <string>
#include <utility>

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
  -t, --line_terminator <mode>         auto | lf | crlf (default: auto)
      --config <path>                  Load configuration from a YAML file
  -n, --inplace                        Overwrite source files instead of stdout

Alignment options (align | flush-left | preserve | infer, default: infer):
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

Indentation options (indent | wrap, default: wrap):
      --port_declarations_indentation
      --formal_parameters_indentation
      --named_parameter_indentation
      --named_port_indentation

Alignment grouping:
      --alignment_group_boundary <rule>  none | blank-lines | separator-comments |
                                         blank-lines-and-separator-comments (default: none)

Boolean options (use --no_<name> to disable):
      --port_declarations_right_align_packed_dimensions
      --port_declarations_right_align_unpacked_dimensions
      --compact_indexing_and_selections   (default: true)
      --class_parameter_space
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

  app_.add_flag("-n,--inplace", inplace_,
                "Overwrite the source files instead of outputting to stdout");

  app_.add_option("--config", config_path_,
                  "Load configuration from a YAML file");

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

  // Positional "files". Tokens not starting with '-' are placed here by
  // CLI11 itself — before attempts to match them with options, so there's
  // no need to manually classify "file or unknown flag".
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
  return run;
}

void FormatArgsBinder::parse(int argc, char** argv) { app_.parse(argc, argv); }

}  // namespace format

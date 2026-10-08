#pragma once

#include <slang/driver/Driver.h>

#include <CLI/CLI.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "data/format_style.h"

namespace format {

class FormatArgsBinder {
 public:
  FormatArgsBinder();

  void printFormatterHelp() const;

  // Parses argc/argv. Unknown flags are not an error; for each one, a warning
  // is written to err and parsing continues. An error in a known flag
  // (e.g., --column_limit=abc) throws CLI::ParseError.
  void parse(int argc, char** argv);

  // Applies explicitly set command-line options on top of a style. Options
  // that were not passed on the command line keep the value from base.
  void applyStyleOverrides(FormatStyle& style) const;

  [[nodiscard]] auto buildRunConfig() const -> RunConfig;

  // Path passed via --config, if any.
  [[nodiscard]] auto configPath() const -> const std::optional<std::string>& {
    return config_path_;
  }

  // Anything that does not look like a flag is collected here by CLI11 itself.
  [[nodiscard]] auto files() const -> const std::vector<std::string>& {
    return files_;
  }

  // Getters for the underlying CLI::App instance
  [[nodiscard]] auto app() -> CLI::App& { return app_; }
  [[nodiscard]] auto app() const -> const CLI::App& { return app_; }

 private:
  CLI::App app_{"formatter"};

  std::vector<std::string> files_{};

  std::optional<uint32_t> column_limit_;
  std::optional<uint32_t> indentation_spaces_;
  std::optional<uint32_t> wrap_spaces_;
  std::optional<uint32_t> line_break_penalty_;
  std::optional<uint32_t> over_column_limit_penalty_;
  std::optional<std::string> line_terminator_;

  std::optional<std::string> port_declarations_alignment_;
  std::optional<std::string> module_net_variable_alignment_;
  std::optional<std::string> assignment_statement_alignment_;
  std::optional<std::string> formal_parameters_alignment_;
  std::optional<std::string> named_parameter_alignment_;
  std::optional<std::string> named_port_alignment_;
  std::optional<std::string> parameter_declaration_alignment_;
  std::optional<std::string> case_items_alignment_;
  std::optional<std::string> enum_assignment_statement_alignment_;
  std::optional<std::string> struct_union_members_alignment_;
  std::optional<std::string> class_member_variable_alignment_;
  std::optional<std::string> distribution_items_alignment_;

  std::optional<std::string> port_declarations_indentation_;
  std::optional<std::string> formal_parameters_indentation_;
  std::optional<std::string> named_parameter_indentation_;
  std::optional<std::string> named_port_indentation_;

  std::optional<std::string> alignment_group_boundary_;

  std::optional<bool> port_declarations_right_align_packed_dimensions_;
  std::optional<bool> port_declarations_right_align_unpacked_dimensions_;
  std::optional<bool> compact_indexing_and_selections_;
  std::optional<bool> class_parameter_space_;
  std::optional<bool> expand_coverpoints_;
  std::optional<bool> try_wrap_long_lines_;
  std::optional<bool> wrap_end_else_clauses_;

  std::optional<bool> inplace_;
  std::optional<std::string> config_path_;
};

}  // namespace format

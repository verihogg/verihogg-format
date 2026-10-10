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

  auto buildStyle() -> std::pair<FormatStyle, RunConfig>;

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
  std::optional<bool> inplace_;
};

}  // namespace format

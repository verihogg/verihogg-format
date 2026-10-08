#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string_view>

#include "data/format_style.h"

namespace format::config {

// Returns the style preset for a named profile. The "lowrisc" and "scr1"
// profiles are placeholders that currently match "default".
[[nodiscard]] auto styleForProfile(std::string_view profile) -> FormatStyle;

// Loads a YAML configuration file, validates it against the embedded JSON
// schema and returns the style it describes. Throws std::runtime_error with
// a "path:line:column: message" description on syntax and validation errors.
[[nodiscard]] auto loadConfigFile(const std::filesystem::path& path)
    -> FormatStyle;

// Resolves the style for individual source files: either from an explicit
// configuration file (--config) or from the nearest
// ".verihogg-format.yaml"/".verihogg-format.yml" found upwards from the
// file's directory.
class ConfigResolver {
 public:
  explicit ConfigResolver(std::optional<std::filesystem::path> explicit_config);

  // Style for a source file. Search starts in the file's directory.
  [[nodiscard]] auto resolve(const std::filesystem::path& source_file)
      -> FormatStyle;

  // Style for stdin input. Search starts in the current working directory.
  [[nodiscard]] auto resolveForStdin() -> FormatStyle;

 private:
  [[nodiscard]] auto resolveFrom(const std::filesystem::path& directory)
      -> FormatStyle;
  [[nodiscard]] auto findConfig(const std::filesystem::path& directory)
      -> std::optional<std::filesystem::path>;
  [[nodiscard]] auto loadCached(const std::filesystem::path& path)
      -> FormatStyle;

  std::optional<std::filesystem::path> explicit_config_;
  std::map<std::filesystem::path, std::optional<std::filesystem::path>>
      search_cache_;
  std::map<std::filesystem::path, FormatStyle> file_cache_;
};

}  // namespace format::config

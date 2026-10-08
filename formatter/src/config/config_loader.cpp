#include "config/config_loader.h"

#include <fmt/ranges.h>
#include <yaml-cpp/yaml.h>

#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <valijson/adapters/yaml_cpp_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>
#include <vector>

#include "config/schema_embedded.h"

namespace format::config {

namespace {

namespace fs = std::filesystem;
using valijson::adapters::YamlCppAdapter;

constexpr std::array<std::string_view, 2> kConfigFileNames = {
    ".verihogg-format.yaml",
    ".verihogg-format.yml",
};

[[nodiscard]] auto errorAt(const fs::path& path, const YAML::Mark& mark,
                           std::string_view message) -> std::string {
  return path.string() + ":" + std::to_string(mark.line + 1) + ":" +
         std::to_string(mark.column + 1) + ": " + std::string(message);
}

[[nodiscard]] auto embeddedSchema() -> const valijson::Schema& {
  static const valijson::Schema schema = [] {
    valijson::Schema result;
    const auto node = YAML::Load(std::string(embeddedSchemaJson()));
    valijson::SchemaParser parser(valijson::SchemaParser::kDraft7);
    parser.populateSchema(YamlCppAdapter(node), result);
    return result;
  }();
  return schema;
}

// Resolves an RFC 6901 JSON pointer within the YAML document.
[[nodiscard]] auto unescapePointerSegment(std::string_view raw) -> std::string {
  std::string segment;
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw.at(i) == '~' && i + 1 < raw.size()) {
      if (raw.at(i + 1) == '0') {
        segment += '~';
        ++i;
        continue;
      }
      if (raw.at(i + 1) == '1') {
        segment += '/';
        ++i;
        continue;
      }
    }
    segment += raw.at(i);
  }
  return segment;
}

[[nodiscard]] auto lastPointerSegment(std::string_view pointer) -> std::string {
  if (pointer.empty()) {
    return {};
  }
  const size_t slash = pointer.rfind('/');
  return unescapePointerSegment(
      pointer.substr(slash == std::string_view::npos ? 0 : slash + 1));
}

[[nodiscard]] auto resolvePointer(const YAML::Node& root,
                                  std::string_view pointer) -> YAML::Node {
  if (pointer.empty()) {
    return root;
  }
  if (pointer.front() != '/') {
    return {};
  }
  auto node = root;
  size_t pos = 1;
  while (pos <= pointer.size()) {
    const size_t slash = pointer.find('/', pos);
    const auto raw = pointer.substr(pos, slash == std::string_view::npos
                                             ? std::string_view::npos
                                             : slash - pos);
    const auto segment = unescapePointerSegment(raw);
    if (node.IsSequence()) {
      try {
        node = node[std::stoul(segment)];
      } catch (const std::exception&) {
        return {};
      }
    } else if (node.IsMap()) {
      node = node[segment];
    } else {
      return {};
    }
    if (!node) {
      return {};
    }
    if (slash == std::string_view::npos) {
      break;
    }
    pos = slash + 1;
  }
  return node;
}

[[nodiscard]] auto unknownKeyFromMessage(std::string_view message)
    -> std::optional<std::string> {
  constexpr std::string_view kMarker = "additionalProperties' constraints: '";
  const size_t pos = message.find(kMarker);
  if (pos == std::string_view::npos) {
    return std::nullopt;
  }
  const size_t start = pos + kMarker.size();
  const size_t end = message.find('\'', start);
  if (end == std::string_view::npos) {
    return std::nullopt;
  }
  return std::string(message.substr(start, end - start));
}

auto validateDocument(const YAML::Node& doc, const fs::path& path) -> void {
  valijson::Validator validator(valijson::Validator::kWeakTypes);
  valijson::ValidationResults results;
  if (validator.validate(embeddedSchema(), YamlCppAdapter(doc), &results)) {
    return;
  }

  std::vector<std::string> messages;
  valijson::ValidationResults::Error error;
  while (results.popError(error)) {
    // Wrapper errors add no location of their own: the leaf error below
    // already describes the actual problem.
    if (error.description.starts_with("Failed to validate against schema")) {
      continue;
    }
    if (const auto key = unknownKeyFromMessage(error.description)) {
      YAML::Node node;
      for (const auto& entry : doc) {
        if (entry.first.as<std::string>() == *key) {
          node = entry.first;
          break;
        }
      }
      messages.push_back(errorAt(path, node ? node.Mark() : doc.Mark(),
                                 "unknown option '" + *key + "'"));
      continue;
    }
    const auto node = resolvePointer(doc, error.jsonPointer);
    const std::string option = lastPointerSegment(error.jsonPointer);
    auto message = option.empty() ? std::string{} : option + ": ";
    message += error.description;
    messages.push_back(errorAt(path, node ? node.Mark() : doc.Mark(), message));
  }
  if (messages.empty()) {
    messages.push_back(errorAt(path, doc.Mark(), "invalid configuration"));
  }
  throw std::runtime_error(fmt::format("{}", fmt::join(messages, "\n")));
}

auto applyDocument(const YAML::Node& doc, FormatStyle& style) -> void {
  for (const auto& entry : doc) {
    const auto key = entry.first.as<std::string>();
    const auto& value = entry.second;
    if (key == "version" || key == "style" || key == "$schema") {
      continue;
    }
    if (key == "column_limit") {
      style.column_limit = value.as<ColumnNumber>();
    } else if (key == "indentation_spaces") {
      style.indentation_spaces = value.as<IndentLevel>();
    } else if (key == "wrap_spaces") {
      style.wrap_spaces = value.as<IndentLevel>();
    } else if (key == "line_break_penalty") {
      style.line_break_penalty = value.as<size_t>();
    } else if (key == "over_column_limit_penalty") {
      style.over_column_limit_penalty = value.as<size_t>();
    } else if (key == "line_terminator") {
      style.line_terminator = lineTerminatorFromString(value.as<std::string>());
    } else if (key == "port_declarations_alignment") {
      style.port_declarations_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "module_net_variable_alignment") {
      style.module_net_variable_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "assignment_statement_alignment") {
      style.assignment_statement_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "formal_parameters_alignment") {
      style.formal_parameters_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "named_parameter_alignment") {
      style.named_parameter_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "named_port_alignment") {
      style.named_port_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "parameter_declaration_alignment") {
      style.parameter_declaration_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "case_items_alignment") {
      style.case_items_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "enum_assignment_statement_alignment") {
      style.enum_assignment_statement_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "struct_union_members_alignment") {
      style.struct_union_members_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "class_member_variable_alignment") {
      style.class_member_variable_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "distribution_items_alignment") {
      style.distribution_items_alignment =
          alignmentPolicyFromString(value.as<std::string>());
    } else if (key == "port_declarations_indentation") {
      style.port_declarations_indentation =
          indentationPolicyFromString(value.as<std::string>());
    } else if (key == "formal_parameters_indentation") {
      style.formal_parameters_indentation =
          indentationPolicyFromString(value.as<std::string>());
    } else if (key == "named_parameter_indentation") {
      style.named_parameter_indentation =
          indentationPolicyFromString(value.as<std::string>());
    } else if (key == "named_port_indentation") {
      style.named_port_indentation =
          indentationPolicyFromString(value.as<std::string>());
    } else if (key == "alignment_group_boundary") {
      style.alignment_group_boundary =
          alignmentGroupBoundaryFromString(value.as<std::string>());
    } else if (key == "port_declarations_right_align_packed_dimensions") {
      style.port_declarations_right_align_packed_dimensions = value.as<bool>();
    } else if (key == "port_declarations_right_align_unpacked_dimensions") {
      style.port_declarations_right_align_unpacked_dimensions =
          value.as<bool>();
    } else if (key == "compact_indexing_and_selections") {
      style.compact_indexing_and_selections = value.as<bool>();
    } else if (key == "class_parameter_space") {
      style.class_parameter_space = value.as<bool>();
    } else if (key == "expand_coverpoints") {
      style.expand_coverpoints = value.as<bool>();
    } else if (key == "try_wrap_long_lines") {
      style.try_wrap_long_lines = value.as<bool>();
    } else if (key == "wrap_end_else_clauses") {
      style.wrap_end_else_clauses = value.as<bool>();
    } else {
      // The schema rejects unknown keys, so this indicates a mapping gap
      // between the schema and this function.
      throw std::logic_error("Unhandled config key: " + key);
    }
  }
}

}  // namespace

auto styleForProfile(std::string_view profile) -> FormatStyle {
  // "lowrisc" and "scr1" are accepted but not tuned yet: their presets
  // currently match the default style.
  if (profile == "default" || profile == "lowrisc" || profile == "scr1") {
    return FormatStyle::defaults();
  }
  throw std::invalid_argument(
      std::string("Unknown style profile: ").append(profile));
}

auto loadConfigFile(const fs::path& path) -> FormatStyle {
  if (!fs::exists(path)) {
    throw std::runtime_error("Config file not found: " + path.string());
  }
  if (!fs::is_regular_file(path)) {
    throw std::runtime_error("Config path is not a file: " + path.string());
  }

  YAML::Node doc;
  try {
    doc = YAML::LoadFile(path.string());
  } catch (const YAML::Exception& e) {
    throw std::runtime_error(errorAt(path, e.mark, e.msg));
  }

  validateDocument(doc, path);

  FormatStyle style = styleForProfile("default");
  if (doc["style"]) {
    style = styleForProfile(doc["style"].as<std::string>());
  }
  applyDocument(doc, style);
  return style;
}

ConfigResolver::ConfigResolver(std::optional<fs::path> explicit_config) {
  if (!explicit_config) {
    return;
  }
  const auto path = fs::absolute(*explicit_config);
  if (!fs::exists(path)) {
    throw std::runtime_error("Config file not found: " + path.string());
  }
  if (!fs::is_regular_file(path)) {
    throw std::runtime_error("Config path is not a file: " + path.string());
  }
  explicit_config_ = path;
}

auto ConfigResolver::resolve(const fs::path& source_file) -> FormatStyle {
  const fs::path absolute = fs::absolute(source_file);
  return resolveFrom(absolute.parent_path());
}

auto ConfigResolver::resolveForStdin() -> FormatStyle {
  return resolveFrom(fs::current_path());
}

auto ConfigResolver::resolveFrom(const fs::path& directory) -> FormatStyle {
  if (explicit_config_) {
    return loadCached(*explicit_config_);
  }
  const auto config = findConfig(directory);
  if (!config) {
    return FormatStyle::defaults();
  }
  return loadCached(*config);
}

auto ConfigResolver::findConfig(const fs::path& directory)
    -> std::optional<fs::path> {
  const auto start = fs::absolute(directory).lexically_normal();
  if (const auto it = search_cache_.find(start); it != search_cache_.end()) {
    return it->second;
  }

  std::optional<fs::path> found;
  for (auto dir = start;;) {
    for (const auto name : kConfigFileNames) {
      const auto candidate = dir / name;
      if (fs::is_regular_file(candidate)) {
        found = candidate;
        break;
      }
    }
    if (found) {
      break;
    }
    const auto parent = dir.parent_path();
    if (parent == dir) {
      break;
    }
    dir = parent;
  }

  search_cache_.emplace(start, found);
  return found;
}

auto ConfigResolver::loadCached(const fs::path& path) -> FormatStyle {
  if (const auto it = file_cache_.find(path); it != file_cache_.end()) {
    return it->second;
  }
  FormatStyle style = loadConfigFile(path);
  file_cache_.emplace(path, style);
  return style;
}

}  // namespace format::config

#include "pipeline/cache_key.h"

#include <openssl/evp.h>

#include <array>
#include <cstdint>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace format {

namespace {

// Binding every member makes adding or removing a FormatStyle field a
// compile error here until the key is updated. Bump the key format version
// when changing the encoding.
auto styleKey(const FormatStyle& style) -> std::string {
  const auto& [column_limit, indentation_spaces, wrap_spaces,
               line_break_penalty, over_column_limit_penalty, line_terminator,
               port_declarations_alignment, module_net_variable_alignment,
               assignment_statement_alignment, formal_parameters_alignment,
               named_parameter_alignment, named_port_alignment,
               parameter_declaration_alignment, case_items_alignment,
               enum_assignment_statement_alignment,
               struct_union_members_alignment, class_member_variable_alignment,
               distribution_items_alignment, port_declarations_indentation,
               formal_parameters_indentation, named_parameter_indentation,
               named_port_indentation, alignment_group_boundary,
               port_declarations_right_align_packed_dimensions,
               port_declarations_right_align_unpacked_dimensions,
               compact_indexing_and_selections, class_parameter_space,
               expand_coverpoints, try_wrap_long_lines, wrap_end_else_clauses] =
      style;
  const std::array values{
      static_cast<uint64_t>(column_limit),
      static_cast<uint64_t>(indentation_spaces),
      static_cast<uint64_t>(wrap_spaces),
      static_cast<uint64_t>(line_break_penalty),
      static_cast<uint64_t>(over_column_limit_penalty),
      static_cast<uint64_t>(line_terminator),
      static_cast<uint64_t>(port_declarations_alignment),
      static_cast<uint64_t>(module_net_variable_alignment),
      static_cast<uint64_t>(assignment_statement_alignment),
      static_cast<uint64_t>(formal_parameters_alignment),
      static_cast<uint64_t>(named_parameter_alignment),
      static_cast<uint64_t>(named_port_alignment),
      static_cast<uint64_t>(parameter_declaration_alignment),
      static_cast<uint64_t>(case_items_alignment),
      static_cast<uint64_t>(enum_assignment_statement_alignment),
      static_cast<uint64_t>(struct_union_members_alignment),
      static_cast<uint64_t>(class_member_variable_alignment),
      static_cast<uint64_t>(distribution_items_alignment),
      static_cast<uint64_t>(port_declarations_indentation),
      static_cast<uint64_t>(formal_parameters_indentation),
      static_cast<uint64_t>(named_parameter_indentation),
      static_cast<uint64_t>(named_port_indentation),
      static_cast<uint64_t>(alignment_group_boundary),
      static_cast<uint64_t>(port_declarations_right_align_packed_dimensions),
      static_cast<uint64_t>(port_declarations_right_align_unpacked_dimensions),
      static_cast<uint64_t>(compact_indexing_and_selections),
      static_cast<uint64_t>(class_parameter_space),
      static_cast<uint64_t>(expand_coverpoints),
      static_cast<uint64_t>(try_wrap_long_lines),
      static_cast<uint64_t>(wrap_end_else_clauses),
  };
  std::string result;
  for (const auto value : values) {
    result += std::to_string(value) + ":";
  }
  return result;
}

}  // namespace

auto sha256(std::string_view contents) -> std::string {
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int length = 0;
  if (EVP_Digest(contents.data(), contents.size(), digest.data(), &length,
                 EVP_sha256(), nullptr) != 1) {
    throw std::runtime_error("Cannot compute cache SHA-256 digest");
  }
  std::string result;
  for (unsigned int i = 0; i < length; ++i) {
    result += std::format("{:02x}", digest.at(i));
  }
  return result;
}

auto sha256File(const std::filesystem::path& path)
    -> std::optional<std::string> {
  std::ifstream in{path, std::ios::binary};
  if (!in) {
    return std::nullopt;
  }
  const std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(
      EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!context ||
      EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
    return std::nullopt;
  }
  constexpr size_t kBufferSize = 65536;
  std::array<char, kBufferSize> buffer{};
  while (in) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (in.gcount() > 0 &&
        EVP_DigestUpdate(context.get(), buffer.data(),
                         static_cast<size_t>(in.gcount())) != 1) {
      return std::nullopt;
    }
  }
  if (in.bad() || !in.eof()) {
    return std::nullopt;
  }
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int length = 0;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &length) != 1) {
    return std::nullopt;
  }
  std::string result;
  for (unsigned int i = 0; i < length; ++i) {
    result += std::format("{:02x}", digest.at(i));
  }
  return result;
}

auto makeCacheKey(std::string_view contents, const FormatStyle& style,
                  std::string_view build_id) -> std::string {
  if (build_id.empty()) {
    throw std::invalid_argument("Cache requires a formatter build identity");
  }
  // Digests have fixed lengths, so arbitrary source and build identity bytes
  // cannot be confused with field separators. Bump v1 if this encoding changes.
  return sha256("verihogg-check-cache-key-v1:" + sha256(build_id) +
                styleKey(style) + sha256(contents));
}

}  // namespace format

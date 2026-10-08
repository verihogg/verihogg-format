#pragma once

#include <string_view>

namespace format::config {

// Returns the JSON schema of the configuration file. The data is generated
// at configure time from schemas/verihogg-format.schema.json, so this header
// stays independent of the build tree.
[[nodiscard]] auto embeddedSchemaJson() -> std::string_view;

}  // namespace format::config

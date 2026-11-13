#pragma once

#include <slang/text/SourceLocation.h>

#include <cstddef>
#include <string>

namespace format {

struct FormatWarning {
  slang::SourceLocation location;
  std::string code;
  std::string message;
  // Short grouping key: the unsupported construct name, annotation failure
  // reason, or a fixed label. Used by the end-of-run summary.
  std::string construct;
  // 1-based line in the formatted output that corresponds to location;
  // 0 when the mapping is unknown.
  std::size_t formatted_line = 0;
};

}  // namespace format

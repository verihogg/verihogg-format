#pragma once

#include <cstdint>
#include <span>

#include "format_style.h"

namespace format {
enum class PartitionPolicy : uint8_t {
  kAlwaysExpand,
  kFitOnLineElseExpand,
  kTabularAlignment,
  kAlreadyFormatted,
};

template <typename Token>
struct UnwrappedLine {
  std::span<Token> tokens;
  IndentLevel indentation_spaces;
  PartitionPolicy partition_policy;
};
}  // namespace format

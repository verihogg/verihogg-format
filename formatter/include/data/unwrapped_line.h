#pragma once

#include <slang/syntax/SyntaxNode.h>

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
struct UnwrappedLine;

template <typename Token>
struct UnwrappedLineNode {
  Token* token;
  std::vector<UnwrappedLine<Token>> children;
};

template <typename Token>
struct UnwrappedLine {
  std::span<UnwrappedLineNode<Token>> tokens;

  IndentLevel indentation_spaces;
  // todo
};

}  // namespace format

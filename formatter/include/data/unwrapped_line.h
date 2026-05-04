#pragma once

#include <slang/parsing/Token.h>
#include <slang/syntax/SyntaxNode.h>

#include <cstdint>
#include <iostream>
#include <vector>

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
  std::vector<Token> tokens;

  IndentLevel indentation_spaces;
  PartitionPolicy partition_policy;
  // todo
};

auto printUnwrappedLine(const UnwrappedLine<slang::parsing::Token>& line,
                        size_t depth = 0, std::ostream& os = std::cout) -> void;

auto printUnwrappedLines(
    const std::vector<UnwrappedLine<slang::parsing::Token>>& lines,
    std::ostream& os = std::cout) -> void;

}  // namespace format

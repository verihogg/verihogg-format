#pragma once

#include <slang/parsing/Token.h>

#include <gsl/span>
#include <string_view>
#include <vector>

#include "data/format_token.h"
#include "data/unwrapped_line.h"
#include "formatter.h"

namespace format {

auto preserveEofComment(gsl::span<const slang::parsing::Token> tokens,
                        std::string_view original_source,
                        std::vector<UnwrappedLine<FormatToken>>& lines) -> void;

auto preserveMacroSemantics(gsl::span<const slang::parsing::Token> tokens,
                            std::string_view original_source,
                            FormatResult result) -> FormatResult;

}  // namespace format

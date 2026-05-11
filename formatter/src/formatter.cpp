#include "formatter.h"

#include <slang/parsing/Token.h>

#include <span>

#include "data/format_style.h"
#include "pipeline/printer.h"
#include "pipeline/tabular_aligner.h"
#include "pipeline/token_annotator.h"
#include "pipeline/tree_unwrapper.h"

namespace format {
auto format(std::span<const slang::parsing::Token> tokens, FormatStyle style)
    -> FormatResult {
  auto unwrappedLines = TreeUnwrapper(tokens, style).unwrap();
  auto annotatedLines = TokenAnnotator(style).annotate(unwrappedLines);
  // joinLines(annotatedLines, style);
  align(annotatedLines, style);
  return FormatResult{.formatted_text = Printer(style).print(annotatedLines)};
}
}  // namespace format

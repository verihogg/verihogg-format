#include "formatter.h"

#include <slang/syntax/SyntaxTree.h>

#include "data/format_style.h"
#include "pipeline/line_joiner.h"
#include "pipeline/tabular_aligner.h"
#include "pipeline/token_annotator.h"
#include "pipeline/tree_unwrapper.h"

namespace format {
auto format(std::span<const slang::parsing::Token> tokens, FormatStyle style)
    -> FormatResult {
  auto unwrappedLines = TreeUnwrapper(tokens, style).unwrap();
  auto annotatedLines = TokenAnnotator(style).annotate(unwrappedLines);
  joinLines(annotatedLines, style);
  align(annotatedLines, style);
  return {.formatted_text = "TODO"};
}
}  // namespace format

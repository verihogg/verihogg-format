#include "formatter.h"

#include <slang/syntax/SyntaxTree.h>

#include <string_view>

#include "data/format_style.h"
#include "pipeline/line_joiner.h"
#include "pipeline/tabular_aligner.h"
#include "pipeline/token_annotator.h"
#include "pipeline/tree_unwrapper.h"

namespace format {
auto format(std::string_view source_text, FormatStyle style) -> FormatResult {
  auto slangTree = slang::syntax::SyntaxTree::fromText(source_text);
  auto tokenTree = TreeUnwrapper(*slangTree, style).unwrap();
  auto annotatedTree = TokenAnnotator(style).annotate(tokenTree);
  joinLines(annotatedTree, style);
  align(annotatedTree, style);
  return {.formatted_text = annotatedTree.toString()};
}
}  // namespace format

#include "formatter.h"

#include <slang/parsing/Token.h>
#include <slang/util/BumpAllocator.h>

#include <gsl/span>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "data/format_style.h"
#include "pipeline/line_joiner.h"
#include "pipeline/line_wrap_searcher.h"
#include "pipeline/policy_assigner.h"
#include "pipeline/printer.h"
#include "pipeline/tabular_aligner.h"
#include "pipeline/token_annotator.h"
#include "pipeline/token_normalizer.h"
#include "pipeline/tree_unwrapper.h"

namespace format {

auto format(gsl::span<const slang::parsing::Token> tokens, FormatStyle style,
            std::string_view original_source) -> FormatResult {
  slang::BumpAllocator normalizer_allocator;
  auto normalized_tokens = normalizeTokens(tokens, normalizer_allocator);
  auto unwrapped = TreeUnwrapper(normalized_tokens, style, original_source)
                       .unwrapWithDiagnostics();
  auto annotated = TokenAnnotator(style).annotateWithDiagnostics(
      unwrapped.lines, original_source);
  PolicyAssigner(style).assign(annotated.lines);
  LineJoiner(style).join(annotated.lines);
  align(annotated.lines, style);
  applyLineWraps(annotated.lines, style);

  std::ostringstream output;
  Printer(style).print(annotated.lines, output);
  unwrapped.warnings.insert(unwrapped.warnings.end(),
                            std::make_move_iterator(annotated.warnings.begin()),
                            std::make_move_iterator(annotated.warnings.end()));
  return FormatResult{.formatted_text = output.str(),
                      .warnings = std::move(unwrapped.warnings)};
}

}  // namespace format

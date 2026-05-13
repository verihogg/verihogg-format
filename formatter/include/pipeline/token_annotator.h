#pragma once

#include <slang/parsing/Token.h>

#include <gsl/span>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {

class TokenAnnotator {
 public:
  explicit TokenAnnotator(const FormatStyle& style) : style(style) {};

  [[nodiscard]] auto annotate(
      const std::vector<UnwrappedLine<slang::parsing::Token>>& lines)
      -> std::vector<UnwrappedLine<FormatToken>>;

 private:
  // Runs three sequential passes for a single logical line.
  //   matchBrackets() - structure (who is paired with whom) ->
  //   determineTokenTypes() - semantics (role of each token) ->
  //   computeInterTokenInfo() - metrics (spaces, penalties, decisions)
  auto annotateUnwrappedLine(UnwrappedLine<FormatToken>& line) const -> void;

  // Pass 1: Structural analysis
  // Links paired tokens: ( with ), [ with ], begin with end, etc.
  // Fills in for each token:
  //   - matching_bracket  - pointer to the paired token (or nullptr)
  //   - nesting_level     - nesting depth (0 = top level)
  //   - balancing         - kOpen / kClose / kNone
  //
  // Implemented as a linear pass with a stack of open brackets.
  // Required before determineTokenTypes, because token classification
  // depends on nesting depth and the presence of a matching token.
  auto matchBrackets(gsl::span<FormatToken> tokens) const -> void;

  // Pass 2: Semantic analysis
  // Determines the TokenType of each token - its role in the formatting
  // context. The same TokenKind may receive different TokenType depending on
  // the context:
  //   TokenKind::LessThan  ->  kBinaryOperator  (a < b)
  //                        ->  kOpenGroup       (#(params))
  //   TokenKind::Comma     ->  kPortListComma / kParameterListComma / ...
  //
  // Uses results from matchBrackets (nesting_level, matching_bracket)
  // and an internal AnnotationContext stack to track the current region
  // (inside a port list, inside an expression, etc.).
  auto determineTokenTypes(gsl::span<FormatToken> tokens) const -> void;

  // Pass 3: Computing formatting metrics
  // Fills in FormatToken::before for each token (except the first):
  //   - spaces_required   - how many spaces to insert when continuing on the
  //   same line
  //   - break_penalty     - line break penalty (0 = free, higher = less
  //   desirable)
  //   - break_decision    - kMustBreak / kMustNotBreak / kUndecided
  //
  // Decisions are made by delegating to the three methods below.
  // Uses TokenType from determineTokenTypes.
  auto computeInterTokenInfo(gsl::span<FormatToken> tokens) const -> void;

  // Returns the number of spaces to insert between left and right
  // when placing them on the same line. Depends on the TokenType of both
  // tokens:
  //   kOpenGroup  -> right:    0 spaces (we don't write "( a")
  //   kBinaryOperator:         1 space  (a + b)
  //   kPortListComma -> right: 1 space  (a, b)
  [[nodiscard]] auto spacesRequired(const FormatToken& left,
                                    const FormatToken& right) const -> size_t;

  // Returns the penalty for a line break before right.
  //   after a comma in a port list -> low penalty
  //   inside an instance name -> high penalty
  [[nodiscard]] auto breakPenalty(const FormatToken& left,
                                  const FormatToken& right) const -> size_t;

  // Returns a mandatory break decision before right, if any.
  //   kMustBreak - break is mandatory (e.g., after begin)
  //   kMustNotBreak - break is prohibited (e.g., inside #(...))
  //   kUndecided - decision is left to the algorithm based on breakPenalty
  [[nodiscard]] auto breakDecision(const FormatToken& left,
                                   const FormatToken& right) const
      -> BreakDecision;

  auto annotateSpan(gsl::span<FormatToken> tokens) const -> void;

  std::reference_wrapper<const FormatStyle> style;
};
}  // namespace format

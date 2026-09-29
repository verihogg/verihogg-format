#include "formatter.h"

#include <slang/parsing/Token.h>

#include <algorithm>
#include <gsl/span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "data/format_style.h"
#include "data/lex_context.h"
#include "pipeline/compiler_directives.h"
#include "pipeline/line_joiner.h"
#include "pipeline/line_wrap_searcher.h"
#include "pipeline/policy_assigner.h"
#include "pipeline/printer.h"
#include "pipeline/tabular_aligner.h"
#include "pipeline/token_annotator.h"
#include "pipeline/token_normalizer.h"
#include "pipeline/tree_unwrapper.h"

namespace format {
namespace {

// A macro definition from this file, another compilation unit, or -D can
// expand __LINE__ indirectly. Keep all invocations on their source lines.
auto macroCallLines(gsl::span<const slang::parsing::Token> tokens,
                    std::string_view source)
    -> std::vector<std::pair<std::string, size_t>> {
  std::vector<std::pair<std::string, size_t>> result;
  size_t cursor = 0;
  size_t line = 1;
  for (const auto& token : tokens) {
    const size_t offset = std::min(token.location().offset(), source.size());
    while (cursor < offset) {
      if (source.at(cursor++) == '\n') {
        ++line;
      }
    }
    if (token.kind != slang::parsing::TokenKind::Directive) {
      continue;
    }
    if (!isCompilerDirective(token)) {
      result.emplace_back(token.rawText(), line);
    }
  }
  return result;
}

struct MacroInvocation {
  std::string name;
  size_t start = 0;
  size_t end = 0;
};

// Capture complete calls, including every byte of their arguments and trivia.
// An outer call already contains any nested macro invocations.
auto macroInvocations(gsl::span<const slang::parsing::Token> tokens,
                      std::string_view source) -> std::vector<MacroInvocation> {
  using TK = slang::parsing::TokenKind;
  std::vector<MacroInvocation> calls;
  for (size_t i = 0; i + 1 < tokens.size(); ++i) {
    const auto& token = tokens[i];
    if (token.kind != TK::Directive || isCompilerDirective(token) ||
        tokens[i + 1].kind != TK::OpenParenthesis) {
      continue;
    }
    const size_t start = token.location().offset();
    if (!calls.empty() && start < calls.back().end) {
      continue;
    }
    size_t depth = 0;
    for (size_t j = i + 1; j < tokens.size(); ++j) {
      if (tokens[j].kind == TK::OpenParenthesis) {
        ++depth;
      } else if (tokens[j].kind == TK::CloseParenthesis && --depth == 0) {
        const size_t end =
            tokens[j].location().offset() + tokens[j].rawText().size();
        if (start <= end && end <= source.size()) {
          calls.push_back({.name = std::string(token.rawText()),
                           .start = start,
                           .end = end});
        }
        break;
      }
    }
  }
  return calls;
}

// Restore macro calls from the user's source while retaining formatting around
// them. Return false when the calls cannot be matched safely.
auto restoreMacroInvocations(
    gsl::span<const slang::parsing::Token> original_tokens,
    std::string_view original_source, std::string& formatted_text) -> bool {
  const auto original_calls =
      macroInvocations(original_tokens, original_source);
  if (original_calls.empty()) {
    return true;
  }

  LexContext formatted_context;
  const auto formatted_tokens = formatted_context.lex_string(formatted_text);
  const auto formatted_calls =
      macroInvocations(formatted_tokens, formatted_context.source_text());
  if (original_calls.size() != formatted_calls.size()) {
    return false;
  }
  for (size_t i = 0; i < original_calls.size(); ++i) {
    if (original_calls.at(i).name != formatted_calls.at(i).name) {
      return false;
    }
  }
  for (size_t i = original_calls.size(); i > 0; --i) {
    const auto& original = original_calls.at(i - 1);
    const auto& formatted = formatted_calls.at(i - 1);
    formatted_text.replace(
        formatted.start, formatted.end - formatted.start,
        original_source.substr(original.start, original.end - original.start));
  }
  return true;
}

}  // namespace

auto format(gsl::span<const slang::parsing::Token> tokens, FormatStyle style,
            std::string_view original_source) -> FormatResult {
  slang::BumpAllocator normalizer_allocator;
  auto normalizedTokens = normalizeTokens(tokens, normalizer_allocator);
  auto unwrapResult = TreeUnwrapper(normalizedTokens, style, original_source)
                          .unwrapWithDiagnostics();
  auto annotationResult = TokenAnnotator(style).annotateWithDiagnostics(
      unwrapResult.lines, original_source);

  // The lexer attaches comments after the final source token to EOF. EOF is
  // not an unwrapped line, so preserve that source suffix explicitly.
  if (!tokens.empty() &&
      tokens.back().kind == slang::parsing::TokenKind::EndOfFile &&
      (annotationResult.lines.empty() ||
       !annotationResult.lines.back().is_opaque)) {
    bool has_eof_comment = false;
    for (const auto& trivia : tokens.back().trivia()) {
      if (trivia.kind == slang::parsing::TriviaKind::LineComment ||
          trivia.kind == slang::parsing::TriviaKind::BlockComment) {
        has_eof_comment = true;
        break;
      }
    }
    if (has_eof_comment) {
      size_t tail_start = 0;
      if (tokens.size() > 1) {
        const auto& last = tokens[tokens.size() - 2];
        tail_start = std::min(last.location().offset() + last.rawText().size(),
                              original_source.size());
      }
      const size_t tail_end =
          std::min(tokens.back().location().offset(), original_source.size());
      if (tail_end > tail_start) {
        annotationResult.lines.push_back({
            .tokens = {},
            .raw_text = std::string(
                original_source.substr(tail_start, tail_end - tail_start)),
            .is_opaque = true,
        });
      }
    }
  }

  PolicyAssigner(style).assign(annotationResult.lines);
  LineJoiner(style).join(annotationResult.lines);
  align(annotationResult.lines, style);
  applyLineWraps(annotationResult.lines, style);
  std::ostringstream oss;
  Printer(style).print(annotationResult.lines, oss);
  unwrapResult.warnings.insert(
      unwrapResult.warnings.end(),
      std::make_move_iterator(annotationResult.warnings.begin()),
      std::make_move_iterator(annotationResult.warnings.end()));
  std::string formatted_text = oss.str();
  if (!original_source.empty() && formatted_text != original_source) {
    if (!restoreMacroInvocations(tokens, original_source, formatted_text)) {
      unwrapResult.warnings.push_back({
          .location = tokens.front().location(),
          .code = "macro-call-preservation",
          .message = "cannot match macro invocations after formatting; "
                     "original text preserved",
      });
      formatted_text = std::string(original_source);
    } else {
      const auto source_macros = macroCallLines(tokens, original_source);
      if (!source_macros.empty()) {
        LexContext formatted_context;
        auto formatted_tokens = formatted_context.lex_string(formatted_text);
        if (source_macros !=
            macroCallLines(formatted_tokens, formatted_context.source_text())) {
          unwrapResult.warnings.push_back({
              .location = tokens.front().location(),
              .code = "macro-line-number",
              .message = "formatting would move a macro invocation to "
                         "another source line; original text preserved",
          });
          formatted_text = std::string(original_source);
        }
      }
    }
  }
  return FormatResult{.formatted_text = std::move(formatted_text),
                      .warnings = std::move(unwrapResult.warnings)};
}
}  // namespace format

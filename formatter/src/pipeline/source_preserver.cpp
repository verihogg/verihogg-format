#include "pipeline/source_preserver.h"

#include <slang/parsing/Token.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/lex_context.h"
#include "pipeline/compiler_directives.h"

namespace format {
namespace {

using Token = slang::parsing::Token;
using TK = slang::parsing::TokenKind;

struct MacroOccurrence {
  std::string name;
  size_t offset = 0;
};

struct MacroInvocation {
  std::string name;
  size_t start = 0;
  size_t end = 0;
};

struct PreservationFailure {
  std::string_view code;
  std::string_view message;
};

auto macroOccurrences(gsl::span<const Token> tokens)
    -> std::vector<MacroOccurrence> {
  std::vector<MacroOccurrence> occurrences;
  for (const auto& token : tokens) {
    if (token.kind == TK::Directive && !isCompilerDirective(token)) {
      occurrences.push_back({.name = std::string(token.rawText()),
                             .offset = token.location().offset()});
    }
  }
  return occurrences;
}

auto macroCallLines(const std::vector<MacroOccurrence>& occurrences,
                    std::string_view source)
    -> std::vector<std::pair<std::string, size_t>> {
  std::vector<std::pair<std::string, size_t>> result;
  size_t cursor = 0;
  size_t line = 1;
  for (const auto& occurrence : occurrences) {
    const size_t offset = std::min(occurrence.offset, source.size());
    while (cursor < offset) {
      if (source.at(cursor++) == '\n') {
        ++line;
      }
    }
    result.emplace_back(occurrence.name, line);
  }
  return result;
}

// Capture complete calls, including every byte of their arguments and trivia.
// An outer call already contains any nested macro invocations.
auto macroInvocations(gsl::span<const Token> tokens,
                      std::string_view source) -> std::vector<MacroInvocation> {
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

auto addPreservationWarning(FormatResult& result, gsl::span<const Token> tokens,
                            std::string_view original_source,
                            PreservationFailure failure) -> void {
  result.warnings.push_back({.location = tokens.front().location(),
                             .code = std::string(failure.code),
                             .message = std::string(failure.message)});
  result.formatted_text = std::string(original_source);
}

// Translate macro offsets through the replacements without lexing the output
// again. Calls restored from source contribute their original inner tokens;
// tokens outside calls retain their formatted positions plus preceding edits.
auto restoredMacroOccurrences(
    const std::vector<MacroOccurrence>& original_occurrences,
    const std::vector<MacroOccurrence>& formatted_occurrences,
    const std::vector<MacroInvocation>& original_calls,
    const std::vector<MacroInvocation>& formatted_calls)
    -> std::vector<MacroOccurrence> {
  std::vector<MacroOccurrence> restored;
  restored.reserve(original_occurrences.size() + formatted_occurrences.size());

  std::vector<size_t> restored_call_starts;
  restored_call_starts.reserve(formatted_calls.size());
  std::ptrdiff_t shift = 0;
  for (size_t i = 0; i < formatted_calls.size(); ++i) {
    const auto& original = original_calls.at(i);
    const auto& formatted = formatted_calls.at(i);
    restored_call_starts.push_back(static_cast<size_t>(
        static_cast<std::ptrdiff_t>(formatted.start) + shift));
    shift += static_cast<std::ptrdiff_t>(original.end - original.start) -
             static_cast<std::ptrdiff_t>(formatted.end - formatted.start);
  }

  for (size_t i = 0; i < original_calls.size(); ++i) {
    const auto& call = original_calls.at(i);
    for (const auto& occurrence : original_occurrences) {
      if (call.start <= occurrence.offset && occurrence.offset < call.end) {
        restored.push_back({.name = occurrence.name,
                            .offset = restored_call_starts.at(i) +
                                      occurrence.offset - call.start});
      }
    }
  }

  for (const auto& occurrence : formatted_occurrences) {
    std::ptrdiff_t preceding_shift = 0;
    bool replaced = false;
    for (size_t i = 0; i < formatted_calls.size(); ++i) {
      const auto& call = formatted_calls.at(i);
      if (occurrence.offset < call.start) {
        break;
      }
      if (occurrence.offset < call.end) {
        replaced = true;
        break;
      }
      const auto& original = original_calls.at(i);
      preceding_shift +=
          static_cast<std::ptrdiff_t>(original.end - original.start) -
          static_cast<std::ptrdiff_t>(call.end - call.start);
    }
    if (!replaced) {
      restored.push_back({.name = occurrence.name,
                          .offset = static_cast<size_t>(
                              static_cast<std::ptrdiff_t>(occurrence.offset) +
                              preceding_shift)});
    }
  }

  std::stable_sort(
      restored.begin(), restored.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.offset < rhs.offset; });
  return restored;
}

}  // namespace

auto preserveEofComment(
    gsl::span<const Token> tokens, std::string_view original_source,
    std::vector<UnwrappedLine<FormatToken>>& lines) -> void {
  // The lexer attaches comments after the final source token to EOF. EOF is
  // not an unwrapped line, so preserve that source suffix explicitly.
  if (tokens.empty() || tokens.back().kind != TK::EndOfFile ||
      (!lines.empty() && lines.back().is_opaque)) {
    return;
  }

  const auto trivia = tokens.back().trivia();
  const bool has_eof_comment =
      std::ranges::any_of(trivia, [](const auto& item) {
        return item.kind == slang::parsing::TriviaKind::LineComment ||
               item.kind == slang::parsing::TriviaKind::BlockComment;
      });
  if (!has_eof_comment) {
    return;
  }

  size_t tail_start = 0;
  if (tokens.size() > 1) {
    const auto& last = tokens[tokens.size() - 2];
    tail_start = std::min(last.location().offset() + last.rawText().size(),
                          original_source.size());
  }
  const size_t tail_end =
      std::min(tokens.back().location().offset(), original_source.size());
  if (tail_end > tail_start) {
    lines.push_back({
        .tokens = {},
        .raw_text = std::string(
            original_source.substr(tail_start, tail_end - tail_start)),
        .is_opaque = true,
    });
  }
}

auto preserveMacroSemantics(gsl::span<const Token> tokens,
                            std::string_view original_source,
                            FormatResult result) -> FormatResult {
  if (original_source.empty() || result.formatted_text == original_source) {
    return result;
  }
  const auto original_occurrences = macroOccurrences(tokens);
  if (original_occurrences.empty()) {
    return result;
  }

  const auto original_calls = macroInvocations(tokens, original_source);
  // The printer can join and wrap lines, so source offsets cannot identify
  // emitted macro calls. Lex the output once to find replacement spans safely.
  LexContext formatted_context;
  const auto formatted_tokens =
      formatted_context.lex_string(result.formatted_text);
  const auto formatted_occurrences = macroOccurrences(formatted_tokens);
  const auto formatted_calls =
      macroInvocations(formatted_tokens, formatted_context.source_text());
  if (original_calls.size() != formatted_calls.size() ||
      !std::equal(original_calls.begin(), original_calls.end(),
                  formatted_calls.begin(), formatted_calls.end(),
                  [](const auto& lhs, const auto& rhs) {
                    return lhs.name == rhs.name;
                  })) {
    addPreservationWarning(
        result, tokens, original_source,
        {.code = "macro-call-preservation",
         .message = "cannot match macro invocations after formatting; "
                    "original text preserved"});
    return result;
  }

  const auto restored_occurrences =
      restoredMacroOccurrences(original_occurrences, formatted_occurrences,
                               original_calls, formatted_calls);
  for (size_t i = original_calls.size(); i > 0; --i) {
    const auto& original = original_calls.at(i - 1);
    const auto& formatted = formatted_calls.at(i - 1);
    result.formatted_text.replace(
        formatted.start, formatted.end - formatted.start,
        original_source.substr(original.start, original.end - original.start));
  }

  if (macroCallLines(original_occurrences, original_source) !=
      macroCallLines(restored_occurrences, result.formatted_text)) {
    addPreservationWarning(
        result, tokens, original_source,
        {.code = "macro-line-number",
         .message = "formatting would move a macro invocation to another "
                    "source line; original text preserved"});
  }
  return result;
}

}  // namespace format

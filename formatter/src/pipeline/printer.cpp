#include "pipeline/printer.h"

#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>

#include <algorithm>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace format {

namespace {

using slang::parsing::Token;
using slang::parsing::Trivia;
using slang::parsing::TriviaKind;

struct CompletedLine {
  std::string text;
  bool accepts_trailing_comment = false;
};

class PrintState {
 public:
  explicit PrintState(std::string_view line_ending)
      : line_ending_(line_ending), lines_{} {}

  auto ensureIndent(size_t indent) -> void {
    if (current_.empty()) {
      current_.append(indent, ' ');
    }
  }

  auto appendSpaces(size_t count) -> void { current_.append(count, ' '); }

  auto appendRaw(std::string_view text) -> void { current_.append(text); }

  auto appendToken(std::string_view text) -> void {
    current_.append(text);
    current_has_code_ = true;
  }

  auto appendComment(std::string_view text) -> void { current_.append(text); }

  auto finishLine(bool preserve_source_break = false) -> void {
    trimCurrentLine();
    // A layout break can precede a trailing comment on the same source line.
    // After a newline preserved inside a multiline comment, subsequent trivia
    // belongs to the next physical line instead.
    lines_.push_back(CompletedLine{
        .text = std::move(current_),
        .accepts_trailing_comment = current_has_code_ && !preserve_source_break,
    });
    current_.clear();
    current_has_code_ = false;
  }

  auto appendPreservedText(std::string_view text) -> void {
    while (!text.empty()) {
      const auto end = text.find_first_of("\r\n");
      appendToken(text.substr(0, end));
      if (end == std::string_view::npos) {
        return;
      }
      const size_t newline_size = text.at(end) == '\r' &&
                                          end + 1 < text.size() &&
                                          text.at(end + 1) == '\n'
                                      ? 2
                                      : 1;
      finishLine(true);
      text.remove_prefix(end + newline_size);
    }
  }

  auto finishLineIfNeeded() -> void {
    if (!current_.empty()) {
      finishLine();
    }
  }

  auto addBlankLine() -> void {
    finishLineIfNeeded();
    if (lines_.empty() || lines_.back().text.empty()) {
      return;
    }
    lines_.push_back(CompletedLine{});
  }

  auto attachLineComment(std::string_view text, size_t indent,
                         std::string_view spaces_before) -> void {
    if (!current_.empty()) {
      appendSpacesBeforeComment(spaces_before);
      appendComment(text);
      finishLine();
      return;
    }

    if (!lines_.empty() && lines_.back().accepts_trailing_comment) {
      appendSpacesBeforeComment(lines_.back().text, spaces_before);
      lines_.back().text.append(text);
      return;
    }

    ensureIndent(indent);
    appendComment(text);
    finishLine();
  }

  auto appendCommentToLastLine(std::string_view text, size_t spaces_before)
      -> void {
    if (lines_.empty()) {
      return;
    }
    auto& last = lines_.back().text;
    last.append(spaces_before, ' ');
    last.append(text);
  }

  // Trivia on the first token of the next partition may still belong to the
  // preceding physical line. Preserve that attachment, including at EOF.
  auto attachTrailingBlockComment(const Trivia& comment,
                                  std::string_view spaces_before) -> bool {
    if (!current_.empty() || lines_.empty() ||
        !lines_.back().accepts_trailing_comment) {
      return false;
    }
    current_ = std::move(lines_.back().text);
    current_has_code_ = true;
    lines_.pop_back();
    appendRaw(spaces_before);
    appendPreservedText(comment.getRawText());
    finishLineIfNeeded();
    return true;
  }

  auto emitDetachedComment(std::string_view text, size_t indent) -> void {
    finishLineIfNeeded();
    size_t start = 0;

    while (start < text.size()) {
      const size_t end = text.find('\n', start);
      const size_t line_end =
          (end == std::string_view::npos) ? text.size() : end;
      size_t part_end = line_end;
      if (part_end > start && text.at(part_end - 1) == '\r') {
        --part_end;
      }

      // The first line gets the surrounding code's indentation. Continuation
      // lines already belong to the comment text; adding indentation to them
      // again on each formatting pass would make multiline comments drift.
      if (start == 0) {
        ensureIndent(indent);
      }
      appendComment(text.substr(start, part_end - start));
      finishLine();

      if (end == std::string_view::npos) {
        break;
      }
      start = end + 1;
    }
  }

  [[nodiscard]] auto currentLineEmpty() const -> bool {
    return current_.empty();
  }

  [[nodiscard]] auto build() const -> std::string {
    size_t size = 0;
    for (const auto& line : lines_) {
      size += line.text.size() + line_ending_.size();
    }

    std::string output;
    output.reserve(size);
    for (const auto& line : lines_) {
      output.append(line.text);
      output.append(line_ending_);
    }
    return output;
  }

 private:
  auto trimCurrentLine() -> void {
    while (!current_.empty() && current_.back() == ' ') {
      current_.pop_back();
    }
  }

  auto appendSpacesBeforeComment(std::string_view spaces_before) -> void {
    appendSpacesBeforeComment(current_, spaces_before);
  }

  static auto appendSpacesBeforeComment(std::string& text,
                                        std::string_view spaces_before)
      -> void {
    text.append(spaces_before);
  }

  std::string_view line_ending_;
  std::vector<CompletedLine> lines_;
  std::string current_;
  bool current_has_code_ = false;
};

struct TriviaEffect {
  struct InlineComment {
    std::string spaces_before;
    std::string_view text;
  };

  std::vector<InlineComment> inline_comments;
  std::string spaces_after_inline_comments;
};

[[nodiscard]] auto resolveLineEnding(const FormatStyle& style)
    -> std::string_view {
  switch (style.line_terminator) {
    case LineTerminator::kCrLf:
      return "\r\n";
    case LineTerminator::kLf:
    case LineTerminator::kAuto:
    default:
      return "\n";
  }
}

[[nodiscard]] auto isCommentLike(TriviaKind kind) -> bool {
  switch (kind) {
    case TriviaKind::LineComment:
    case TriviaKind::BlockComment:
    case TriviaKind::DisabledText:
    case TriviaKind::Directive:
      return true;
    default:
      return false;
  }
}

[[nodiscard]] auto triviaText(const Trivia& trivia) -> std::string_view {
  return trivia.getRawText();
}

auto preserveBlankLines(size_t newline_count, PrintState& state) -> void {
  if (newline_count > 1) {
    state.addBlankLine();
  }
}

struct Indent {
  size_t indent;
  size_t trailing_comment_spaces;
};

[[nodiscard]] auto emitLeadingTrivia(const Token& token, Indent indent,
                                     bool starts_line, PrintState& state)
    -> TriviaEffect {
  TriviaEffect effect;
  bool seen_newline = false;
  size_t newline_count = 0;
  std::string pending_whitespace;
  auto remaining_breaks =
      std::ranges::count_if(token.trivia(), [](const auto& t) {
        return t.kind == TriviaKind::EndOfLine ||
               t.kind == TriviaKind::LineComment;
      });

  for (const Trivia& trivia : token.trivia()) {
    switch (trivia.kind) {
      case TriviaKind::Whitespace:
        if (!seen_newline) {
          pending_whitespace.append(triviaText(trivia));
        }
        continue;

      case TriviaKind::EndOfLine:
        --remaining_breaks;
        seen_newline = true;
        ++newline_count;
        pending_whitespace.clear();
        continue;

      case TriviaKind::LineComment: {
        --remaining_breaks;
        const std::string_view text = triviaText(trivia);
        if (text.empty()) {
          continue;
        }
        if (!seen_newline) {
          if (indent.trailing_comment_spaces > 0) {
            state.appendCommentToLastLine(text, indent.trailing_comment_spaces);
          } else {
            state.attachLineComment(text, indent.indent, pending_whitespace);
          }
        } else {
          preserveBlankLines(newline_count, state);
          state.emitDetachedComment(text, indent.indent);
        }
        seen_newline = true;
        newline_count = 0;
        pending_whitespace.clear();
        continue;
      }

      default:
        break;
    }

    if (!isCommentLike(trivia.kind)) {
      continue;
    }

    const std::string_view text = triviaText(trivia);
    if (text.empty()) {
      continue;
    }

    if (trivia.kind == TriviaKind::BlockComment && !seen_newline &&
        starts_line &&
        state.attachTrailingBlockComment(trivia, pending_whitespace)) {
      pending_whitespace.clear();
      continue;
    }

    // Leading block comments and comments before a wrapped token get their
    // own line. Trailing comments remain attached to the preceding code above.
    // Do not defer a block comment past a following newline / line comment:
    // those are emitted immediately, so deferral would reverse their order.
    if (!seen_newline && remaining_breaks == 0 &&
        !(starts_line && trivia.kind == TriviaKind::BlockComment)) {
      effect.inline_comments.push_back(TriviaEffect::InlineComment{
          .spaces_before = std::move(pending_whitespace),
          .text = text,
      });
      pending_whitespace.clear();
    } else {
      preserveBlankLines(newline_count, state);
      state.emitDetachedComment(text, indent.indent);
      seen_newline = true;
      newline_count = 0;
      pending_whitespace.clear();
    }
  }

  if (token.kind != slang::parsing::TokenKind::EndOfFile) {
    preserveBlankLines(newline_count, state);
  }
  effect.spaces_after_inline_comments = std::move(pending_whitespace);
  return effect;
}

auto printLine(const UnwrappedLine<FormatToken>& line, PrintState& state)
    -> void {
  if (line.tokens.empty()) {
    return;
  }

  const size_t indent = line.indentation_spaces;
  for (size_t i = 0; i < line.tokens.size(); ++i) {
    const FormatToken& ft = line.tokens.at(i);
    const InterTokenDecision decision =
        (i == 0) ? InterTokenDecision{.spaces_before = 0,
                                      .action = TokenAction::kAppend}
                 : ft.decision;

    const size_t tcs = (i == 0) ? ft.before.comment_spaces : 0;

    const TriviaEffect trivia = emitLeadingTrivia(
        ft.token, Indent(indent, tcs),
        i == 0 || decision.action == TokenAction::kWrap, state);

    if (ft.token.kind == slang::parsing::TokenKind::EndOfFile) {
      return;
    }

    if (i == 0 || state.currentLineEmpty()) {
      state.finishLineIfNeeded();
      state.ensureIndent(decision.action == TokenAction::kWrap
                             ? decision.spaces_before
                             : indent);
    } else if (decision.action == TokenAction::kWrap) {
      state.finishLine();
      state.ensureIndent(decision.spaces_before);
    } else if (trivia.inline_comments.empty()) {
      state.appendSpaces(decision.spaces_before);
    }

    for (const auto& comment : trivia.inline_comments) {
      state.appendRaw(comment.spaces_before);
      state.appendComment(comment.text);
    }
    if (!trivia.inline_comments.empty()) {
      state.appendRaw(trivia.spaces_after_inline_comments);
    }

    state.appendToken(ft.token.rawText());
  }

  state.finishLineIfNeeded();
}

}  // namespace

Printer::Printer(const FormatStyle& style)
    : line_ending_(resolveLineEnding(style)) {}

auto Printer::print(const std::vector<UnwrappedLine<FormatToken>>& lines,
                    std::ostream& os) const -> void {
  PrintState state(line_ending_);

  for (const auto& line : lines) {
    printLine(line, state);
  }

  os << state.build();
}

}  // namespace format

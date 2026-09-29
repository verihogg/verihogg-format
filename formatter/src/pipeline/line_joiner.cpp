#include "pipeline/line_joiner.h"

#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>

#include <cstdint>
#include <iterator>
#include <vector>

namespace format {
namespace {

using TK = slang::parsing::TokenKind;
using TriviaKind = slang::parsing::TriviaKind;
using Line = UnwrappedLine<FormatToken>;

enum class HeaderKind : uint8_t { kNone, kStatement, kIf, kElseIf, kElse };

// Only headers with one unbraced statement may absorb a body. Indentation
// alone cannot distinguish these from module/function headers or named blocks.
[[nodiscard]] auto headerKind(const Line& line) -> HeaderKind {
  if (line.tokens.empty()) {
    return HeaderKind::kNone;
  }
  size_t first = 0;
  if (line.tokens.size() > 2 &&
      line.tokens.front().token.kind == TK::Identifier &&
      line.tokens.at(1).token.kind == TK::Colon) {
    first = 2;  // statement label
  }
  switch (line.tokens.at(first).token.kind) {
    case TK::IfKeyword:
      return HeaderKind::kIf;
    case TK::ElseKeyword:
      if (first + 1 == line.tokens.size()) {
        return HeaderKind::kElse;
      }
      return line.tokens.at(first + 1).token.kind == TK::IfKeyword
                 ? HeaderKind::kElseIf
                 : HeaderKind::kNone;
    case TK::AlwaysKeyword:
    case TK::AlwaysCombKeyword:
    case TK::AlwaysFFKeyword:
    case TK::AlwaysLatchKeyword:
    case TK::InitialKeyword:
    case TK::FinalKeyword:
    case TK::ForKeyword:
    case TK::ForeachKeyword:
    case TK::WhileKeyword:
    case TK::RepeatKeyword:
    case TK::ForeverKeyword:
    case TK::DefaultKeyword:
      return HeaderKind::kStatement;
    default:
      return line.partition_policy == PartitionPolicy::kTabularAlignment &&
                     line.tokens.back().token.kind == TK::Colon
                 ? HeaderKind::kStatement
                 : HeaderKind::kNone;
  }
}

[[nodiscard]] auto hasTriviaBarrier(const FormatToken& token) -> bool {
  size_t newlines = 0;
  for (const auto& trivia : token.token.trivia()) {
    switch (trivia.kind) {
      case TriviaKind::Whitespace:
        break;
      case TriviaKind::EndOfLine:
        if (++newlines > 1) {
          return true;
        }
        break;
      default:
        // Comments, disabled text and directives are printed separately from
        // tokens. They can add width or physical lines not in our width model.
        return true;
    }
  }
  return false;
}

[[nodiscard]] auto canFlatten(const Line& line, bool isAnchor) -> bool {
  if (line.tokens.empty() || line.is_fallback ||
      line.partition_policy == PartitionPolicy::kAlreadyFormatted ||
      line.partition_policy == PartitionPolicy::kFitOnLineElseExpand) {
    return false;
  }
  for (size_t i = 0; i < line.tokens.size(); ++i) {
    const auto& token = line.tokens.at(i);
    // Macro expansion boundaries are unknown to this stage.
    if (token.token.kind == TK::Directive ||
        token.token.kind == TK::BeginKeyword ||
        token.token.kind == TK::ForkKeyword) {
      return false;
    }
    if ((i > 0 || !isAnchor) && hasTriviaBarrier(token)) {
      return false;
    }
    if (i == 0 && isAnchor) {
      // At a partition boundary, Printer attaches trailing comments to the
      // preceding line and emits standalone comments before this line. Other
      // trivia (disabled text, directives, skipped tokens) has no measured
      // layout.
      for (const auto& trivia : token.token.trivia()) {
        if (trivia.kind != TriviaKind::Whitespace &&
            trivia.kind != TriviaKind::EndOfLine &&
            trivia.kind != TriviaKind::LineComment &&
            trivia.kind != TriviaKind::BlockComment) {
          return false;
        }
      }
    }
    // The first token starts an unwrapped line; its break decision is not an
    // inter-token constraint. All internal mandatory breaks must survive.
    if (i > 0 && token.before.break_decision == BreakDecision::kMustBreak) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] auto isSimpleBody(const Line& line) -> bool {
  return canFlatten(line, false) && headerKind(line) == HeaderKind::kNone &&
         line.partition_policy != PartitionPolicy::kTabularAlignment &&
         line.tokens.back().token.kind == TK::Semicolon;
}

[[nodiscard]] auto hasTrailingComment(const std::vector<Line>& lines,
                                      size_t index) -> bool {
  ++index;
  while (index < lines.size() && lines.at(index).tokens.empty()) {
    ++index;
  }
  if (index == lines.size()) {
    return false;
  }
  // Slang stores a trailing comment in the next token's leading trivia;
  // empty partitions do not change which token owns that trivia.
  for (const auto& trivia : lines.at(index).tokens.front().token.trivia()) {
    if (trivia.kind == TriviaKind::EndOfLine) {
      return false;
    }
    if (trivia.kind != TriviaKind::Whitespace) {
      return true;
    }
  }
  return false;
}

// Charge width against the remaining budget to avoid overflowing size_t with
// a very large column limit or indentation. A separator belongs to each part.
[[nodiscard]] auto fits(const Line& line, size_t& remaining,
                        bool isAnchor = false) -> bool {
  for (size_t i = 0; i < line.tokens.size(); ++i) {
    const auto& token = line.tokens.at(i);
    const size_t spaces = i == 0 ? (isAnchor ? line.indentation_spaces : 1)
                                 : token.before.spaces_required;
    const size_t width = token.token.rawText().size();
    if (spaces > remaining) {
      return false;
    }
    remaining -= spaces;
    if (width > remaining) {
      return false;
    }
    remaining -= width;
  }
  return true;
}

[[nodiscard]] auto merge(std::vector<Line>& lines, size_t begin, size_t end)
    -> Line {
  Line joined;
  joined.indentation_spaces = lines.at(begin).indentation_spaces;
  joined.nesting_level = lines.at(begin).nesting_level;
  joined.partition_policy = lines.at(begin).partition_policy;
  size_t count = 0;
  for (size_t i = begin; i < end; ++i) {
    count += lines.at(i).tokens.size();
  }
  joined.tokens.reserve(count);
  for (size_t i = begin; i < end; ++i) {
    auto& tokens = lines.at(i).tokens;
    if (i != begin) {
      tokens.front().before.spaces_required = 1;
      tokens.front().before.break_decision = BreakDecision::kMustNotBreak;
    }
    joined.tokens.insert(joined.tokens.end(),
                         std::make_move_iterator(tokens.begin()),
                         std::make_move_iterator(tokens.end()));
  }
  // Annotation links are local to each original token vector. Rebase only
  // after the destination has reached its final size; reserve/insert alone
  // would leave both the header's and the body's bracket links dangling.
  size_t offset = 0;
  for (size_t i = begin; i < end; ++i) {
    const auto& source = lines.at(i).tokens;
    for (size_t j = 0; j < source.size(); ++j) {
      auto& token = joined.tokens.at(offset + j);
      if (token.matching_bracket != nullptr) {
        const auto relative = token.matching_bracket - source.data();
        token.matching_bracket =
            &joined.tokens.at(offset + static_cast<size_t>(relative));
      }
    }
    offset += source.size();
  }
  return joined;
}

}  // namespace

auto LineJoiner::join(std::vector<Line>& lines) const -> void {
  const auto& st = style.get();
  if (lines.size() < 2) {
    return;
  }
  std::vector<Line> result;
  result.reserve(lines.size());
  size_t i = 0;
  while (i < lines.size()) {
    const Line& anchor = lines.at(i);
    HeaderKind kind = headerKind(anchor);
    if (!anchor.tokens.empty() &&
        anchor.tokens.back().token.kind == TK::Semicolon) {
      kind = HeaderKind::kNone;
    }
    size_t remaining = st.column_limit;
    size_t end = i + 1;
    const auto isBody = [&](size_t index) {
      const auto& body = lines.at(index);
      return body.nesting_level > anchor.nesting_level &&
             body.nesting_level - anchor.nesting_level == 1 &&
             isSimpleBody(body) && !hasTrailingComment(lines, index);
    };
    if (kind != HeaderKind::kNone && canFlatten(anchor, true) &&
        fits(anchor, remaining, true) && end < lines.size() && isBody(end) &&
        fits(lines.at(end), remaining)) {
      ++end;
      // An else belongs to an if, never to an arbitrary preceding statement.
      while ((kind == HeaderKind::kIf || kind == HeaderKind::kElseIf) &&
             end + 1 < lines.size()) {
        const auto& branch = lines.at(end);
        const HeaderKind branchKind = headerKind(branch);
        size_t branchBudget = remaining;
        if (branch.nesting_level != anchor.nesting_level ||
            (!branch.tokens.empty() &&
             branch.tokens.back().token.kind == TK::Semicolon) ||
            (branchKind != HeaderKind::kElse &&
             branchKind != HeaderKind::kElseIf) ||
            !canFlatten(branch, false) || !isBody(end + 1) ||
            !fits(branch, branchBudget) ||
            !fits(lines.at(end + 1), branchBudget)) {
          break;
        }
        remaining = branchBudget;
        kind = branchKind;
        end += 2;
      }
    }
    if (end == i + 1) {
      result.push_back(std::move(lines.at(i)));
    } else {
      result.push_back(merge(lines, i, end));
    }
    i = end;
  }
  lines = std::move(result);
}

}  // namespace format

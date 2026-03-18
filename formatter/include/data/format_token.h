#pragma once

#include <slang/parsing/Token.h>

#include <cstdint>
#include <string_view>
#include <utility>

#include "format_style.h"

namespace format {

enum class BreakDecision : uint8_t {
  kUndecided,
  kMustBreak,
  kMustNotBreak,
};

struct InterTokenInfo {
  int spaces_required{0};
  int break_penalty{0};
  BreakDecision break_decision{BreakDecision::kUndecided};
};

enum class TokenAction : uint8_t {
  kAppend,    // продолжить строку: вставить spaces_before пробелов
  kWrap,      // новая строка + отступ
  kPreserve,  // сохранить оригинал (disabled_range)
};

struct InterTokenDecision {
  int spaces_before{0};
  TokenAction action{TokenAction::kAppend};

  bool needsNewline() const noexcept { return action == TokenAction::kWrap; }
};

enum class GroupBalancing : uint8_t {
  kNone,
  kOpen,   // ( [ { begin
  kClose,  // ) ] } end
};

struct FormatToken {
  slang::parsing::Token token;
  InterTokenInfo before;
  InterTokenDecision decision;
  GroupBalancing balancing{GroupBalancing::kNone};

  std::string_view text() const noexcept { return token.rawText(); }
  slang::TokenKind kind() const noexcept { return token.kind; }
  bool newlineBefore() const noexcept { return !token.isOnSameLine(); }
};

using FormatTokenRange = std::pair<FormatToken*, FormatToken*>;

}  // namespace format

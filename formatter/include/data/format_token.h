#pragma once

#include <slang/parsing/Token.h>

#include <cstdint>
#include <string_view>
#include <utility>
#include <span>

#include "format_style.h"

namespace format {

enum class BreakDecision : uint8_t {
  kMustBreak,
  kMustNotBreak,
};

struct InterTokenInfo {
  size_t spaces_required;
  size_t break_penalty;
  BreakDecision break_decision;
};

enum class TokenAction : uint8_t {
  kAppend,    // продолжить строку: вставить spaces_before пробелов
  kWrap,      // новая строка + отступ
  kPreserve,  // сохранить оригинал (disabled_range)
};

struct InterTokenDecision {
  size_t spaces_before;
  TokenAction action;

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

}  // namespace format

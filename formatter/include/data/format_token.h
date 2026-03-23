#pragma once

#include <slang/parsing/Token.h>

#include <cstdint>

namespace format {

enum class BreakDecision : uint8_t {
  kMustBreak,
  kMustNotBreak,
  kUndecided,
};

struct InterTokenInfo {
  size_t spaces_required;
  size_t break_penalty;
  BreakDecision break_decision;
};

enum class TokenAction : uint8_t {
  kAppend,    // continue line: insert spaces_before spaces
  kWrap,      // newline + indent
  kPreserve,  // preserve original (disabled_range)
};

struct InterTokenDecision {
  size_t spaces_before;
  TokenAction action;
};

enum class GroupBalancing : uint8_t {
  kNone,
  kOpen,   // ( [ { begin
  kClose,  // ) ] } end
};

struct FormatToken {
  slang::parsing::Token token;
  InterTokenInfo before{};
  InterTokenDecision decision{};
  GroupBalancing balancing{GroupBalancing::kNone};
};

}  // namespace format

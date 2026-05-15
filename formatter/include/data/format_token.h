#pragma once

#include <slang/parsing/Token.h>

namespace format {

enum class BreakDecision : uint8_t {
  kMustBreak,
  kMustNotBreak,
  kUndecided,
};

struct InterTokenInfo {
  size_t spaces_required;
  size_t break_penalty;
  size_t comment_spaces;
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

enum class TokenType : uint8_t {
  kUnknown,

  // Operators
  kBinaryOperator,
  kUnaryOperator,
  kAssignmentOperator,

  // Delimiters
  kSemicolon,
  kColon,
  kComma,
  kDot,
  kHash,
  kAtSign,

  // Identifiers
  kModuleName,
  kPortName,
  kParameterName,
  kTypeName,
  kInstanceName,

  // Keywords — structural
  kModuleKeyword,
  kEndModuleKeyword,
  kFunctionKeyword,
  kEndFunctionKeyword,
  kTaskKeyword,
  kEndTaskKeyword,
  kClassKeyword,
  kEndClassKeyword,
  kGenerateKeyword,
  kEndGenerateKeyword,

  // Keywords — control flow
  kAlwaysKeyword,
  kBeginKeyword,
  kEndKeyword,
  kIfKeyword,
  kElseKeyword,
  kForKeyword,
  kCaseKeyword,
  kEndCaseKeyword,
  kDefaultKeyword,
  kCaseLabel,

  // Types and directions (for tabular alignment)
  kPortDirection,  // input/output/inout/ref
  kTypeKeyword,    // logic/wire/reg/bit/int...

  // Comments
  kLineComment,
  kBlockComment,

  kIntegerBase,
  kEdgeKeyword,
  kAssignKeyword,
  kDirective,
  kTernaryOperator,
  kSvaBinaryOperator,
  kSvaKeyword
};

struct FormatToken {
  slang::parsing::Token token;
  InterTokenInfo before{};
  InterTokenDecision decision{};
  GroupBalancing balancing{GroupBalancing::kNone};

  TokenType type = TokenType::kUnknown;
  FormatToken* matching_bracket = nullptr;  // matching bracket/begin-end pair
  size_t nesting_level = 0;
};

}  // namespace format

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

  // Операторы
  kBinaryOperator,
  kUnaryOperator,
  kAssignmentOperator,

  // Разделители
  kSemicolon,
  kColon,
  kComma,
  kDot,
  kHash,
  kAtSign,

  // Идентификаторы
  kModuleName,
  kPortName,
  kParameterName,
  kTypeName,
  kInstanceName,

  // Ключевые слова - структурные
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

  // Ключевые слова - управляющие
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

  // Типы и направления (для табличного выравнивания)
  kPortDirection,  // input/output/inout/ref
  kTypeKeyword,    // logic/wire/reg/bit/int...

  // Комментарии
  kLineComment,
  kBlockComment,
};

struct FormatToken {
  slang::parsing::Token token;
  InterTokenInfo before{};
  InterTokenDecision decision{};
  GroupBalancing balancing{GroupBalancing::kNone};

  TokenType type = TokenType::kUnknown;
  FormatToken* matching_bracket = nullptr;  // парная скобка/begin-end
  size_t nesting_level = 0;
};

}  // namespace format
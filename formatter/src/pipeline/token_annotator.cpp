#include "pipeline/token_annotator.h"

#include <fmt/core.h>
#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>

#include <cassert>
#include <cstddef>
#include <gsl/span>
#include <stack>
#include <vector>

#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {

// ---------------------------------------------------------------------------
// Internal utilities
// ---------------------------------------------------------------------------

namespace {

using TK = slang::parsing::TokenKind;

// -- Penalties for line breaks -----------------------------------------------
struct Penalty {
  static constexpr size_t kNone = 0;
  static constexpr size_t kSoft = 10;
  static constexpr size_t kMedium = 50;
  static constexpr size_t kHard = 200;
};

// -- Classification of brackets ---------------------------------------------
[[nodiscard]] auto groupBalancing(TK k) -> GroupBalancing {
  switch (k) {
    case TK::OpenParenthesis:
    case TK::OpenBracket:
    case TK::OpenBrace:
    case TK::BeginKeyword:
    case TK::ForkKeyword:
      return GroupBalancing::kOpen;

    case TK::CloseParenthesis:
    case TK::CloseBracket:
    case TK::CloseBrace:
    case TK::EndKeyword:
    case TK::JoinKeyword:
      return GroupBalancing::kClose;

    default:
      return GroupBalancing::kNone;
  }
}

// -- Unary vs Binary ----------------------------------------------------
[[nodiscard]] auto isOperand(TK k) -> bool {
  switch (k) {
    case TK::Identifier:
    case TK::SystemIdentifier:
    case TK::IntegerLiteral:
    case TK::RealLiteral:
    case TK::TimeLiteral:
    case TK::StringLiteral:
    case TK::CloseParenthesis:
    case TK::CloseBracket:
    case TK::CloseBrace:
      return true;
    default:
      return false;
  }
}

// -- Annotation context -----------------------------------------------------
enum class Context : uint8_t {
  kTopLevel,
  kPortList,
  kParameterList,
  kExpression,
  kInstancePorts,
  kConcatenation,
  kCaseBody,
};

// -- Pair of adjacent tokens --------------------------------------------------
struct TokenPair {
  const FormatToken* left;   // never null
  const FormatToken* right;  // never null
};

// Forward declarations
[[nodiscard]] auto implSpacesRequired(TokenPair p) -> size_t;
[[nodiscard]] auto implBreakPenalty(TokenPair p) -> size_t;
[[nodiscard]] auto implBreakDecision(TokenPair p) -> BreakDecision;
auto implComputeInterTokenInfo(gsl::span<FormatToken> tokens) -> void;

}  // namespace

// ---------------------------------------------------------------------------
// Pass 1: matchBrackets
// ---------------------------------------------------------------------------

auto TokenAnnotator::matchBrackets(gsl::span<FormatToken> tokens) const
    -> void {
  std::stack<FormatToken*> open_stack;
  size_t depth = 0;

  for (auto& ft : tokens) {
    ft.balancing = groupBalancing(ft.token.kind);

    switch (ft.balancing) {
      case GroupBalancing::kOpen:
        ft.nesting_level = depth;
        open_stack.push(&ft);
        ++depth;
        break;

      case GroupBalancing::kClose:
        if (depth > 0) {
          --depth;
        }
        ft.nesting_level = depth;
        if (!open_stack.empty()) {
          FormatToken* opener = open_stack.top();
          open_stack.pop();
          opener->matching_bracket = &ft;
          ft.matching_bracket = opener;
        }
        break;

      default:
        ft.nesting_level = depth;
        break;
    }
  }
}

// ---------------------------------------------------------------------------
// Pass 2: determineTokenTypes
// ---------------------------------------------------------------------------

auto TokenAnnotator::determineTokenTypes(gsl::span<FormatToken> tokens) const
    -> void {
  std::stack<Context> ctx;
  ctx.push(Context::kTopLevel);

  bool after_hash = false;
  bool next_is_module_name = false;
  bool next_is_instance_name = false;

  for (size_t i = 0; i < tokens.size(); ++i) {
    auto& ft = tokens[i];
    const TK kind = ft.token.kind;

    const TK prev_kind = (i > 0) ? tokens[i - 1].token.kind : TK::Unknown;
    const TokenType prev_type =
        (i > 0) ? tokens[i - 1].type : TokenType::kUnknown;

    const bool was_after_hash = after_hash;
    after_hash = false;

    switch (kind) {
      // -- Structural keywords -------------------------------------
      case TK::ModuleKeyword:
      case TK::MacromoduleKeyword:
        ft.type = TokenType::kModuleKeyword;
        next_is_module_name = true;
        continue;

      case TK::EndModuleKeyword:
        ft.type = TokenType::kEndModuleKeyword;
        continue;

      case TK::FunctionKeyword:
        ft.type = TokenType::kFunctionKeyword;
        continue;

      case TK::EndFunctionKeyword:
        ft.type = TokenType::kEndFunctionKeyword;
        continue;

      case TK::TaskKeyword:
        ft.type = TokenType::kTaskKeyword;
        continue;

      case TK::EndTaskKeyword:
        ft.type = TokenType::kEndTaskKeyword;
        continue;

      case TK::ClassKeyword:
        ft.type = TokenType::kClassKeyword;
        continue;

      case TK::EndClassKeyword:
        ft.type = TokenType::kEndClassKeyword;
        continue;

      case TK::GenerateKeyword:
        ft.type = TokenType::kGenerateKeyword;
        continue;

      case TK::EndGenerateKeyword:
        ft.type = TokenType::kEndGenerateKeyword;
        continue;

      // -- Control keywords -------------------------------------
      case TK::AlwaysKeyword:
      case TK::AlwaysCombKeyword:
      case TK::AlwaysFFKeyword:
      case TK::AlwaysLatchKeyword:
      case TK::InitialKeyword:
      case TK::FinalKeyword:
        ft.type = TokenType::kAlwaysKeyword;
        continue;

      case TK::BeginKeyword:
      case TK::ForkKeyword:
        ft.type = TokenType::kBeginKeyword;
        ctx.push(Context::kTopLevel);
        continue;

      case TK::EndKeyword:
      case TK::JoinKeyword:
      case TK::JoinAnyKeyword:
      case TK::JoinNoneKeyword:
        ft.type = TokenType::kEndKeyword;
        if (ctx.size() > 1) {
          ctx.pop();
        }
        continue;

      case TK::IfKeyword:
        ft.type = TokenType::kIfKeyword;
        continue;

      case TK::ElseKeyword:
        ft.type = TokenType::kElseKeyword;
        continue;

      case TK::ForKeyword:
      case TK::ForeachKeyword:
      case TK::ForeverKeyword:
      case TK::RepeatKeyword:
      case TK::WhileKeyword:
      case TK::DoKeyword:
        ft.type = TokenType::kForKeyword;
        continue;

      case TK::CaseKeyword:
      case TK::CaseXKeyword:
      case TK::CaseZKeyword:
      case TK::PriorityKeyword:
      case TK::UniqueKeyword:
      case TK::Unique0Keyword:
        ft.type = TokenType::kCaseKeyword;
        ctx.push(Context::kCaseBody);
        continue;

      case TK::EndCaseKeyword:
        ft.type = TokenType::kEndCaseKeyword;
        if (ctx.top() == Context::kCaseBody) {
          ctx.pop();
        }
        continue;

      case TK::DefaultKeyword:
        ft.type = (ctx.top() == Context::kCaseBody) ? TokenType::kDefaultKeyword
                                                    : TokenType::kUnknown;
        continue;

      // -- Separators ----------------------------------------------------
      case TK::Semicolon:
        ft.type = TokenType::kSemicolon;
        continue;

      case TK::Colon:
        ft.type = TokenType::kColon;
        continue;

      case TK::Comma:
        ft.type = TokenType::kComma;
        continue;

      case TK::Dot:
        ft.type = TokenType::kDot;
        continue;

      case TK::Hash:
        ft.type = TokenType::kHash;
        after_hash = true;
        continue;

      case TK::At:
        ft.type = TokenType::kAtSign;
        continue;

      // -- Port directions ---------------------------------------------
      case TK::InputKeyword:
      case TK::OutputKeyword:
      case TK::InOutKeyword:
      case TK::RefKeyword:
        ft.type = TokenType::kPortDirection;
        continue;

      // -- Data types ----------------------------------------------------
      case TK::LogicKeyword:
      case TK::WireKeyword:
      case TK::RegKeyword:
      case TK::BitKeyword:
      case TK::IntKeyword:
      case TK::IntegerKeyword:
      case TK::LongIntKeyword:
      case TK::ShortIntKeyword:
      case TK::ByteKeyword:
      case TK::RealKeyword:
      case TK::ShortRealKeyword:
      case TK::RealTimeKeyword:
      case TK::StringKeyword:
      case TK::VoidKeyword:
      case TK::CHandleKeyword:
      case TK::EventKeyword:
      case TK::TypedefKeyword:
      case TK::EnumKeyword:
      case TK::StructKeyword:
      case TK::UnionKeyword:
      case TK::UnsignedKeyword:
      case TK::SignedKeyword:
        ft.type = TokenType::kTypeKeyword;
        continue;

      // -- Assignment operators --------------------------------------------
      case TK::Equals:
      case TK::PlusEqual:
      case TK::MinusEqual:
      case TK::StarEqual:
      case TK::SlashEqual:
      case TK::PercentEqual:
      case TK::AndEqual:
      case TK::OrEqual:
      case TK::XorEqual:
      case TK::LeftShiftEqual:
      case TK::RightShiftEqual:
      case TK::TripleLeftShiftEqual:
      case TK::TripleRightShiftEqual:
        ft.type = TokenType::kAssignmentOperator;
        continue;

      // -- Unambiguously binary --------------------------------------------
      case TK::LessThan:
      case TK::GreaterThan:
      case TK::DoubleAnd:
      case TK::DoubleOr:
      case TK::LessThanEquals:
      case TK::GreaterThanEquals:
      case TK::DoubleEquals:
      case TK::ExclamationEquals:
      case TK::TripleEquals:
      case TK::ExclamationDoubleEquals:
      case TK::DoubleEqualsQuestion:
      case TK::ExclamationEqualsQuestion:
      case TK::LeftShift:
      case TK::RightShift:
      case TK::TripleLeftShift:
      case TK::TripleRightShift:
      case TK::Slash:
      case TK::Percent:
      case TK::DoubleStar:
        ft.type = TokenType::kBinaryOperator;
        continue;

      // -- Potentially unary -------------------------------------------
      case TK::Plus:
      case TK::Minus:
      case TK::Star:
      case TK::And:
      case TK::Or:
      case TK::Xor:
      case TK::XorTilde:
        ft.type = isOperand(prev_kind) ? TokenType::kBinaryOperator
                                       : TokenType::kUnaryOperator;
        continue;

      // -- Unambiguously unary ---------------------------------------------
      case TK::Tilde:
      case TK::TildeAnd:
      case TK::TildeOr:
      case TK::TildeXor:
      case TK::Exclamation:
        ft.type = TokenType::kUnaryOperator;
        continue;

      // -- Parentheses --------------------------------------------------------
      case TK::OpenParenthesis:
        ft.type = TokenType::kUnknown;
        if (was_after_hash) {
          ctx.push(Context::kParameterList);
        } else if (prev_type == TokenType::kModuleName ||
                   prev_type == TokenType::kInstanceName) {
          ctx.push(Context::kInstancePorts);
        } else {
          ctx.push(Context::kExpression);
        }
        continue;

      case TK::CloseParenthesis:
        ft.type = TokenType::kUnknown;
        if (ctx.size() > 1) {
          ctx.pop();
        }
        continue;

      case TK::OpenBracket:
        ft.type = TokenType::kUnknown;
        ctx.push(Context::kExpression);
        continue;

      case TK::CloseBracket:
        ft.type = TokenType::kUnknown;
        if (ctx.size() > 1) {
          ctx.pop();
        }
        continue;

      case TK::OpenBrace:
        ft.type = TokenType::kUnknown;
        ctx.push(Context::kConcatenation);
        continue;

      case TK::CloseBrace:
        ft.type = TokenType::kUnknown;
        if (ctx.size() > 1) {
          ctx.pop();
        }
        continue;

      // -- Identifiers -------------------------------------------------
      case TK::Identifier:
      case TK::SystemIdentifier: {
        if (next_is_module_name) {
          ft.type = TokenType::kModuleName;
          next_is_module_name = false;
        } else if (next_is_instance_name) {
          ft.type = TokenType::kInstanceName;
          next_is_instance_name = false;
        } else if ((prev_type == TokenType::kDot &&
                    ctx.top() == Context::kInstancePorts) ||
                   (prev_type == TokenType::kPortDirection ||
                    prev_type == TokenType::kTypeKeyword)) {
          ft.type = TokenType::kPortName;
        } else if (prev_type == TokenType::kTypeName) {
          ft.type = TokenType::kInstanceName;
        } else if (prev_kind == TK::Identifier &&
                   prev_type == TokenType::kUnknown) {
          tokens[i - 1].type = TokenType::kTypeName;
          ft.type = TokenType::kInstanceName;
        } else {
          ft.type = TokenType::kUnknown;
        }
        continue;
      }

      case TK::ApostropheOpenBrace:
        ft.type = TokenType::kUnknown;
        ctx.push(Context::kConcatenation);
        continue;

      case TK::Directive:
        ft.type = TokenType::kDirective;
        continue;

      case TK::OrMinusArrow:  // |->
        ft.type = TokenType::kBinaryOperator;
        continue;

      case TK::AssignKeyword:
        ft.type = TokenType::kAssignKeyword;
        continue;

      case TK::PosEdgeKeyword:
      case TK::NegEdgeKeyword:
        ft.type = TokenType::kEdgeKeyword;
        continue;

      case TK::AssertKeyword:
      case TK::PropertyKeyword:
      case TK::IffKeyword:
      case TK::DisableKeyword:
        ft.type = TokenType::kSvaKeyword;
        continue;

      case TK::Question:
        ft.type = TokenType::kTernaryOperator;
        continue;

      case TK::IntegerBase:  // 4'b, 8'h
        ft.type = TokenType::kIntegerBase;
        continue;

      case TK::UnbasedUnsizedLiteral:  // '0 '1 'x 'z
      default:
        ft.type = TokenType::kUnknown;
        continue;
    }
  }
}

// ---------------------------------------------------------------------------
// Pass 3: computeInterTokenInfo
// ---------------------------------------------------------------------------

namespace {

auto implSpacesRequired(TokenPair p) -> size_t {
  const FormatToken& left = *p.left;
  const FormatToken& right = *p.right;

  const TK lk = left.token.kind;
  const TK rk = right.token.kind;

  if (rk == TK::IntegerBase) {
    return 0;
  }  // "2" + "'b" → "2'b"
  if (lk == TK::IntegerBase) {
    return 0;
  }  // "'b" + "00" → "'b00"

  if (lk == TK::IntegerBase && rk == TK::UnbasedUnsizedLiteral) {
    return 0;
  }
  if (lk == TK::UnbasedUnsizedLiteral && rk == TK::UnbasedUnsizedLiteral) {
    return 0;
  }

  if (rk == TK::ApostropheOpenBrace) {
    return 0;
  }  // "logic" + "'{"
  if (lk == TK::ApostropheOpenBrace) {
    return 0;
  }  // "'{" + "..."

  if (rk == TK::Apostrophe) {
    return 0;
  }  // "logic" + "'"
  if (lk == TK::Apostrophe) {
    return 0;
  }  // "'" + "("

  if (right.token.kind == TK::Question &&
      (left.type == TokenType::kIntegerBase ||
       left.token.kind == TK::Question)) {
    return 0;
  }
  if (left.type == TokenType::kTernaryOperator ||
      right.type == TokenType::kTernaryOperator) {
    return 1;
  }

  if (right.token.kind == TK::Apostrophe &&
      (left.token.kind == TK::Identifier || left.type == TokenType::kTypeName ||
       left.token.kind == TK::IntegerLiteral)) {
    return 0;  // type'(...) for cast
  }

  if (left.token.kind == TK::Apostrophe) {
    return 0;  // "'(" and "'0", "'1", etc.
  }
  if (right.token.kind == TK::Question &&
      (left.type == TokenType::kIntegerBase ||
       left.token.kind == TK::Question)) {
    return 0;  // "5'b?" stays as "5'b?" not "5'b ?"
  }
  if (left.token.kind == TK::IntegerLiteral &&
      right.token.kind == TK::Identifier) {
    return 0;
  }

  if (right.type == TokenType::kComma || right.type == TokenType::kSemicolon) {
    return 0;
  }
  if (left.balancing == GroupBalancing::kOpen ||
      right.balancing == GroupBalancing::kClose) {
    return 0;
  }
  if (left.type == TokenType::kUnaryOperator) {
    return 0;
  }
  if (left.type == TokenType::kHash) {
    return 0;
  }
  if (left.type == TokenType::kDot || right.type == TokenType::kDot) {
    return 0;
  }
  if (left.type == TokenType::kAtSign) {
    return 0;
  }
  return 1;
}

auto implBreakPenalty(TokenPair p) -> size_t {
  const FormatToken& left = *p.left;
  const FormatToken& right = *p.right;

  if (left.type == TokenType::kComma) {
    return Penalty::kNone;
  }
  if (left.type == TokenType::kBinaryOperator ||
      left.type == TokenType::kAssignmentOperator) {
    return Penalty::kSoft;
  }
  if (left.type == TokenType::kDot || left.type == TokenType::kHash) {
    return Penalty::kHard;
  }
  if (right.balancing == GroupBalancing::kClose) {
    return Penalty::kHard;
  }
  return Penalty::kMedium;
}

[[nodiscard]] auto isBlockEnd(TokenType t) -> bool {
  switch (t) {
    case TokenType::kEndKeyword:
    case TokenType::kEndModuleKeyword:
    case TokenType::kEndFunctionKeyword:
    case TokenType::kEndTaskKeyword:
    case TokenType::kEndClassKeyword:
    case TokenType::kEndGenerateKeyword:
    case TokenType::kEndCaseKeyword:
      return true;
    default:
      return false;
  }
}

auto implBreakDecision(TokenPair p) -> BreakDecision {
  const FormatToken& left = *p.left;
  const FormatToken& right = *p.right;

  if (implSpacesRequired(p) == 0 && right.balancing != GroupBalancing::kClose) {
    return BreakDecision::kMustNotBreak;
  }
  if (right.type == TokenType::kDirective ||
      left.type == TokenType::kDirective) {
    return BreakDecision::kMustBreak;
  }
  if (left.type == TokenType::kBeginKeyword) {
    return BreakDecision::kMustBreak;
  }
  if (isBlockEnd(right.type) || isBlockEnd(left.type)) {
    return BreakDecision::kMustBreak;
  }
  if (left.type == TokenType::kSemicolon && right.nesting_level == 0) {
    return BreakDecision::kMustBreak;
  }
  if (left.type == TokenType::kAssignKeyword) {
    return BreakDecision::kMustNotBreak;
  }
  return BreakDecision::kUndecided;
}

auto implComputeInterTokenInfo(gsl::span<FormatToken> tokens) -> void {
  if (tokens.size() < 2) {
    return;
  }
  for (size_t i = 1; i < tokens.size(); ++i) {
    const TokenPair p{.left = &tokens[i - 1], .right = &tokens[i]};
    // For debugging
    // const size_t sp = implSpacesRequired(p);
    // if (sp > 0) {
    //   const auto lk = p.left->token.kind;
    //   const auto rk = p.right->token.kind;
    //   if (rk == TK::IntegerBase || lk == TK::IntegerBase ||
    //       rk == TK::ApostropheOpenBrace || lk == TK::ApostropheOpenBrace) {
    //     fmt::print("SPACE BETWEEN: left={} right={} spaces={}\n",
    //                slang::parsing::toString(lk),
    //                slang::parsing::toString(rk), sp);
    //   }
    // }

    tokens[i].before = {
        .spaces_required = implSpacesRequired(p),
        .break_penalty = implBreakPenalty(p),
        .comment_spaces = 0,
        .break_decision = implBreakDecision(p),
    };
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// annotateSpan — three passes over a contiguous buffer
// ---------------------------------------------------------------------------

auto TokenAnnotator::annotateSpan(gsl::span<FormatToken> tokens) const -> void {
  matchBrackets(tokens);
  determineTokenTypes(tokens);
  implComputeInterTokenInfo(tokens);
}

auto TokenAnnotator::annotateUnwrappedLine(
    UnwrappedLine<FormatToken>& line) const -> void {
  if (line.tokens.empty()) {
    return;
  }
  annotateSpan(line.tokens);
}

auto TokenAnnotator::annotate(
    const std::vector<UnwrappedLine<slang::parsing::Token>>& lines)
    -> std::vector<UnwrappedLine<FormatToken>> {
  std::vector<UnwrappedLine<FormatToken>> result;
  result.reserve(lines.size());

  for (const auto& line : lines) {
    result.push_back(
        line.map([](const slang::parsing::Token& tok) -> FormatToken {
          return FormatToken{.token = tok};
        }));
  }

  for (auto& line : result) {
    annotateUnwrappedLine(line);
  }

  return result;
}

}  // namespace format

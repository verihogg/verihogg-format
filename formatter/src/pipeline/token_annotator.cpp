#include "pipeline/token_annotator.h"

#include <fmt/core.h>
#include <slang/parsing/Token.h>
#include <slang/parsing/TokenKind.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <gsl/span>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/format_token.h"
#include "data/unwrapped_line.h"
#include "pipeline/compiler_directives.h"

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
        ft.type = TokenType::kGeneric;
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
        ft.type = TokenType::kGeneric;
        if (ctx.size() > 1) {
          ctx.pop();
        }
        continue;

      case TK::OpenBracket:
        ft.type = TokenType::kGeneric;
        ctx.push(Context::kExpression);
        continue;

      case TK::CloseBracket:
        ft.type = TokenType::kGeneric;
        if (ctx.size() > 1) {
          ctx.pop();
        }
        continue;

      case TK::OpenBrace:
        ft.type = TokenType::kGeneric;
        ctx.push(Context::kConcatenation);
        continue;

      case TK::CloseBrace:
        ft.type = TokenType::kGeneric;
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
                   prev_type == TokenType::kGeneric) {
          tokens[i - 1].type = TokenType::kTypeName;
          ft.type = TokenType::kInstanceName;
        } else {
          ft.type = TokenType::kGeneric;
        }
        continue;
      }

      case TK::ApostropheOpenBrace:
        ft.type = TokenType::kGeneric;
        ctx.push(Context::kConcatenation);
        continue;

      // Recognized tokens handled by the general spacing rules.
      case TK::IntegerLiteral:
      case TK::RealLiteral:
      case TK::TimeLiteral:
      case TK::StringLiteral:
      case TK::UnbasedUnsizedLiteral:
      case TK::Apostrophe:
      case TK::ParameterKeyword:
      case TK::LocalParamKeyword:
      case TK::GenVarKeyword:
        ft.type = TokenType::kGeneric;
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

  // Escaped identifiers end at whitespace, even before punctuation.
  if (lk == TK::Identifier && left.token.rawText().starts_with('\\')) {
    return 1;
  }

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
  if (right.type == TokenType::kComma || right.type == TokenType::kSemicolon) {
    return 0;
  }
  // Group balancing also includes begin/end and fork/join. Only punctuation
  // brackets suppress spaces; applying this rule to keywords can merge tokens
  // and prevent the required line break after a block opener in fallback lines.
  if (lk == TK::OpenParenthesis || lk == TK::OpenBracket ||
      lk == TK::OpenBrace || rk == TK::CloseParenthesis ||
      rk == TK::CloseBracket || rk == TK::CloseBrace) {
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
  if (right.type == TokenType::kDirective && isCompilerDirective(right.token)) {
    return BreakDecision::kMustBreak;
  }
  if (left.type == TokenType::kDirective) {
    return BreakDecision::kMustNotBreak;
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

[[nodiscard]] auto isBasedLiteralFragment(TK kind) -> bool {
  return kind == TK::IntegerLiteral || kind == TK::RealLiteral ||
         kind == TK::Identifier;
}

[[nodiscard]] auto continuesBasedLiteral(gsl::span<FormatToken> tokens,
                                         size_t index) -> bool {
  if (!isBasedLiteralFragment(tokens[index].token.kind)) {
    return false;
  }

  // The lexer can read hex digits such as "510e527f" as a real literal
  // followed by an identifier. Keep adjacent fragments joined to their base.
  for (size_t i = index; i > 0; --i) {
    const auto& previous = tokens[i - 1].token;
    const auto& current = tokens[i].token;
    // Whitespace is legal between a base and its first digit. The formatter
    // removes it, but distinct digit fragments must touch in the source.
    if (previous.kind == TK::IntegerBase) {
      return true;
    }
    if (previous.location().offset() + previous.rawText().size() !=
        current.location().offset()) {
      return false;
    }
    if (!isBasedLiteralFragment(previous.kind)) {
      return false;
    }
  }
  return false;
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
    if (continuesBasedLiteral(tokens, i)) {
      tokens[i].before.spaces_required = 0;
      tokens[i].before.break_decision = BreakDecision::kMustNotBreak;
    }
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

namespace {

struct AnnotationFailure {
  slang::SourceLocation location;
  std::string reason;
};

[[nodiscard]] auto expectedClose(TK kind) -> std::optional<TK> {
  switch (kind) {
    case TK::OpenParenthesis:
      return TK::CloseParenthesis;
    case TK::OpenBracket:
      return TK::CloseBracket;
    case TK::OpenBrace:
    case TK::ApostropheOpenBrace:
      return TK::CloseBrace;
    default:
      return std::nullopt;
  }
}

[[nodiscard]] auto isClose(TK kind) -> bool {
  return kind == TK::CloseParenthesis || kind == TK::CloseBracket ||
         kind == TK::CloseBrace;
}

[[nodiscard]] auto isBasedLiteralQuestion(
    const std::vector<slang::parsing::Token>& tokens, size_t index) -> bool {
  for (size_t i = index; i > 0; --i) {
    const auto& previous = tokens.at(i - 1);
    const auto& current = tokens.at(i);
    if (previous.location().offset() + previous.rawText().size() !=
        current.location().offset()) {
      return false;
    }
    if (previous.kind == TK::IntegerBase) {
      return true;
    }
    if (previous.kind != TK::IntegerLiteral &&
        previous.kind != TK::Identifier && previous.kind != TK::Question) {
      return false;
    }
  }
  return false;
}

[[nodiscard]] auto validateLine(
    const UnwrappedLine<slang::parsing::Token>& line)
    -> std::optional<AnnotationFailure> {
  std::vector<TK> bracket_stack;
  std::vector<std::pair<size_t, slang::SourceLocation>> questions;
  for (size_t i = 0; i < line.tokens.size(); ++i) {
    const auto& token = line.tokens.at(i);
    const TK kind = token.kind;
    if (kind == TK::Unknown) {
      return AnnotationFailure{.location = token.location(),
                               .reason = "unknown lexical token"};
    }
    if (auto close = expectedClose(kind)) {
      bracket_stack.push_back(*close);
      continue;
    }
    if (isClose(kind)) {
      if (bracket_stack.empty()) {
        // A module or parameter list can close on its own unwrapped line.
        if (kind != TK::CloseParenthesis || i != 0) {
          return AnnotationFailure{.location = token.location(),
                                   .reason = "unmatched closing bracket"};
        }
      } else if (bracket_stack.back() != kind) {
        return AnnotationFailure{.location = token.location(),
                                 .reason = "mismatched closing bracket"};
      } else {
        bracket_stack.pop_back();
      }
      continue;
    }
    if (kind == TK::Question) {
      if (isBasedLiteralQuestion(line.tokens, i)) {
        return AnnotationFailure{
            .location = token.location(),
            .reason = "based literal wildcard is not supported"};
      }
      questions.emplace_back(bracket_stack.size(), token.location());
    } else if (kind == TK::Colon && !questions.empty() &&
               questions.back().first == bracket_stack.size()) {
      questions.pop_back();
    }
  }
  if (!questions.empty()) {
    return AnnotationFailure{
        .location = questions.back().second,
        .reason = "conditional operator has no matching colon"};
  }
  if (!bracket_stack.empty() && !line.tokens.empty() &&
      line.tokens.back().kind == TK::Semicolon) {
    return AnnotationFailure{.location = line.tokens.back().location(),
                             .reason = "unclosed bracket before statement end"};
  }
  return std::nullopt;
}

[[nodiscard]] auto lineStart(std::string_view source, size_t offset) -> size_t {
  offset = std::min(offset, source.size());
  if (offset == 0) {
    return 0;
  }
  const size_t newline = source.find_last_of("\r\n", offset - 1);
  return newline == std::string_view::npos ? 0 : newline + 1;
}

[[nodiscard]] auto canResumeAt(const UnwrappedLine<FormatToken>& line,
                               std::string_view source, size_t failed_offset)
    -> bool {
  if (line.is_opaque || line.tokens.empty()) {
    return false;
  }
  const size_t offset =
      std::min(line.tokens.front().token.location().offset(), source.size());
  const size_t start = lineStart(source, offset);
  return start > failed_offset &&
         std::ranges::all_of(source.substr(start, offset - start),
                             [](char ch) { return ch == ' ' || ch == '\t'; });
}

}  // namespace

auto TokenAnnotator::annotateWithDiagnostics(
    const std::vector<UnwrappedLine<slang::parsing::Token>>& lines,
    std::string_view original_source) -> AnnotationResult {
  AnnotationResult result;
  result.lines.reserve(lines.size());
  std::vector<std::optional<AnnotationFailure>> failures;
  failures.reserve(lines.size());

  for (const auto& line : lines) {
    auto failure = original_source.empty() || line.is_opaque
                       ? std::nullopt
                       : validateLine(line);
    result.lines.push_back(
        line.map([](const slang::parsing::Token& tok) -> FormatToken {
          return FormatToken{.token = tok};
        }));
    if (!failure) {
      annotateUnwrappedLine(result.lines.back());
      if (!original_source.empty() && !line.is_opaque) {
        for (const auto& token : result.lines.back().tokens) {
          if (token.type == TokenType::kUnknown) {
            failure = AnnotationFailure{
                .location = token.token.location(),
                .reason =
                    "unsupported token kind " +
                    std::string(slang::parsing::toString(token.token.kind))};
            break;
          }
        }
      }
    }
    failures.push_back(failure);
    if (failure) {
      result.warnings.push_back({
          .location = failure->location,
          .code = "invalid-annotation",
          .message = "cannot annotate SystemVerilog line (" + failure->reason +
                     "); original text preserved",
      });
    }
  }

  if (result.warnings.empty()) {
    return result;
  }

  std::vector<UnwrappedLine<FormatToken>> recovered;
  recovered.reserve(result.lines.size());
  size_t source_cursor = 0;
  for (size_t i = 0; i < result.lines.size();) {
    auto& line = result.lines.at(i);
    if (!failures.at(i)) {
      if (line.is_opaque) {
        source_cursor = std::min(source_cursor + line.raw_text.size(),
                                 original_source.size());
      } else if (!line.tokens.empty()) {
        const auto& last = line.tokens.back().token;
        source_cursor =
            std::min(last.location().offset() + last.rawText().size(),
                     original_source.size());
      }
      recovered.push_back(std::move(line));
      ++i;
      continue;
    }

    size_t next = i + 1;
    while (next < result.lines.size() &&
           (failures.at(next) ||
            !canResumeAt(result.lines.at(next), original_source,
                         line.tokens.front().token.location().offset()))) {
      ++next;
    }
    const size_t raw_end =
        next == result.lines.size()
            ? original_source.size()
            : lineStart(original_source, result.lines.at(next)
                                             .tokens.front()
                                             .token.location()
                                             .offset());
    if (raw_end > source_cursor) {
      recovered.push_back({
          .tokens = {},
          .raw_text = std::string(
              original_source.substr(source_cursor, raw_end - source_cursor)),
          .is_opaque = true,
      });
    }
    source_cursor = raw_end;
    i = next;
  }
  result.lines = std::move(recovered);
  return result;
}

auto TokenAnnotator::annotate(
    const std::vector<UnwrappedLine<slang::parsing::Token>>& lines,
    std::string_view original_source)
    -> std::vector<UnwrappedLine<FormatToken>> {
  return annotateWithDiagnostics(lines, original_source).lines;
}

}  // namespace format

#pragma once

#include <slang/syntax/SyntaxNode.h>

#include <concepts>
#include <cstdint>
#include <vector>

#include "format_style.h"

namespace format {

enum class PartitionPolicy : uint8_t {
  kAlwaysExpand,
  kFitOnLineElseExpand,
  kTabularAlignment,
  kAlreadyFormatted,
};

template <typename Token>
struct UnwrappedLine;

template <typename Token>
struct UnwrappedLineNode {
  Token token;
  std::vector<UnwrappedLine<Token>> children;

  auto map(std::invocable<Token&&> auto&& func) && -> UnwrappedLineNode<
      std::invoke_result_t<decltype(func), Token&&>> {
    using T = std::invoke_result_t<decltype(func), Token&&>;
    std::vector<UnwrappedLine<T>> children{};
    std::transform(
        std::make_move_iterator(this->children.begin()),
        std::make_move_iterator(this->children.end()),
        std::back_inserter(children),
        [&func](UnwrappedLine<Token>&& line) -> auto {
          return std::move(line).map(std::forward<decltype(func)>(func));
        });
    return {
        .token = std::forward<decltype(func)>(func)(std::move(this->token)),
        .children = std::move(children),
    };
  }

  auto map(std::invocable<const Token&> auto&& func) const
      -> UnwrappedLineNode<std::invoke_result_t<decltype(func), Token&&>> {
    using T = std::invoke_result_t<decltype(func), Token&&>;
    std::vector<UnwrappedLine<T>> children{};
    std::transform(
        this->children.begin(), this->children.end(),
        std::back_inserter(children),
        [&func](const UnwrappedLine<Token>& line) -> auto {
          return std::move(line).map(std::forward<decltype(func)>(func));
        });
    return {
        .token = std::forward<decltype(func)>(func)(this->token),
        .children = std::move(children),
    };
  }
};

template <typename Token>
struct UnwrappedLine {
  std::vector<UnwrappedLineNode<Token>> tokens;

  IndentLevel indentation_spaces;
  PartitionPolicy partition_policy;
  // todo

  auto map(std::invocable<Token&&> auto&& func) && -> UnwrappedLine<
      std::invoke_result_t<decltype(func), Token&&>> {
    using T = std::invoke_result_t<decltype(func), Token&&>;
    std::vector<UnwrappedLineNode<T>> tokens{};
    std::transform(
        std::make_move_iterator(this->tokens.begin()),
        std::make_move_iterator(this->tokens.end()), std::back_inserter(tokens),
        [&func](UnwrappedLineNode<Token>&& node) -> auto {
          return std::move(node).map(std::forward<decltype(func)>(func));
        });
    return {
        .tokens = std::move(tokens),
        .indentation_spaces = this->indentation_spaces,
        .partition_policy = this->partition_policy,
    };
  }

  auto map(std::invocable<const Token&> auto&& func) const
      -> UnwrappedLine<std::invoke_result_t<decltype(func), Token&&>> {
    using T = std::invoke_result_t<decltype(func), Token&&>;
    std::vector<UnwrappedLineNode<T>> tokens{};
    std::transform(this->tokens.begin(), this->tokens.end(),
                   std::back_inserter(tokens),
                   [&func](const UnwrappedLineNode<Token>& node) -> auto {
                     return node.map(std::forward<decltype(func)>(func));
                   });
    return {
        .tokens = std::move(tokens),
        .indentation_spaces = this->indentation_spaces,
        .partition_policy = this->partition_policy,
    };
  }
};

}  // namespace format

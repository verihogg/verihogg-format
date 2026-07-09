#include "pipeline/line_wrap_searcher.h"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <limits>
#include <map>
#include <queue>
#include <utility>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {

namespace {

inline constexpr size_t kNoParent = std::numeric_limits<size_t>::max();

struct Shape {
  ColumnNumber base_indent = 0;
  ColumnNumber column_limit = 0;
  IndentLevel wrap_spaces = 0;

  [[nodiscard]] static auto forLine(const UnwrappedLine<FormatToken>& line,
                                    const FormatStyle& style,
                                    ColumnNumber initial_column) -> Shape {
    return Shape{
        .base_indent = initial_column + line.indentation_spaces,
        .column_limit = style.column_limit,
        .wrap_spaces = style.wrap_spaces,
    };
  }
};

struct SearchNode {
  size_t parent = kNoParent;
  size_t next_token = 0;
  ColumnNumber current_column = 0;
  size_t cumulative_cost = 0;
  std::vector<ColumnNumber> wrap_columns;
  ColumnNumber spaces_before = 0;
  TokenAction action = TokenAction::kAppend;
  bool is_active = true;
};

struct StateKey {
  size_t next_token = 0;
  TokenAction last_action = TokenAction::kAppend;
  ColumnNumber current_column = 0;
  std::vector<ColumnNumber> wrap_columns;

  auto operator<=>(const StateKey&) const = default;
};

struct QueueEntry {
  size_t node_index = 0;
  size_t cumulative_cost = 0;
  ColumnNumber current_column = 0;
  size_t sequence = 0;
};

struct GoalNodeIndex {
  size_t value = 0;
};

struct TokenCount {
  size_t value = 0;
};

struct QueueCompare {
  [[nodiscard]] auto operator()(const QueueEntry& left,
                                const QueueEntry& right) const -> bool {
    if (left.cumulative_cost != right.cumulative_cost) {
      return left.cumulative_cost > right.cumulative_cost;
    }
    if (left.current_column != right.current_column) {
      return left.current_column > right.current_column;
    }
    return left.sequence > right.sequence;
  }
};

[[nodiscard]] auto tokenWidth(const FormatToken& token) -> ColumnNumber {
  return token.token.rawText().size();
}

[[nodiscard]] auto keyFor(const SearchNode& node) -> StateKey {
  return StateKey{.next_token = node.next_token,
                  .last_action = node.action,
                  .current_column = node.current_column,
                  .wrap_columns = node.wrap_columns};
}

auto closeGroupBalance(std::vector<ColumnNumber>& wrap_columns) -> void {
  if (wrap_columns.size() > 1) {
    wrap_columns.pop_back();
  }
}

auto openGroupBalance(std::vector<ColumnNumber>& wrap_columns,
                      const SearchNode& parent,
                      const FormatToken& previous_token, TokenAction action,
                      const Shape& shape) -> void {
  if (previous_token.balancing != GroupBalancing::kOpen) {
    return;
  }

  switch (action) {
    case TokenAction::kWrap:
      wrap_columns.push_back(parent.wrap_columns.back() + shape.wrap_spaces);
      break;
    case TokenAction::kAppend:
      wrap_columns.push_back(parent.current_column);
      break;
    case TokenAction::kPreserve:
      break;
  }
}

[[nodiscard]] auto addOverflowPenalty(size_t cost, ColumnNumber column,
                                      const Shape& shape,
                                      const FormatStyle& style) -> size_t {
  if (column <= shape.column_limit) {
    return cost;
  }
  return cost + style.over_column_limit_penalty + column - shape.column_limit;
}

[[nodiscard]] auto makeRoot(const UnwrappedLine<FormatToken>& line,
                            const Shape& shape) -> SearchNode {
  SearchNode root{
      .parent = kNoParent,
      .next_token = std::min<size_t>(line.tokens.size(), 1),
      .current_column = shape.base_indent,
      .cumulative_cost = 0,
      .wrap_columns = {shape.base_indent + shape.wrap_spaces},
  };

  if (!line.tokens.empty()) {
    root.current_column += tokenWidth(line.tokens.front());
  }
  return root;
}

[[nodiscard]] auto makeChild(const UnwrappedLine<FormatToken>& line,
                             const Shape& shape, const FormatStyle& style,
                             const SearchNode& parent, size_t parent_index,
                             TokenAction action) -> SearchNode {
  const size_t token_index = parent.next_token;
  const FormatToken& current_token = line.tokens.at(token_index);
  const FormatToken& previous_token = line.tokens.at(token_index - 1);

  SearchNode child{
      .parent = parent_index,
      .next_token = token_index + 1,
      .current_column = parent.current_column,
      .cumulative_cost = parent.cumulative_cost,
      .wrap_columns = parent.wrap_columns,
      .action = action,
  };

  bool opened_group = false;
  bool closed_group = false;

  if (action == TokenAction::kWrap) {
    if (current_token.balancing == GroupBalancing::kClose) {
      closeGroupBalance(child.wrap_columns);
      closed_group = true;
    }
    if (parent.action == TokenAction::kWrap) {
      openGroupBalance(child.wrap_columns, parent, previous_token, action,
                       shape);
      opened_group = true;
    }
  }

  const ColumnNumber width = tokenWidth(current_token);

  switch (action) {
    case TokenAction::kWrap: {
      const ColumnNumber token_start = child.wrap_columns.back();
      child.spaces_before = token_start;
      child.current_column = token_start + width;
      child.cumulative_cost += style.line_break_penalty;
      child.cumulative_cost += current_token.before.break_penalty;
      break;
    }
    case TokenAction::kAppend:
    case TokenAction::kPreserve:
      child.spaces_before = current_token.before.spaces_required;
      child.current_column =
          parent.current_column + current_token.before.spaces_required + width;
      child.cumulative_cost = addOverflowPenalty(
          child.cumulative_cost, child.current_column, shape, style);
      break;
  }

  if (!opened_group) {
    openGroupBalance(child.wrap_columns, parent, previous_token, action, shape);
  }

  if (!closed_group && current_token.balancing == GroupBalancing::kClose) {
    closeGroupBalance(child.wrap_columns);
  }

  return child;
}

auto enqueueNode(std::vector<SearchNode>& nodes,
                 std::priority_queue<QueueEntry, std::vector<QueueEntry>,
                                     QueueCompare>& worklist,
                 std::map<StateKey, size_t>& best_nodes, SearchNode node,
                 size_t& next_sequence) -> void {
  StateKey key = keyFor(node);
  const size_t node_index = nodes.size();
  const auto [best, inserted] =
      best_nodes.try_emplace(std::move(key), node_index);

  if (!inserted) {
    SearchNode& best_node = nodes.at(best->second);
    if (best_node.cumulative_cost <= node.cumulative_cost) {
      return;
    }
    best_node.is_active = false;
    best->second = node_index;
  }

  const size_t cumulative_cost = node.cumulative_cost;
  const ColumnNumber current_column = node.current_column;
  nodes.push_back(std::move(node));
  worklist.push(QueueEntry{.node_index = node_index,
                           .cumulative_cost = cumulative_cost,
                           .current_column = current_column,
                           .sequence = next_sequence++});
}

[[nodiscard]] auto reconstructDecisions(const std::vector<SearchNode>& nodes,
                                        GoalNodeIndex goal_index,
                                        TokenCount token_count)
    -> std::vector<InterTokenDecision> {
  std::vector<InterTokenDecision> decisions(token_count.value);
  if (token_count.value == 0) {
    return decisions;
  }

  decisions.front() =
      InterTokenDecision{.spaces_before = 0, .action = TokenAction::kAppend};

  size_t index = goal_index.value;
  while (index != kNoParent) {
    const SearchNode& node = nodes.at(index);
    if (node.next_token > 0 && node.next_token <= token_count.value) {
      decisions.at(node.next_token - 1) = InterTokenDecision{
          .spaces_before = node.spaces_before,
          .action = node.action,
      };
    }
    index = node.parent;
  }

  return decisions;
}

[[nodiscard]] auto appendOnlyDecisions(const UnwrappedLine<FormatToken>& line)
    -> std::vector<InterTokenDecision> {
  std::vector<InterTokenDecision> decisions;
  decisions.reserve(line.tokens.size());

  if (line.tokens.empty()) {
    return decisions;
  }

  decisions.push_back(
      InterTokenDecision{.spaces_before = 0, .action = TokenAction::kAppend});

  for (size_t i = 1; i < line.tokens.size(); ++i) {
    decisions.push_back(InterTokenDecision{
        .spaces_before = line.tokens.at(i).before.spaces_required,
        .action = TokenAction::kAppend,
    });
  }
  return decisions;
}

}  // namespace

[[nodiscard]] auto searchLineWraps(const UnwrappedLine<FormatToken>& line,
                                   const FormatStyle& style,
                                   ColumnNumber initial_column)
    -> std::vector<InterTokenDecision> {
  if (line.tokens.empty()) {
    return {};
  }

  const Shape shape = Shape::forLine(line, style, initial_column);

  std::vector<SearchNode> nodes;
  nodes.reserve(line.tokens.size() * 2);

  std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueCompare>
      worklist;
  std::map<StateKey, size_t> best_nodes;
  size_t next_sequence = 0;

  enqueueNode(nodes, worklist, best_nodes, makeRoot(line, shape),
              next_sequence);

  while (!worklist.empty()) {
    const QueueEntry entry = worklist.top();
    worklist.pop();

    if (!nodes.at(entry.node_index).is_active) {
      continue;
    }

    const size_t next_token = nodes.at(entry.node_index).next_token;
    if (next_token == line.tokens.size()) {
      return reconstructDecisions(nodes,
                                  GoalNodeIndex{.value = entry.node_index},
                                  TokenCount{.value = line.tokens.size()});
    }

    const BreakDecision break_decision =
        line.tokens.at(next_token).before.break_decision;

    if (break_decision != BreakDecision::kMustBreak) {
      SearchNode child =
          makeChild(line, shape, style, nodes.at(entry.node_index),
                    entry.node_index, TokenAction::kAppend);
      enqueueNode(nodes, worklist, best_nodes, std::move(child), next_sequence);
    }

    if (break_decision != BreakDecision::kMustNotBreak) {
      SearchNode child =
          makeChild(line, shape, style, nodes.at(entry.node_index),
                    entry.node_index, TokenAction::kWrap);
      enqueueNode(nodes, worklist, best_nodes, std::move(child), next_sequence);
    }
  }

  return appendOnlyDecisions(line);
}

auto applyLineWraps(std::vector<UnwrappedLine<FormatToken>>& lines,
                    const FormatStyle& style) -> void {
  for (auto& line : lines) {
    const std::vector<InterTokenDecision> decisions =
        line.partition_policy == PartitionPolicy::kAlreadyFormatted
            ? appendOnlyDecisions(line)
            : searchLineWraps(line, style);

    for (size_t i = 0; i < line.tokens.size(); ++i) {
      line.tokens.at(i).decision = decisions.at(i);
    }
  }
}
}  // namespace format

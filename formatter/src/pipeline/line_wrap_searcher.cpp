#include "pipeline/line_wrap_searcher.h"

#include <limits>
#include <vector>

#include "data/format_style.h"
#include "data/format_token.h"
#include "data/unwrapped_line.h"

namespace format {

[[nodiscard]] auto searchLineWraps(const UnwrappedLine<FormatToken>& line,
                                   const FormatStyle& style,
                                   ColumnNumber initial_column)
    -> std::vector<InterTokenDecision> {
  const auto& tokens = line.tokens;
  const size_t n = tokens.size();

  if (n == 0)
    return {};

  auto noWrap = [&]() -> std::vector<InterTokenDecision> {
    std::vector<InterTokenDecision> dec(n);
    dec[0] = {.spaces_before = 0, .action = TokenAction::kAppend};
    for (size_t i = 1; i < n; ++i) {
      dec[i] = {.spaces_before = tokens[i].before.spaces_required,
                .action = TokenAction::kAppend};
    }
    return dec;
  };

  if (n == 1)
    return noWrap();
  if (line.partition_policy == PartitionPolicy::kAlreadyFormatted)
    return noWrap();

  std::vector<size_t> width(n);
  for (size_t i = 0; i < n; ++i)
    width[i] = tokens[i].token.rawText().size();

  std::vector<size_t> cum_width(n + 1, 0);
  for (size_t i = 0; i < n; ++i) {
    cum_width[i + 1] = cum_width[i] + width[i];
    if (i + 1 < n)
      cum_width[i + 1] += tokens[i + 1].before.spaces_required;
  }

  const size_t base_indent =
      (initial_column > 0) ? initial_column : line.indentation_spaces;

  // Check if any forced break exists; if so, we can't short-circuit.
  bool has_any_forced_break = false;
  for (size_t i = 1; i < n; ++i) {
    if (tokens[i].before.break_decision == BreakDecision::kMustBreak) {
      has_any_forced_break = true;
      break;
    }
  }
  if (!has_any_forced_break && base_indent + cum_width[n] <= style.column_limit)
    return noWrap();

  std::vector<bool> must_break(n, false);
  std::vector<bool> must_not_break(n, false);
  std::vector<size_t> break_penalty(n, 0);
  for (size_t i = 1; i < n; ++i) {
    must_break[i] =
        (tokens[i].before.break_decision == BreakDecision::kMustBreak);
    must_not_break[i] =
        (tokens[i].before.break_decision == BreakDecision::kMustNotBreak);
    break_penalty[i] = tokens[i].before.break_penalty;
  }

  const size_t wrap_indent = base_indent + style.wrap_spaces;

  constexpr size_t kInf = std::numeric_limits<size_t>::max();
  std::vector<size_t> dp(n + 1, kInf);
  std::vector<size_t> prev(n + 1, static_cast<size_t>(-1));
  dp[0] = 0;
  prev[0] = 0;

  for (size_t i = 0; i < n; ++i) {
    if (dp[i] == kInf)
      continue;

    const size_t line_indent = (i == 0) ? base_indent : wrap_indent;

    for (size_t j = i + 1; j <= n; ++j) {
      bool has_forced_break = false;
      for (size_t k = i + 1; k < j; ++k) {
        if (must_break[k]) {
          has_forced_break = true;
          break;
        }
      }
      if (has_forced_break)
        break;

      if (j < n && must_not_break[j])
        continue;

      const size_t line_width = line_indent + (cum_width[j] - cum_width[i]);

      size_t cost = 0;
      if (line_width > style.column_limit) {
        cost += style.over_column_limit_penalty *
                (line_width - style.column_limit);
      }

      if (j < n) {
        cost += break_penalty[j];
        cost += style.line_break_penalty;
      }

      const size_t new_cost = dp[i] + cost;
      if (new_cost < dp[j]) {
        dp[j] = new_cost;
        prev[j] = i;
      }

      if (j < n && must_break[j])
        break;
    }
  }

  std::vector<bool> break_before(n, false);
  if (dp[n] != kInf) {
    size_t pos = n;
    while (pos > 0) {
      const size_t start = prev[pos];
      if (start > 0)
        break_before[start] = true;
      pos = start;
    }
  } else {
    size_t cur_width = base_indent;
    for (size_t i = 0; i < n; ++i) {
      cur_width += width[i];
      if (i + 1 < n) {
        const size_t next_spaces = tokens[i + 1].before.spaces_required;
        if (must_break[i + 1] ||
            cur_width + next_spaces + width[i + 1] > style.column_limit) {
          break_before[i + 1] = true;
          cur_width = wrap_indent;
        } else {
          cur_width += next_spaces;
        }
      }
    }
  }

  std::vector<InterTokenDecision> decisions(n);
  for (size_t i = 0; i < n; ++i) {
    if (break_before[i]) {
      decisions[i] = {.spaces_before = 0, .action = TokenAction::kWrap};
    } else {
      decisions[i] = {.spaces_before = (i == 0) ? 0
                                                : tokens[i].before.spaces_required,
                      .action = TokenAction::kAppend};
    }
  }

  return decisions;
}

void optimizeLineWraps(std::vector<UnwrappedLine<FormatToken>>& lines,
                       const FormatStyle& style) {
  for (auto& line : lines) {
    if (line.partition_policy == PartitionPolicy::kAlreadyFormatted)
      continue;
    if (line.tokens.empty())
      continue;

    // Don't wrap `define macro bodies — the body tokens (including
    // backtick-referenced macros as Directive tokens) must stay on one line.
    if (line.tokens.front().type == TokenType::kDirective &&
        line.tokens.front().token.rawText() == "`define")
      continue;

    auto decisions = searchLineWraps(line, style, 0);
    for (size_t i = 0; i < line.tokens.size() && i < decisions.size(); ++i) {
      line.tokens[i].decision = decisions[i];
    }
  }
}

}  // namespace format

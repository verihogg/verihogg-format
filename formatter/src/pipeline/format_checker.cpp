#include "pipeline/format_checker.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <utility>

#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/unified_diff.h"

namespace format {

namespace fs = std::filesystem;

auto FormatChecker::checkFile(const fs::path& path, bool include_diff) const
    -> CheckResult {
  std::ifstream f{path, std::ios::binary};
  if (!f) {
    return {.status = CheckStatus::kUnreadable};
  }
  std::string original;
  try {
    original.assign(std::istreambuf_iterator<char>{f},
                    std::istreambuf_iterator<char>{});
  } catch (const std::ios_base::failure&) {
    return {.status = CheckStatus::kUnreadable};
  }

  if (f.bad()) {
    return {.status = CheckStatus::kUnreadable};
  }

  // Lex the text already in memory so the file is read only once.
  LexContext ctx;
  auto tokens = ctx.lex_string(original);
  auto result = format::format(tokens, style_, ctx.source_text());

  CheckResult check{.status = result.formatted_text == original
                                  ? CheckStatus::kClean
                                  : CheckStatus::kDirty,
                    .warnings = std::move(result.warnings)};
  if (include_diff && check.status == CheckStatus::kDirty) {
    const auto label =
        path.is_absolute() ? path.lexically_relative(fs::current_path()) : path;
    check.diff = makeUnifiedDiff(original, result.formatted_text, label);
  }
  return check;
}

}  // namespace format

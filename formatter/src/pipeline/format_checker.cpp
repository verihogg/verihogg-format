#include "pipeline/format_checker.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <utility>

#include "data/lex_context.h"
#include "formatter.h"

namespace format {

namespace fs = std::filesystem;

auto FormatChecker::checkFile(const fs::path& path) const -> CheckResult {
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
  return check;
}

}  // namespace format

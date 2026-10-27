#include "pipeline/format_checker.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/cache_key.h"
#include "pipeline/cache_manager.h"

namespace format {

namespace fs = std::filesystem;

auto FormatChecker::checkFile(const fs::path& path, const CacheManager* cache,
                              std::string_view build_id) const -> CheckResult {
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

  std::optional<std::string> key;
  bool cache_error = cache != nullptr && build_id.empty();
  if (cache != nullptr && !build_id.empty()) {
    try {
      key = makeCacheKey(original, style_, build_id);
      if (cache->contains(*key)) {
        return {.status = CheckStatus::kClean, .cache_hit = true};
      }
    } catch (const std::runtime_error&) {
      key.reset();
      cache_error = true;
    }
  }

  // Lex the text already in memory so the file is read only once.
  LexContext ctx;
  auto tokens = ctx.lex_string(original);
  auto result = format::format(tokens, style_, ctx.source_text());

  CheckResult check{.status = result.formatted_text == original
                                  ? CheckStatus::kClean
                                  : CheckStatus::kDirty,
                    .warnings = std::move(result.warnings)};
  check.cache_error = cache_error;
  if (key && check.status == CheckStatus::kClean && check.warnings.empty()) {
    try {
      check.cache_error = !cache->store(*key);
    } catch (const std::runtime_error&) {
      check.cache_error = true;
    }
  }
  return check;
}

}  // namespace format

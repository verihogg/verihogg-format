#include "pipeline/format_checker.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>

#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/cache_manager.h"
#include "pipeline/hash_utils.h"

namespace format {

namespace {

namespace fs = std::filesystem;

constexpr auto kRacyWindow = std::chrono::seconds{2};

struct FileStat {
  uint64_t size = 0;
  int64_t mtime_ns = FileStamp::kUnknownMtime;
};

auto statFile(const fs::path& path) -> std::optional<FileStat> {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec) {
    return std::nullopt;
  }
  const auto mtime = fs::last_write_time(path, ec);
  if (ec) {
    return std::nullopt;
  }

  FileStat stat{.size = size};
  if (fs::file_time_type::clock::now() - mtime >= kRacyWindow) {
    stat.mtime_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        mtime.time_since_epoch())
                        .count();
  }
  return stat;
}

}  // namespace

auto FormatChecker::checkFile(const fs::path& path,
                              const CacheManager* cache) const -> CheckResult {
  const auto stat = cache != nullptr ? statFile(path) : std::nullopt;
  const FileStamp* cached = stat ? cache->find(path) : nullptr;

  if (stat && cached != nullptr && stat->mtime_ns != FileStamp::kUnknownMtime &&
      cached->size == stat->size && cached->mtime_ns == stat->mtime_ns) {
    return {.status = CheckStatus::kClean, .stamp = *cached};
  }

  std::ifstream f{path, std::ios::binary};
  if (!f) {
    return {.status = CheckStatus::kUnreadable};
  }
  const std::string original{std::istreambuf_iterator<char>{f},
                             std::istreambuf_iterator<char>{}};

  std::optional<FileStamp> stamp;
  if (stat) {
    stamp = FileStamp{.size = stat->size,
                      .mtime_ns = stat->mtime_ns,
                      .content_hash = fnv1a64(original)};
    if (cached != nullptr && cached->size == stamp->size &&
        cached->content_hash == stamp->content_hash) {
      return {.status = CheckStatus::kClean, .stamp = stamp};
    }
  }

  // Lex the text already in memory so the file is read only once.
  LexContext ctx;
  auto tokens = ctx.lex_string(original);
  auto result = format::format(tokens, style_);

  CheckResult check{.status = result.formatted_text == original
                                  ? CheckStatus::kClean
                                  : CheckStatus::kDirty,
                    .warnings = std::move(result.warnings)};
  if (check.status == CheckStatus::kClean && check.warnings.empty()) {
    check.stamp = stamp;
  }
  return check;
}

}  // namespace format

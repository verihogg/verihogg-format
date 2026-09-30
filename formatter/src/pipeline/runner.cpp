#include "pipeline/runner.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <gsl/span>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "data/format_style.h"
#include "data/format_warning.h"
#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/cache_manager.h"
#include "pipeline/format_checker.h"

namespace format {

namespace {

auto writeFile(const std::filesystem::path& path, std::string_view content)
    -> void {
  std::ofstream f{path, std::ios::binary | std::ios::trunc};
  if (!f) {
    throw std::runtime_error("Cannot open: " + std::string{path});
  }
  f << content;
}

auto readFile(const std::filesystem::path& path) -> std::string {
  std::ifstream f{path, std::ios::binary};
  if (!f) {
    throw std::runtime_error("Cannot open: " + std::string{path});
  }
  return {std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
}

auto printWarning(std::ostream& os, std::string_view path,
                  const FormatWarning& warning) -> void {
  os << "Warning";
  if (!path.empty()) {
    os << " in " << path;
  }
  os << ": " << warning.message << " [" << warning.code << "]\n";
}

// Checks files on a pool of worker threads. Results are stored by file index,
// so the output order does not depend on thread scheduling.
auto checkInParallel(gsl::span<const std::filesystem::path> files,
                     const FormatChecker& checker, const CacheManager* cache)
    -> std::vector<CheckResult> {
  std::vector<CheckResult> results(files.size());
  std::atomic<size_t> next{0};
  std::exception_ptr error;
  std::mutex error_mutex;

  auto worker = [&] {
    try {
      for (size_t i = next++; i < files.size(); i = next++) {
        results.at(i) = checker.checkFile(files[i], cache);
      }
    } catch (...) {
      const std::scoped_lock lock{error_mutex};
      if (!error) {
        error = std::current_exception();
      }
      next = files.size();
    }
  };

  const size_t thread_count = std::min<size_t>(
      std::max(1U, std::thread::hardware_concurrency()), files.size());
  {
    std::vector<std::jthread> threads;
    threads.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
      threads.emplace_back(worker);
    }
  }  // threads join here

  if (error) {
    std::rethrow_exception(error);
  }
  return results;
}

// Returns the number of files that need formatting or could not be read.
auto runCheck(gsl::span<const std::filesystem::path> files,
              const format::FormatStyle& style, const RunConfig& run,
              Streams streams) -> int {
  std::optional<CacheManager> cache;
  if (run.cache_file.has_value()) {
    cache.emplace(*run.cache_file, makeCacheKey(style));
    if (!cache->load()) {
      *streams.err << "Warning: ignoring corrupted cache "
                   << run.cache_file->string() << "\n";
    }
  }

  const FormatChecker checker{style};
  const auto results =
      checkInParallel(files, checker, cache ? &*cache : nullptr);
  int failed = 0;
  for (size_t i = 0; i < files.size(); ++i) {
    const auto& path = files[i];
    const auto& result = results.at(i);
    for (const auto& warning : result.warnings) {
      printWarning(*streams.err, path.string(), warning);
    }

    switch (result.status) {
      case CheckStatus::kClean:
        break;
      case CheckStatus::kDirty:
        *streams.err << "Needs formatting: " << path.string() << "\n";
        ++failed;
        break;
      case CheckStatus::kUnreadable:
        *streams.err << "Error: cannot read " << path.string() << "\n";
        ++failed;
        break;
    }
    if (cache && result.stamp) {
      cache->update(path, *result.stamp);
    }
  }

  if (cache && !cache->save()) {
    *streams.err << "Warning: cannot write cache " << cache->file().string()
                 << "\n";
  }
  return failed;
}

}  // namespace
auto runFormatter(gsl::span<const std::filesystem::path> files,
                  const format::FormatStyle& style,
                  const format::RunConfig& run, Streams streams) -> int {
  if (run.check) {
    return runCheck(files, style, run, streams);
  }

  int warnings = 0;
  for (const auto& path : files) {
    LexContext ctx;
    auto tokens = ctx.lex_file(path);
    if (tokens.empty()) {
      *streams.err << "Warning: no tokens in " << path << "\n";
      ++warnings;
      continue;
    }

    auto result = format::format(tokens, style);
    for (const auto& warning : result.warnings) {
      printWarning(*streams.err, path.string(), warning);
      ++warnings;
    }

    if (run.inplace) {
      // Skip the write when nothing changed to avoid needless disk writes.
      if (readFile(path) != result.formatted_text) {
        writeFile(path, result.formatted_text);
      }
    } else {
      *streams.out << result.formatted_text;
    }
  }
  return warnings;
}
}  // namespace format

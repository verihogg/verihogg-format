#include "pipeline/runner.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <gsl/span>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

#include "data/format_style.h"
#include "data/format_warning.h"
#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/cache_build_id.h"
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

// Workers only write their own result slot; diagnostics are emitted after
// joining so their order follows the input files rather than scheduling.
auto checkInParallel(gsl::span<const std::filesystem::path> files,
                     gsl::span<const FormatStyle> styles,
                     const CacheManager* cache, std::string_view build_id)
    -> std::vector<CheckResult> {
  std::vector<CheckResult> results(files.size());
  std::atomic<size_t> next{0};
  std::atomic<bool> stop{false};
  std::exception_ptr error;
  std::mutex error_mutex;

  auto worker = [&] {
    try {
      while (!stop.load()) {
        const size_t i = next.fetch_add(1);
        if (i >= files.size()) {
          break;
        }
        results.at(i) =
            FormatChecker(styles[i]).checkFile(files[i], cache, build_id);
      }
    } catch (...) {
      const std::scoped_lock lock{error_mutex};
      if (!error) {
        error = std::current_exception();
      }
      stop.store(true);
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
  }
  if (error) {
    std::rethrow_exception(error);
  }
  return results;
}

}  // namespace

auto printWarning(std::ostream& os, std::string_view path,
                  const FormatWarning& warning) -> void {
  os << "Warning";
  if (!path.empty()) {
    os << " in " << path;
  }
  os << ": " << warning.message << " [" << warning.code << "]\n";
}

auto runFormatter(gsl::span<const std::filesystem::path> files,
                  gsl::span<const FormatStyle> styles, const RunConfig& run,
                  Streams streams) -> int {
  if (run.check) {
    std::optional<CacheManager> cache;
    std::string_view build_id;
    if (run.cache_directory) {
      const auto& identity = formatterBuildId();
      if (identity) {
        cache.emplace(*run.cache_directory);
        build_id = *identity;
      } else {
        *streams.err
            << "Warning: cannot identify formatter build; cache disabled\n";
      }
    }
    const auto results =
        checkInParallel(files, styles, cache ? &*cache : nullptr, build_id);
    int failed = 0;
    for (size_t i = 0; i < files.size(); ++i) {
      const auto& result = results.at(i);
      if (result.cache_error) {
        *streams.err << "Warning: cannot use cache for " << files[i].string()
                     << "\n";
      }
      for (const auto& warning : result.warnings) {
        printWarning(*streams.err, files[i].string(), warning);
      }
      switch (result.status) {
        case CheckStatus::kClean:
          break;
        case CheckStatus::kDirty:
          *streams.err << "Needs formatting: " << files[i].string() << "\n";
          ++failed;
          break;
        case CheckStatus::kUnreadable:
          *streams.err << "Error: cannot read " << files[i].string() << "\n";
          ++failed;
          break;
      }
    }
    return failed;
  }

  int warnings = 0;
  for (size_t i = 0; i < files.size(); ++i) {
    const auto& path = files[i];
    LexContext ctx;
    auto tokens = ctx.lex_file(path);
    if (tokens.empty()) {
      *streams.err << "Warning: no tokens in " << path << "\n";
      ++warnings;
      continue;
    }

    auto result = format::format(tokens, styles[i], ctx.source_text());
    for (const auto& warning : result.warnings) {
      printWarning(*streams.err, path.string(), warning);
      ++warnings;
    }

    if (run.inplace) {
      writeFile(path, result.formatted_text);
    } else {
      *streams.out << result.formatted_text;
    }
  }
  return warnings;
}
}  // namespace format

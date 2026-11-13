#include "pipeline/runner.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <gsl/span>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "data/format_style.h"
#include "data/format_warning.h"
#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/format_checker.h"
#include "pipeline/summary.h"

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
                     gsl::span<const FormatStyle> styles)
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
        results.at(i) = FormatChecker(styles[i]).checkFile(files[i]);
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

// Converts diagnostics of one file into (construct, line) pairs for the
// summary aggregation.
[[nodiscard]] auto toSummaryItems(LexContext& ctx,
                                  const std::vector<FormatWarning>& warnings)
    -> std::vector<std::pair<std::string, std::size_t>> {
  std::vector<std::pair<std::string, std::size_t>> items;
  items.reserve(warnings.size());
  for (const auto& warning : warnings) {
    std::size_t line = warning.formatted_line;
    if (line == 0 && warning.location.valid()) {
      line = ctx.source_manager().getLineNumber(warning.location);
    }
    std::string construct =
        warning.construct.empty() ? warning.code : warning.construct;
    items.emplace_back(std::move(construct), line);
  }
  return items;
}

// Same as above, for callers without a LexContext (--check): relies on the
// output line already being attached to the warning.
[[nodiscard]] auto toSummaryItems(const std::vector<FormatWarning>& warnings)
    -> std::vector<std::pair<std::string, std::size_t>> {
  std::vector<std::pair<std::string, std::size_t>> items;
  items.reserve(warnings.size());
  for (const auto& warning : warnings) {
    std::string construct =
        warning.construct.empty() ? warning.code : warning.construct;
    items.emplace_back(std::move(construct), warning.formatted_line);
  }
  return items;
}

}  // namespace

auto runFormatter(gsl::span<const std::filesystem::path> files,
                  gsl::span<const FormatStyle> styles, const RunConfig& run,
                  Streams streams) -> int {
  if (run.check) {
    const auto results = checkInParallel(files, styles);
    int failed = 0;
    bool has_warnings = false;
    std::vector<summary::FileReport> reports;
    reports.reserve(files.size());
    for (size_t i = 0; i < files.size(); ++i) {
      const auto& result = results.at(i);
      has_warnings = has_warnings || !result.warnings.empty();
      reports.push_back(summary::makeFileReport(
          files[i].string(), toSummaryItems(result.warnings)));
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
    if (has_warnings) {
      summary::printSummary(
          *streams.err, reports,
          {.inplace = false, .links = !run.no_links, .file_status = false});
    }
    return failed;
  }

  int warnings = 0;
  std::vector<summary::FileReport> reports;
  reports.reserve(files.size());
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
    warnings += static_cast<int>(result.warnings.size());
    reports.push_back(summary::makeFileReport(
        path.string(), toSummaryItems(ctx, result.warnings)));

    if (run.inplace) {
      writeFile(path, result.formatted_text);
    } else {
      *streams.out << result.formatted_text;
    }
  }

  summary::printSummary(*streams.err, reports,
                        {.inplace = run.inplace, .links = !run.no_links});
  return warnings;
}
}  // namespace format

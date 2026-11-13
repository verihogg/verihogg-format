#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace format::summary {

// Aggregated occurrences of one construct/cause inside a single file.
struct ConstructStat {
  std::string construct;
  std::size_t count = 0;
  std::size_t first_line = 0;  // 1-based; 0 when the location is unknown
};

// Per-file report. constructs is sorted by count descending.
struct FileReport {
  std::string path;  // path as shown to the user
  std::vector<ConstructStat> constructs;
};

struct SummaryConfig {
  bool inplace = false;
  bool links = true;
  bool file_status = true;  // print the per-file "processed"/"written" lines
};

// application/x-www-form-urlencoded percent-encoding (space becomes '+').
[[nodiscard]] auto percentEncode(std::string_view text) -> std::string;

// Groups (construct, line) pairs for one file: counts occurrences and keeps
// the earliest line per construct. Result is sorted by count descending.
[[nodiscard]] auto makeFileReport(
    std::string path, std::vector<std::pair<std::string, std::size_t>> items)
    -> FileReport;

// Pre-filled GitHub issue URLs for the end-of-run summary. Both pre-fill the
// title and the environment field of the matching issue form; the unsupported
// URL additionally pre-fills the aggregated construct list.
[[nodiscard]] auto buildUnsupportedUrl(const std::vector<FileReport>& reports)
    -> std::string;
[[nodiscard]] auto buildWrongFormattingUrl() -> std::string;

// Prints the single end-of-run message to os (stderr): one status line per
// file, then — when there are diagnostics — the explanation, the grouped
// list and the issue links, then the privacy note.
void printSummary(std::ostream& os, const std::vector<FileReport>& reports,
                  const SummaryConfig& config);

}  // namespace format::summary

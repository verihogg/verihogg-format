#include "pipeline/summary.h"

#include <algorithm>
#include <ostream>
#include <string>

#ifndef VERIHOGG_VERSION
#define VERIHOGG_VERSION "unknown"
#endif

namespace format::summary {

namespace {

constexpr std::string_view kUnsupportedTemplate =
    "https://github.com/verihogg/verihogg-format/issues/new"
    "?template=unsupported_construct.yml";
constexpr std::string_view kWrongFormattingTemplate =
    "https://github.com/verihogg/verihogg-format/issues/new"
    "?template=wrong_formatting.yml";

constexpr std::string_view kTitleParam = "&title=";
constexpr std::string_view kEnvironmentParam = "&environment=";
constexpr std::string_view kConstructsParam = "&constructs=";

// Target upper bound for a single issue URL.
constexpr std::size_t kMaxUrlLength = 1500;

struct Entry {
  const FileReport* report;
  const ConstructStat* stat;
};

[[nodiscard]] auto osName() -> std::string {
#if defined(__linux__)
  return "Linux";
#elif defined(__APPLE__)
  return "macOS";
#elif defined(_WIN32)
  return "Windows";
#else
  return "Unknown";
#endif
}

[[nodiscard]] auto baseName(std::string_view path) -> std::string_view {
  const auto pos = path.find_last_of("/\\");
  if (pos == std::string_view::npos) {
    return path;
  }
  return path.substr(pos + 1);
}

// A shared basename would make the location ambiguous, so reports whose
// basename occurs more than once are shown with their full path instead.
[[nodiscard]] auto usesFullPath(const std::vector<FileReport>& reports,
                                std::string_view base) -> bool {
  std::size_t matches = 0;
  for (const auto& report : reports) {
    if (report.constructs.empty() || baseName(report.path) != base) {
      continue;
    }
    if (++matches > 1) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] auto pad(std::string_view text, std::size_t width)
    -> std::string {
  std::string out{text};
  if (out.size() < width) {
    out.append(width - out.size(), ' ');
  }
  return out;
}

// Flattens reports into entries in file order (already sorted by count
// within each file).
[[nodiscard]] auto collectEntries(const std::vector<FileReport>& reports)
    -> std::vector<Entry> {
  std::vector<Entry> entries;
  for (const auto& report : reports) {
    for (const auto& stat : report.constructs) {
      entries.push_back({&report, &stat});
    }
  }
  return entries;
}

[[nodiscard]] auto environmentBlock() -> std::string {
  return "Formatter version: " + std::string{VERIHOGG_VERSION} +
         "\nOS: " + osName() + "\n";
}

// Aggregated construct counts across all files: one row per construct name,
// sorted by count descending (ties broken alphabetically).
[[nodiscard]] auto aggregateConstructs(const std::vector<FileReport>& reports)
    -> std::vector<std::pair<std::string, std::size_t>> {
  std::vector<std::pair<std::string, std::size_t>> aggregated;
  for (const auto& report : reports) {
    for (const auto& stat : report.constructs) {
      auto it = std::find_if(
          aggregated.begin(), aggregated.end(),
          [&](const auto& item) { return item.first == stat.construct; });
      if (it == aggregated.end()) {
        aggregated.emplace_back(stat.construct, stat.count);
      } else {
        it->second += stat.count;
      }
    }
  }
  std::sort(aggregated.begin(), aggregated.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.second != rhs.second) {
                return lhs.second > rhs.second;
              }
              return lhs.first < rhs.first;
            });
  return aggregated;
}

// Assembles the issue URL: pre-filled title and environment first, then the
// aggregated construct lines for as long as the encoded URL stays within the
// limit. Space for the marker is reserved while filling, so a cut-off list is
// always marked with an ellipsis.
[[nodiscard]] auto buildIssueUrl(std::string_view prefix,
                                 const std::string& title,
                                 const std::vector<std::string>& constructs)
    -> std::string {
  std::string url{prefix};
  url += kTitleParam;
  url += percentEncode(title);
  url += kEnvironmentParam;
  url += percentEncode(environmentBlock());

  const std::string marker = percentEncode("...\n");
  const std::size_t used = url.size() + kConstructsParam.size();
  const std::size_t budget = used <= kMaxUrlLength ? kMaxUrlLength - used : 0;

  std::string encoded;
  for (std::size_t i = 0; i < constructs.size(); ++i) {
    const std::string part = percentEncode(constructs.at(i));
    // Keep room for the truncation marker unless this is the last line: if
    // everything fits, no marker is needed.
    const std::size_t reserve = i + 1 < constructs.size() ? marker.size() : 0;
    if (encoded.size() + part.size() + reserve > budget) {
      if (encoded.size() + marker.size() <= budget) {
        encoded += marker;
      }
      break;
    }
    encoded += part;
  }
  if (!encoded.empty()) {
    url += kConstructsParam;
    url += encoded;
  }
  return url;
}

}  // namespace

auto percentEncode(std::string_view text) -> std::string {
  constexpr std::string_view kHex = "0123456789ABCDEF";
  constexpr std::size_t kNibbleBits = 4;
  constexpr std::size_t kNibbleMask = 15;
  std::string out;
  out.reserve(text.size());
  for (const unsigned char c : text) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      out.push_back(static_cast<char>(c));
    } else if (c == ' ') {
      out.push_back('+');
    } else {
      const auto value = static_cast<std::size_t>(c);
      out.push_back('%');
      out.push_back(kHex.at(value >> kNibbleBits));
      out.push_back(kHex.at(value & kNibbleMask));
    }
  }
  return out;
}

auto makeFileReport(std::string path,
                    std::vector<std::pair<std::string, std::size_t>> items)
    -> FileReport {
  FileReport report{.path = std::move(path), .constructs = {}};
  for (auto& [construct, line] : items) {
    auto it = std::find_if(
        report.constructs.begin(), report.constructs.end(),
        [&](const ConstructStat& s) { return s.construct == construct; });
    if (it == report.constructs.end()) {
      report.constructs.push_back(
          {.construct = std::move(construct), .count = 1, .first_line = line});
    } else {
      ++it->count;
      if (line != 0 && (it->first_line == 0 || line < it->first_line)) {
        it->first_line = line;
      }
    }
  }
  std::sort(report.constructs.begin(), report.constructs.end(),
            [](const ConstructStat& a, const ConstructStat& b) {
              if (a.count != b.count) {
                return a.count > b.count;
              }
              return a.construct < b.construct;
            });
  return report;
}

auto buildUnsupportedUrl(const std::vector<FileReport>& reports)
    -> std::string {
  const auto aggregated = aggregateConstructs(reports);
  if (aggregated.empty()) {
    return {};
  }

  const std::string title =
      aggregated.size() == 1
          ? "Unsupported construct: '" + aggregated.front().first + "'"
          : "Unsupported constructs";

  std::vector<std::string> lines;
  lines.reserve(aggregated.size());
  for (const auto& [construct, count] : aggregated) {
    lines.push_back("- " + construct + "  x" + std::to_string(count) + "\n");
  }
  return buildIssueUrl(kUnsupportedTemplate, title, lines);
}

auto buildWrongFormattingUrl() -> std::string {
  return buildIssueUrl(kWrongFormattingTemplate, "Wrong formatting report", {});
}

void printSummary(std::ostream& os, const std::vector<FileReport>& reports,
                  const SummaryConfig& config) {
  if (config.file_status) {
    const char* verb = config.inplace ? "written" : "processed";
    for (const auto& report : reports) {
      os << "verihogg-format: " << report.path << ' ' << verb;
      if (!report.constructs.empty()) {
        os << " (some regions left unchanged)";
      }
      os << ".\n";
    }
  }

  const auto entries = collectEntries(reports);

  if (!entries.empty()) {
    os << '\n'
       << "The formatter does not support these constructs yet, so it left "
          "those\n"
          "parts exactly as written. This is a current limitation of the "
          "formatter,\n"
          "not necessarily a problem in your source.\n\n";

    std::size_t name_width = 0;
    std::size_t label_width = 0;
    for (const auto& entry : entries) {
      name_width = std::max(name_width, entry.stat->construct.size());
      const std::size_t occurrences = entry.stat->count;
      const std::string label =
          std::to_string(occurrences) +
          (occurrences == 1 ? " occurrence," : " occurrences,");
      label_width = std::max(label_width, label.size());
    }
    for (const auto& entry : entries) {
      const std::size_t occurrences = entry.stat->count;
      const std::string label =
          std::to_string(occurrences) +
          (occurrences == 1 ? " occurrence," : " occurrences,");
      const auto base = baseName(entry.report->path);
      std::string location =
          usesFullPath(reports, base) ? entry.report->path : std::string{base};
      if (entry.stat->first_line != 0) {
        location += ":" + std::to_string(entry.stat->first_line);
      }
      os << "  - " << pad(entry.stat->construct, name_width) << "  "
         << pad(label, label_width) << " first at " << location << '\n';
    }
  }

  if (!config.links) {
    return;
  }

  if (!entries.empty()) {
    os << '\n'
       << "If you'd like to help us add support, open an issue with the "
          "details\n"
          "pre-filled. Add a short code example (please don't include "
          "confidential\n"
          "or proprietary code) and press \"Submit\":\n\n"
       << buildUnsupportedUrl(reports) << '\n';
  }

  os << '\n'
     << "Did the formatter change something it should have left alone, or "
        "reformat\n"
        "a part incorrectly? That's a bug — please report it here (template\n"
        "pre-filled, add a small input/output example):\n\n"
     << buildWrongFormattingUrl() << '\n'
     << '\n'
     << "Please review the pre-filled report before submitting and remove\n"
        "anything you don't want to share.\n";
}

}  // namespace format::summary

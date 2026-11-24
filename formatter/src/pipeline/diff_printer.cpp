#include "pipeline/diff_printer.h"

#include <charconv>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace format {
namespace {

constexpr std::string_view kNoNewline = "\\ No newline at end of file";

struct PatchLine {
  std::string_view text;
  std::string_view ending;
};

struct ChangedRange {
  size_t begin;
  size_t end;
};

struct LineChanges {
  std::vector<ChangedRange> removed;
  std::vector<ChangedRange> added;
};

struct LineOffsets {
  size_t original = 0;
  size_t formatted = 0;
};

auto takeLine(std::string_view& patch) -> PatchLine {
  const auto end = patch.find('\n');
  auto line = patch.substr(0, end);
  std::string_view ending;
  if (end == std::string_view::npos) {
    patch = {};
  } else {
    patch.remove_prefix(end + 1);
    ending = "\n";
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
      ending = "\r\n";
    }
  }
  return {line, ending};
}

auto parseLineNumber(std::string_view text) -> std::optional<size_t> {
  size_t number = 0;
  const auto result = std::from_chars(text.begin(), text.end(), number);
  if (result.ec != std::errc{} || result.ptr != text.end()) {
    return std::nullopt;
  }
  return number;
}

auto firstChangedLine(const PatchLine& header, std::string_view body)
    -> std::optional<size_t> {
  if (!header.text.starts_with("@@ -")) {
    return std::nullopt;
  }
  const auto end = header.text.find(' ', 4);
  if (end == std::string_view::npos) {
    return std::nullopt;
  }
  const auto range = header.text.substr(4, end - 4);
  const auto comma = range.find(',');
  auto line = parseLineNumber(range.substr(0, comma));
  const auto count = comma == std::string_view::npos
                         ? std::optional<size_t>{1}
                         : parseLineNumber(range.substr(comma + 1));
  if (!line || !count) {
    return std::nullopt;
  }
  if (*count == 0) {
    if (*line == std::numeric_limits<size_t>::max()) {
      return std::nullopt;
    }
    ++*line;
  }
  while (!body.empty()) {
    const auto next = takeLine(body);
    if (next.text.starts_with('-')) {
      return *line == 0 ? 1 : *line;
    }
    if (next.text.starts_with('+')) {
      return *line > 1 ? *line - 1 : 1;
    }
    if (!next.text.starts_with(' ') ||
        *line == std::numeric_limits<size_t>::max()) {
      break;
    }
    ++*line;
  }
  return std::nullopt;
}

auto makeFileUri(const std::filesystem::path& path) -> std::string {
  std::error_code error;
  auto absolute = std::filesystem::canonical(path, error);
  if (error) {
    absolute = std::filesystem::absolute(path);
  }
  const auto name = absolute.generic_string();
  // UNC paths already include an authority; drive-letter paths need a slash.
  std::string uri = name.starts_with("//") ? "file:" : "file://";
  if (!name.starts_with('/')) {
    uri += '/';
  }
  constexpr std::string_view kHex = "0123456789ABCDEF";
  constexpr unsigned int kNibbleBits = 4;
  constexpr unsigned int kNibbleMask = 0xf;
  for (const char ch : name) {
    const auto byte = static_cast<unsigned char>(ch);
    if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
        (byte >= '0' && byte <= '9') || ch == '-' || ch == '.' || ch == '_' ||
        ch == '~' || ch == '/' || ch == ':') {
      uri += ch;
    } else {
      uri += '%';
      uri += kHex.at(byte >> kNibbleBits);
      uri += kHex.at(byte & kNibbleMask);
    }
  }
  return uri;
}

auto spaceEnd(std::string_view text, size_t pos) -> size_t {
  while (pos < text.size() && (text.at(pos) == ' ' || text.at(pos) == '\t')) {
    ++pos;
  }
  return pos;
}

auto sameNonWhitespace(std::string_view original, std::string_view formatted)
    -> bool {
  size_t old_pos = 0;
  size_t new_pos = 0;
  while (true) {
    old_pos = spaceEnd(original, old_pos);
    new_pos = spaceEnd(formatted, new_pos);
    if (old_pos == original.size() || new_pos == formatted.size()) {
      return old_pos == original.size() && new_pos == formatted.size();
    }
    if (original.at(old_pos) != formatted.at(new_pos)) {
      return false;
    }
    ++old_pos;
    ++new_pos;
  }
}

auto isUtf8Continuation(std::string_view text, size_t pos) -> bool {
  constexpr unsigned char kContinuationMask = 0xc0;
  constexpr unsigned char kContinuationTag = 0x80;
  return pos < text.size() && (static_cast<unsigned char>(text.at(pos)) &
                               kContinuationMask) == kContinuationTag;
}

// Exclude matching ends, keeping ANSI boundaries outside UTF-8 code points.
auto addChangedRanges(LineChanges& changes, std::string_view original,
                      std::string_view formatted, LineOffsets offsets = {})
    -> void {
  size_t prefix = 0;
  while (prefix < original.size() && prefix < formatted.size() &&
         original.at(prefix) == formatted.at(prefix)) {
    ++prefix;
  }
  while (prefix > 0 && (isUtf8Continuation(original, prefix) ||
                        isUtf8Continuation(formatted, prefix))) {
    --prefix;
  }
  size_t suffix = 0;
  while (suffix < original.size() - prefix &&
         suffix < formatted.size() - prefix &&
         original.at(original.size() - suffix - 1) ==
             formatted.at(formatted.size() - suffix - 1)) {
    ++suffix;
  }
  while (suffix > 0 &&
         (isUtf8Continuation(original, original.size() - suffix) ||
          isUtf8Continuation(formatted, formatted.size() - suffix))) {
    --suffix;
  }
  if (prefix + suffix < original.size()) {
    changes.removed.push_back({offsets.original + prefix,
                               offsets.original + original.size() - suffix});
  }
  if (prefix + suffix < formatted.size()) {
    changes.added.push_back({offsets.formatted + prefix,
                             offsets.formatted + formatted.size() - suffix});
  }
}

auto findChanges(std::string_view original, std::string_view formatted)
    -> LineChanges {
  LineChanges changes;
  if (!sameNonWhitespace(original, formatted)) {
    addChangedRanges(changes, original, formatted);
    return changes;
  }

  // Matching non-whitespace characters anchor each gap independently. This
  // keeps separate spacing changes from highlighting the code between them.
  size_t old_pos = 0;
  size_t new_pos = 0;
  while (old_pos < original.size() || new_pos < formatted.size()) {
    const auto old_end = spaceEnd(original, old_pos);
    const auto new_end = spaceEnd(formatted, new_pos);
    addChangedRanges(changes, original.substr(old_pos, old_end - old_pos),
                     formatted.substr(new_pos, new_end - new_pos),
                     {.original = old_pos, .formatted = new_pos});
    old_pos = old_end;
    new_pos = new_end;
    while (old_pos < original.size() && new_pos < formatted.size() &&
           spaceEnd(original, old_pos) == old_pos &&
           spaceEnd(formatted, new_pos) == new_pos) {
      ++old_pos;
      ++new_pos;
    }
  }
  return changes;
}

auto printChangedLine(std::ostream& out, const PatchLine& line,
                      std::string_view color,
                      const std::vector<ChangedRange>& ranges) -> void {
  out << color << line.text.substr(0, 1);
  const auto body = line.text.substr(1);
  size_t pos = 0;
  for (const auto& range : ranges) {
    out << body.substr(pos, range.begin - pos) << "\033[7m"
        << body.substr(range.begin, range.end - range.begin) << "\033[27m";
    pos = range.end;
  }
  out << body.substr(pos) << "\033[0m" << line.ending;
}

auto printReplacement(std::ostream& out, const PatchLine& removed,
                      std::string_view& patch) -> bool {
  auto rest = patch;
  PatchLine old_marker{};
  PatchLine new_marker{};
  if (rest.starts_with(kNoNewline)) {
    old_marker = takeLine(rest);
  }
  const auto added = takeLine(rest);
  if (!added.text.starts_with('+')) {
    return false;
  }
  if (rest.starts_with(kNoNewline)) {
    new_marker = takeLine(rest);
  }
  if (rest.starts_with('+') || rest.starts_with('-')) {
    return false;
  }

  const auto changes =
      findChanges(removed.text.substr(1), added.text.substr(1));
  printChangedLine(out, removed, "\033[31m", changes.removed);
  out << old_marker.text << old_marker.ending;
  printChangedLine(out, added, "\033[32m", changes.added);
  out << new_marker.text << new_marker.ending;
  patch = rest;
  return true;
}

}  // namespace

auto printDiff(std::ostream& out, std::string_view patch,
               const DiffPrintOptions& options) -> void {
  if (patch.empty()) {
    return;
  }
  if (!options.color && (!options.hyperlinks || options.source_path.empty())) {
    out << patch;
    return;
  }

  const auto file_uri = options.hyperlinks && !options.source_path.empty()
                            ? makeFileUri(options.source_path)
                            : std::string{};
  bool in_hunk = false;
  bool in_change = false;
  while (!patch.empty()) {
    const auto line = takeLine(patch);
    if (options.color && in_hunk && !in_change && line.text.starts_with('-') &&
        printReplacement(out, line, patch)) {
      in_change = true;
      continue;
    }

    std::string_view prefix;
    std::optional<size_t> target;
    if (line.text.starts_with("diff --git ")) {
      in_hunk = false;
      in_change = false;
    }
    if (line.text.starts_with("@@ ")) {
      in_hunk = true;
      prefix = "\033[36m";
      if (!file_uri.empty()) {
        target = firstChangedLine(line, patch);
      }
    } else if (!in_hunk) {
      prefix = "\033[1m";
    } else if (line.text.starts_with('-')) {
      prefix = "\033[31m";
    } else if (line.text.starts_with('+')) {
      prefix = "\033[32m";
    }
    if (in_hunk && !line.text.starts_with(kNoNewline)) {
      in_change = line.text.starts_with('-') || line.text.starts_with('+');
    }

    if (target) {
      out << "\033]8;;" << file_uri << '#' << *target << "\033\\";
    }
    if (!options.color || prefix.empty()) {
      out << line.text;
    } else {
      out << prefix << line.text << "\033[0m";
    }
    if (target) {
      out << "\033]8;;\033\\";
    }
    out << line.ending;
  }
}

}  // namespace format

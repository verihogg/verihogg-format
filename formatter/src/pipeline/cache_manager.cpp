#include "pipeline/cache_manager.h"

#include <charconv>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "data/format_style.h"
#include "pipeline/hash_utils.h"

namespace format {

namespace {

constexpr std::string_view kMagic = "verihogg-format-cache";
constexpr int kCacheFormatVersion = 1;
constexpr int kDecimal = 10;
constexpr int kHex = 16;
constexpr size_t kFormatStyleSize = 48;

auto formatterFingerprint() -> std::string {
  std::error_code ec;
  const auto exe = std::filesystem::canonical("/proc/self/exe", ec);
  if (ec) {
    return "unknown";
  }
  const auto size = std::filesystem::file_size(exe, ec);
  if (ec) {
    return "unknown";
  }
  const auto mtime = std::filesystem::last_write_time(exe, ec);
  if (ec) {
    return "unknown";
  }
  return std::format("{};{};{}", exe.string(), size,
                     mtime.time_since_epoch().count());
}

auto styleToString(const FormatStyle& style) -> std::string {
  // Every FormatStyle field must be listed below: a missed one would let the
  // cache survive a style change. Update the size when adding fields.
  static_assert(sizeof(FormatStyle) == kFormatStyleSize,
                "FormatStyle changed: update styleToString()");
  return std::format(
      "column_limit={};indentation_spaces={};wrap_spaces={};"
      "line_break_penalty={};over_column_limit_penalty={};"
      "line_terminator={}",
      style.column_limit, style.indentation_spaces, style.wrap_spaces,
      style.line_break_penalty, style.over_column_limit_penalty,
      static_cast<int>(style.line_terminator));
}

template <typename T>
auto parseNumber(std::string_view text, T& value, int base = kDecimal) -> bool {
  const auto* first = std::to_address(text.begin());
  const auto* last = std::to_address(text.end());
  auto [ptr, ec] = std::from_chars(first, last, value, base);
  return ec == std::errc{} && ptr == last;
}

// Splits off the text before the next space.
auto nextField(std::string_view& line) -> std::string_view {
  const auto pos = line.find(' ');
  if (pos == std::string_view::npos) {
    return {};
  }
  auto field = line.substr(0, pos);
  line.remove_prefix(pos + 1);
  return field;
}

// Entry line: "<content_hash hex> <size> <mtime_ns> <path>". The path goes
// last because it may contain spaces.
auto parseEntry(std::string_view line, std::string& path, FileStamp& stamp)
    -> bool {
  const auto hash = nextField(line);
  const auto size = nextField(line);
  const auto mtime = nextField(line);
  if (line.empty() || !parseNumber(hash, stamp.content_hash, kHex) ||
      !parseNumber(size, stamp.size) || !parseNumber(mtime, stamp.mtime_ns)) {
    return false;
  }
  path = line;
  return true;
}

}  // namespace

auto makeCacheKey(const FormatStyle& style) -> std::string {
  return formatterFingerprint() + ";" + styleToString(style);
}

CacheManager::CacheManager(std::filesystem::path cache_file,
                           std::string_view key)
    : cache_file_{std::move(cache_file)},
      base_dir_{std::filesystem::absolute(cache_file_)
                    .parent_path()
                    .lexically_normal()},
      key_{std::format("{} {} {:016x}", kMagic, kCacheFormatVersion,
                       fnv1a64(key))} {}

auto CacheManager::load() -> bool {
  entries_.clear();
  changed_ = false;

  std::ifstream in{cache_file_, std::ios::binary};
  if (!in) {
    return true;  // no cache yet
  }

  std::string line;
  if (!std::getline(in, line) || !line.starts_with(kMagic)) {
    return false;
  }
  if (line != key_) {
    return true;  // other formatter build, style or cache version
  }

  std::string path;
  FileStamp stamp;
  while (std::getline(in, line)) {
    if (!parseEntry(line, path, stamp)) {
      entries_.clear();
      return false;
    }
    entries_.insert_or_assign(path, stamp);
  }
  return true;
}

auto CacheManager::save() -> bool {
  if (!changed_) {
    return true;
  }

  // Write a temporary file and rename it over the cache, so an interrupted
  // run never leaves a truncated cache behind.
  auto tmp = cache_file_;
  tmp += ".tmp";
  {
    std::ofstream out{tmp, std::ios::binary | std::ios::trunc};
    out << key_ << '\n';
    for (const auto& [path, stamp] : entries_) {
      out << std::format("{:016x} {} {} {}\n", stamp.content_hash, stamp.size,
                         stamp.mtime_ns, path);
    }
    out.flush();
    if (!out) {
      std::error_code ignored;
      std::filesystem::remove(tmp, ignored);
      return false;
    }
  }

  std::error_code ec;
  std::filesystem::rename(tmp, cache_file_, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return false;
  }
  changed_ = false;
  return true;
}

auto CacheManager::find(const std::filesystem::path& file) const
    -> const FileStamp* {
  const auto it = entries_.find(entryKey(file));
  return it == entries_.end() ? nullptr : &it->second;
}

void CacheManager::update(const std::filesystem::path& file,
                          const FileStamp& stamp) {
  auto [it, inserted] = entries_.try_emplace(entryKey(file), stamp);
  if (!inserted && it->second == stamp) {
    return;
  }
  it->second = stamp;
  changed_ = true;
}

// Paths are stored relative to the cache file, so the cache stays valid when
// the whole project is checked out somewhere else (e.g. on CI).
auto CacheManager::entryKey(const std::filesystem::path& file) const
    -> std::string {
  const auto abs = std::filesystem::absolute(file).lexically_normal();
  const auto rel = abs.lexically_relative(base_dir_);
  return (rel.empty() ? abs : rel).generic_string();
}

}  // namespace format

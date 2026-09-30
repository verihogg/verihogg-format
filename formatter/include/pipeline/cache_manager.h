#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "data/format_style.h"

namespace format {

struct FileStamp {
  static constexpr int64_t kUnknownMtime = 0;

  uint64_t size = 0;
  int64_t mtime_ns = kUnknownMtime;
  uint64_t content_hash = 0;

  auto operator==(const FileStamp&) const -> bool = default;
};

// Key that invalidates the whole cache when the formatter binary or the style
// changes.
[[nodiscard]] auto makeCacheKey(const FormatStyle& style) -> std::string;

// Remembers files that are already formatted, so that --check can skip them.
//
// Not synchronized: find() may be called from many threads at once, but load(),
// update() and save() must only be called while no find() is running.
class CacheManager {
 public:
  CacheManager(std::filesystem::path cache_file, std::string_view key);

  // Missing file or a different key give an empty cache. Returns false only if
  // the file exists but could not be parsed; the cache is empty then as well.
  auto load() -> bool;

  // Writes the cache atomically, and only if it changed. Returns false on I/O
  // errors.
  auto save() -> bool;

  [[nodiscard]] auto find(const std::filesystem::path& file) const
      -> const FileStamp*;
  void update(const std::filesystem::path& file, const FileStamp& stamp);

  [[nodiscard]] auto file() const -> const std::filesystem::path& {
    return cache_file_;
  }

 private:
  [[nodiscard]] auto entryKey(const std::filesystem::path& file) const
      -> std::string;

  std::filesystem::path cache_file_;
  std::filesystem::path base_dir_;
  std::string key_;
  std::unordered_map<std::string, FileStamp> entries_;
  bool changed_ = false;
};

}  // namespace format

#include "pipeline/cache_manager.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace format {

namespace {

namespace fs = std::filesystem;
constexpr size_t kKeyLength = 64;
constexpr size_t kMaxAttempts = 128;
constexpr std::string_view kHeader = "verihogg-check-cache-record-v1\n";

// Reserving a directory with create_directory gives this writer exclusive
// ownership. Only its contents may be removed, including on failed writes.
class TemporaryDirectory {
 public:
  explicit TemporaryDirectory(fs::path directory)
      : directory_(std::move(directory)) {}
  ~TemporaryDirectory() {
    std::error_code ignored;
    fs::remove_all(directory_, ignored);
  }
  TemporaryDirectory(const TemporaryDirectory&) = delete;
  auto operator=(const TemporaryDirectory&) -> TemporaryDirectory& = delete;
  TemporaryDirectory(TemporaryDirectory&&) = delete;
  auto operator=(TemporaryDirectory&&) -> TemporaryDirectory& = delete;

 private:
  fs::path directory_;
};

auto validKey(std::string_view key) -> bool {
  return key.size() == kKeyLength && std::ranges::all_of(key, [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

auto record(std::string_view key) -> std::string {
  return std::string(kHeader) + std::string(key) + "\n";
}

}  // namespace

CacheManager::CacheManager(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

auto CacheManager::contains(std::string_view key) const -> bool {
  if (!validKey(key)) {
    return false;
  }
  const auto entry = directory_ / key;
  std::error_code ec;
  if (!fs::is_regular_file(entry, ec)) {
    return false;
  }
  std::ifstream in{entry, std::ios::binary};
  if (!in) {
    return false;
  }
  const auto expected = record(key);
  // Read at most one byte past a valid record, so trailing garbage is rejected
  // without allocating memory based on an untrusted cache file size.
  std::string actual(expected.size() + 1, '\0');
  in.read(actual.data(), static_cast<std::streamsize>(actual.size()));
  actual.resize(static_cast<size_t>(in.gcount()));
  return !in.bad() && actual == expected;
}

auto CacheManager::store(std::string_view key) const -> bool {
  if (!validKey(key)) {
    return false;
  }
  if (contains(key)) {
    return true;
  }
  std::error_code ec;
  fs::create_directories(directory_, ec);
  if (ec) {
    return false;
  }
  const auto base =
      ".tmp-" + std::string(key) + "-" +
      std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count());
  for (size_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const auto temporary = directory_ / (base + "-" + std::to_string(attempt));
    if (!fs::create_directory(temporary, ec)) {
      if (ec && ec != std::errc::file_exists) {
        return false;
      }
      continue;
    }
    const TemporaryDirectory cleanup(temporary);
    const auto entry = temporary / "entry";
    {
      std::ofstream out{entry, std::ios::binary | std::ios::trunc};
      out << record(key);
      out.close();
      if (!out) {
        return false;
      }
    }
    fs::rename(entry, directory_ / key, ec);
    // Another writer may have published the same key in the meantime.
    return !ec || contains(key);
  }
  return false;
}

}  // namespace format

#pragma once

#include <filesystem>
#include <string_view>

namespace format {

// Persistent set of successful check keys. Keys already identify source bytes,
// resolved style and formatter build; source paths are not stored.
// Each key has an independent record, allowing concurrent readers and writers.
class CacheManager {
 public:
  explicit CacheManager(std::filesystem::path directory);

  // Missing, malformed, non-regular and unreadable records are cache misses.
  [[nodiscard]] auto contains(std::string_view key) const -> bool;

  // Publishes a complete record atomically. Returns false on I/O errors or
  // invalid keys; valid keys are lowercase hexadecimal SHA-256 digests.
  [[nodiscard]] auto store(std::string_view key) const -> bool;

 private:
  std::filesystem::path directory_;
};

}  // namespace format

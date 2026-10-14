#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "data/format_style.h"
#include "data/format_warning.h"
#include "pipeline/cache_manager.h"

namespace format {

enum class CheckStatus : uint8_t {
  kClean,       // file is already formatted
  kDirty,       // formatting would change the file
  kUnreadable,  // file could not be read
};

struct CheckResult {
  CheckStatus status = CheckStatus::kClean;
  std::vector<FormatWarning> warnings{};
  // Set for clean files without warnings when a cache is used: what to record
  // in the cache for this file.
  std::optional<FileStamp> stamp{};
};

// Checks whether files are formatted according to the style without touching
// them on disk (used by --check).
class FormatChecker {
 public:
  explicit FormatChecker(const FormatStyle& style) : style_{style} {}

  [[nodiscard]] auto checkFile(const std::filesystem::path& path,
                               const CacheManager* cache = nullptr) const
      -> CheckResult;

 private:
  FormatStyle style_;
};

}  // namespace format

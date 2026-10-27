#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include "data/format_style.h"
#include "data/format_warning.h"

namespace format {

class CacheManager;

enum class CheckStatus : uint8_t {
  kClean,       // file is already formatted
  kDirty,       // formatting would change the file
  kUnreadable,  // file could not be read
};

struct CheckResult {
  CheckStatus status = CheckStatus::kClean;
  std::vector<FormatWarning> warnings{};
  bool cache_hit = false;
  bool cache_error = false;
};

// Checks whether files are formatted according to the style without touching
// them on disk (used by --check).
class FormatChecker {
 public:
  explicit FormatChecker(const FormatStyle& style) : style_{style} {}

  [[nodiscard]] auto checkFile(const std::filesystem::path& path,
                               const CacheManager* cache = nullptr,
                               std::string_view build_id = {}) const
      -> CheckResult;

 private:
  FormatStyle style_;
};

}  // namespace format

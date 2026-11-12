#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "data/format_style.h"
#include "data/format_warning.h"

namespace format {

enum class CheckStatus : uint8_t {
  kClean,       // file is already formatted
  kDirty,       // formatting would change the file
  kUnreadable,  // file could not be read
};

struct CheckResult {
  CheckStatus status = CheckStatus::kClean;
  std::vector<FormatWarning> warnings{};
  std::string diff{};
};

// Checks whether files are formatted according to the style without touching
// them on disk (used by --check).
class FormatChecker {
 public:
  explicit FormatChecker(const FormatStyle& style) : style_{style} {}

  // include_diff generates a patch for dirty files, labeled relative to cwd.
  [[nodiscard]] auto checkFile(const std::filesystem::path& path,
                               bool include_diff = false) const -> CheckResult;

 private:
  FormatStyle style_;
};

}  // namespace format

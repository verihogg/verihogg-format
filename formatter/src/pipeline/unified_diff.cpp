#include "pipeline/unified_diff.h"

#include <git2.h>

#include <algorithm>
#include <cstdint>
#include <gsl/util>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace format {
namespace {

auto checkGit(int status) -> void {
  if (status < 0) {
    const auto* error = git_error_last();
    throw std::runtime_error(std::string("Cannot generate diff: ") +
                             (error != nullptr && error->message != nullptr
                                  ? error->message
                                  : "libgit2 error"));
  }
}

class Libgit2Runtime {
 public:
  Libgit2Runtime() { checkGit(git_libgit2_init()); }
  ~Libgit2Runtime() { git_libgit2_shutdown(); }
  Libgit2Runtime(const Libgit2Runtime&) = delete;
  auto operator=(const Libgit2Runtime&) -> Libgit2Runtime& = delete;
  Libgit2Runtime(Libgit2Runtime&&) = delete;
  auto operator=(Libgit2Runtime&&) -> Libgit2Runtime& = delete;
};

}  // namespace

auto makeUnifiedDiff(std::string_view original, std::string_view formatted,
                     const std::filesystem::path& path, size_t context_lines)
    -> std::string {
  if (original == formatted) {
    return {};
  }
  const auto name = path.lexically_normal().generic_string();
  if (name.empty() || path.is_absolute() ||
      name.find('\0') != std::string::npos) {
    throw std::invalid_argument("Diff requires a nonempty patch-relative path");
  }
  static const Libgit2Runtime runtime;
  git_diff_options options;
  checkGit(git_diff_options_init(&options, GIT_DIFF_OPTIONS_VERSION));
  options.flags = GIT_DIFF_FORCE_TEXT;
  // Context beyond the number of input lines has no effect. Clamp it before
  // passing it to libgit2, whose public context field has a narrower type.
  const auto lines =
      std::max(std::count(original.begin(), original.end(), '\n'),
               std::count(formatted.begin(), formatted.end(), '\n'));
  options.context_lines = static_cast<uint32_t>(
      std::min({context_lines, static_cast<size_t>(lines) + 1,
                static_cast<size_t>(std::numeric_limits<int>::max())}));

  git_patch* raw_patch = nullptr;
  // A null buffer means an absent file in libgit2, not an existing empty file.
  const auto status = git_patch_from_buffers(
      &raw_patch, original.empty() ? "" : original.data(), original.size(),
      name.c_str(), formatted.empty() ? "" : formatted.data(), formatted.size(),
      name.c_str(), &options);
  const std::unique_ptr<git_patch, decltype(&git_patch_free)> patch(
      raw_patch, git_patch_free);
  checkGit(status);
  git_buf buffer = GIT_BUF_INIT;
  const auto cleanup = gsl::finally([&buffer] { git_buf_dispose(&buffer); });
  checkGit(git_patch_to_buf(&buffer, patch.get()));
  return buffer.size == 0 ? std::string{}
                          : std::string(buffer.ptr, buffer.size);
}

}  // namespace format

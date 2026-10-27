#include "pipeline/cache_build_id.h"

#if defined(__linux__)
#include <link.h>
#endif

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "pipeline/cache_key.h"

namespace format {
namespace {

#if defined(__linux__)
struct LoadedObjects {
  std::vector<std::string> paths{"/proc/self/exe"};
  bool failed = false;
};

auto collectObject(dl_phdr_info* info, [[maybe_unused]] size_t info_size,
                   void* data) -> int {
  auto& objects = *static_cast<LoadedObjects*>(data);
  try {
    const std::string_view name(info->dlpi_name);
    // The vDSO has no backing file. An empty name represents the executable,
    // which is already included through /proc/self/exe.
    if (!name.empty() && !name.starts_with("linux-vdso")) {
      objects.paths.emplace_back(name);
    }
  } catch (...) {
    // Do not propagate C++ exceptions through the dynamic loader callback.
    objects.failed = true;
    return 1;
  }
  return 0;
}
#endif

auto computeBuildId() -> std::optional<std::string> {
#if defined(__linux__)
  try {
    // Initialize the digest provider before enumerating loaded libraries.
    (void)sha256("");
    LoadedObjects objects;
    if (dl_iterate_phdr(collectObject, &objects) != 0 || objects.failed) {
      return std::nullopt;
    }
    std::ranges::sort(objects.paths);
    std::string hashes;
    for (const auto& path : objects.paths) {
      const auto digest = sha256File(path);
      if (!digest) {
        return std::nullopt;
      }
      hashes += *digest;
    }
    return sha256(hashes);
  } catch (const std::runtime_error&) {
    return std::nullopt;
  }
#else
  return std::nullopt;
#endif
}

}  // namespace

auto formatterBuildId() -> const std::optional<std::string>& {
  static const auto identity = computeBuildId();
  return identity;
}

}  // namespace format

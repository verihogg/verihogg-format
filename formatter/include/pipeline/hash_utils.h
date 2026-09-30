#pragma once

#include <cstdint>
#include <string_view>

namespace format {

inline constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
inline constexpr uint64_t kFnvPrime = 1099511628211ULL;

// 64-bit FNV-1a. Not cryptographic: it only has to tell apart different
// versions of the same file, which is all the format cache needs.
[[nodiscard]] constexpr auto fnv1a64(std::string_view data,
                                     uint64_t hash = kFnvOffsetBasis) noexcept
    -> uint64_t {
  for (const char c : data) {
    hash ^= static_cast<unsigned char>(c);
    hash *= kFnvPrime;
  }
  return hash;
}

}  // namespace format

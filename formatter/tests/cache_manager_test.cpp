#include "pipeline/cache_manager.h"

#include <gtest/gtest.h>

#if defined(__linux__)
#include <sys/stat.h>
#endif

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "pipeline/cache_key.h"

namespace {
namespace fs = std::filesystem;

class CacheManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto base =
        fs::temp_directory_path() /
        ("verihogg_cache_test_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    constexpr size_t kMaxAttempts = 128;
    for (size_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
      const fs::path candidate = base.string() + "_" + std::to_string(attempt);
      if (fs::create_directory(candidate)) {
        dir_ = candidate;
        break;
      }
    }
    ASSERT_FALSE(dir_.empty());
  }
  void TearDown() override {
    if (!dir_.empty()) {
      fs::remove_all(dir_);
    }
  }

  [[nodiscard]] auto directory() const -> fs::path {
    return dir_ / "cache with spaces\nand newline";
  }
  void corrupt(const fs::path& entry, std::string_view contents) const {
    std::ofstream out{entry, std::ios::binary | std::ios::trunc};
    out << contents;
    ASSERT_TRUE(out.good());
  }

 private:
  fs::path dir_;
};

TEST_F(CacheManagerTest, MissingRecordIsMissAndDoesNotCreateDirectory) {
  const format::CacheManager cache(directory());
  EXPECT_FALSE(cache.contains(format::sha256("source")));
  EXPECT_FALSE(fs::exists(directory()));
}

TEST_F(CacheManagerTest,
       RecordsSurviveReopeningAndSupportUnusualDirectoryNames) {
  const auto key = format::sha256("source");
  ASSERT_TRUE(format::CacheManager(directory()).store(key));
  const format::CacheManager reopened(directory());
  EXPECT_TRUE(reopened.contains(key));
  EXPECT_FALSE(reopened.contains(format::sha256("different source")));
}

TEST_F(CacheManagerTest, InvalidKeysCannotEscapeCacheDirectory) {
  const format::CacheManager cache(directory());
  for (const auto key : {"", "../outside", "abc", "/absolute"}) {
    EXPECT_FALSE(cache.store(key));
    EXPECT_FALSE(cache.contains(key));
  }
  EXPECT_FALSE(fs::exists(directory()));
}

#if defined(__linux__)
TEST_F(CacheManagerTest, FifoRecordIsNotReadAndCanBeReplaced) {
  const format::CacheManager cache(directory());
  const auto key = format::sha256("source");
  const auto entry = directory() / key;
  fs::create_directories(directory());
  ASSERT_EQ(mkfifo(entry.c_str(), S_IRUSR | S_IWUSR), 0);
  // Keep both ends open and provide enough bytes for a record-sized read.
  // Consuming the FIFO changes its first byte and fails without hanging.
  std::fstream fifo{entry, std::ios::in | std::ios::out | std::ios::binary};
  ASSERT_TRUE(fifo.good());
  constexpr size_t kPayloadSize = 256;
  fifo << 'a' << std::string(kPayloadSize, 'b');
  fifo.flush();
  ASSERT_TRUE(fifo.good());
  EXPECT_FALSE(cache.contains(key));
  EXPECT_EQ(fifo.get(), 'a');
  ASSERT_TRUE(cache.store(key));
  EXPECT_TRUE(fs::is_regular_file(entry));
  EXPECT_TRUE(cache.contains(key));
}
#endif

TEST_F(CacheManagerTest, CorruptRecordIsMissAndCanBeReplaced) {
  const format::CacheManager cache(directory());
  const auto key = format::sha256("source");
  ASSERT_TRUE(cache.store(key));
  corrupt(directory() / key, "truncated");
  EXPECT_FALSE(cache.contains(key));
  ASSERT_TRUE(cache.store(key));
  EXPECT_TRUE(cache.contains(key));
}

TEST_F(CacheManagerTest, TrailingGarbageIsRejected) {
  const format::CacheManager cache(directory());
  const auto key = format::sha256("source");
  ASSERT_TRUE(cache.store(key));
  {
    std::ofstream out{directory() / key, std::ios::binary | std::ios::app};
    out << "garbage";
  }
  EXPECT_FALSE(cache.contains(key));
}

TEST_F(CacheManagerTest, RecordForAnotherKeyIsRejected) {
  const format::CacheManager cache(directory());
  const auto first = format::sha256("first");
  const auto second = format::sha256("second");
  ASSERT_TRUE(cache.store(first));
  fs::copy_file(directory() / first, directory() / second);
  EXPECT_FALSE(cache.contains(second));
}

TEST_F(CacheManagerTest, ExistingRecordIsNotRewritten) {
  const format::CacheManager cache(directory());
  const auto key = format::sha256("source");
  ASSERT_TRUE(cache.store(key));
  const auto old_time =
      fs::last_write_time(directory() / key) - std::chrono::hours(1);
  fs::last_write_time(directory() / key, old_time);
  ASSERT_TRUE(cache.store(key));
  EXPECT_EQ(fs::last_write_time(directory() / key), old_time);
}

TEST_F(CacheManagerTest, FileInsteadOfCacheDirectoryIsFailure) {
  {
    std::ofstream out{directory()};
    out << "not a directory";
  }
  const format::CacheManager cache(directory());
  EXPECT_FALSE(cache.store(format::sha256("source")));
  EXPECT_FALSE(cache.contains(format::sha256("source")));
}

TEST_F(CacheManagerTest, FailedPublishCleansOnlyItsTemporaryDirectory) {
  const format::CacheManager cache(directory());
  const auto key = format::sha256("source");
  fs::create_directories(directory() / key);
  const auto unrelated = directory() / ".tmp-other-writer";
  fs::create_directory(unrelated);
  EXPECT_FALSE(cache.contains(key));
  EXPECT_FALSE(cache.store(key));
  EXPECT_TRUE(fs::is_directory(unrelated));
  size_t entries = 0;
  for (const auto& entry : fs::directory_iterator(directory())) {
    EXPECT_TRUE(entry.path() == directory() / key || entry.path() == unrelated);
    ++entries;
  }
  EXPECT_EQ(entries, 2U);
}

TEST_F(CacheManagerTest, ConcurrentWritersPreserveDifferentAndIdenticalKeys) {
  const format::CacheManager cache(directory());
  const auto shared = format::sha256("shared");
  constexpr size_t kWriters = 16;
  std::atomic<bool> success{true};
  {
    std::vector<std::jthread> writers;
    writers.reserve(kWriters);
    for (size_t i = 0; i < kWriters; ++i) {
      writers.emplace_back([&, i] {
        const auto key = format::sha256(std::to_string(i));
        if (!cache.store(key) || !cache.store(shared)) {
          success.store(false);
        }
      });
    }
  }
  ASSERT_TRUE(success.load());
  EXPECT_TRUE(cache.contains(shared));
  for (size_t i = 0; i < kWriters; ++i) {
    EXPECT_TRUE(cache.contains(format::sha256(std::to_string(i))));
  }
  for (const auto& entry : fs::directory_iterator(directory())) {
    EXPECT_TRUE(entry.is_regular_file());
    EXPECT_FALSE(entry.path().filename().string().starts_with(".tmp-"));
  }
}

}  // namespace

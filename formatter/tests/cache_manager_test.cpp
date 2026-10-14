#include "pipeline/cache_manager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "data/format_style.h"

namespace {

namespace fs = std::filesystem;

class CacheManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = fs::temp_directory_path() /
           ("verihogg_cache_test_" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir_);
  }

  void TearDown() override { fs::remove_all(dir_); }

  [[nodiscard]] auto cacheFile() const -> fs::path { return dir_ / "cache"; }
  [[nodiscard]] auto file(const std::string& name) const -> fs::path {
    return dir_ / name;
  }

  void writeCache(const std::string& content) const {
    std::ofstream f{cacheFile(), std::ios::binary | std::ios::trunc};
    f << content;
  }

 private:
  fs::path dir_;
};

constexpr format::FileStamp kStamp{
    .size = 42, .mtime_ns = 123456789, .content_hash = 0xdeadbeef};

TEST_F(CacheManagerTest, MissingFileGivesEmptyCache) {
  format::CacheManager cache{cacheFile(), "key"};

  EXPECT_TRUE(cache.load());
  EXPECT_EQ(cache.find(file("a.sv")), nullptr);
}

TEST_F(CacheManagerTest, SaveAndLoadRoundTrip) {
  {
    format::CacheManager cache{cacheFile(), "key"};
    cache.update(file("a.sv"), kStamp);
    cache.update(file("with space.sv"), kStamp);
    ASSERT_TRUE(cache.save());
  }

  format::CacheManager cache{cacheFile(), "key"};
  ASSERT_TRUE(cache.load());
  const auto* a = cache.find(file("a.sv"));
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(*a, kStamp);
  const auto* spaced = cache.find(file("with space.sv"));
  ASSERT_NE(spaced, nullptr);
  EXPECT_EQ(*spaced, kStamp);
  EXPECT_EQ(cache.find(file("b.sv")), nullptr);
}

TEST_F(CacheManagerTest, DifferentKeyGivesEmptyCache) {
  {
    format::CacheManager cache{cacheFile(), "old key"};
    cache.update(file("a.sv"), kStamp);
    ASSERT_TRUE(cache.save());
  }

  format::CacheManager cache{cacheFile(), "new key"};
  EXPECT_TRUE(cache.load());
  EXPECT_EQ(cache.find(file("a.sv")), nullptr);
}

TEST_F(CacheManagerTest, UnknownHeaderIsCorrupted) {
  writeCache("not a cache\n");
  format::CacheManager cache{cacheFile(), "key"};

  EXPECT_FALSE(cache.load());
}

TEST_F(CacheManagerTest, BrokenEntryIsCorrupted) {
  {
    format::CacheManager cache{cacheFile(), "key"};
    cache.update(file("a.sv"), kStamp);
    ASSERT_TRUE(cache.save());
  }
  {
    std::ofstream f{cacheFile(), std::ios::binary | std::ios::app};
    f << "zz 1 2 b.sv\n";
  }

  format::CacheManager cache{cacheFile(), "key"};
  EXPECT_FALSE(cache.load());
  EXPECT_EQ(cache.find(file("a.sv")), nullptr);
}

TEST_F(CacheManagerTest, SaveWithoutChangesDoesNotWrite) {
  format::CacheManager cache{cacheFile(), "key"};
  ASSERT_TRUE(cache.load());
  EXPECT_TRUE(cache.save());
  EXPECT_FALSE(fs::exists(cacheFile()));

  cache.update(file("a.sv"), kStamp);
  ASSERT_TRUE(cache.save());
  const auto old_time =
      fs::last_write_time(cacheFile()) - std::chrono::hours(1);
  fs::last_write_time(cacheFile(), old_time);

  cache.update(file("a.sv"), kStamp);  // same stamp: nothing changes
  EXPECT_TRUE(cache.save());
  EXPECT_EQ(fs::last_write_time(cacheFile()), old_time);
}

TEST_F(CacheManagerTest, KeyDependsOnStyle) {
  auto style = format::FormatStyle::defaults();
  const auto key = format::makeCacheKey(style);
  EXPECT_EQ(format::makeCacheKey(style), key);

  style.wrap_spaces += 1;
  EXPECT_NE(format::makeCacheKey(style), key);
}

}  // namespace

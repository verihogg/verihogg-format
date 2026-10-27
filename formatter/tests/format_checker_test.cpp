#include "pipeline/format_checker.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/cache_build_id.h"
#include "pipeline/cache_key.h"
#include "pipeline/cache_manager.h"

namespace {

namespace fs = std::filesystem;

class FormatCheckerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto base =
        fs::temp_directory_path() /
        ("verihogg_checker_test_" +
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
    ASSERT_FALSE(dir_.empty()) << "Could not create a unique test directory";
    path_ = dir_ / "input.sv";
  }

  void TearDown() override {
    if (!dir_.empty()) {
      fs::remove_all(dir_);
    }
  }

  void write(std::string_view text) const {
    std::ofstream out{path_, std::ios::binary};
    out << text;
    ASSERT_TRUE(out.good());
  }

  auto read() const -> std::string {
    std::ifstream in{path_, std::ios::binary};
    return {std::istreambuf_iterator<char>{in},
            std::istreambuf_iterator<char>{}};
  }

  auto check(format::FormatStyle style = {}) const -> format::CheckResult {
    return format::FormatChecker(style).checkFile(path_);
  }

  static auto formatted(std::string_view source, format::FormatStyle style = {})
      -> format::FormatResult {
    LexContext ctx;
    auto tokens = ctx.lex_string(source);
    return format::format(tokens, style, ctx.source_text());
  }

  [[nodiscard]] auto path() const -> const fs::path& { return path_; }

  [[nodiscard]] auto cacheDirectory() const -> fs::path {
    return path_.parent_path() / "cache";
  }

  auto checkCached(format::FormatStyle style = {},
                   std::string_view build_id = "test-build") const
      -> format::CheckResult {
    const format::CacheManager cache(cacheDirectory());
    return format::FormatChecker(style).checkFile(path_, &cache, build_id);
  }

 private:
  fs::path dir_;
  fs::path path_;
};

TEST_F(FormatCheckerTest, DirtyFileIsNotModified) {
  const std::string source = "module m; assign   a=b; endmodule";
  write(source);
  const auto old_time = fs::last_write_time(path()) - std::chrono::hours(1);
  fs::last_write_time(path(), old_time);

  EXPECT_EQ(check().status, format::CheckStatus::kDirty);
  EXPECT_EQ(read(), source);
  EXPECT_EQ(fs::last_write_time(path()), old_time);
}

TEST_F(FormatCheckerTest, FormattedFileIsClean) {
  write(formatted("module m; assign a=b; endmodule").formatted_text);
  const auto result = check();
  EXPECT_EQ(result.status, format::CheckStatus::kClean);
  EXPECT_TRUE(result.warnings.empty());
}

TEST_F(FormatCheckerTest, MissingFileIsUnreadable) {
  EXPECT_EQ(check().status, format::CheckStatus::kUnreadable);
}

TEST_F(FormatCheckerTest, DirectoryIsUnreadable) {
  EXPECT_EQ(format::FormatChecker({}).checkFile(path().parent_path()).status,
            format::CheckStatus::kUnreadable);
}

TEST_F(FormatCheckerTest, EmptyFileIsClean) {
  write("");
  EXPECT_EQ(check().status, format::CheckStatus::kClean);
}

TEST_F(FormatCheckerTest, UsesSuppliedStyle) {
  auto style = format::FormatStyle::defaults();
  style.indentation_spaces = 4;
  const auto text =
      formatted("module m; assign a=b; endmodule", style).formatted_text;
  write(text);
  EXPECT_EQ(check(style).status, format::CheckStatus::kClean);
  EXPECT_EQ(check().status, format::CheckStatus::kDirty);
}

TEST_F(FormatCheckerTest, PreservesRecoveryWarnings) {
  const std::string source =
      "module m;\n  assign y  = foo]bar;   // bad\nendmodule\n";
  const auto expected = formatted(source);
  ASSERT_FALSE(expected.warnings.empty());
  write(source);

  const auto result = check();
  EXPECT_EQ(result.status, expected.formatted_text == source
                               ? format::CheckStatus::kClean
                               : format::CheckStatus::kDirty);
  ASSERT_EQ(result.warnings.size(), expected.warnings.size());
  EXPECT_EQ(result.warnings.front().code, expected.warnings.front().code);
  EXPECT_EQ(result.warnings.front().message, expected.warnings.front().message);
  EXPECT_EQ(read(), source);
}

TEST_F(FormatCheckerTest, CleanFileIsCachedOnSecondCheck) {
  write(formatted("module m; assign a=b; endmodule").formatted_text);
  const auto first = checkCached();
  EXPECT_EQ(first.status, format::CheckStatus::kClean);
  EXPECT_FALSE(first.cache_hit);
  EXPECT_FALSE(first.cache_error);
  const auto second = checkCached();
  EXPECT_EQ(second.status, format::CheckStatus::kClean);
  EXPECT_TRUE(second.cache_hit);
  EXPECT_FALSE(second.cache_error);
}

TEST_F(FormatCheckerTest, CacheDetectsChangedContentWithSameSizeAndMtime) {
  const auto clean =
      formatted("module m; assign a=b; endmodule").formatted_text;
  write(clean);
  ASSERT_EQ(checkCached().status, format::CheckStatus::kClean);
  ASSERT_TRUE(checkCached().cache_hit);
  const auto mtime = fs::last_write_time(path());
  auto dirty = clean;
  const auto pos = dirty.find("a = b;");
  ASSERT_NE(pos, std::string::npos);
  dirty.replace(pos, std::string_view("a = b;").size(), "a =b ;");
  ASSERT_EQ(dirty.size(), clean.size());
  write(dirty);
  fs::last_write_time(path(), mtime);
  const auto result = checkCached();
  EXPECT_EQ(result.status, format::CheckStatus::kDirty);
  EXPECT_FALSE(result.cache_hit);
}

TEST_F(FormatCheckerTest, ChangedStyleAndBuildInvalidateCache) {
  auto style = format::FormatStyle::defaults();
  style.indentation_spaces = 4;
  write(formatted("module m; assign a=b; endmodule", style).formatted_text);
  ASSERT_EQ(checkCached(style).status, format::CheckStatus::kClean);
  ASSERT_TRUE(checkCached(style).cache_hit);
  const auto changed_style = checkCached();
  EXPECT_EQ(changed_style.status, format::CheckStatus::kDirty);
  EXPECT_FALSE(changed_style.cache_hit);
  const auto changed_build = checkCached(style, "another-build");
  EXPECT_EQ(changed_build.status, format::CheckStatus::kClean);
  EXPECT_FALSE(changed_build.cache_hit);
  EXPECT_TRUE(checkCached(style, "another-build").cache_hit);
}

TEST_F(FormatCheckerTest, DirtyFilesAndWarningsAreNotCached) {
  write("module m; assign   a=b; endmodule");
  EXPECT_EQ(checkCached().status, format::CheckStatus::kDirty);
  EXPECT_FALSE(checkCached().cache_hit);
  EXPECT_FALSE(fs::exists(cacheDirectory()));
  write(formatted("module m;\n  assign y  = foo]bar;   // bad\nendmodule\n")
            .formatted_text);
  const auto first = checkCached();
  ASSERT_FALSE(first.warnings.empty());
  const auto second = checkCached();
  EXPECT_FALSE(second.cache_hit);
  EXPECT_EQ(second.warnings.size(), first.warnings.size());
  EXPECT_FALSE(fs::exists(cacheDirectory()));
}

TEST_F(FormatCheckerTest, CacheFailureDoesNotFailCleanCheck) {
  write(formatted("module m; endmodule").formatted_text);
  {
    std::ofstream out{cacheDirectory()};
    out << "not a directory";
  }
  const auto result = checkCached();
  EXPECT_EQ(result.status, format::CheckStatus::kClean);
  EXPECT_FALSE(result.cache_hit);
  EXPECT_TRUE(result.cache_error);
}

TEST_F(FormatCheckerTest, CorruptRecordIsRecheckedAndReplaced) {
  write(formatted("module m; endmodule").formatted_text);
  ASSERT_EQ(checkCached().status, format::CheckStatus::kClean);
  ASSERT_TRUE(checkCached().cache_hit);
  for (const auto& entry : fs::directory_iterator(cacheDirectory())) {
    std::ofstream out{entry.path(), std::ios::binary | std::ios::trunc};
    out << "corrupted";
  }
  const auto result = checkCached();
  EXPECT_EQ(result.status, format::CheckStatus::kClean);
  EXPECT_FALSE(result.cache_hit);
  EXPECT_FALSE(result.cache_error);
  EXPECT_TRUE(checkCached().cache_hit);
}

TEST_F(FormatCheckerTest, MissingBuildIdentityDisablesCache) {
  write(formatted("module m; endmodule").formatted_text);
  const auto result = checkCached({}, "");
  EXPECT_EQ(result.status, format::CheckStatus::kClean);
  EXPECT_FALSE(result.cache_hit);
  EXPECT_TRUE(result.cache_error);
  EXPECT_FALSE(fs::exists(cacheDirectory()));
}

TEST_F(FormatCheckerTest, StreamingHashReadsAllBytes) {
  constexpr size_t kContentSize = 131073;
  std::string contents(kContentSize, 'x');
  contents.at(contents.size() / 2) = '\0';
  write(contents);
  EXPECT_EQ(format::sha256File(path()), format::sha256(contents));
  contents.back() = 'y';
  write(contents);
  EXPECT_EQ(format::sha256File(path()), format::sha256(contents));
  EXPECT_FALSE(format::sha256File(path().parent_path()).has_value());
  EXPECT_FALSE(format::sha256File(path().parent_path() / "absent").has_value());
}

TEST(CacheBuildIdTest, IdentityIsStableWithinProcess) {
  const auto& identity = format::formatterBuildId();
#if defined(__linux__)
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity.value_or("").size(), format::sha256("").size());
  EXPECT_EQ(format::formatterBuildId(), identity);
#else
  EXPECT_FALSE(identity.has_value());
#endif
}

}  // namespace

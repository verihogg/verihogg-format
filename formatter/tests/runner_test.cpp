#include "pipeline/runner.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "data/format_style.h"

namespace {

namespace fs = std::filesystem;

// Unformatted variant of the formatted test module with exactly the same size,
// or an empty string if the formatted text doesn't look as expected.
auto sameSizeDirty(std::string text) -> std::string {
  const std::string_view formatted = "a = b;";
  const auto pos = text.find(formatted);
  if (pos == std::string::npos) {
    return {};
  }
  text.replace(pos, formatted.size(), "a =b ;");
  return text;
}

class RunnerInplaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = fs::temp_directory_path() /
           ("verihogg_runner_test_" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir_);
    path_ = dir_ / "input.sv";
  }

  void TearDown() override { fs::remove_all(dir_); }

  void writeInput(const std::string& content) const {
    std::ofstream f{path_, std::ios::binary | std::ios::trunc};
    f << content;
  }

  [[nodiscard]] auto readInput() const -> std::string {
    std::ifstream f{path_, std::ios::binary};
    return {std::istreambuf_iterator<char>{f},
            std::istreambuf_iterator<char>{}};
  }

  void runInplace() {
    format::RunConfig run;
    run.inplace = true;
    std::vector<fs::path> files{path_};
    format::runFormatter(files, format::FormatStyle::defaults(), run,
                         {.out = &out_, .err = &err_});
  }

  auto runCheck() -> int { return runCheck({path_}); }

  auto runCheck(const std::vector<fs::path>& files) -> int {
    format::RunConfig run;
    run.check = true;
    return format::runFormatter(files, format::FormatStyle::defaults(), run,
                                {.out = &out_, .err = &err_});
  }

  auto runCheckCached() -> int {
    format::RunConfig run;
    run.check = true;
    run.cache_file = cacheFile();
    std::vector<fs::path> files{path_};
    return format::runFormatter(files, format::FormatStyle::defaults(), run,
                                {.out = &out_, .err = &err_});
  }

  // Writes content and moves mtime into the past, out of the racy window in
  // which the cache does not trust mtime.
  void writeInputAged(const std::string& content,
                      std::chrono::hours age = std::chrono::hours(1)) const {
    writeInput(content);
    setInputAge(age);
  }

  void setInputAge(std::chrono::hours age) const {
    fs::last_write_time(path_, fs::file_time_type::clock::now() - age);
  }

  void clearStreams() {
    out_.str("");
    err_.str("");
  }

  [[nodiscard]] auto cacheFile() const -> fs::path { return dir_ / "cache"; }

  [[nodiscard]] auto makeFile(const fs::path& name,
                              const std::string& content) const -> fs::path {
    auto path = dir_ / name;
    std::ofstream f{path, std::ios::binary | std::ios::trunc};
    f << content;
    return path;
  }

  [[nodiscard]] auto path() const -> const fs::path& { return path_; }
  [[nodiscard]] auto out() const -> std::string { return out_.str(); }
  [[nodiscard]] auto err() const -> std::string { return err_.str(); }

 private:
  fs::path dir_;
  fs::path path_;
  std::ostringstream out_;
  std::ostringstream err_;
};

TEST_F(RunnerInplaceTest, RewritesUnformattedFile) {
  const std::string original = "module m;\nassign   a=b;\nendmodule\n";
  writeInput(original);

  runInplace();

  EXPECT_NE(readInput(), original);
}

TEST_F(RunnerInplaceTest, SkipsWriteWhenAlreadyFormatted) {
  writeInput("module m;\nassign   a=b;\nendmodule\n");
  runInplace();
  const std::string formatted = readInput();

  // Move mtime into the past so that any rewrite would be observable.
  const auto old_time = fs::last_write_time(path()) - std::chrono::hours(1);
  fs::last_write_time(path(), old_time);

  runInplace();

  EXPECT_EQ(readInput(), formatted);
  EXPECT_EQ(fs::last_write_time(path()), old_time);
}

TEST_F(RunnerInplaceTest, CheckReportsUnformattedFileWithoutWriting) {
  const std::string original = "module m;\nassign   a=b;\nendmodule\n";
  writeInput(original);

  EXPECT_EQ(runCheck(), 1);
  EXPECT_EQ(readInput(), original);
  EXPECT_TRUE(out().empty());
  EXPECT_NE(err().find("Needs formatting"), std::string::npos);
}

TEST_F(RunnerInplaceTest, CheckPassesOnFormattedFile) {
  writeInput("module m;\nassign   a=b;\nendmodule\n");
  runInplace();

  EXPECT_EQ(runCheck(), 0);
  EXPECT_EQ(err().find("Needs formatting"), std::string::npos);
}

TEST_F(RunnerInplaceTest, CheckManyFilesReportsDirtyInInputOrder) {
  const std::string dirty = "module m;\nassign   a=b;\nendmodule\n";
  writeInput(dirty);
  runInplace();
  const std::string clean = readInput();

  constexpr int kFileCount = 32;
  std::vector<fs::path> files;
  std::string expected_err;
  for (int i = 0; i < kFileCount; ++i) {
    const bool is_dirty = i % 3 == 0;
    files.push_back(
        makeFile("f" + std::to_string(i) + ".sv", is_dirty ? dirty : clean));
    if (is_dirty) {
      expected_err += "Needs formatting: " + files.back().string() + "\n";
    }
  }

  EXPECT_EQ(runCheck(files), (kFileCount + 2) / 3);
  EXPECT_EQ(err(), expected_err);
}

// ---------------------------------------------------------------------------
// --check --cache
// ---------------------------------------------------------------------------

TEST_F(RunnerInplaceTest, CacheFastPathSkipsFileWithSameSizeAndMtime) {
  writeInput("module m;\nassign   a=b;\nendmodule\n");
  runInplace();
  const std::string clean = readInput();
  const std::string dirty = sameSizeDirty(clean);
  ASSERT_FALSE(dirty.empty());

  writeInputAged(clean);
  EXPECT_EQ(runCheckCached(), 0);
  EXPECT_TRUE(fs::exists(cacheFile()));

  // Same size and mtime: the file must not even be read, so the new
  // unformatted content goes unnoticed. This proves the fast path is taken.
  const auto mtime = fs::last_write_time(path());
  writeInput(dirty);
  fs::last_write_time(path(), mtime);
  EXPECT_EQ(runCheckCached(), 0);

  // A different mtime makes the checker look at the content again.
  setInputAge(std::chrono::hours(2));
  EXPECT_EQ(runCheckCached(), 1);
}

TEST_F(RunnerInplaceTest, CacheAcceptsTouchedFileByContentHash) {
  writeInput("module m;\nassign   a=b;\nendmodule\n");
  runInplace();
  setInputAge(std::chrono::hours(1));
  EXPECT_EQ(runCheckCached(), 0);
  const auto cache_mtime = fs::last_write_time(cacheFile());

  setInputAge(std::chrono::hours(2));
  EXPECT_EQ(runCheckCached(), 0);
  // The new mtime is recorded, so the cache is rewritten.
  EXPECT_NE(fs::last_write_time(cacheFile()), cache_mtime);
}

TEST_F(RunnerInplaceTest, CacheIgnoresRecentMtime) {
  writeInput("module m;\nassign   a=b;\nendmodule\n");
  runInplace();
  const std::string clean = readInput();
  const std::string dirty = sameSizeDirty(clean);
  ASSERT_FALSE(dirty.empty());

  // The file was just written, so its mtime could repeat after another edit.
  EXPECT_EQ(runCheckCached(), 0);
  const auto mtime = fs::last_write_time(path());
  writeInput(dirty);
  fs::last_write_time(path(), mtime);
  EXPECT_EQ(runCheckCached(), 1);
}

TEST_F(RunnerInplaceTest, CacheDoesNotRememberDirtyFiles) {
  writeInputAged("module m;\nassign   a=b;\nendmodule\n");

  EXPECT_EQ(runCheckCached(), 1);
  EXPECT_EQ(runCheckCached(), 1);
}

TEST_F(RunnerInplaceTest, CorruptedCacheIsIgnoredWithWarning) {
  writeInput("module m;\nassign   a=b;\nendmodule\n");
  runInplace();
  setInputAge(std::chrono::hours(1));
  {
    std::ofstream f{cacheFile(), std::ios::binary | std::ios::trunc};
    f << "garbage\n";
  }

  EXPECT_EQ(runCheckCached(), 0);
  EXPECT_NE(err().find("corrupted cache"), std::string::npos);

  clearStreams();
  EXPECT_EQ(runCheckCached(), 0);
  EXPECT_TRUE(err().empty());
}

TEST_F(RunnerInplaceTest, CheckReportsMissingFile) {
  EXPECT_EQ(runCheck(), 1);
  EXPECT_NE(err().find("cannot read"), std::string::npos);
}

}  // namespace

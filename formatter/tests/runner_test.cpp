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

#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/cache_build_id.h"

namespace {
namespace fs = std::filesystem;

class RunnerCheckTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto base =
        fs::temp_directory_path() /
        ("verihogg_check_test_" +
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
  }

  void TearDown() override {
    if (!dir_.empty()) {
      fs::remove_all(dir_);
    }
  }

  auto makeFile(const fs::path& name, std::string_view text) -> fs::path {
    const auto path = dir_ / name;
    std::ofstream out{path, std::ios::binary};
    out << text;
    return path;
  }

  static auto formatted(std::string_view text, format::FormatStyle style = {})
      -> std::string {
    LexContext ctx;
    const auto tokens = ctx.lex_string(text);
    return format::format(tokens, style, ctx.source_text()).formatted_text;
  }

  auto check(const std::vector<fs::path>& files,
             const std::vector<format::FormatStyle>& styles,
             bool use_cache = false) -> int {
    format::RunConfig run;
    run.check = true;
    if (use_cache) {
      run.cache_directory = (dir_ / "cache").string();
    }
    return format::runFormatter(files, styles, run,
                                {.out = &out_, .err = &err_});
  }

  [[nodiscard]] auto dir() const -> const fs::path& { return dir_; }
  [[nodiscard]] auto out() const -> std::string { return out_.str(); }
  [[nodiscard]] auto err() const -> std::string { return err_.str(); }

 private:
  fs::path dir_;
  std::ostringstream out_;
  std::ostringstream err_;
};

TEST_F(RunnerCheckTest, ReportsDirtyAndMissingFilesInInputOrderWithoutWriting) {
  const std::string dirty = "module m; assign   a=b; endmodule";
  const auto clean = makeFile("clean.sv", formatted(dirty));
  const auto path = makeFile("dirty.sv", dirty);
  const auto missing = dir() / "missing.sv";
  const auto old_time = fs::last_write_time(path) - std::chrono::hours(1);
  fs::last_write_time(path, old_time);

  EXPECT_EQ(check({clean, path, missing}, {{}, {}, {}}), 2);
  EXPECT_TRUE(out().empty());
  EXPECT_EQ(err(), "Needs formatting: " + path.string() +
                       "\nError: cannot read " + missing.string() + "\n");
  std::ifstream in{path, std::ios::binary};
  const std::string after{std::istreambuf_iterator<char>{in},
                          std::istreambuf_iterator<char>{}};
  EXPECT_EQ(after, dirty);
  EXPECT_EQ(fs::last_write_time(path), old_time);
}

TEST_F(RunnerCheckTest, ReadErrorDoesNotAbortOtherChecks) {
  const auto dirty = makeFile("dirty.sv", "module m; assign   a=b; endmodule");
  EXPECT_EQ(check({dir(), dirty}, {{}, {}}), 2);
  EXPECT_TRUE(out().empty());
  EXPECT_EQ(err(), "Error: cannot read " + dir().string() +
                       "\nNeeds formatting: " + dirty.string() + "\n");
}

TEST_F(RunnerCheckTest, UsesIndividualStylesForFiles) {
  const std::string source = "module m; assign a=b; endmodule";
  const auto first = format::FormatStyle::defaults();
  auto second = first;
  second.indentation_spaces = 4;
  const auto a = makeFile("a.sv", formatted(source, first));
  const auto b = makeFile("b.sv", formatted(source, second));

  EXPECT_EQ(check({a, b}, {first, second}), 0);
  EXPECT_TRUE(out().empty());
  EXPECT_TRUE(err().empty());
  EXPECT_EQ(check({b}, {first}), 1);
}

TEST_F(RunnerCheckTest, ReportsRecoveryWarnings) {
  const auto path = makeFile(
      "bad.sv", "module m;\n  assign y  = foo]bar;   // bad\nendmodule\n");
  check({path}, {{}});
  EXPECT_TRUE(out().empty());
  EXPECT_NE(err().find("invalid-annotation"), std::string::npos);
  EXPECT_NE(err().find(path.string()), std::string::npos);
}

TEST_F(RunnerCheckTest, ManyFilesKeepIndividualStylesAndDiagnosticOrder) {
  const std::string dirty = "module m; assign   a=b; endmodule";
  std::vector<fs::path> files;
  std::vector<format::FormatStyle> styles;
  std::string expected;
  int failed = 0;
  constexpr size_t kFileCount = 64;
  constexpr size_t kMissingInterval = 5;
  for (size_t i = 0; i < kFileCount; ++i) {
    auto style = format::FormatStyle::defaults();
    style.indentation_spaces = i % 2 == 0 ? 2 : 4;
    styles.push_back(style);
    const auto name = "file" + std::to_string(i) + ".sv";
    if (i % kMissingInterval == 0) {
      files.push_back(dir() / name);
      expected += "Error: cannot read " + files.back().string() + "\n";
      ++failed;
    } else if (i % 3 == 0) {
      files.push_back(makeFile(name, dirty));
      expected += "Needs formatting: " + files.back().string() + "\n";
      ++failed;
    } else {
      files.push_back(makeFile(name, formatted(dirty, style)));
    }
  }

  EXPECT_EQ(check(files, styles), failed);
  EXPECT_TRUE(out().empty());
  EXPECT_EQ(err(), expected);
}

TEST_F(RunnerCheckTest, EmptyInputHasNoDiagnostics) {
  EXPECT_EQ(check({}, {}), 0);
  EXPECT_TRUE(out().empty());
  EXPECT_TRUE(err().empty());
}

TEST_F(RunnerCheckTest, CachedChecksKeepPerFileStylesAndDiagnostics) {
  if (!format::formatterBuildId()) {
    GTEST_SKIP() << "Build identity is unavailable on this platform";
  }
  const std::string source = "module m; assign a=b; endmodule";
  auto style = format::FormatStyle::defaults();
  style.indentation_spaces = 4;
  const auto clean = makeFile("clean.sv", formatted(source, style));
  const auto dirty = makeFile("dirty.sv", source);
  EXPECT_EQ(check({clean, dirty}, {style, {}}, true), 1);
  const auto expected = "Needs formatting: " + dirty.string() + "\n";
  EXPECT_EQ(err(), expected);
  EXPECT_EQ(check({clean, dirty}, {style, {}}, true), 1);
  EXPECT_EQ(err(), expected + expected);
  EXPECT_TRUE(out().empty());
}

TEST_F(RunnerCheckTest, CacheWriteFailureIsOnlyWarning) {
  if (!format::formatterBuildId()) {
    GTEST_SKIP() << "Build identity is unavailable on this platform";
  }
  makeFile("cache", "not a directory");
  const auto clean = makeFile("clean.sv", formatted("module m; endmodule"));
  EXPECT_EQ(check({clean}, {{}}, true), 0);
  EXPECT_NE(err().find("Warning: cannot use cache"), std::string::npos);
  EXPECT_TRUE(out().empty());
}

}  // namespace

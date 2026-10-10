#include "cli/format_args.h"

#include <gtest/gtest.h>

#include "data/format_style.h"

namespace {
class FormatArgsTest : public ::testing::Test {
 protected:
  void SetUp() override { binder.emplace(); }

  [[nodiscard]] auto parse(const std::vector<const char*>& args) -> bool {
    if (!binder.has_value()) {
      return false;
    }

    std::vector<std::string> storage;
    storage.reserve(args.size() + 1);
    storage.emplace_back("formatter");
    for (const auto* arg : args) {
      storage.emplace_back(arg);
    }

    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (auto& s : storage) {
      argv.push_back(s.data());
    }

    try {
      binder->parse(static_cast<int>(argv.size()), argv.data());
      return true;
    } catch (const CLI::ParseError&) {
      return false;
    }
  }

  [[nodiscard]] auto buildStyle(const std::vector<const char*>& args = {})
      -> std::pair<format::FormatStyle, format::RunConfig> {
    EXPECT_TRUE(parse(args));
    if (!binder.has_value()) {
      throw std::runtime_error("binder is not initialized");
    }
    return binder->buildStyle();
  }

  [[nodiscard]] auto getBinder() -> format::FormatArgsBinder& {
    if (!binder.has_value()) {
      throw std::runtime_error("binder is not initialized");
    }
    return *binder;
  }

 private:
  std::optional<format::FormatArgsBinder> binder;
};

// ---------------------------------------------------------------------------
// Default values
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, DefaultsAreApplied) {
  auto [style, run] = buildStyle();

  EXPECT_EQ(style.column_limit, format::defaults::kColumnLimit);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
  EXPECT_EQ(style.wrap_spaces, format::defaults::kWrapSpaces);
  EXPECT_EQ(style.line_break_penalty, format::defaults::kLineBreakPenalty);
  EXPECT_EQ(style.over_column_limit_penalty,
            format::defaults::kOverColumnLimitPenalty);
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kAuto);
  EXPECT_FALSE(run.inplace);
}

TEST_F(FormatArgsTest, CustomColumnLimit) {
  auto [style, run] = buildStyle({"--column_limit", "120"});

  EXPECT_EQ(style.column_limit, 120U);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
  EXPECT_EQ(style.wrap_spaces, format::defaults::kWrapSpaces);
  EXPECT_EQ(style.line_break_penalty, format::defaults::kLineBreakPenalty);
  EXPECT_EQ(style.over_column_limit_penalty,
            format::defaults::kOverColumnLimitPenalty);
  EXPECT_FALSE(run.inplace);
}

TEST_F(FormatArgsTest, CustomIndentationSpaces) {
  auto [style, run] = buildStyle({"--indentation_spaces", "4"});

  EXPECT_EQ(style.indentation_spaces, 4U);
  EXPECT_EQ(style.column_limit, format::defaults::kColumnLimit);
  EXPECT_EQ(style.wrap_spaces, format::defaults::kWrapSpaces);
}

TEST_F(FormatArgsTest, CustomWrapSpaces) {
  auto [style, run] = buildStyle({"--wrap_spaces", "8"});

  EXPECT_EQ(style.wrap_spaces, 8U);
  EXPECT_EQ(style.column_limit, format::defaults::kColumnLimit);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
}

TEST_F(FormatArgsTest, CustomLineBreakPenalty) {
  auto [style, run] = buildStyle({"--line_break_penalty", "10"});

  EXPECT_EQ(style.line_break_penalty, 10U);
  EXPECT_EQ(style.over_column_limit_penalty,
            format::defaults::kOverColumnLimitPenalty);
}

TEST_F(FormatArgsTest, CustomOverColumnLimitPenalty) {
  auto [style, run] = buildStyle({"--over_column_limit_penalty", "50"});

  EXPECT_EQ(style.over_column_limit_penalty, 50U);
  EXPECT_EQ(style.line_break_penalty, format::defaults::kLineBreakPenalty);
}

// ---------------------------------------------------------------------------
// --inplace flag
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, InplaceFlagSetsRunConfig) {
  auto [style, run] = buildStyle({"--inplace"});

  EXPECT_TRUE(run.inplace);
  EXPECT_EQ(style.column_limit, format::defaults::kColumnLimit);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
}

// ---------------------------------------------------------------------------
// --line_terminator: all three valid values
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, LineTerminatorAuto) {
  auto [style, run] = buildStyle({"--line_terminator", "auto"});
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kAuto);
}

TEST_F(FormatArgsTest, LineTerminatorLf) {
  auto [style, run] = buildStyle({"--line_terminator", "lf"});
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kLf);
}

TEST_F(FormatArgsTest, LineTerminatorCrlf) {
  auto [style, run] = buildStyle({"--line_terminator", "crlf"});
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kCrLf);
}

TEST_F(FormatArgsTest, InvalidLineTerminatorRejectedByParser) {
  EXPECT_FALSE(parse({"--line_terminator", "windows"}));
}

TEST_F(FormatArgsTest, EmptyLineTerminatorRejectedByParser) {
  EXPECT_FALSE(parse({"--line_terminator", ""}));
}

// ---------------------------------------------------------------------------
// Short Aliases (Тесты для короче флагов: -c, -i, -w и т.д.)
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, ShortFlagsWork) {
  auto [style, run] = buildStyle({"-c", "80", "-i", "4", "-w", "8", "-b", "5",
                                  "-p", "200", "-t", "lf", "-n"});

  EXPECT_EQ(style.column_limit, 80U);
  EXPECT_EQ(style.indentation_spaces, 4U);
  EXPECT_EQ(style.wrap_spaces, 8U);
  EXPECT_EQ(style.line_break_penalty, 5U);
  EXPECT_EQ(style.over_column_limit_penalty, 200U);
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kLf);
  EXPECT_TRUE(run.inplace);
}

// ---------------------------------------------------------------------------
// Boundary values for numeric flags
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, ColumnLimitOfOne) {
  auto [style, run] = buildStyle({"--column_limit", "1"});
  EXPECT_EQ(style.column_limit, 1U);
}

TEST_F(FormatArgsTest, LargeColumnLimit) {
  constexpr auto kMax = std::numeric_limits<uint32_t>::max();
  auto [style, run] =
      buildStyle({"--column_limit", std::to_string(kMax).c_str()});
  EXPECT_EQ(style.column_limit, kMax);
}

TEST_F(FormatArgsTest, ZeroIndentationSpaces) {
  auto [style, run] = buildStyle({"--indentation_spaces", "0"});
  EXPECT_EQ(style.indentation_spaces, 0U);
}

TEST_F(FormatArgsTest, ColumnLimitOfZeroAcceptedByParser) {
  auto [style, run] = buildStyle({"--column_limit", "0"});
  EXPECT_EQ(style.column_limit, 0U);
}

// ---------------------------------------------------------------------------
// Invalid input and unknown flags
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, NonNumericColumnLimitRejectedByParser) {
  EXPECT_FALSE(parse({"--column_limit", "abc"}));
}

TEST_F(FormatArgsTest, NegativeColumnLimitRejectedByParser) {
  EXPECT_FALSE(parse({"--column_limit", "-1"}));
}

}  // namespace

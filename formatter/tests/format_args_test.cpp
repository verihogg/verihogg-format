#include "cli/format_args.h"

#include <gtest/gtest.h>

#include "data/format_style.h"

namespace {
class FormatArgsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    driver.addStandardArgs();
    binder.emplace(driver);
  }

  [[nodiscard]] auto parse(std::vector<const char*> args) -> bool {
    args.insert(args.begin(), "formatter");
    return driver.parseCommandLine(static_cast<int>(args.size()), args.data());
  }

  [[nodiscard]] auto buildStyle(std::vector<const char*> args = {})
      -> std::pair<format::FormatStyle, format::RunConfig> {
    EXPECT_TRUE(parse(std::move(args)));
    return binder->buildStyle();
  }

  template <typename Exception>
  auto expectBuildStyleThrows() -> void {
    EXPECT_THROW(std::ignore = binder->buildStyle(), Exception);
  }

 private:
  slang::driver::Driver driver;
  std::optional<format::FormatArgsBinder> binder;
};

// ---------------------------------------------------------------------------
// Дефолтные значения
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
// Флаг --inplace
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, InplaceFlagSetsRunConfig) {
  auto [style, run] = buildStyle({"--inplace"});

  EXPECT_TRUE(run.inplace);
  // --inplace не должен влиять на стиль форматирования.
  EXPECT_EQ(style.column_limit, format::defaults::kColumnLimit);
  EXPECT_EQ(style.indentation_spaces, format::defaults::kIndentationSpaces);
}

// ---------------------------------------------------------------------------
// --line_terminator: все три допустимых значения
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

TEST_F(FormatArgsTest, InvalidLineTerminatorThrows) {
  // parseCommandLine принимает строку как есть - исключение бросает buildStyle.
  ASSERT_TRUE(parse({"--line_terminator", "windows"}));
  expectBuildStyleThrows<std::invalid_argument>();
}

TEST_F(FormatArgsTest, EmptyLineTerminatorThrows) {
  ASSERT_TRUE(parse({"--line_terminator", ""}));
  expectBuildStyleThrows<std::invalid_argument>();
}

// ---------------------------------------------------------------------------
// Несколько флагов одновременно - независимость друг от друга
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, MultipleNumericFlagsAreIndependent) {
  auto [style, run] = buildStyle({
      "--column_limit",
      "80",
      "--indentation_spaces",
      "4",
      "--wrap_spaces",
      "8",
      "--line_break_penalty",
      "5",
      "--over_column_limit_penalty",
      "200",
      "--line_terminator",
      "lf",
      "--inplace",
  });

  EXPECT_EQ(style.column_limit, 80U);
  EXPECT_EQ(style.indentation_spaces, 4U);
  EXPECT_EQ(style.wrap_spaces, 8U);
  EXPECT_EQ(style.line_break_penalty, 5U);
  EXPECT_EQ(style.over_column_limit_penalty, 200U);
  EXPECT_EQ(style.line_terminator, format::LineTerminator::kLf);
  EXPECT_TRUE(run.inplace);
}

// ---------------------------------------------------------------------------
// Граничные значения числовых флагов
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, ColumnLimitOfOne) {
  // Значение 1 технически допустимо на уровне CLI - бизнес-логика проверяет
  // корректность отдельно, парсер не должен отвергать.
  auto [style, run] = buildStyle({"--column_limit", "1"});
  EXPECT_EQ(style.column_limit, 1U);
}

TEST_F(FormatArgsTest, LargeColumnLimit) {
  // slang парсит числовые аргументы как uint32_t, поэтому верхняя граница -
  // numeric_limits<uint32_t>::max(), а не size_t.
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
  // Ноль не имеет смысла семантически, но парсер не должен его отвергать -
  // валидация значений - ответственность бизнес-логики.
  auto [style, run] = buildStyle({"--column_limit", "0"});
  EXPECT_EQ(style.column_limit, 0U);
}

// ---------------------------------------------------------------------------
// Невалидный числовой флаг - parseCommandLine должен вернуть false.
// ---------------------------------------------------------------------------

TEST_F(FormatArgsTest, NonNumericColumnLimitRejectedByParser) {
  EXPECT_FALSE(parse({"--column_limit", "abc"}));
}

TEST_F(FormatArgsTest, NegativeColumnLimitRejectedByParser) {
  EXPECT_FALSE(parse({"--column_limit", "-1"}));
}

TEST_F(FormatArgsTest, UnknownFlagRejectedByParser) {
  EXPECT_FALSE(parse({"--unknown_flag"}));
}

}  // namespace

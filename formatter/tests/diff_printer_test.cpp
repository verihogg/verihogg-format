#include "pipeline/diff_printer.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "pipeline/unified_diff.h"

namespace {

auto printed(std::string_view patch, bool color = true) -> std::string {
  std::ostringstream out;
  format::printDiff(out, patch, {.color = color});
  return out.str();
}

auto withoutStyles(std::string text) -> std::string {
  constexpr std::array<std::string_view, 7> kStyles = {
      "\033[0m",  "\033[1m",  "\033[7m", "\033[27m",
      "\033[31m", "\033[32m", "\033[36m"};
  for (const auto style : kStyles) {
    for (auto pos = text.find(style); pos != std::string::npos;
         pos = text.find(style, pos)) {
      text.erase(pos, style.size());
    }
  }
  return text;
}

TEST(DiffPrinterTest, PlainOutputPreservesPatchBytes) {
  const auto patch =
      format::makeUnifiedDiff("old\r\nlast", "new\r\nlast\n", "space name.sv");
  EXPECT_EQ(printed(patch, false), patch);
}

TEST(DiffPrinterTest, HighlightsChangesAndHeadersWithoutColoringContext) {
  EXPECT_EQ(printed("diff --git a/a.sv b/a.sv\n"
                    "--- a/a.sv\n+++ b/a.sv\n"
                    "@@ -1,2 +1,2 @@\n context\n-old\n+new\n"
                    "\\ No newline at end of file\n"),
            "\033[1mdiff --git a/a.sv b/a.sv\033[0m\n"
            "\033[1m--- a/a.sv\033[0m\n\033[1m+++ b/a.sv\033[0m\n"
            "\033[36m@@ -1,2 +1,2 @@\033[0m\n context\n"
            "\033[31m-\033[7mold\033[27m\033[0m\n"
            "\033[32m+\033[7mnew\033[27m\033[0m\n"
            "\\ No newline at end of file\n");
}

TEST(DiffPrinterTest, ContentResemblingFileHeadersUsesChangeColors) {
  const auto patch = format::makeUnifiedDiff("-- old\n", "++ new\n", "a.sv");
  const auto output = printed(patch);
  EXPECT_NE(output.find("\033[31m-\033[7m-- old\033[27m\033[0m\n"),
            std::string::npos);
  EXPECT_NE(output.find("\033[32m+\033[7m++ new\033[27m\033[0m\n"),
            std::string::npos);
}

TEST(DiffPrinterTest, MultipleFilesKeepTheirHeaderColors) {
  const auto patch = format::makeUnifiedDiff("a\n", "b\n", "first.sv") +
                     format::makeUnifiedDiff("a\n", "b\n", "second.sv");
  const auto output = printed(patch);
  EXPECT_NE(output.find("\033[1mdiff --git a/second.sv b/second.sv\033[0m\n"),
            std::string::npos);
  EXPECT_NE(output.find("\033[1m--- a/second.sv\033[0m\n"), std::string::npos);
}

TEST(DiffPrinterTest, ResetsColorBeforeLineEndingsAndAtEndOfOutput) {
  EXPECT_EQ(printed("@@ -1 +1 @@\r\n-old\r\n+new"),
            "\033[36m@@ -1 +1 @@\033[0m\r\n"
            "\033[31m-\033[7mold\033[27m\033[0m\r\n"
            "\033[32m+\033[7mnew\033[27m\033[0m");
  EXPECT_TRUE(printed("").empty());
}

TEST(DiffPrinterTest, HighlightsInsertedSpacesWithoutHighlightingCodeBetween) {
  const auto patch =
      format::makeUnifiedDiff("  assign a=b;\n", "  assign a = b;\n", "a.sv");
  const auto output = printed(patch);
  EXPECT_NE(output.find("\033[31m-  assign a=b;\033[0m\n"), std::string::npos);
  EXPECT_NE(output.find("\033[32m+  assign a\033[7m \033[27m="
                        "\033[7m \033[27mb;\033[0m\n"),
            std::string::npos);
  EXPECT_EQ(withoutStyles(output), patch);
}

TEST(DiffPrinterTest, HighlightsRemovedIndentationAndExtraSpaces) {
  const auto patch = format::makeUnifiedDiff("    assign  a = b;\n",
                                             "  assign a = b;\n", "a.sv");
  const auto output = printed(patch);
  EXPECT_NE(output.find("\033[31m-  \033[7m  \033[27massign "
                        "\033[7m \033[27ma = b;\033[0m\n"),
            std::string::npos);
  EXPECT_NE(output.find("\033[32m+  assign a = b;\033[0m\n"),
            std::string::npos);
  EXPECT_EQ(withoutStyles(output), patch);
}

TEST(DiffPrinterTest, HighlightsTabsAndTrailingWhitespace) {
  const auto patch = format::makeUnifiedDiff(" \tassign a = b;\t\n",
                                             "  assign a = b; \n", "a.sv");
  const auto output = printed(patch);
  EXPECT_NE(output.find("\033[31m- \033[7m\t\033[27massign a = b;"
                        "\033[7m\t\033[27m\033[0m\n"),
            std::string::npos);
  EXPECT_NE(output.find("\033[32m+ \033[7m \033[27massign a = b;"
                        "\033[7m \033[27m\033[0m\n"),
            std::string::npos);
  EXPECT_EQ(withoutStyles(output), patch);
}

TEST(DiffPrinterTest, TextReplacementKeepsCommonPrefixAndSuffixUnemphasized) {
  const auto patch = format::makeUnifiedDiff("assign old_value;\n",
                                             "assign new_value;\n", "a.sv");
  const auto output = printed(patch);
  EXPECT_NE(output.find("\033[31m-assign \033[7mold\033[27m_value;\033[0m"),
            std::string::npos);
  EXPECT_NE(output.find("\033[32m+assign \033[7mnew\033[27m_value;\033[0m"),
            std::string::npos);
  EXPECT_EQ(withoutStyles(output), patch);
}

TEST(DiffPrinterTest, EmphasisDoesNotSplitUtf8CodePoints) {
  const auto patch =
      format::makeUnifiedDiff("// café à\n", "// cafê ã\n", "a.sv");
  const auto output = printed(patch);
  EXPECT_NE(output.find("\033[7mé à\033[27m"), std::string::npos);
  EXPECT_NE(output.find("\033[7mê ã\033[27m"), std::string::npos);
  EXPECT_EQ(withoutStyles(output), patch);
}

TEST(DiffPrinterTest, MissingNewlineMarkersDoNotPreventPairing) {
  const auto patch =
      format::makeUnifiedDiff("assign a=b;", "assign a = b;", "a.sv");
  const auto output = printed(patch);
  EXPECT_NE(output.find("\033[7m \033[27m"), std::string::npos);
  EXPECT_NE(output.find("\033[0m\n\\ No newline at end of file\n"),
            std::string::npos);
  EXPECT_EQ(withoutStyles(output), patch);
}

TEST(DiffPrinterTest, MultiLineReplacementsKeepWholeLineColors) {
  const std::array<std::pair<std::string_view, std::string_view>, 3> sources = {
      {{"old\n", "new\nother\n"},
       {"old\nother\n", "new\n"},
       {"old\nother\n", "new\nextra\n"}}};
  for (const auto& [original, formatted] : sources) {
    const auto patch = format::makeUnifiedDiff(original, formatted, "a.sv");
    const auto output = printed(patch);
    EXPECT_EQ(output.find("\033[7m"), std::string::npos);
    EXPECT_NE(output.find("\033[31m-old\033[0m\n"), std::string::npos);
    EXPECT_NE(output.find("\033[32m+new\033[0m\n"), std::string::npos);
    EXPECT_EQ(withoutStyles(output), patch);
  }
}

TEST(DiffPrinterTest, SeparateHunksEachHighlightTheirOwnReplacement) {
  const auto patch = format::makeUnifiedDiff(
      "assign a=b;\ncontext\nassign c=d;\n",
      "assign a = b;\ncontext\nassign c = d;\n", "a.sv", 0);
  const auto output = printed(patch);
  EXPECT_NE(output.find("a\033[7m \033[27m=\033[7m \033[27mb;"),
            std::string::npos);
  EXPECT_NE(output.find("c\033[7m \033[27m=\033[7m \033[27md;"),
            std::string::npos);
  EXPECT_EQ(withoutStyles(output), patch);
}

TEST(DiffPrinterTest, LongLinesAndCrLfPreserveTheirBytes) {
  constexpr size_t kLength = 100000;
  const std::string prefix(kLength, 'x');
  const auto patch =
      format::makeUnifiedDiff(prefix + "=b;\r\n", prefix + " = b;\r\n", "a.sv");
  const auto output = printed(patch);
  EXPECT_NE(output.find("\033[7m \033[27m=\033[7m \033[27mb;\033[0m\r\n"),
            std::string::npos);
  EXPECT_EQ(withoutStyles(output), patch);
  EXPECT_EQ(printed(patch, false), patch);
}

}  // namespace

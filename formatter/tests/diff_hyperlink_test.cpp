#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "pipeline/diff_printer.h"
#include "pipeline/unified_diff.h"

namespace {

auto linked(std::string_view patch,
            const std::filesystem::path& path = "input.sv", bool color = false)
    -> std::string {
  std::ostringstream out;
  format::printDiff(out, patch,
                    {.color = color, .hyperlinks = true, .source_path = path});
  return out.str();
}

auto withoutHyperlinks(std::string text) -> std::string {
  constexpr std::string_view kOpen = "\033]8;;";
  constexpr std::string_view kTerminator = "\033\\";
  for (auto pos = text.find(kOpen); pos != std::string::npos;
       pos = text.find(kOpen, pos)) {
    const auto end = text.find(kTerminator, pos + kOpen.size());
    if (end == std::string::npos) {
      throw std::runtime_error("Unterminated hyperlink");
    }
    text.erase(pos, end + kTerminator.size() - pos);
  }
  return text;
}

TEST(DiffHyperlinkTest, LinksFirstChangedOriginalLineInsteadOfContextStart) {
  const auto patch = format::makeUnifiedDiff(
      "first\nsecond\nold\nlast\n", "first\nsecond\nnew\nlast\n", "input.sv");
  const auto output = linked(patch);
  EXPECT_NE(output.find("#3\033\\@@ -1,4 +1,4 @@\033]8;;\033\\\n"),
            std::string::npos);
  EXPECT_NE(output.find("\033]8;;file:///"), std::string::npos);
  EXPECT_EQ(output.find("\033["), std::string::npos);
  EXPECT_EQ(withoutHyperlinks(output), patch);
}

TEST(DiffHyperlinkTest,
     SeparateHunksKeepOriginalCoordinatesAfterLineCountChanges) {
  const auto patch =
      format::makeUnifiedDiff("old\na\nb\nc\nd\nlast\n",
                              "new\nextra\na\nb\nc\nd\nend\n", "input.sv", 1);
  const auto output = linked(patch);
  EXPECT_NE(output.find("#1\033\\@@"), std::string::npos);
  EXPECT_NE(output.find("#6\033\\@@"), std::string::npos);
  EXPECT_EQ(withoutHyperlinks(output), patch);
}

TEST(DiffHyperlinkTest, PureInsertionsUsePrecedingLineWithAndWithoutContext) {
  for (const auto context : {0U, 3U}) {
    const auto patch = format::makeUnifiedDiff(
        "first\nlast\n", "first\nnew\nlast\n", "input.sv", context);
    const auto output = linked(patch);
    EXPECT_NE(output.find("#1\033\\@@"), std::string::npos);
    EXPECT_EQ(withoutHyperlinks(output), patch);
  }
}

TEST(DiffHyperlinkTest, EmptyOriginalAndBeginningInsertionsUseLineOne) {
  for (const auto original : {"", "last\n"}) {
    const auto patch = format::makeUnifiedDiff(
        original, std::string("new\n") + original, "input.sv", 0);
    EXPECT_NE(linked(patch).find("#1\033\\@@"), std::string::npos);
  }
}

TEST(DiffHyperlinkTest, PureDeletionUsesFirstRemovedLine) {
  const auto patch = format::makeUnifiedDiff("first\nold\nlast\n",
                                             "first\nlast\n", "input.sv");
  EXPECT_NE(linked(patch).find("#2\033\\@@"), std::string::npos);
}

TEST(DiffHyperlinkTest, FileUriEncodesReservedControlAndUtf8Bytes) {
  const auto patch = format::makeUnifiedDiff("old\n", "new\n", "input.sv");
  const auto path =
      std::filesystem::temp_directory_path() / "space #%?\t\né.sv";
  const auto output = linked(patch, path);
  EXPECT_NE(output.find("/space%20%23%25%3F%09%0A%C3%A9.sv#1\033\\"),
            std::string::npos);
  EXPECT_EQ(withoutHyperlinks(output), patch);
}

TEST(DiffHyperlinkTest, RelativePathProducesAbsoluteFileUri) {
  const auto patch = format::makeUnifiedDiff("old\n", "new\n", "input.sv");
  const auto path = std::filesystem::current_path() / "missing-input.sv";
  EXPECT_EQ(linked(patch, "missing-input.sv"), linked(patch, path));
}

TEST(DiffHyperlinkTest, LinksAndColorsDoNotChangeEachOther) {
  const auto patch = format::makeUnifiedDiff("assign a=b;\r\n",
                                             "assign a = b;\r\n", "input.sv");
  std::ostringstream colored;
  format::printDiff(colored, patch, {.color = true});
  const auto output = linked(patch, "input.sv", true);
  EXPECT_NE(output.find("\033]8;;file:///"), std::string::npos);
  EXPECT_NE(output.find("\033[7m"), std::string::npos);
  EXPECT_EQ(withoutHyperlinks(output), colored.str());
}

TEST(DiffHyperlinkTest, EmptyPathLeavesStdinUnlinked) {
  const auto patch = format::makeUnifiedDiff("old\n", "new\n", "<stdin>");
  EXPECT_EQ(linked(patch, {}), patch);
  EXPECT_TRUE(linked("").empty());
}

TEST(DiffHyperlinkTest, MalformedOrOverflowingHunkRangesStayPlain) {
  constexpr std::array<std::string_view, 4> kHeaders = {
      "@@ -bad +1 @@", "@@ -1", "@@ -1,no +1 @@",
      "@@ -18446744073709551616 +1 @@"};
  for (const auto header : kHeaders) {
    const auto patch = std::string(header) + "\n-old\n+new\n";
    EXPECT_EQ(linked(patch), patch);
  }
}

}  // namespace

#include "pipeline/unified_diff.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

// Ignore Git metadata (including content-derived object IDs) in body tests.
auto unifiedBody(std::string_view before, std::string_view after,
                 const std::filesystem::path& path, size_t context = 3)
    -> std::string {
  const auto patch = format::makeUnifiedDiff(before, after, path, context);
  const auto header = patch.find("\n--- ");
  return header == std::string::npos ? patch : patch.substr(header + 1);
}

TEST(UnifiedDiffTest, IdenticalBytesHaveNoDiff) {
  EXPECT_TRUE(unifiedBody("", "", "input.sv").empty());
  EXPECT_TRUE(unifiedBody("same\r\nlast", "same\r\nlast", "input.sv").empty());
}

TEST(UnifiedDiffTest, ReplacementIncludesContext) {
  EXPECT_EQ(unifiedBody("first\nold\nlast\n", "first\nnew\nlast\n", "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -1,3 +1,3 @@\n"
            " first\n-old\n+new\n last\n");
}

TEST(UnifiedDiffTest, EmptyFileInsertionAndDeletion) {
  EXPECT_EQ(unifiedBody("", "new\n", "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -0,0 +1 @@\n+new\n");
  EXPECT_EQ(unifiedBody("old\n", "", "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -1 +0,0 @@\n-old\n");
}

TEST(UnifiedDiffTest, InsertionsAndDeletionsKeepUnchangedLines) {
  EXPECT_EQ(unifiedBody("a\nb\nc\n", "a\nx\nb\n", "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -1,3 +1,3 @@\n"
            " a\n+x\n b\n-c\n");
}

TEST(UnifiedDiffTest, ZeroContextSeparatesChanges) {
  EXPECT_EQ(unifiedBody("a\nb\nc\n", "x\nb\ny\n", "input.sv", 0),
            "--- a/input.sv\n+++ b/input.sv\n"
            "@@ -1 +1 @@\n-a\n+x\n@@ -3 +3 @@ b\n-c\n+y\n");
}

TEST(UnifiedDiffTest, DistantChangesHaveSeparateHunks) {
  EXPECT_EQ(
      unifiedBody("a\n1\n2\n3\n4\nb\n", "x\n1\n2\n3\n4\ny\n", "input.sv", 1),
      "--- a/input.sv\n+++ b/input.sv\n"
      "@@ -1,2 +1,2 @@\n-a\n+x\n 1\n"
      "@@ -5,2 +5,2 @@ a\n 4\n-b\n+y\n");
}

TEST(UnifiedDiffTest, TouchingContextMergesHunks) {
  EXPECT_EQ(unifiedBody("a\n1\n2\nb\n", "x\n1\n2\ny\n", "input.sv", 1),
            "--- a/input.sv\n+++ b/input.sv\n"
            "@@ -1,4 +1,4 @@\n-a\n+x\n 1\n 2\n-b\n+y\n");
}

TEST(UnifiedDiffTest, MissingFinalNewlineIsExplicit) {
  EXPECT_EQ(unifiedBody("old", "new", "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -1 +1 @@\n"
            "-old\n\\ No newline at end of file\n"
            "+new\n\\ No newline at end of file\n");
  EXPECT_EQ(unifiedBody("same", "same\n", "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -1 +1 @@\n"
            "-same\n\\ No newline at end of file\n+same\n");
}

TEST(UnifiedDiffTest, CrLfIsPreservedAsBytes) {
  EXPECT_EQ(unifiedBody("old\r\n", "new\r\n", "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -1 +1 @@\n-old\r\n+new\r\n");
  EXPECT_EQ(unifiedBody("same\r\n", "same\n", "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -1 +1 @@\n-same\r\n+same\n");
}

TEST(UnifiedDiffTest, FileLabelsEscapeControlCharacters) {
  const auto diff = unifiedBody("a\n", "b\n", "a b\t\n\"\\.sv");
  EXPECT_TRUE(
      diff.starts_with("--- \"a/a b\\t\\n\\\"\\\\.sv\"\n"
                       "+++ \"b/a b\\t\\n\\\"\\\\.sv\"\n"));
}

TEST(UnifiedDiffTest, VeryLargeContextDoesNotOverflow) {
  EXPECT_EQ(unifiedBody("a\nb\n", "x\nb\n", "input.sv",
                        std::numeric_limits<size_t>::max()),
            "--- a/input.sv\n+++ b/input.sv\n@@ -1,2 +1,2 @@\n-a\n+x\n b\n");
}

TEST(UnifiedDiffTest, LargeReplacementProducesCompletePatch) {
  constexpr size_t kLines = 1000;
  std::string before;
  std::string after;
  for (size_t i = 0; i < kLines; ++i) {
    before += "old\n";
    after += "new\n";
  }
  const auto diff = unifiedBody(before, after, "input.sv");
  EXPECT_TRUE(
      diff.starts_with("--- a/input.sv\n+++ b/input.sv\n"
                       "@@ -1,1000 +1,1000 @@\n-old\n"));
  EXPECT_TRUE(diff.ends_with("+new\n"));
}

TEST(UnifiedDiffTest, IncludesGitPatchMetadata) {
  const auto patch = format::makeUnifiedDiff("old\n", "new\n", "input.sv");
  EXPECT_TRUE(patch.starts_with("diff --git a/input.sv b/input.sv\nindex "));
  EXPECT_NE(patch.find("\n--- a/input.sv\n+++ b/input.sv\n"),
            std::string::npos);
}

TEST(UnifiedDiffTest, InvalidLabelsAreRejected) {
  EXPECT_THROW((void)format::makeUnifiedDiff("a", "b", ""),
               std::invalid_argument);
  EXPECT_THROW((void)format::makeUnifiedDiff(
                   "a", "b", std::filesystem::current_path() / "input.sv"),
               std::invalid_argument);
  EXPECT_THROW((void)format::makeUnifiedDiff(
                   "a", "b", std::filesystem::path{std::string("a\0b", 3)}),
               std::invalid_argument);
}

TEST(UnifiedDiffTest, NullEmptyViewsRepresentExistingEmptyFiles) {
  EXPECT_EQ(unifiedBody({}, "new\n", "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -0,0 +1 @@\n+new\n");
  EXPECT_EQ(unifiedBody("old\n", {}, "input.sv"),
            "--- a/input.sv\n+++ b/input.sv\n@@ -1 +0,0 @@\n-old\n");
}

TEST(UnifiedDiffTest, ConcurrentCallsProduceIdenticalPatches) {
  constexpr size_t kWorkers = 8;
  std::vector<std::string> patches(kWorkers);
  std::atomic<bool> success{true};
  {
    std::vector<std::jthread> workers;
    workers.reserve(kWorkers);
    for (size_t i = 0; i < kWorkers; ++i) {
      workers.emplace_back([&, i] {
        try {
          patches.at(i) = format::makeUnifiedDiff("old\n", "new\n", "input.sv");
        } catch (...) {
          success.store(false);
        }
      });
    }
  }
  ASSERT_TRUE(success.load());
  ASSERT_FALSE(patches.front().empty());
  for (const auto& patch : patches) {
    EXPECT_EQ(patch, patches.front());
  }
}

}  // namespace

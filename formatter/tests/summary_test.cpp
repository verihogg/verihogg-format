#include "pipeline/summary.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

auto makeReport(std::string path, std::string construct, std::size_t line)
    -> format::summary::FileReport {
  return format::summary::makeFileReport(std::move(path),
                                         {{std::move(construct), line}});
}

}  // namespace

TEST(SummaryTest, DisambiguatesSharedBasenamesWithFullPaths) {
  std::ostringstream out;
  format::summary::printSummary(
      out,
      {makeReport("src/alu.sv", "unsupported construct", 2),
       makeReport("tests/alu.sv", "unsupported construct", 2)},
      {.links = false});

  EXPECT_NE(out.str().find("first at src/alu.sv:2"), std::string::npos);
  EXPECT_NE(out.str().find("first at tests/alu.sv:2"), std::string::npos);
}

TEST(SummaryTest, KeepsBaseNameWhenItIsUnique) {
  std::ostringstream out;
  format::summary::printSummary(
      out,
      {makeReport("src/a.sv", "unsupported construct", 2),
       makeReport("tests/b.sv", "other construct", 3)},
      {.links = false});

  EXPECT_NE(out.str().find("first at a.sv:2"), std::string::npos);
  EXPECT_NE(out.str().find("first at b.sv:3"), std::string::npos);
}

TEST(SummaryTest, MarksTruncatedConstructListInIssueUrl) {
  std::vector<std::pair<std::string, std::size_t>> items;
  constexpr std::size_t kConstructCount = 300;
  items.reserve(kConstructCount);
  for (std::size_t i = 0; i < kConstructCount; ++i) {
    items.emplace_back("unsupported construct number " + std::to_string(i), 1);
  }
  const auto report =
      format::summary::makeFileReport("big.sv", std::move(items));
  const std::string url = format::summary::buildUnsupportedUrl({report});

  EXPECT_LE(url.size(), 1500);
  EXPECT_NE(url.find("...%0A"), std::string::npos);
  EXPECT_EQ(url.find(format::summary::percentEncode(
                "unsupported construct number 299")),
            std::string::npos);
}

TEST(SummaryTest, KeepsCompleteConstructListInIssueUrl) {
  const auto report = format::summary::makeFileReport(
      "small.sv", {{"missing label", 1}, {"unsupported token", 2}});
  const std::string url = format::summary::buildUnsupportedUrl({report});

  EXPECT_LE(url.size(), 1500);
  EXPECT_EQ(url.find("...%0A"), std::string::npos);
  EXPECT_NE(url.find("missing+label"), std::string::npos);
  EXPECT_NE(url.find("unsupported+token"), std::string::npos);
}

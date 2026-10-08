#include "config/config_loader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "data/format_style.h"

namespace {

namespace fs = std::filesystem;

using format::AlignmentGroupBoundary;
using format::AlignmentPolicy;
using format::FormatStyle;
using format::IndentationPolicy;
using format::LineTerminator;
using format::config::ConfigResolver;
using format::config::loadConfigFile;

constexpr std::string_view kAllOptionsConfig = R"(
version: "1.0"
$schema: "https://example.com/schema.json"
style: default
column_limit: 120
indentation_spaces: 4
wrap_spaces: 8
line_terminator: crlf
line_break_penalty: 3
over_column_limit_penalty: 50
port_declarations_alignment: flush-left
module_net_variable_alignment: align
assignment_statement_alignment: preserve
formal_parameters_alignment: infer
named_parameter_alignment: align
named_port_alignment: flush-left
parameter_declaration_alignment: preserve
case_items_alignment: infer
enum_assignment_statement_alignment: align
struct_union_members_alignment: flush-left
class_member_variable_alignment: preserve
distribution_items_alignment: infer
port_declarations_indentation: indent
formal_parameters_indentation: wrap
named_parameter_indentation: indent
named_port_indentation: wrap
alignment_group_boundary: blank-lines-and-separator-comments
port_declarations_right_align_packed_dimensions: true
port_declarations_right_align_unpacked_dimensions: true
compact_indexing_and_selections: false
class_parameter_space: true
expand_coverpoints: true
try_wrap_long_lines: true
wrap_end_else_clauses: true
)";

class ConfigLoaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static int counter = 0;
    dir_ = fs::temp_directory_path() /
           ("vhg-config-test-" + std::to_string(counter++));
    fs::create_directories(dir_);
  }

  void TearDown() override {
    std::error_code code;
    fs::remove_all(dir_, code);
  }

  auto writeFile(const fs::path& relative, std::string_view content)
      -> fs::path {
    const fs::path path = dir_ / relative;
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    return path;
  }

  [[nodiscard]] auto dir() const -> const fs::path& { return dir_; }

 private:
  fs::path dir_;
};

TEST_F(ConfigLoaderTest, MinimalConfigKeepsDefaults) {
  const fs::path path = writeFile("cfg.yaml", "version: \"1.0\"\n");

  const FormatStyle style = loadConfigFile(path);
  const FormatStyle defaults = FormatStyle::defaults();

  EXPECT_EQ(style.column_limit, defaults.column_limit);
  EXPECT_EQ(style.indentation_spaces, defaults.indentation_spaces);
  EXPECT_EQ(style.wrap_spaces, defaults.wrap_spaces);
  EXPECT_EQ(style.line_break_penalty, defaults.line_break_penalty);
  EXPECT_EQ(style.over_column_limit_penalty,
            defaults.over_column_limit_penalty);
  EXPECT_EQ(style.line_terminator, LineTerminator::kAuto);
  EXPECT_EQ(style.port_declarations_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.alignment_group_boundary, AlignmentGroupBoundary::kNone);
  EXPECT_TRUE(style.compact_indexing_and_selections);
}

TEST_F(ConfigLoaderTest, AllSchemaOptionsAreApplied) {
  const fs::path path = writeFile("cfg.yaml", kAllOptionsConfig);

  const FormatStyle style = loadConfigFile(path);

  EXPECT_EQ(style.column_limit, 120U);
  EXPECT_EQ(style.indentation_spaces, 4U);
  EXPECT_EQ(style.wrap_spaces, 8U);
  EXPECT_EQ(style.line_terminator, LineTerminator::kCrLf);
  EXPECT_EQ(style.line_break_penalty, 3U);
  EXPECT_EQ(style.over_column_limit_penalty, 50U);

  EXPECT_EQ(style.port_declarations_alignment, AlignmentPolicy::kFlushLeft);
  EXPECT_EQ(style.module_net_variable_alignment, AlignmentPolicy::kAlign);
  EXPECT_EQ(style.assignment_statement_alignment, AlignmentPolicy::kPreserve);
  EXPECT_EQ(style.formal_parameters_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.named_parameter_alignment, AlignmentPolicy::kAlign);
  EXPECT_EQ(style.named_port_alignment, AlignmentPolicy::kFlushLeft);
  EXPECT_EQ(style.parameter_declaration_alignment, AlignmentPolicy::kPreserve);
  EXPECT_EQ(style.case_items_alignment, AlignmentPolicy::kInfer);
  EXPECT_EQ(style.enum_assignment_statement_alignment, AlignmentPolicy::kAlign);
  EXPECT_EQ(style.struct_union_members_alignment, AlignmentPolicy::kFlushLeft);
  EXPECT_EQ(style.class_member_variable_alignment, AlignmentPolicy::kPreserve);
  EXPECT_EQ(style.distribution_items_alignment, AlignmentPolicy::kInfer);

  EXPECT_EQ(style.port_declarations_indentation, IndentationPolicy::kIndent);
  EXPECT_EQ(style.formal_parameters_indentation, IndentationPolicy::kWrap);
  EXPECT_EQ(style.named_parameter_indentation, IndentationPolicy::kIndent);
  EXPECT_EQ(style.named_port_indentation, IndentationPolicy::kWrap);

  EXPECT_EQ(style.alignment_group_boundary,
            AlignmentGroupBoundary::kBlankLinesAndSeparatorComments);

  EXPECT_TRUE(style.port_declarations_right_align_packed_dimensions);
  EXPECT_TRUE(style.port_declarations_right_align_unpacked_dimensions);
  EXPECT_FALSE(style.compact_indexing_and_selections);
  EXPECT_TRUE(style.class_parameter_space);
  EXPECT_TRUE(style.expand_coverpoints);
  EXPECT_TRUE(style.try_wrap_long_lines);
  EXPECT_TRUE(style.wrap_end_else_clauses);
}

TEST_F(ConfigLoaderTest, UnknownOptionReportsKeyAndPosition) {
  const fs::path path =
      writeFile("cfg.yaml", "version: \"1.0\"\ncolumn_limitt: 100\n");

  try {
    (void)loadConfigFile(path);
    FAIL() << "expected loadConfigFile to throw";
  } catch (const std::runtime_error& e) {
    const std::string message = e.what();
    EXPECT_NE(message.find("unknown option 'column_limitt'"), std::string::npos)
        << message;
    EXPECT_NE(message.find(":2:"), std::string::npos) << message;
  }
}

TEST_F(ConfigLoaderTest, WrongOptionTypeFails) {
  const fs::path path =
      writeFile("cfg.yaml", "version: \"1.0\"\ncolumn_limit: abc\n");

  try {
    (void)loadConfigFile(path);
    FAIL() << "expected loadConfigFile to throw";
  } catch (const std::runtime_error& e) {
    EXPECT_NE(std::string(e.what()).find("column_limit"), std::string::npos);
    EXPECT_NE(std::string(e.what()).find(":2:"), std::string::npos);
  }
}

TEST_F(ConfigLoaderTest, UnknownEnumValueFails) {
  const fs::path path =
      writeFile("cfg.yaml", "version: \"1.0\"\nline_terminator: bogus\n");

  try {
    (void)loadConfigFile(path);
    FAIL() << "expected loadConfigFile to throw";
  } catch (const std::runtime_error& e) {
    EXPECT_NE(std::string(e.what()).find("line_terminator"), std::string::npos);
  }
}

TEST_F(ConfigLoaderTest, MissingVersionFails) {
  const fs::path path = writeFile("cfg.yaml", "column_limit: 100\n");

  try {
    (void)loadConfigFile(path);
    FAIL() << "expected loadConfigFile to throw";
  } catch (const std::runtime_error& e) {
    EXPECT_NE(std::string(e.what()).find("version"), std::string::npos);
  }
}

TEST_F(ConfigLoaderTest, UnsupportedVersionFails) {
  const fs::path path = writeFile("cfg.yaml", "version: \"2.0\"\n");

  EXPECT_THROW((void)loadConfigFile(path), std::runtime_error);
}

TEST_F(ConfigLoaderTest, SyntaxErrorReportsPosition) {
  const fs::path path = writeFile("cfg.yaml", "version: [1.0\n");

  try {
    (void)loadConfigFile(path);
    FAIL() << "expected loadConfigFile to throw";
  } catch (const std::runtime_error& e) {
    const std::string message = e.what();
    EXPECT_NE(message.find(path.string()), std::string::npos) << message;
    EXPECT_NE(message.find(":1:"), std::string::npos) << message;
  }
}

TEST_F(ConfigLoaderTest, SchemaKeyIsAccepted) {
  const fs::path path = writeFile(
      "cfg.yaml",
      "version: \"1.0\"\n$schema: \"https://example.com/schema.json\"\n");

  const FormatStyle style = loadConfigFile(path);
  EXPECT_EQ(style.column_limit, 100U);
}

TEST_F(ConfigLoaderTest, ProfilePlaceholderAndOverrides) {
  const fs::path path = writeFile(
      "cfg.yaml", "version: \"1.0\"\nstyle: scr1\ncolumn_limit: 120\n");

  const FormatStyle style = loadConfigFile(path);
  EXPECT_EQ(style.column_limit, 120U);
  EXPECT_EQ(style.wrap_spaces, 4U);
}

TEST_F(ConfigLoaderTest, UnknownProfileFails) {
  const fs::path path =
      writeFile("cfg.yaml", "version: \"1.0\"\nstyle: nope\n");

  EXPECT_THROW((void)loadConfigFile(path), std::runtime_error);
}

TEST_F(ConfigLoaderTest, QuotedNumbersAreAccepted) {
  const fs::path path =
      writeFile("cfg.yaml", "version: \"1.0\"\ncolumn_limit: \"120\"\n");

  const FormatStyle style = loadConfigFile(path);
  EXPECT_EQ(style.column_limit, 120U);
}

TEST_F(ConfigLoaderTest, NearestConfigWins) {
  writeFile("root/.verihogg-format.yaml",
            "version: \"1.0\"\ncolumn_limit: 120\n");
  writeFile("root/sub/.verihogg-format.yaml",
            "version: \"1.0\"\ncolumn_limit: 80\n");

  ConfigResolver resolver(std::nullopt);

  EXPECT_EQ(resolver.resolve(dir() / "root/sub/deep/file.sv").column_limit,
            80U);
  EXPECT_EQ(resolver.resolve(dir() / "root/file.sv").column_limit, 120U);
}

TEST_F(ConfigLoaderTest, MissingConfigKeepsDefaults) {
  ConfigResolver resolver(std::nullopt);

  EXPECT_EQ(resolver.resolve(dir() / "nowhere/file.sv").column_limit, 100U);
}

TEST_F(ConfigLoaderTest, ExplicitConfigBeatsSearch) {
  const fs::path explicit_config =
      writeFile("other/custom.yaml", "version: \"1.0\"\ncolumn_limit: 90\n");
  writeFile("root/.verihogg-format.yaml",
            "version: \"1.0\"\ncolumn_limit: 120\n");

  ConfigResolver resolver(explicit_config);

  EXPECT_EQ(resolver.resolve(dir() / "root/file.sv").column_limit, 90U);
}

TEST_F(ConfigLoaderTest, MissingExplicitConfigFails) {
  EXPECT_THROW(ConfigResolver(dir() / "missing.yaml"), std::runtime_error);
}

TEST_F(ConfigLoaderTest, YmlExtensionIsSupported) {
  writeFile("root/.verihogg-format.yml",
            "version: \"1.0\"\ncolumn_limit: 110\n");

  ConfigResolver resolver(std::nullopt);

  EXPECT_EQ(resolver.resolve(dir() / "root/file.sv").column_limit, 110U);
}

TEST_F(ConfigLoaderTest, ResolveForStdinUsesWorkingDirectory) {
  writeFile(".verihogg-format.yaml", "version: \"1.0\"\ncolumn_limit: 111\n");

  const fs::path old_cwd = fs::current_path();
  fs::current_path(dir());
  FormatStyle style;
  try {
    ConfigResolver resolver(std::nullopt);
    style = resolver.resolveForStdin();
  } catch (...) {
    fs::current_path(old_cwd);
    throw;
  }
  fs::current_path(old_cwd);

  EXPECT_EQ(style.column_limit, 111U);
}

}  // namespace

#include "pipeline/runner.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gsl/span>
#include <string_view>

#include "data/format_style.h"
#include "data/format_warning.h"
#include "data/lex_context.h"
#include "formatter.h"

namespace format {

namespace {

auto writeFile(const std::filesystem::path& path, std::string_view content)
    -> void {
  std::ofstream f{path, std::ios::binary | std::ios::trunc};
  if (!f) {
    throw std::runtime_error("Cannot open: " + std::string{path});
  }
  f << content;
}

}  // namespace

auto printWarning(std::ostream& os, std::string_view path,
                  const FormatWarning& warning) -> void {
  os << "Warning";
  if (!path.empty()) {
    os << " in " << path;
  }
  os << ": " << warning.message << " [" << warning.code << "]\n";
}

auto runFormatter(gsl::span<const std::filesystem::path> files,
                  gsl::span<const FormatStyle> styles, const RunConfig& run,
                  Streams streams) -> int {
  int warnings = 0;
  for (size_t i = 0; i < files.size(); ++i) {
    const auto& path = files[i];
    LexContext ctx;
    auto tokens = ctx.lex_file(path);
    if (tokens.empty()) {
      *streams.err << "Warning: no tokens in " << path << "\n";
      ++warnings;
      continue;
    }

    auto result = format::format(tokens, styles[i]);
    for (const auto& warning : result.warnings) {
      printWarning(*streams.err, path.string(), warning);
      ++warnings;
    }

    if (run.inplace) {
      writeFile(path, result.formatted_text);
    } else {
      *streams.out << result.formatted_text;
    }
  }
  return warnings;
}
}  // namespace format

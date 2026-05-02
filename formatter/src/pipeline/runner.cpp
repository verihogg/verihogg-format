#include "pipeline/runner.h"

#include <filesystem>
#include <fstream>
#include <span>

#include "data/format_style.h"
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
auto runFormatter(std::span<const std::filesystem::path> files,
                  const format::FormatStyle& style,
                  const format::RunConfig& run, Streams streams) -> int {
  int warnings = 0;
  for (const auto& path : files) {
    LexContext ctx;
    auto tokens = ctx.lex_file(path.string());
    if (tokens.empty()) {
      *streams.err << "Warning: no tokens in " << path << "\n";
      ++warnings;
      continue;
    }

    auto result = format::format(tokens, style);

    if (run.inplace) {
      writeFile(path, result.formatted_text);
    } else {
      *streams.out << result.formatted_text;
    }
  }
  return warnings;
}
}  // namespace format

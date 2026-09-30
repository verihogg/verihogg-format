#include <slang/driver/Driver.h>

#include <cassert>
#include <exception>
#include <iostream>
#include <string_view>

#include "cli/format_args.h"
#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/runner.h"

namespace {

auto printWarning(std::ostream& os, std::string_view path,
                  const format::FormatWarning& warning) -> void {
  os << "Warning";
  if (!path.empty()) {
    os << " in " << path;
  }
  os << ": " << warning.message << " [" << warning.code << "]\n";
}

}  // namespace

auto main(int argc, char** argv) -> int {
  try {
    format::FormatArgsBinder binder;

    auto args = gsl::span(argv, argc);
    for (std::string_view arg : args.subspan(1)) {
      if (arg == "--help" || arg == "-h") {
        binder.printFormatterHelp();
        return 0;
      }
    }

    try {
      binder.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
      return binder.app().exit(e);
    }

    slang::driver::Driver driver;
    for (const auto& file : binder.files()) {
      driver.sourceLoader.addFiles(file);
    }

    auto [style, run] = binder.buildStyle();

    const auto& files = driver.sourceLoader.getFilePaths();

    if (run.inplace && files.empty()) {
      std::cerr << "Warning: --inplace has no effect when reading from stdin\n";
    }
    if (run.check && files.empty()) {
      std::cerr << "Error: --check requires input files\n";
      return 1;
    }

    if (files.empty()) {
      LexContext ctx;
      auto tokens = ctx.lex_file("<stdin>");
      auto result = format::format(tokens, style);
      for (const auto& warning : result.warnings) {
        printWarning(std::cerr, "<stdin>", warning);
      }
      std::cout << result.formatted_text;
      return 0;
    }
    const int result =
        runFormatter(files, style, run, {.out = &std::cout, .err = &std::cerr});
    return run.check && result > 0 ? 1 : 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

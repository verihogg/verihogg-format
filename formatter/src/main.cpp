#include <slang/driver/Driver.h>

#include <cassert>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cli/format_args.h"
#include "config/config_loader.h"
#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/runner.h"

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

    format::config::ConfigResolver resolver(
        binder.configPath()
            ? std::optional<std::filesystem::path>{*binder.configPath()}
            : std::nullopt);
    const format::RunConfig run = binder.buildRunConfig();

    auto applyOverrides = [&binder](format::FormatStyle style) {
      binder.applyStyleOverrides(style);
      return style;
    };
    auto styleFor = [&resolver,
                     &applyOverrides](const std::filesystem::path& file) {
      return applyOverrides(resolver.resolve(file));
    };

    const auto& files = driver.sourceLoader.getFilePaths();

    if (run.inplace && files.empty()) {
      std::cerr << "Warning: --inplace has no effect when reading from stdin\n";
    }

    if (files.empty()) {
      const auto style = applyOverrides(resolver.resolveForStdin());
      std::string source{std::istreambuf_iterator<char>(std::cin),
                         std::istreambuf_iterator<char>()};
      if (source.empty()) {
        return 0;
      }
      LexContext ctx;
      auto tokens = ctx.lex_string(source);
      auto result = format::format(tokens, style);
      for (const auto& warning : result.warnings) {
        format::printWarning(std::cerr, "<stdin>", warning);
      }
      std::cout << result.formatted_text;
      return 0;
    }

    // Resolve all styles before formatting anything so that a broken config
    // cannot leave a partially formatted run behind.
    std::vector<format::FormatStyle> styles;
    styles.reserve(files.size());
    for (const auto& file : files) {
      styles.push_back(styleFor(file));
    }
    runFormatter(files, styles, run, {.out = &std::cout, .err = &std::cerr});
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

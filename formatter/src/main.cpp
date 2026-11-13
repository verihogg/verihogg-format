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
#include "pipeline/summary.h"

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
    const bool input_errors =
        run.check && !driver.sourceLoader.getErrors().empty();
    if (input_errors) {
      for (const auto& error : driver.sourceLoader.getErrors()) {
        std::cerr << "Error: " << error << "\n";
      }
    }

    auto applyOverrides = [&binder](format::FormatStyle style) {
      binder.applyStyleOverrides(style);
      return style;
    };
    auto styleFor = [&resolver,
                     &applyOverrides](const std::filesystem::path& file) {
      return applyOverrides(resolver.resolve(file));
    };

    const auto& files = driver.sourceLoader.getFilePaths();

    if (run.check && files.empty()) {
      if (!input_errors) {
        std::cerr << "Error: --check requires input files\n";
      }
      return 1;
    }

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
      auto result = format::format(tokens, style, ctx.source_text());
      std::cout << result.formatted_text;

      std::vector<std::pair<std::string, std::size_t>> items;
      items.reserve(result.warnings.size());
      for (const auto& warning : result.warnings) {
        std::size_t line = warning.formatted_line;
        if (line == 0 && warning.location.valid()) {
          line = ctx.source_manager().getLineNumber(warning.location);
        }
        std::string construct =
            warning.construct.empty() ? warning.code : warning.construct;
        items.emplace_back(std::move(construct), line);
      }
      std::vector<format::summary::FileReport> reports;
      reports.push_back(
          format::summary::makeFileReport(run.stdin_name, std::move(items)));
      format::summary::printSummary(std::cerr, reports,
                                    {.inplace = false, .links = !run.no_links});
      return 0;
    }

    // Resolve all styles before formatting anything so that a broken config
    // cannot leave a partially formatted run behind.
    std::vector<format::FormatStyle> styles;
    styles.reserve(files.size());
    for (const auto& file : files) {
      styles.push_back(styleFor(file));
    }
    const int result = runFormatter(files, styles, run,
                                    {.out = &std::cout, .err = &std::cerr});
    return run.check && (input_errors || result > 0) ? 1 : 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

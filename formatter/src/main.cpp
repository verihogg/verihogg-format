#include <slang/driver/Driver.h>
#include <slang/util/OS.h>

#include <cassert>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "cli/format_args.h"
#include "config/config_loader.h"
#include "data/lex_context.h"
#include "formatter.h"
#include "pipeline/diff_printer.h"
#include "pipeline/runner.h"
#include "pipeline/summary.h"
#include "pipeline/unified_diff.h"

namespace {

auto diffColorsEnabled(format::ColorMode mode) -> bool {
  if (mode == format::ColorMode::kNever) {
    return false;
  }
  if (mode == format::ColorMode::kAlways) {
    slang::OS::tryEnableColors();
    return true;
  }
  if (!slang::OS::getEnv("NO_COLOR").empty() ||
      slang::OS::getEnv("TERM") == "dumb") {
    return false;
  }
  slang::OS::tryEnableColors();
  return slang::OS::fileSupportsColors(stdout);
}

auto diffHyperlinksEnabled(format::HyperlinkMode mode) -> bool {
  if (mode == format::HyperlinkMode::kNever) {
    return false;
  }
  if (mode == format::HyperlinkMode::kAlways) {
    slang::OS::tryEnableColors();
    return true;
  }
  if (slang::OS::getEnv("TERM") == "dumb") {
    return false;
  }
  slang::OS::tryEnableColors();
#ifdef _WIN32
  return _isatty(_fileno(stdout)) != 0;
#else
  return isatty(fileno(stdout)) != 0;
#endif
}

}  // namespace

auto main(int argc, char** argv) -> int {
  try {
    format::FormatArgsBinder binder;

    auto args = gsl::span(argv, argc);
    for (std::string_view arg : args.subspan(1)) {
      if (arg == "--") {
        break;
      }
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
    const bool diff_color = run.diff && diffColorsEnabled(run.color);
    const bool diff_hyperlinks = run.diff && !binder.files().empty() &&
                                 diffHyperlinksEnabled(run.hyperlinks);
    const bool input_errors =
        (run.check || run.diff) && !driver.sourceLoader.getErrors().empty();
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

    if (run.diff && files.empty() && !binder.files().empty()) {
      std::cerr << "Error: no input files matched\n";
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
      if (run.diff) {
        format::printDiff(
            std::cout,
            format::makeUnifiedDiff(source, result.formatted_text,
                                    run.stdin_name, run.diff_context),
            {.color = diff_color});
        std::cout.flush();
        if (!std::cout) {
          throw std::runtime_error("Cannot write diff output");
        }
      } else {
        std::cout << result.formatted_text;
      }

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
      if (!run.diff || !result.warnings.empty()) {
        format::summary::printSummary(std::cerr, reports,
                                      {.inplace = false,
                                       .links = !run.no_links,
                                       .file_status = !run.diff});
      }
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
                                    {.out = &std::cout,
                                     .err = &std::cerr,
                                     .diff_color = diff_color,
                                     .diff_hyperlinks = diff_hyperlinks});
    return (run.check || run.diff) && (input_errors || result > 0) ? 1 : 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

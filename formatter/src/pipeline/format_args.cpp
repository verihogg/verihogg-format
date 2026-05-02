#include "cli/format_args.h"

#include <slang/driver/Driver.h>

#include <utility>

#include "data/format_style.h"

namespace format {

FormatArgsBinder::FormatArgsBinder(slang::driver::Driver& driver) {
  auto& cl = driver.cmdLine;

  cl.add("--column_limit", column_limit_, "Maximum line length (default: 100)",
         "<N>");

  cl.add("--indentation_spaces", indentation_spaces_,
         "Spaces for one indentation level (default: 2)", "<N>");

  cl.add("--wrap_spaces", wrap_spaces_,
         "Additional indentation when hyphenating a line (default: 4)", "<N>");

  cl.add("--line_break_penalty", line_break_penalty_,
         "Penalty for each line break (default: 2)", "<N>");

  cl.add("--over_column_limit_penalty", over_column_limit_penalty_,
         "Penalty for each character beyond column_limit (default: 100)",
         "<N>");

  cl.add("--line_terminator", line_terminator_,
         "End of line character: auto | lf | crlf (default: auto)", "<mode>");
  cl.add("--inplace", inplace_,
         "Overwrite the source files instead of outputting to stdout");
}

auto FormatArgsBinder::buildStyle() -> std::pair<FormatStyle, RunConfig> {
  FormatStyle s = FormatStyle::defaults();

  if (column_limit_.has_value()) {
    s.column_limit = *column_limit_;
  }
  if (indentation_spaces_.has_value()) {
    s.indentation_spaces = *indentation_spaces_;
  }
  if (wrap_spaces_.has_value()) {
    s.wrap_spaces = *wrap_spaces_;
  }
  if (line_break_penalty_.has_value()) {
    s.line_break_penalty = *line_break_penalty_;
  }
  if (over_column_limit_penalty_.has_value()) {
    s.over_column_limit_penalty = *over_column_limit_penalty_;
  }
  if (line_terminator_.has_value()) {
    s.line_terminator = lineTerminatorFromString(*line_terminator_);
  }

  RunConfig run;
  if (inplace_.has_value()) {
    run.inplace = *inplace_;
  }

  return {s, run};
}

}  // namespace format

# verihogg-format

[![CMake Build](https://github.com/verihogg/verihogg-format/actions/workflows/build.yml/badge.svg)](https://github.com/verihogg/verihogg-format/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**verihogg-format** formats SystemVerilog source files. It adjusts indentation and spacing, aligns groups such as module ports, joins short statements where they fit, and searches for line breaks around a configurable column target. It can print a preview, rewrite files, or check formatting in CI.

The formatter is under active development. It recognizes common source patterns but does not cover the full SystemVerilog language. When it cannot safely format a region, it reports a warning and preserves the original text where possible.

## Quick start

Use the published Docker image from a SystemVerilog project directory:

```sh
# Preview a file on stdout; diagnostics go to stderr.
docker run --rm -v "$(pwd)":/data -w /data \
  ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format rtl/alu.sv

# Rewrite a file in place.
docker run --rm -v "$(pwd)":/data -w /data \
  ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format --inplace rtl/alu.sv
```

With a local build, use `build/bin/verihogg-format` in place of `verihogg-format` in the commands below.

## Formatting examples

### Ports and expressions

Input:

```systemverilog
module arithmetic(input logic [31:0] left,input logic[31:0] right,output logic[31:0] result);
assign result=left+right;
endmodule
```

Output with the default style:

```systemverilog
module arithmetic (
  input   logic [31:0]        left,
  input   logic [31:0]        right,
  output  logic [31:0]        result
);
  assign result = left + right;
endmodule
```

### A narrower column target

Input:

```systemverilog
module demo(input logic clk,input logic[7:0] a,output logic[7:0] y);
always_comb begin
if(a==8'h00) y=8'hff;
else y=a+8'h01;
end
endmodule
```

Output with `--column_limit 40`:

```systemverilog
module demo (
  input   logic              clk,
  input   logic [7:0]        a,
  output  logic [7:0]        y
);
  always_comb begin
    if (a == 8'h00) y = 8'hff;
    else y = a + 8'h01;
  end
endmodule
```

The column limit is a layout target, not a guarantee: an unbreakable token or a preserved region can still exceed it. At the default limit of 100, both branches in this example fit together on one line: `if (a == 8'h00) y = 8'hff; else y = a + 8'h01;`.

## Command-line use

| Command | Result |
| --- | --- |
| `verihogg-format rtl/alu.sv` | Print formatted source to stdout; leave the file untouched. |
| `verihogg-format --inplace rtl/alu.sv` | Replace the file with formatted source. |
| `verihogg-format --check rtl/alu.sv` | Compare the file with formatted output without writing it; useful in CI. |
| `verihogg-format < rtl/alu.sv` | Read from stdin and print to stdout. |
| `verihogg-format --no-links rtl/alu.sv` | Omit GitHub issue links from the diagnostic summary. |

Multiple file paths are accepted. In preview mode their formatted contents are written consecutively to stdout, without file separators; use `--inplace` or `--check` when processing many files independently. `--check` requires file paths and cannot be combined with `--inplace`.

`--check` writes nothing to stdout. It exits with **0** when all supplied files already match the formatter output, and **1** if a file needs formatting, cannot be read, or another error occurs. Normal preview and in-place runs return **0** even when they report unsupported constructs; an execution error returns **1**.

If an input path cannot be found, `--check` reports the error and continues checking the other matched files; the final exit code is still **1**.

### Diagnostics and issue links

Normal preview and in-place runs write a status line for each processed file to stderr. When there are formatter warnings, an end-of-run summary groups them by construct and file, shows occurrence counts, and points to the first relevant line **in the formatted output**. For example:

```text
verihogg-format: rtl/demo.sv processed (some regions left unchanged).

...
  - covergroup  1 occurrence, first at demo.sv:5
```

If multiple reported files with warnings share a basename, the summary keeps their full paths in locations such as `first at /project/rtl/demo.sv:5` to distinguish them.

The summary also offers pre-filled GitHub issue links for unsupported constructs and incorrect formatting. In normal preview and in-place runs, the incorrect-formatting link is shown even when there are no warnings, because a user may still notice a formatting mistake. `--no-links` removes the links and their explanatory text while retaining status and warning information. It works in `--check` mode too. Do not include private source code in an issue without reviewing it first.

`--check` prints `Needs formatting: <path>` for files that differ and errors for unreadable files. When there are formatter warnings, it also prints the grouped summary and issue links, without the per-file `processed` or `written` status lines. Without warnings, it omits the summary and links. Summary line numbers refer to the formatted output that would be produced, even though files are not rewritten. The return code depends on whether files match the formatter output and whether errors occurred, not on the number of warnings.

## Formatting options

These options affect the current formatter:

| Option | Default | Effect |
| --- | --- | --- |
| `-c, --column_limit N` | `100` | Target output width. |
| `-i, --indentation_spaces N` | `2` | Spaces per nesting level. |
| `-w, --wrap_spaces N` | `4` | Extra indentation for continuation lines. |
| `-b, --line_break_penalty N` | `2` | Cost of adding a line break in layout search. |
| `-p, --over_column_limit_penalty N` | `100` | Overflow penalty used by layout search. |
| `-t, --line_terminator MODE` | `auto` | `auto`, `lf`, or `crlf`. Currently `auto` emits LF. |
| `--config PATH` | automatic search | Load one specific YAML configuration file. |
| `-n, --inplace` | off | Rewrite input files. |
| `--check` | off | Check without rewriting input files. |
| `--no-links` | off | Hide GitHub issue links in the diagnostic summary, including in `--check`. |
| `--wrap_end_else_clauses` | off | Put a block-closing `end` and the following `else` on separate lines. |

The formatter currently runs its line-wrap search regardless of the reserved `try_wrap_long_lines` setting. Comments and unsupported regions are preserved rather than reflowed into a new comment style.

The CLI and schema also accept **reserved style options** that are validated and stored but do not yet change output. They include per-context alignment (`port_declarations_alignment`, `assignment_statement_alignment`, and the other `*_alignment` options), per-context indentation (`*_indentation`), `alignment_group_boundary`, and the boolean options `port_declarations_right_align_packed_dimensions`, `port_declarations_right_align_unpacked_dimensions`, `compact_indexing_and_selections`, `class_parameter_space`, `expand_coverpoints`, and `try_wrap_long_lines`. See `verihogg-format --help` and the [configuration schema](schemas/verihogg-format.schema.json) for the complete list and allowed values. Boolean style options, including `wrap_end_else_clauses`, have a `--no_<name>` form to override a true value from a config file.

## Configuration

Place `.verihogg-format.yaml` or `.verihogg-format.yml` in a project directory:

```yaml
$schema: "https://raw.githubusercontent.com/verihogg/verihogg-format/main/schemas/verihogg-format.schema.json"
version: "0.1.0"
column_limit: 80
indentation_spaces: 4
line_terminator: lf
```

`version: "0.1.0"` is required. All other fields are optional. The formatter searches upward from each input file's directory; for stdin it searches from the current working directory. `--config PATH` selects a file explicitly. Values are applied in this order, with later values taking precedence: built-in defaults, named style profile, YAML file, CLI overrides. Different input files can therefore use different nearby config files in the same command.

The config is checked against the [JSON Schema](schemas/verihogg-format.schema.json), which is embedded in the binary. Unknown keys and invalid values produce an error with file and source position. The `style` key accepts `default`, `lowrisc`, and `scr1`, but these profiles currently have the same defaults; the latter two are not tuned presets yet. Reserved style options are accepted in YAML as well as on the CLI but currently have no effect on formatting.

## How it works and current limits

The pipeline lexes source with [slang](https://github.com/MikePopoloski/slang), normalizes tokens, forms structural lines, annotates spacing, assigns layout policies, joins eligible short lines, aligns groups, searches for line breaks, and prints the result. The formatter works on source text; it does not elaborate or compile a design.

- The column limit is a soft target. Line wrapping and alignment may still leave long lines.
- Short control-flow bodies can join their headers when they fit. Blocks, directives, comments, and required breaks keep separate lines.
- Unsupported syntax can leave a region byte-for-byte as written while surrounding supported code is reformatted. Review warnings and output before applying large in-place edits.
- `line_terminator: auto` currently emits LF. Choose `crlf` explicitly when needed.
- The formatter does not reflow comment text or implement the reserved style controls listed above.

If formatting changes code unexpectedly, use the incorrect-formatting issue link printed by the CLI or open an [issue](https://github.com/verihogg/verihogg-format/issues/new/choose) with a small input, actual output, and expected output.

## Installation and development

### Docker

The image is published as `ghcr.io/verihogg/verihogg-format:latest` after successful builds on `main`. Mount the directory containing the files and set it as the container working directory:

```sh
docker run --rm -v "$(pwd)":/data -w /data \
  ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format --check rtl/alu.sv
```

### Nix

The flake pins dependencies, including the SCR1 source used by integration tests:

```sh
git clone https://github.com/verihogg/verihogg-format.git
cd verihogg-format
nix build
./result/bin/verihogg-format --help
```

Use `nix develop` for a development shell with build tools and clang tools. The repository also provides `default.nix` and `shell.nix` for `nix-build` and `nix-shell`.

### CMake

You need a C++20 compiler, CMake 3.20+, and the development packages for [slang](https://github.com/MikePopoloski/slang), [Microsoft GSL](https://github.com/microsoft/GSL), [CLI11](https://github.com/CLIUtils/CLI11), [yaml-cpp](https://github.com/jbeder/yaml-cpp), and [valijson](https://github.com/tristanpenman/valijson). CMake also finds the platform thread library. Depending on your package layout, set `CMAKE_PREFIX_PATH` or the corresponding `*_DIR` variables.

To build only the formatter executable, without discovering Google Test:

```sh
cmake -S . -B build -DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE
cmake --build build
./build/bin/verihogg-format --help
```

If Google Test is installed and enabled, CMake requires `SCR1_ROOT` to point to a local [SCR1](https://github.com/syntacore/scr1) checkout. Set it when configuring, then run the unit and integration tests:

```sh
cmake -S . -B build -DCMAKE_DISABLE_FIND_PACKAGE_GTest=FALSE -DSCR1_ROOT=/path/to/scr1
cmake --build build
ctest --test-dir build --output-on-failure
```

## Repository layout

```text
formatter/include/   Public formatter and pipeline interfaces
formatter/src/       CLI, configuration loader, formatter pipeline
formatter/tests/     Unit, CLI, and SCR1 integration tests
schemas/             JSON Schema for YAML configuration
scripts/             C++ formatting and lint helper scripts
.github/workflows/   Build, formatting, lint, and Docker workflows
.github/ISSUE_TEMPLATE/  GitHub issue forms used by CLI links
flake.nix            Nix package and development shell
default.nix          Compatibility entry point for nix-build
shell.nix            Compatibility entry point for nix-shell
Dockerfile           Docker image assembled from the Nix build
```

## Contributing

Bug reports should include a minimal SystemVerilog input, the formatter output, and the expected result. The CLI's issue links pre-fill the formatter version, OS, and (for unsupported constructs) as many aggregated construct names as fit in the URL. Please remove any confidential source before submitting.

For code changes, add or update tests in `formatter/tests/`, run `ctest --test-dir build --output-on-failure`, and open a pull request against `main`.

## License

[MIT](LICENSE).

# verihogg-format

[![CMake Build](https://github.com/verihogg/verihogg-format/actions/workflows/build.yml/badge.svg)](https://github.com/verihogg/verihogg-format/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/verihogg/verihogg-format?include_prereleases&sort=semver)](https://github.com/verihogg/verihogg-format/releases)

**verihogg-format** is a code formatter for SystemVerilog (IEEE 1800-2017) that automatically reformats source code: indentation, spacing, and tabular alignment of ports and declarations.

> **Note:** Automatic line wrapping (breaking long lines to fit within the column limit) is not yet implemented — it is the next major feature on the roadmap.

> **Status:** This tool is under active development. It handles common SystemVerilog patterns but may produce unexpected formatting on some constructs. If you encounter such a case, please [open an issue](https://github.com/verihogg/verihogg-format/issues) with a minimal example.

## Table of contents

- [Features](#features)
- [Example](#example)
- [Quick start](#quick-start)
- [Command-line options](#command-line-options)
- [Configuration file](#configuration-file)
- [Usage](#usage)
  - [Docker (recommended)](#docker-recommended)
  - [Build from source with Nix](#build-from-source-with-nix)
  - [Build from source with CMake](#build-from-source-with-cmake)
- [Project structure](#project-structure)
- [Testing](#testing)
- [Contributing](#contributing)
- [License](#license)

---

## Features

**Implemented**

- Basic indentation with a configurable number of spaces
- Tabular alignment of port lists and declarations
- YAML configuration file support (`.verihogg-format.yaml`, validated against a JSON Schema)
- Docker image published to GitHub Container Registry
- Nix build for reproducible environments

**Planned**

- Automatic line wrapping (breaking long lines to fit the column limit)
- Formatting of comments
- Predefined formatting styles (e.g. SCR1, UVM, custom profiles)

---

## Example

The fragment below is from the ALU module of the open-source [SCR1](https://github.com/syntacore/scr1) RISC-V core by Syntacore.

**Before**

```verilog
module scr1_pipe_ialu #(parameter SCR1_XLEN = 32)(
input logic clk,input logic rst_n,
input  logic [SCR1_XLEN-1:0]  main_op1,
input logic[SCR1_XLEN-1:0] main_op2,
input  type_scr1_ialu_cmd_e   cmd,
output logic[SCR1_XLEN-1:0] result,
output logic cmp_res
);

always_comb begin
  result='0;
  if(cmd==SCR1_IALU_CMD_AND)
    result=main_op1&main_op2;
  else if(cmd==SCR1_IALU_CMD_OR)
    result=main_op1|main_op2;
end
```

**After**

```verilog
module scr1_pipe_ialu #(parameter SCR1_XLEN = 32) (
input     logic                        clk,
input     logic                        rst_n,
input     logic [SCR1_XLEN - 1 : 0]    main_op1,
input     logic [SCR1_XLEN - 1 : 0]    main_op2,
input     type_scr1_ialu_cmd_e         cmd,
output    logic [SCR1_XLEN - 1 : 0]    result,
output    logic                        cmp_res
);
  always_comb
    begin
      result = '0;
      if (cmd == SCR1_IALU_CMD_AND)
        result = main_op1 & main_op2;
      else if (cmd == SCR1_IALU_CMD_OR)
        result = main_op1 | main_op2;
    end
```

---

## Quick start

```bash
# Format a single file and print to stdout
docker run --rm -v "$(pwd)":/data -w /data \
  ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format file.sv

# Format in place
docker run --rm -v "$(pwd)":/data -w /data \
  ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format --inplace file.sv
```

---

## Command-line options

| Option                              | Description                                                    |
| ----------------------------------- | -------------------------------------------------------------- |
| `-n, --inplace`                     | Overwrite source files instead of printing to stdout.          |
| `-c, --column_limit N`              | Maximum line length (default: `100`).                          |
| `-i, --indentation_spaces N`        | Spaces per indent level (default: `2`).                        |
| `-w, --wrap_spaces N`               | Extra indent for wrapped lines (default: `4`).                 |
| `-b, --line_break_penalty N`        | Penalty for breaking a line (default: `2`).                    |
| `-p, --over_column_limit_penalty N` | Penalty per character over the column limit (default: `100`).  |
| `-t, --line_terminator MODE`        | Line ending style: `auto` \| `lf` \| `crlf` (default: `auto`). |
| `--config PATH`                     | Use this configuration file instead of searching for one.      |
| `-h, --help`                        | Show help and exit.                                            |

### Style options

Every option below mirrors a key of the configuration file, so any style
setting can also be given on the command line. A value passed on the command
line overrides the value from the configuration file.

| Option                                       | Description                                                                                          |
| -------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| `--port_declarations_alignment MODE`         | Alignment of port direction, type, dimensions and name.                                              |
| `--module_net_variable_alignment MODE`       | Alignment of net and variable declarations inside blocks.                                            |
| `--assignment_statement_alignment MODE`      | Alignment of assignment statements.                                                                  |
| `--formal_parameters_alignment MODE`         | Alignment of formal parameters in module, interface and class headers.                               |
| `--named_parameter_alignment MODE`           | Alignment of named parameters in instantiations.                                                     |
| `--named_port_alignment MODE`                | Alignment of named port connections.                                                                 |
| `--parameter_declaration_alignment MODE`     | Alignment of parameter and localparam declarations in block bodies.                                  |
| `--case_items_alignment MODE`                | Alignment of case item labels.                                                                       |
| `--enum_assignment_statement_alignment MODE` | Alignment of enum elements with assignments.                                                         |
| `--struct_union_members_alignment MODE`      | Alignment of struct and union members.                                                               |
| `--class_member_variable_alignment MODE`     | Alignment of class member variables.                                                                 |
| `--distribution_items_alignment MODE`        | Alignment of distribution items.                                                                     |
| `--port_declarations_indentation MODE`       | Indentation of ports in a module or interface header.                                                |
| `--formal_parameters_indentation MODE`       | Indentation of formal parameters in a header.                                                        |
| `--named_parameter_indentation MODE`         | Indentation of named parameters in an instantiation.                                                 |
| `--named_port_indentation MODE`              | Indentation of named ports in an instantiation.                                                      |
| `--alignment_group_boundary RULE`            | Rule determining where an alignment group ends.                                                      |
| `--port_declarations_right_align_packed_dimensions`   | Right-align packed dimensions in port declarations (default: `false`).                      |
| `--port_declarations_right_align_unpacked_dimensions` | Right-align unpacked dimensions in port declarations (default: `false`).                    |
| `--compact_indexing_and_selections`          | Use compact expressions inside indexes and selections (default: `true`).                             |
| `--class_parameter_space`                    | Insert a space before `#` in parameterized class typedefs (default: `false`).                        |
| `--expand_coverpoints`                       | Always expand coverpoints (default: `false`).                                                        |
| `--try_wrap_long_lines`                      | Allow optimization-based wrapping of long lines (default: `false`).                                  |
| `--wrap_end_else_clauses`                    | Place `end` and `else` clauses on separate lines (default: `false`).                                 |

For an alignment option `MODE` is one of `align`, `flush-left`, `preserve` or
`infer` (default: `infer`).
For an indentation option it is `indent` or `wrap` (default: `wrap`).
`RULE` is one of `none`, `blank-lines`, `separator-comments`
or `blank-lines-and-separator-comments` (default: `none`).

Each boolean flag also has a `--no_<name>` counterpart that sets it to `false`,
so a value coming from the configuration file can be turned off again:

```sh
formatter --no_compact_indexing_and_selections src/*.sv
```

### Exit codes

| Code | Meaning                                       |
| ---- | --------------------------------------------- |
| `0`  | Formatting complete, no errors.               |
| `1`  | Invalid command-line argument, configuration file error or parse error. |

---

## Configuration file

Options can be stored in a YAML configuration file named `.verihogg-format.yaml`
(or `.verihogg-format.yml`). The file is searched upwards starting from the
directory of each input file; for stdin, the search starts in the current
working directory. `--config PATH` loads a specific file instead of searching.

```yaml
version: "1.0"
column_limit: 120
indentation_spaces: 4
line_terminator: lf
port_declarations_alignment: flush-left
```

`version` is required and must be `"1.0"`; all other keys are optional. Values
are resolved in this order (later wins):

1. built-in defaults,
2. the selected `style` profile (`default`, `lowrisc` or `scr1`; the latter two
   currently match `default`),
3. values from the configuration file,
4. command-line options.

The file is validated against
[`schemas/verihogg-format.schema.json`](schemas/verihogg-format.schema.json):
unknown keys and invalid values are rejected with a `path:line:column` message.
Editors can pick up the same schema when the file references it:

```yaml
$schema: "https://raw.githubusercontent.com/verihogg/verihogg-format/main/schemas/verihogg-format.schema.json"
version: "1.0"
column_limit: 120
```

---

## Usage

### Docker (recommended)

The prebuilt image is published to GitHub Container Registry at `ghcr.io/verihogg/verihogg-format` and is updated on every push to `main`.

Navigate to your SystemVerilog project directory, then run:

#### Single file

```bash
docker run --rm -v "$(pwd)":/data -w /data \
  ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format file.sv
```

#### In-place formatting

```bash
docker run --rm -v "$(pwd)":/data -w /data \
  ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format --inplace file.sv
```

#### Multiple files

```bash
docker run --rm -v "$(pwd)":/data -w /data \
  ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format file1.sv file2.sv file3.sv
```

#### All `.sv` files in the current directory

```bash
docker run --rm -v "$(pwd)":/data -w /data \
  ghcr.io/verihogg/verihogg-format:latest \
  sh -c 'verihogg-format --inplace *.sv'
```

---

### Build from source with Nix

Nix pins all dependencies and produces a reproducible binary.

```bash
git clone https://github.com/verihogg/verihogg-format.git
cd verihogg-format
nix-build
```

This creates a `result` symlink. The binary is at `./result/bin/verihogg-format`.

#### Development shell

```bash
nix-shell
```

The development shell includes clang-tools (`clang-format`, `clang-tidy`) in addition to all build dependencies.

#### Running the local build

```bash
# Print formatted output to stdout
./result/bin/verihogg-format mymodule.sv

# In-place formatting
./result/bin/verihogg-format --inplace mymodule.sv
```

---

### Build from source with CMake

If you are not using Nix, install the following dependencies manually:

- C++20-compatible compiler
- CMake 3.20+
- [slang](https://github.com/MikePopoloski/slang) — SystemVerilog parser
- [CLI11](https://github.com/CLIUtils/CLI11) — command-line parser
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) — YAML parser (configuration files)
- [valijson](https://github.com/tristanpenman/valijson) — JSON Schema validation (configuration files)
- [Microsoft GSL](https://github.com/microsoft/GSL) — Guidelines Support Library
- Google Test (optional, required for tests)

Build:

```bash
cmake -B build
cmake --build build
```

The binary is placed at `build/bin/verihogg-format`.

---

## Project structure

```
.
├── formatter/
│   ├── include/            # Public headers
│   │   ├── cli/            # CLI argument binding (format_args.h)
│   │   ├── config/         # Configuration file loading (config_loader.h)
│   │   ├── data/           # Core data structures (format_style.h, unwrapped_line.h, format_token.h, lex_context.h)
│   │   ├── pipeline/       # Pipeline stage interfaces (tree_unwrapper.h, token_annotator.h,
│   │   │                   #   tabular_aligner.h, line_wrap_searcher.h, printer.h, runner.h)
│   │   └── formatter.h     # Top-level formatter interface
│   ├── src/
│   │   ├── config/         # Configuration file loading and validation
│   │   ├── data/           # Data structure implementations
│   │   ├── pipeline/       # Pipeline stage implementations
│   │   ├── formatter.cpp
│   │   └── main.cpp
│   └── tests/              # GTest-based unit and integration tests
│       ├── config_loader_test.cpp
│       ├── format_args_test.cpp
│       ├── printer_test.cpp
│       ├── scr1_tests.cpp
│       ├── tabular_align_test.cpp
│       └── test_tree_unwrapper.cpp
├── schemas/                # JSON Schema for the configuration file
├── scripts/
│   ├── check-format.sh
│   ├── format.sh
│   └── lint.sh
├── nix/
│   └── shared.nix          # Shared dependencies for Nix expressions
├── build.nix               # Nix derivation
├── default.nix             # Entry point — imports build.nix
├── shell.nix               # Nix development shell
├── Dockerfile              # Multi-stage Docker build via nix-build
└── CMakeLists.txt
```

---

## Testing

Tests use source files from the [SCR1](https://github.com/syntacore/scr1) RISC-V core project by Syntacore. The test suite includes:

- **Unit tests** — `format_args_test.cpp` (CLI argument parsing), `printer_test.cpp` (formatting output), `tabular_align_test.cpp` (tabular alignment), `test_tree_unwrapper.cpp` (tree unwrapper)
- **Integration tests** — `scr1_tests.cpp` (formats all SCR1 `.sv`/`.svh` files and verifies the syntax tree is preserved)

The `SCR1_ROOT` environment variable (or CMake variable) must point to a local SCR1 checkout for the integration tests to run; they are skipped automatically if the path is absent.

```bash
cmake -B build -DSCR1_ROOT=/path/to/scr1
cmake --build build
ctest --output-on-failure --test-dir build
```

CI runs several checks on each push and pull request, including build, Docker image build, static analysis, and formatting.

---

## Contributing

Contributions are welcome — bug reports, feature requests, and pull requests alike.

1. Open an [issue](https://github.com/verihogg/verihogg-format/issues) to discuss bugs or ideas.
2. Fork the repository and create a branch for your change.
3. Add or update tests in `formatter/tests/`.
4. Run `ctest --test-dir build` to verify everything passes.
5. Open a pull request against `main`.

---

## License

verihogg-format is licensed under the **MIT License**.
See the [LICENSE](LICENSE) file for the full text.

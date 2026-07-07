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

- Basic indentation with a fixed number of spaces (configurability is planned)
- Tabular alignment of port lists and declarations
- Docker image published to GitHub Container Registry
- Nix build for reproducible environments

**Planned**

- Automatic line wrapping (breaking long lines to fit the column limit)
- Formatting of comments
- Configuration file support
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

### Currently implemented

| Option          | Description                                           |
| --------------- | ----------------------------------------------------- |
| `-n, --inplace` | Overwrite source files instead of printing to stdout. |
| `-h, --help`    | Show help and exit.                                   |

### Planned options (not yet implemented)

The following options are part of the roadmap and will be added in future releases.

| Option                              | Description                                                    |
| ----------------------------------- | -------------------------------------------------------------- |
| `-c, --column_limit N`              | Maximum line length (default: `100`).                          |
| `-i, --indentation_spaces N`        | Spaces per indent level (default: `2`).                        |
| `-w, --wrap_spaces N`               | Extra indent for wrapped lines (default: `4`).                 |
| `-b, --line_break_penalty N`        | Penalty for breaking a line (default: `2`).                    |
| `-p, --over_column_limit_penalty N` | Penalty per character over the column limit (default: `100`).  |
| `-t, --line_terminator MODE`        | Line ending style: `auto` \| `lf` \| `crlf` (default: `auto`). |

### Exit codes

| Code | Meaning                                       |
| ---- | --------------------------------------------- |
| `0`  | Formatting complete, no errors.               |
| `1`  | Invalid command-line argument or parse error. |

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
│   │   ├── data/           # Core data structures (format_style.h, unwrapped_line.h, format_token.h, lex_context.h)
│   │   ├── pipeline/       # Pipeline stage interfaces (tree_unwrapper.h, token_annotator.h,
│   │   │                   #   tabular_aligner.h, line_wrap_searcher.h, printer.h, runner.h)
│   │   └── formatter.h     # Top-level formatter interface
│   ├── src/
│   │   ├── data/           # Data structure implementations
│   │   ├── pipeline/       # Pipeline stage implementations
│   │   ├── formatter.cpp
│   │   └── main.cpp
│   └── tests/              # GTest-based unit and integration tests
│       ├── format_args_test.cpp
│       ├── printer_test.cpp
│       ├── scr1_tests.cpp
│       ├── tabular_align_test.cpp
│       └── test_tree_unwrapper.cpp
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

# verihogg-format

**verihogg-format** is a code formatter for SystemVerilog (IEEE 1800-2017).  
The project automatically reformats source code: indentation, spacing, tabular alignment of ports and declarations, and line wrapping.

```
verihogg-format --inplace src/**/*.sv
```

---

## Command-line options

| Option                                  | Description                                                         |
| --------------------------------------- | ------------------------------------------------------------------- |
| `-n, --inplace`                         | Overwrite source files instead of printing to stdout.               |
| `-c, --column_limit N`                  | Maximum line length (default: `100`).                               |
| `-i, --indentation_spaces N`            | Spaces per indent level (default: `2`).                             |
| `-w, --wrap_spaces N`                   | Extra indent for wrapped lines (default: `4`).                      |
| `-b, --line_break_penalty N`            | Penalty for breaking a line (default: `2`).                         |
| `-p, --over_column_limit_penalty N`     | Penalty per character over the column limit (default: `100`).       |
| `-t, --line_terminator MODE`            | Line ending style: `auto` \| `lf` \| `crlf` (default: `auto`).     |
| `-h, --help`                            | Show help and exit.                                                 |

### Exit codes

| Code | Meaning                              |
| ---- | ------------------------------------ |
| `0`  | Formatting complete.                 |
| `1`  | Invalid command-line argument.       |

---

## Example

The open-source processor [SCR1](https://github.com/syntacore/scr1) — a RISC-V core by Syntacore — formats with verihogg-format without issues. Below is a fragment from the ALU module before and after running with default settings:

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

## Project structure

- `formatter/include/` – header files (data structures, pipeline stages, CLI).
- `formatter/src/` – implementation: `main.cpp`, `formatter.cpp`, pipeline stages.
- `formatter/tests/` – GTest-based tests.
- `default.nix` / `build.nix` / `shell.nix` – Nix definitions for build and development shell.

---

## Usage

### Recommended: via prebuilt Docker image

You can use the prebuilt image published to GitHub Container Registry:

#### Single file

```bash
cd /path/to/sv
docker run --rm -v "$(pwd)":/data -w /data ghcr.io/verihogg/verihogg-format:latest verihogg-format file.sv
```

#### Using filelist (.f)

The recommended way to format entire projects is using a filelist.

Example `files.f`:

```
rtl/pkg/common_pkg.sv
rtl/interfaces/bus_if.sv
rtl/core/top.sv
rtl/core/alu.sv
rtl/core/regfile.sv
```

Run with Docker:

```bash
cd /path/to/project
docker run --rm -v "$(pwd)":/data -w /data ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format --inplace -f files.f
```

#### Multiple files or wildcards

```bash
# Multiple files
docker run --rm -v "$(pwd)":/data -w /data ghcr.io/verihogg/verihogg-format:latest \
  verihogg-format file1.sv file2.sv file3.sv

# All .sv files in current directory
docker run --rm -v "$(pwd)":/data -w /data ghcr.io/verihogg/verihogg-format:latest \
  sh -c 'verihogg-format --inplace *.sv'
```

---

## Build and run locally

### 1. Build from source (Nix)

Use Nix to build the project and all required dependencies in a reproducible environment.

```bash
git clone https://github.com/verihogg/verihogg-format.git
cd verihogg-format
nix-build
```

This creates a `result` symlink pointing to the built output in the Nix store.

### 2. Run from console (local build)

#### Single file

```bash
./result/bin/verihogg-format mymodule.sv
```

#### In-place formatting

```bash
./result/bin/verihogg-format --inplace mymodule.sv
./result/bin/verihogg-format --inplace src/**/*.sv
```

#### stdin / stdout

```bash
cat mymodule.sv | ./result/bin/verihogg-format
```

#### Custom settings

```bash
./result/bin/verihogg-format --inplace \
  --column_limit 120 \
  --indentation_spaces 4 \
  src/**/*.sv
```

You can run `./result/bin/verihogg-format --help` to see all available options.

### Requirements (without Nix)

If you don't use Nix, install the following manually:

- C++20 compatible compiler
- CMake 3.20+
- [slang](https://github.com/MikePopoloski/slang) — SystemVerilog parser
- [Microsoft GSL](https://github.com/microsoft/GSL) — Guidelines Support Library
- Google Test (optional, for tests)

Then build:

```bash
cmake -B build
cmake --build build
```

Binary will be at `build/bin/verihogg-format`.

---

## Testing

Tests use source files from the [SCR1](https://github.com/syntacore/scr1) RISC-V core project (by Syntacore) and are built with the Google Test framework.

To run tests:

```bash
cmake -B build
cmake --build build
ctest --output-on-failure --test-dir build
```

---

## License

MIT

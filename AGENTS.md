# verihogg-format — agent instructions

## Build

```bash
# CMake (binary → build/bin/verihogg-format)
cmake -B build && cmake --build build

# Nix (binary → result/bin/verihogg-format)
nix-build

# Docker
docker build -t verihogg-format .
```

Use `nix-shell` (or `direnv allow`) for a dev shell with clang-tools, cmake, ninja. The `.envrc` runs `use flake`, so `direnv` auto-loads.

## Test

```bash
# Requires SCR1_ROOT pointing to a checkout of https://github.com/syntacore/scr1
cmake -B build -DSCR1_ROOT=/path/to/scr1 && cmake --build build
ctest --output-on-failure --test-dir build
# or direct:
build/bin/verihogg-tests
```

Tests skip automatically if `SCR1_ROOT` is unset. Integration tests format all SCR1 `.sv`/`.svh` files and verify the slang syntax tree is preserved.

## Lint / format

```bash
# Format C++ sources with clang-format
bash scripts/format.sh

# Check only (dry-run)
bash scripts/check-format.sh

# clang-tidy (requires build/ with CMAKE_EXPORT_COMPILE_COMMANDS=ON)
bash scripts/lint.sh build
```

All clang-tidy warnings are errors. Any change must pass `scripts/check-format.sh` and `scripts/lint.sh build`.

## Code conventions

- C++20, trailing return types (`auto func() -> int`).
- Google style for clang-format (`.clang-format`).
- Naming enforced by clang-tidy: `CamelCase` for functions/classes/types, `kCamelCase` for constants, `camelBack` for locals and params. `modernize-use-trailing-return-type` is on.
- `#pragma once`, no `#include` guards.
- All source lives under `formatter/` — headers in `formatter/include/`, sources in `formatter/src/`, tests in `formatter/tests/`.

## Architecture

Single binary `verihogg-format`. Entry point: `formatter/src/main.cpp`.

Formatting is a 5-stage pipeline (see `formatter/src/formatter.cpp`):
1. `TreeUnwrapper` — parse tokens into unwrapped lines
2. `TokenAnnotator` — annotate token roles
3. `LineJoiner` — join short consecutive lines
4. `tabular_aligner::align` — align port/declaration columns
5. `Printer` — emit formatted text

Automatic line wrapping (breaking long lines) is **not yet implemented**.

## CI

5 workflows under `.github/workflows/`:
- `build.yml` — `nix-build`
- `clang-format.yml` — `scripts/check-format.sh`
- `clang-tidy.yml` — cmake debug + `scripts/lint.sh build`
- `docker-build.yml` — Docker image build on every push/PR
- `docker-push.yml` — pushes to `ghcr.io/verihogg/verihogg-format:latest` on main-branch pushes

All CI runs on `ubuntu-24.04` via Nix.

## CLI

| Option | Description |
|--------|-------------|
| `-n, --inplace` | Overwrite files in place |
| `-h, --help` | Show help |

Exit codes: `0` ok, `1` invalid args or parse error. Docker image at `ghcr.io/verihogg/verihogg-format`.

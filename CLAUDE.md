# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MASP (Assembly Preprocessor) is a fork of GASP (GNU Assembler Preprocessor) with modifications. It's a macro preprocessor for assembly language that adds directives, macros, and conditional assembly support before passing output to an assembler like GAS.

Key differences from GASP:
- Default directive prefix is `\` instead of `.` (configurable via `-P/--prefixchar`)
- Number prefix syntax changed (e.g., `0b0011` for binary instead of `B'0011`)
- Macro syntax requires commas between arguments
- Mode switching: `\masp` and `\gasp` directives to switch between syntaxes
- No external dependencies (libiberty removed)

## Build Commands

### Standard build:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The `masp` executable will be in `build/src/masp`.

### Build with AddressSanitizer (enabled by default):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

### Build without AddressSanitizer:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=OFF
cmake --build build -j
```

### Run tests:
```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Or use the 'check' target
cmake --build build --target check
```

**Important**: Unit tests only run when ASAN is enabled. Without ASAN, MASP has known memory corruption bugs that cause crashes. Tests are automatically skipped when `ENABLE_ASAN=OFF`.

### Install:
```bash
cmake --install build --prefix /usr/local
```

## Architecture

### Core Components

1. **String Buffers (`sb.h`, `sb.c`)**
   - Custom string buffer implementation (`sb` struct)
   - Avoids null-terminated string manipulation issues
   - Provides efficient string growth and allocation
   - Used throughout for text manipulation

2. **Macro System (`macro.h`, `macro.c`)**
   - Handles macro definitions and expansions
   - `formal_entry`: describes macro formal arguments
   - `macro_entry`: describes complete macro with substitution text
   - Maintains hash tables for fast formal argument lookup
   - Supports nested macros (tracked via `macro_nest`)

3. **Hash Tables (`hash.h`, `hash.c`)**
   - Used for symbol lookup and macro storage
   - Generic hash table implementation

4. **Main Preprocessor (`masp.c`)**
   - ~3000+ lines of core preprocessing logic
   - Character classification system via `chartype[]` array with bit flags:
     - `FIRSTBIT`, `NEXTBIT`: identifier characters
     - `SEPBIT`, `WHITEBIT`: separators and whitespace
     - `COMMENTBIT`: comment characters
     - `BASEBIT`: base prefix characters
   - Directive processing system with keyword constants (K_EQU, K_MACRO, etc.)
   - Conditional assembly via `ifstack[]` (max 100 nesting levels)
   - Mode switching between MASP and GASP syntax
   - Number base conversion (0b, 0q, 0h, 0d, 0a prefixes)

5. **Compatibility Layer (`compat.h`, `compat.c`)**
   - Portability abstractions
   - Platform-specific implementations

### Build System

- CMake 3.15+ required
- `src/CMakeLists.txt`: defines MASP_SOURCES, generates config.h
- Compiler warnings are conditional (checked via CheckCCompilerFlag)
- MinGW builds link against `gnurx` for POSIX regex support
- ASAN is enabled by default (`-DENABLE_ASAN=ON`)
- Optional UBSan in CI: `-fsanitize=undefined`

### Test Structure

- **Unit tests** (`test/unit/`):
  - `test_masp_cli.c`: in-process CLI tests (links against MASP object files)
  - `test_stress_parallel.c`: stress test for concurrent usage
  - Tests compile MASP sources directly to avoid CLI parsing issues
  - Only run when ASAN is enabled

- **Integration test files** (`test/*.vcl`):
  - Assembly files for testing preprocessor output
  - `test/include/`: self-contained test dependencies

### Memory Safety

**Critical**: MASP has known memory corruption bugs that cause crashes (SIGABRT) without AddressSanitizer. ASAN is enabled by default and should only be disabled for production builds where performance is critical.

When running with ASAN in CI/CD:
- Leak detection is disabled (`detect_leaks=0`)
- Known minor leaks (~36KB at exit) are not critical
- Focus is on crashes, buffer overflows, and use-after-free

### CI/CD

GitHub Actions workflow (`.github/workflows/compilation.yml`):
- Tests on macOS (ARM64, x86_64), Ubuntu, Windows (MinGW)
- Matrix builds with/without ASAN
- Tests only run when ASAN is enabled
- ASAN options: `check_initialization_order=1:strict_string_checks=1:detect_leaks=0`

## Development Notes

### When modifying the preprocessor:

1. **Directive handling**: New directives are added in `masp.c`:
   - Define a K_* constant (e.g., `K_MASP`, `K_GASP`)
   - Add to the keyword processing system
   - Handle in the main processing loop

2. **Macro changes**: Be careful with macro argument handling:
   - Arguments are comma-separated
   - Escape sequences: `\,` for commas, `\\` for backslashes in macro args
   - String literals passed as-is (but `\` needs escaping)

3. **String operations**: Always use `sb_*` functions:
   - `sb_new()`, `sb_kill()` for allocation/deallocation
   - `sb_add_char()`, `sb_add_string()`, `sb_add_buffer()` for appending
   - `sb_reset()` to clear without deallocating
   - Never manipulate `sb.ptr` directly

4. **Testing strategy**:
   - Always build with ASAN during development
   - Run tests with `ctest --test-dir build --output-on-failure`
   - Tests are designed to catch memory corruption early
   - Add new test cases to `test/unit/test_masp_cli.c`

### Platform-specific concerns:

- **Windows/MinGW**: Requires `gnurx` library for POSIX regex
- **macOS**: Tested on both ARM64 and Intel
- **POSIX regex**: Used in `masp.c` (needs `<regex.h>`)

### Known limitations (from README):

- No comprehensive documentation
- Inherited bugs from original GASP
- MRI compatibility mode and alternate syntax deprecated
- Macro comments can cause issues (avoid in macro definitions and calls)
- Comments are not processed (syntax errors in comments are ignored)

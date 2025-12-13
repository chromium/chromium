# Overview of the //tools Directory

This document provides a high-level guide to the scripts and utilities in the
`//tools` directory. Its purpose is to improve the discoverability of these
tools for both human developers and AI assistants.

Many tools have their own `README.md` files with more detailed information.

## Tool Categories

### Build & Compilation

*   **`gn/`**: Contains helper scripts for the GN meta-build system.
*   **`grit/`**: The Google Resource and Internationalization Tool. Used for
    processing `.grd` files, which contain strings, images, and other resources
    that are compiled into the binary. See `grit/README.md` for details.
*   **`json_schema_compiler/`**: A tool that takes a JSON schema as input and
    generates C++ classes for serialization and deserialization.
*   **`mb/`**: The Meta-Build wrapper. A Python script that wraps GN argument
    generation and Ninja invocation for different configurations. See the
    [user_guide.md](mb/docs/user_guide.md) for more.
*   **`pgo/`**: Tools for Profile-Guided Optimization.

### Code Analysis & Formatting

*   **`clang/`**: Contains scripts for managing and using the Clang compiler and
    its related tools, such as `clang-format` and `clang-tidy`.
*   **`checklicenses/`**: A script to check that all third-party code has an
    appropriate license. See [licenses/README.md](licenses/README.md).
*   **`pylint/`**: Scripts for running Python lint checks.
*   **`code_analysis/`**: A directory for helper scripts that perform static
    analysis on the codebase.
    *   `find_mojo_implementations.py`: A script to find the C++ classes that
        implement a given Mojo interface.
*   **`uberblame.py`**: A powerful tool for assigning blame for code changes.

### Testing & Test Management

*   **`bisect/`**: Home of `bisect-builds.py`, a powerful script for
    automatically bisecting a regression by downloading pre-built revisions of
    Chrome.
*   **`perf/`**: The primary location for performance testing tools and test
    suites. This is a large and complex area of the codebase.
*   **`run-swarmed.py`**: A script for running tests on the Swarming distributed
    testing infrastructure.
*   **`flakiness/`**: Tools for dealing with test flakiness. See
    [flakiness/README.md](flakiness/README.md).

### Dependency Management

*   **Note:** Most dependency management is handled by `depot_tools`, which is
    not in this directory. However, some in-tree tools exist.
*   **`crates/`**: Scripts for managing third-party Rust libraries (crates) from
    `crates.io`.

### Performance Analysis & Debugging

*   **`profiling/`**: Contains various scripts for profiling and analyzing
    performance, including heap and CPU profilers.
*   **`memory/`**: Tools specifically for memory analysis, such as scripts for
    investigating memory leaks.
*   **`binary_size/`**: Scripts for analyzing and diagnosing the binary size of
    Chrome.
*   **`tracing/`**: Scripts for analyzing performance traces. See
    [tracing/README.md](tracing/README.md).
*   **`gdb/`**: GDB-related tools and scripts. See [gdb/README.md](gdb/README.md).
*   **`valgrind/`**: Valgrind-related tools and suppressions. See
    [valgrind/README.md](valgrind/README.md).
*   **`symsrc/`**: Tools for managing symbols and source indexing. See
    [symsrc/README.chromium](symsrc/README.chromium).

### Metrics & Telemetry

*   **`metrics/`**: Tools for UMA histograms and other metrics. See
    [metrics/histograms/README.md](metrics/histograms/README.md) and
    [metrics/README.md](metrics/README.md).
*   **`traffic_annotation/`**: Tools for annotating network traffic. See
    [traffic_annotation/README.md](traffic_annotation/README.md).

### WebUI

*   **`style_variable_generator/`**: Tools for generating C++ files from CSS
    variables. See
    [style_variable_generator/README.md](style_variable_generator/README.md).
*   **`typescript/`**: Tools for working with TypeScript in WebUI.

### Developer Workflow

*   **`cr/`**: A wrapper for many common developer commands. See
    [cr/README](cr/README).

### Platform-Specific Utilities

*   **`android/`**: Tools specific to the Android build and development process.
*   **`chromeos/`**: Tools specific to ChromeOS development.
*   **`fuchsia/`**: Tools specific to Fuchsia development.
*   **`mac/`**: Tools specific to macOS development.
*   **`win/`**: Tools specific to Windows development.
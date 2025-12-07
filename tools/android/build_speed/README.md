# Android Build Speed Tools

[TOC]

## Best Practices

- Ensure that the workstation is not being used while running any benchmarks.
  This helps increase the reproducibility of these benchmarks.
- Avoid making code edits while a benchmark is running, since it may break
  in-progress builds.

## Tool: `benchmark.py`

This script can be used to measure several benchmarks for build speed, such as
GN generation time and incremental build times after various simulated code
changes.

This tool will modify certain source files in your current src repository during
benchmarks. It will attempt to restore those files to their original content
after completing all benchmarks.

### Quick Start

To run a common suite of incremental build benchmarks on an emulator, run:

```bash
tools/android/build_speed/benchmark.py all_incremental --emulator <your_avd_name>
```

Where `<your_avd_name>` can be a filename like:
`android_30_google_apis_x86.textpb`.

### Command-line Options

```
usage: benchmark.py [-h] [--bundle] [--test] [--no-server]
                    [--no-incremental-install] [--no-component-build]
                    [--build-64bit] [-r REPEAT] [-C OUTPUT_DIRECTORY]
                    [--emulator EMULATOR] [--target TARGET] [-v] [-q] [--json]
                    [-n]
                    [BENCHMARK ...]

Tool to run build benchmarks (e.g. incremental build time).

Example Command:
    tools/android/build_speed/benchmark.py all_incremental

Example Output:
    Summary
    gn args: target_os="android" use_remoteexec=true incremental_install=true
    gn_gen: 6.7s
    chrome_nosig: 36.1s avg (35.9s, 36.3s)
    chrome_sig: 38.9s avg (38.8s, 39.1s)
    base_nosig: 41.0s avg (41.1s, 40.9s)
    base_sig: 93.1s avg (93.1s, 93.2s)

Note: This tool will make edits on files in your local repo. It will revert the
      edits afterwards.

Suites and Individual Benchmarks:
    all_incremental
    all_chrome_java
    all_module_java
    all_base_java
    extra_incremental
    chrome_nosig
    chrome_sig
    module_public_sig
    module_internal_nosig
    base_nosig
    base_sig
    turbine_headers
    compile_java
    errorprone
    write_build_config
    cta_test_sig

positional arguments:
  BENCHMARK             Names of benchmark(s) or suites(s) to run.

options:
  -h, --help            show this help message and exit
  --bundle              Switch the default target from apk to bundle.
  --test                Switch the default target to a test apk.
  --no-server           Do not start a faster local dev server before running
                        the test.
  --no-incremental-install
                        Do not use incremental install.
  --no-component-build  Turn off component build.
  --build-64bit         Build 64-bit by default, even with no emulator.
  -r, --repeat REPEAT   Number of times to repeat the benchmark.
  -C, --output-directory OUTPUT_DIRECTORY
                        If outdir is not provided, will attempt to guess.
  --emulator EMULATOR   Specify this to override the default emulator.
  --target TARGET       Specify this to override the default target.
  -v, --verbose         1 to print logging, 2 to print ninja output.
  -q, --quiet           Do not print the summary.
  --json                Output machine-readable output per benchmark.
  -n, --dry-run         Do everything except the build/test/run steps, which
                        will return random times.
```

### Usage

To see all available options, benchmarks, and suites, run:

```bash
tools/android/build_speed/benchmark.py -h
```

You can run individual benchmarks or suites of benchmarks. Suites are a
collection of benchmarks and can be passed by name the same way as any
individual benchmark. This is convenient if you want to run a set of benchmarks
one after the other. e.g. the `all_incremental` suite runs all incremental
benchmarks in serial.

To repeat a benchmark multiple times, use the `-r` or `--repeat` flag:

```bash
tools/android/build_speed/benchmark.py -r 3 chrome_sig
```

### Emulator

Some benchmarks measure install and run times, which require a running Android
emulator. Use the `--emulator` flag to specify the AVD config name for the
emulator you want to use. The available emulator configs can be found in
[`//tools/android/avd/proto`](../../../tools/android/avd/proto).

If an emulator is required and not specified, the script will error.

### Output

The script outputs timings for different stages of the build process. For a
benchmark named `example_benchmark`, you might see:

- `example_benchmark_compile`: Time to compile the target after the code change.
- `example_benchmark_install`: Time to install the target on the device.
- `example_benchmark_run`: Time to run the test on the device (for test
  targets).

### Configuration

You can customize the build configuration with various flags:

- `--no-server`: Do not use the faster local development server.
- `--no-incremental-install`: Disable incremental install.
- `--bundle` or `--test`: Change the build target to a bundle or test APK
  instead of the default APK.

Since most benchmarks require the full capacity and parallelism of the
workstation, there is no option to run benchmarks in parallel.

### Running Tests

To run the unit tests for this script, use the following command:

```bash
vpython3 tools/android/build_speed/benchmark_unittest.py
```

## Future plans

- Add a script to compare benchmarks between two revisions.
- Add options to allow ninja tracing files to be stored and examined for each
  benchmark.
- Separate out time used by ninja versus time used by build steps.

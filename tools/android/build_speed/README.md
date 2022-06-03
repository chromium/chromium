# Android Build Speed Tools

[TOC]

## Best Practices

* Ensure that the workstation is not being used while running any benchmarks.
  This helps increase the reproducibility of these benchmarks.
* Avoid making code edits while a benchmark is running, since it may break
  in-progress builds.

## Tool: `benchmark.py`

This script can be used to measure several benchmarks for build speed. Simply
run `tools/android/build_speed/benchmark.py -h` to see example input/output and
a list of available benchmarks and suites.

Suites are a collection of benchmarks and can be passed by name the same way as
any individual benchmark. This is convenient if you want to run a set of
benchmarks one after the other. e.g. the `all_incremental` suite runs all
incremental benchmarks in serial.

Since most benchmarks require the full capacity and parallelism of the
workstation, there is no option to run benchmarks in parallel.

This tool will modify certain source files in your current src repository during
benchmarks. It will attempt to restore those files to their original content
after completing all benchmarks.

## Future plans

* Add a script to compare benchmarks between two revisions.
* Add options to allow ninja tracing files to be stored and examined for each
  benchmark.
* Separate out time used by ninja versus time used by build steps.

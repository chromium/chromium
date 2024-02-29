<!-- Copyright 2024 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# Shared Storage Performance Benchmarks

## Overview

This directory houses performance benchmark tests for the Shared Storage API.

See the
[Shared Storage API Explainer](https://github.com/WICG/shared-storage/blob/main/README.md)
for more details about the API itself.

## How to Run

This assumes you are in the root directory of a local
[chromium checkout](https://chromium.googlesource.com/chromium/src/+/main/docs/get_the_code.md).

Select one of the available shared storage benchmarks, where the qualifier
describes the size of the database after setup and prior to the test:
* `shared_storage.fresh`
* `shared_storage.small`
* `shared_storage.medium`
* `shared_storage.large`

Run the following bash command in your Chromium source directory to see a list of available browser types on your machine:
```bash
tools/perf/run_benchmark --browser=list
```

Sample output from the above command where four available types are found:
```
Available browsers:
   desktop
     content-shell-default
     default
     stable
     system
```

Select a browser type from the list, e.g. `system`, which refers to your system's default installation of Chrome.

Run the following bash command in your Chromium source directory, substituting in your chosen benchmark and
browser type:
```bash
tools/perf/run_benchmark shared_storage.small --browser=system
```

Alternatively you can omit the browser type in the command if you select browser type `default`:
```bash
tools/perf/run_benchmark shared_storage.small
```

Note that the `default` browser type is only available if you have a compiled build in a Chromium out directory named "Default". (The latest compiled build will be used.)

Similarly, you can make `release` and `debug` browser types available by having compiled release and debug builds in Chromium out directories named "Release" and "Debug", respectively.

Another alternative to selecting browser type is to pass your build directory via `--chromium-output-directory`, as long as you have named your out directory as one of the recognized out directories "Default", "Release", "Release_x64", "Debug", or "Debug_x64".

For example, the following command uses the `debug` browser type, or in other words the most recent build in `$CHROMIUM_SRC/out/Default`, where `$CHROMIUM_SRC` is your Chromium source directory:

```bash
tools/perf/run_benchmark shared_storage.small --chromium-output-directory=out/Debug
```

Optionally, you can qualify your command further as follows:
* `--story-filter=STORY_FILTER`
        Only use stories whose names match the given filter regexp.
* `--iterations=ITERATIONS`
        Override the default number of action iterations per story with the
        given number. Default is 10. Maximum allowed is 10.
* `--pageset-repeat=PAGESET_REPEAT`
        Number of times to repeat the entire story set. Default is 10.
* `--xvfb`
        Runs tests with Xvfb server if possible.
* `--verbose-cpu-metrics`
        Enables non-UMA CPU metrics.
* `--verbose-memory-metrics`
        Enables non-UMA memory metrics.
* `-v`, `--verbose`
        Increase verbosity level (repeat as needed).

More options can be found by running:
```bash
tools/perf/run_benchmark run --help

```

For example, a modified version of the benchmark command is:
```bash
tools/perf/run_benchmark shared_storage.small --chromium-output-directory=out/Debug --story-filter=Append --iterations=5 --pageset-repeat=1 --xvfb --verbose-cpu-metrics --verbose-memory-metrics --verbose
```

## Post-Test Result Processing

By default, shared storage perf test results will be displayed in a file at path `$CHROMIUM_SRC/tools/perf/results.html`, where `$CHROMIUM_SRC` is your Chromium source directory.

The shared storage perf tests have a script to process results further into various human-friendly files with paths `$CHROMIUM_SRC/tools/perf/contrib/shared_storage/data/histograms_*.json`.

To run the script, run the following command manually after a perf test run completes:
```bash
tools/perf/contrib/shared_storage/process_results
```

In particular, the above script will compare the actual histogram sample counts recorded during the run with the expected counts, and notify you of any deltas. If there are any non-zero deltas.

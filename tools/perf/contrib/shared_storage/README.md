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

Select a browser type:
* `system`
* `stable`

Run the following bash command, substituting in your chosen benchmark and
browser type:
```bash
tools/perf/run_benchmark shared_storage.small --browser=system
```

Optionally, you can qualify your command further as follows:
* `--story-filter=STORY_FILTER`
        Only use stories whose names match the given filter regexp.
* `--iterations=ITERATIONS`
        Override the default number of action iterations per story with the
        given number. Default is 10. Maximum allowed is 10.
* `--pageset-repeat=PAGESET_REPEAT`
        Number of times to repeat the entire story set. Default is 10.
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

For example, a modified version of the original benchmark command is:
```bash
tools/perf/run_benchmark shared_storage.small --browser=system --story-filter=Append --iterations=5 --pageset-repeat=1 --verbose-cpu-metrics --verbose-memory-metrics --verbose
```

<!-- Copyright 2019 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# Results Processor

Results Processor is a tool to process intermediate results produced by
Telemetry or other test-running frameworks and extract performance measurements
from them. It expects its input in the form of a directory with the following
contents:
- A `_test_results.jsonl` file.
- Subdirectories with test artifacts (including traces) - one per test result.

## Test results file

The `_test_results.jsonl` file tries to follow the
[LUCI test results format](https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/sink/proto/v1/test_result.proto).
Its every line is a json message of the `testResult` type. There are following
additional conventions:

- The following keys are mandatory:
  - status
  - testPath
- For [json3 output](https://chromium.googlesource.com/chromium/src/+/main/docs/testing/json_test_results_format.md), the following are also mandatory:
  - expected
  - runDuration
- testPath is either in the form `{benchmark_name}/{story_name}` or in the form
`{test_suite_name}.{test_case_name}`. The format to use is specified by the
command-line flag `--test-path-format`.
- TBMv2 metrics, if necessary, are listed as tags with key `tbmv2`.
- Traces, if present, are listed as output artifacts with their names starting
with `trace/`. For details, see the [Traces subsection](#traces) below.
- Device and benchmark diagnostics (also optional) are stored in a special
artifact with the name `diagnostics.json` in the form of a json dict.
Diagnostic names must be listed in the
[reserved infos](https://cs.chromium.org/chromium/src/third_party/catapult/tracing/tracing/value/diagnostics/reserved_infos.py)
module.
- All artifacts files for a single test result are in a separate subdirectory.

### Traces

Traces should be output artifacts with their names starting with `trace/`. The
names should also have an extension. The extension of a trace name corresponds
to its format as follows:

|extension | format
|--- | ---
|.json | Json trace
|.json.gz | Gzipped json trace
|.pb | Proto trace
|.pb.gz | Gzipped proto trace
|.txt | Other (e.g. atrace)
|.txt.gz | Gzipped other

Note that this convention relates to the artifact name only, the actual file
stored in the host does not have to follow it.

### Example

Example of a single testResult message (formatted across multiple lines for ease
of reading):

    {
        "testResult": {
            "testPath": "benchmark/story1",
            "status": "PASS",
            "expected": true,
            "startTime": "2019-10-23T09:43:32.046792Z",
            "runDuration": "15.06s",
            "outputArtifacts": {
                "diagnostics.json": {
                    "contentType": "application/json",
                    "filePath": "/output/run_1/story1/diagnostics.json"
                },
                "trace/cpu/1.json": {
                    "contentType": "application/json",
                    "filePath": "/output/run_1/story1/trace/cpu/1.json"
                },
                "trace/chrome/1.json.gz": {
                    "contentType": "application/gzip",
                    "filePath": "/output/run_1/story1/trace/chrome/1.json.gz"
                }
            },
            "tags": [
                {
                    "key": "tbmv2",
                    "value": "consoleErrorMetric"
                },
                {
                    "key": "tbmv2",
                    "value": "cpuTimeMetric"
                }
            ]
        }
    }

## Two modes of operation

Results Processor can be invoked as a standalone script
[tools/perf/results_processor](https://cs.chromium.org/chromium/src/tools/perf/results_processor?q=tools/perf/results_processor)
or as a python module (see e.g.
[benchmark_runner.py](https://cs.chromium.org/chromium/src/tools/perf/core/benchmark_runner.py)
).

The standalone script has a mandatory argument `--intermediate-dir` that should
point to a directory with intermediate results as described above. If invoked
from another script, the `results_processor.ProcessOptions()` method provides
a reasonable default for the `--intermediate-dir` argument.

In both modes final results are placed inside the output directory provided
by the `--output-dir` argument. By default it’s the base directory of the
running script.


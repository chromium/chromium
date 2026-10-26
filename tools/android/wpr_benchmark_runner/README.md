# On-device WPR benchmark runner

An Android instrumentation APK that runs Web Page Replay (WPR) benchmarks
on device without host orchestration. This is also useful for running benchmarks
on emulators.

The [examples](examples/) folder contains LoadLine2, Speedometer 3.0, and
JetStream 3.0 benchmark definitions. Users must provide their own assets or use
existing assets in Crossbench.

## GN argument

Set this in `args.gn`; each runner APK packages one benchmark definition.

```gn
# JSON definition to package; select any example or your own config.
wpr_benchmark_runner_benchmark_config =
    "//tools/android/wpr_benchmark_runner/examples/loadline2_phone.json"
```

## Browser configuration

Set `browser.package`, `browser.http_port`, and `browser.https_port` in the JSON
definition. Set `browser.flags` to an array of Chrome switches, including the
replay certificate's SPKI allowlist. Replay routing is generated from these
ports; do not supply `--host-resolver-rules`. The examples show these fields.

## Run configuration

- `actions_file`: Crossbench-style JSON or HJSON containing `pages`, named
  `blocks`, and action arrays. Paths are relative to the runner JSON, or start
  with `//` for the Chromium source root.
- `probe`: Set `"enabled": true` to start tracing before the page actions and
  stop afterward. `trace_config` selects the trace categories and buffer settings.
- `trace_processor`: Set `output` to name the collected trace; processing actions
  refer to this name in their `input` fields. Set `"enabled": true` to build and
  package Perfetto's trace processor. Disabled when omitted or `enabled` is
  `false`. The `trace_processor` and `process_llp2_trace` actions require it;
  otherwise packaging fails. Trace collection is configured separately through
  `probe`.
- `output_processing`: Actions executed after the page actions and trace
  collection, such as `process_llp2_trace`, `js`, and `dump_to_syslog`.
- `file_output`: Set `enabled` and a `files` array of `{input, filename}`
  entries. Files are exported only when `-e results-dir <directory>` is supplied.
  Trace files are exported before output processing; result files afterward.

## Actions

Each action sets `action` to a type below. Optional parameters show their
defaults.

| Action | Required parameters | Optional parameters | Description |
| --- | --- | --- | --- |
| `get` | `url` | `target="_self"`, `ready_state="interactive"`, `url_contains=url`, `timeout="10s"` | Navigate in a new (`_new_tab`) or current (`_self`) tab. Wait for document readiness and the URL substring. `ready_state` accepts `any`, `interactive`, or `complete`. Create the first tab with `_new_tab`. |
| `wait` | `duration` | — | Pause for the given duration. |
| `tap` | `position: {"x": ..., "y": ...}` | — | Inject an Android touch at display pixel coordinates. |
| `js` | `script` | `output` (none), `timeout="5s"` | Execute a JavaScript function body through CDP. Use `return` and `output` to store its result. |
| `wait_for_condition` | `condition` | `failure_condition` (none), `timeout="10s"`, `poll_interval_ms=100`, `command_timeout_ms=5000` | Poll a JavaScript body until it returns `true`; fail if the optional failure body returns `true`. |
| `trace_processor` | `input`, `query_file`, `output` | `timeout="120s"` | Run a SQL query on a completed trace and store one CSV result table. Cells remain strings. |
| `process_llp2_trace` | `input`, `query_file`, `version`, `output` | `timeout="120s"` | Query a completed trace and calculate LLP2 phone page metrics and the total score. `version` accepts `2` or `2.x`, such as `2.5.0`. |
| `dump_to_syslog` | `input` | — | Print a result to logcat under `cr_WprBenchmark`. |

## Run

Use an Android 14+ device and Chromium built with `debuggable_apks = true`.

From the device shell:

```sh
am instrument -w \
  org.chromium.wpr_benchmark_runner/org.chromium.wpr_benchmark_runner.WprBenchmarkInstrumentation
```
